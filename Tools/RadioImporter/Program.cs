// RadioGuesser — RadioImporter Tool
// Fetches stations from Radio Browser API and upserts them into Supabase PostgreSQL.
// Usage: dotnet run -- [--country XX] [--limit N]

using System.Text.Json;
using System.Text.Json.Serialization;
using Npgsql;
using NetTopologySuite.Geometries;
using NetTopologySuite.IO;

// ── Configuration ─────────────────────────────────────────────────────────────
// Connection string — populated from env var or .env file.
// Never commit real credentials.
var connectionString = Environment.GetEnvironmentVariable("RG_DB_CONNECTION")
    ?? ReadEnvFile(".env.local", "RG_DB_CONNECTION")
    ?? throw new InvalidOperationException(
        "Set RG_DB_CONNECTION env var.\n" +
        "Example: Host=db.dmwnegtvotnrajzpyfad.supabase.co;Database=postgres;" +
        "Username=postgres;Password=YOUR_PASSWORD;SSL Mode=Require;Trust Server Certificate=true");

var limit   = 1000;
var country = string.Empty;

// Parse CLI args
for (int i = 0; i < args.Length; i++)
{
    if (args[i] == "--limit"   && i + 1 < args.Length) limit   = int.Parse(args[i + 1]);
    if (args[i] == "--country" && i + 1 < args.Length) country = args[i + 1].ToUpperInvariant();
}

Console.WriteLine("=== RadioGuesser Radio Importer ===");
Console.WriteLine($"Target: {(country.Length > 0 ? $"Country={country}" : "Global")} | Limit={limit}");

// ── Fetch from Radio Browser ──────────────────────────────────────────────────
using var http = new HttpClient();
http.DefaultRequestHeaders.Add("User-Agent", "Radioguesser/1.0 (import tool)");
http.BaseAddress = new Uri("https://all.api.radio-browser.info/json/");

string apiUrl = country.Length > 0
    ? $"stations/bycountrycodeexact/{Uri.EscapeDataString(country)}?hidebroken=true&order=votes&reverse=true&limit={limit}"
    : $"stations?hidebroken=true&order=votes&reverse=true&limit={limit}";

Console.WriteLine($"Fetching: {http.BaseAddress}{apiUrl}");

HttpResponseMessage response;
try
{
    response = await http.GetAsync(apiUrl);
    response.EnsureSuccessStatusCode();
}
catch (Exception ex)
{
    Console.WriteLine($"[ERROR] Radio Browser fetch failed: {ex.Message}");
    return 1;
}

var dtos = await JsonSerializer.DeserializeAsync<List<RadioBrowserDto>>(
    await response.Content.ReadAsStreamAsync(),
    new JsonSerializerOptions { PropertyNameCaseInsensitive = true })
    ?? [];

Console.WriteLine($"Fetched {dtos.Count} stations from Radio Browser.");

// ── Write to Supabase ─────────────────────────────────────────────────────────
var dataSourceBuilder = new NpgsqlDataSourceBuilder(connectionString);
dataSourceBuilder.UseNetTopologySuite();
await using var dataSource = dataSourceBuilder.Build();
await using var conn = await dataSource.OpenConnectionAsync();

// NTS WKB writer for PostGIS
var wkbWriter = new WKBWriter { Strict = false };

int inserted = 0, updated = 0, skipped = 0;

foreach (var dto in dtos)
{
    if (string.IsNullOrWhiteSpace(dto.UrlResolved) && string.IsNullOrWhiteSpace(dto.Url))
    {
        skipped++;
        continue;
    }

    var streamUrl  = (dto.UrlResolved ?? dto.Url)!.Trim();
    var externalId = dto.StationUuid?.Trim() ?? string.Empty;
    if (string.IsNullOrEmpty(externalId)) { skipped++; continue; }

    // Build geometry point
    Point? point = null;
    if (dto.GeoLat is double lat && dto.GeoLong is double lon
        && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180
        && !(lat == 0 && lon == 0))
    {
        point = new Point(lon, lat) { SRID = 4326 };
    }

    var tags = (dto.Tags ?? "")
        .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    // Upsert station
    await using var cmd = conn.CreateCommand();
    cmd.CommandText = """
        INSERT INTO radio_stations
            (external_id, name, country_code, country, city, language, tags, genre,
             homepage, favicon_url, location, health_status, is_active, source)
        VALUES
            (@extId, @name, @cc, @country, @city, @lang, @tags, @genre,
             @homepage, @favicon, @loc, 0, TRUE, 'radio_browser')
        ON CONFLICT (external_id) DO UPDATE SET
            name        = EXCLUDED.name,
            country_code= EXCLUDED.country_code,
            country     = EXCLUDED.country,
            city        = EXCLUDED.city,
            language    = EXCLUDED.language,
            tags        = EXCLUDED.tags,
            genre       = EXCLUDED.genre,
            homepage    = EXCLUDED.homepage,
            favicon_url = EXCLUDED.favicon_url,
            location    = EXCLUDED.location,
            is_active   = TRUE,
            updated_at  = NOW()
        RETURNING (xmax = 0) AS inserted;
        """;

    cmd.Parameters.AddWithValue("extId",   externalId);
    cmd.Parameters.AddWithValue("name",    dto.Name?.Trim() ?? "");
    cmd.Parameters.AddWithValue("cc",      dto.CountryCode?.ToUpperInvariant() ?? "");
    cmd.Parameters.AddWithValue("country", dto.Country ?? "");
    cmd.Parameters.AddWithValue("city",    dto.State ?? "");
    cmd.Parameters.AddWithValue("lang",    dto.Language ?? "");
    cmd.Parameters.AddWithValue("tags",    tags);
    cmd.Parameters.AddWithValue("genre",   tags.FirstOrDefault() ?? "");
    cmd.Parameters.AddWithValue("homepage", dto.Homepage ?? "");
    cmd.Parameters.AddWithValue("favicon", dto.Favicon ?? "");
    cmd.Parameters.AddWithValue("loc",     (object?)point ?? DBNull.Value);

    var wasInserted = (bool)(await cmd.ExecuteScalarAsync() ?? false);
    if (wasInserted) inserted++; else updated++;

    // Get or create station id for stream upsert
    await using var idCmd = conn.CreateCommand();
    idCmd.CommandText = "SELECT id FROM radio_stations WHERE external_id = @extId LIMIT 1";
    idCmd.Parameters.AddWithValue("extId", externalId);
    var stationId = (Guid)(await idCmd.ExecuteScalarAsync())!;

    // Upsert primary stream
    await using var streamCmd = conn.CreateCommand();
    streamCmd.CommandText = """
        INSERT INTO radio_streams (station_id, stream_url, codec, bitrate, is_primary)
        VALUES (@sid, @url, @codec, @bitrate, TRUE)
        ON CONFLICT DO NOTHING;
        """;
    streamCmd.Parameters.AddWithValue("sid",    stationId);
    streamCmd.Parameters.AddWithValue("url",    streamUrl);
    streamCmd.Parameters.AddWithValue("codec",  dto.Codec ?? "");
    streamCmd.Parameters.AddWithValue("bitrate", dto.Bitrate);
    await streamCmd.ExecuteNonQueryAsync();

    if ((inserted + updated) % 100 == 0)
        Console.Write($"\r  Progress: {inserted + updated}/{dtos.Count} (inserted={inserted} updated={updated} skipped={skipped})   ");
}

Console.WriteLine();
Console.WriteLine($"\n=== Import complete ===");
Console.WriteLine($"  Inserted : {inserted}");
Console.WriteLine($"  Updated  : {updated}");
Console.WriteLine($"  Skipped  : {skipped}");
Console.WriteLine($"  Total    : {inserted + updated}");
return 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

static string? ReadEnvFile(string path, string key)
{
    if (!File.Exists(path)) return null;
    foreach (var line in File.ReadLines(path))
    {
        var trimmed = line.Trim();
        if (trimmed.StartsWith('#') || !trimmed.Contains('=')) continue;
        var idx = trimmed.IndexOf('=');
        var k   = trimmed[..idx].Trim();
        var v   = trimmed[(idx + 1)..].Trim().Trim('"', '\'');
        if (k == key) return v;
    }
    return null;
}

// ── Radio Browser DTO ─────────────────────────────────────────────────────────

internal sealed class RadioBrowserDto
{
    [JsonPropertyName("stationuuid")]   public string?  StationUuid { get; set; }
    [JsonPropertyName("name")]          public string?  Name        { get; set; }
    [JsonPropertyName("url")]           public string?  Url         { get; set; }
    [JsonPropertyName("url_resolved")]  public string?  UrlResolved { get; set; }
    [JsonPropertyName("homepage")]      public string?  Homepage    { get; set; }
    [JsonPropertyName("favicon")]       public string?  Favicon     { get; set; }
    [JsonPropertyName("country")]       public string?  Country     { get; set; }
    [JsonPropertyName("countrycode")]   public string?  CountryCode { get; set; }
    [JsonPropertyName("state")]         public string?  State       { get; set; }
    [JsonPropertyName("language")]      public string?  Language    { get; set; }
    [JsonPropertyName("tags")]          public string?  Tags        { get; set; }
    [JsonPropertyName("codec")]         public string?  Codec       { get; set; }
    [JsonPropertyName("bitrate")]       public int      Bitrate     { get; set; }
    [JsonPropertyName("geo_lat")]       public double?  GeoLat      { get; set; }
    [JsonPropertyName("geo_long")]      public double?  GeoLong     { get; set; }
}
