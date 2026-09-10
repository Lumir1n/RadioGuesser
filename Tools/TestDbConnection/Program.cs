// RadioGuesser — Direct DB Import Tool

using Npgsql;
using System.Text.Json;
using System.Text.Json.Serialization;

var cs = "Host=2a05:d018:175d:b600:a29e:fc33:9217:e4fb;Port=5432;Database=postgres;Username=postgres;Password=518148721742859;SSL Mode=Require;Trust Server Certificate=true;Timeout=30;Command Timeout=60";

var countries = new[] {
    "US","GB","DE","FR","IT","ES","PL","BR","RU","JP","CN","IN","AU","CA","NL",
    "BE","AT","CH","SE","NO","DK","FI","PT","HU","CZ","RO","SK","HR","RS","UA",
    "GR","BG","TR","AR","MX","CO","CL","PE","ZA","EG","NG","KE","MA","TH","ID",
    "KR","TW","PH","VN","MY","SG","NZ","IE","IS","LT","LV","EE"
};

using var http = new HttpClient();
http.DefaultRequestHeaders.Add("User-Agent", "Radioguesser/1.0");
http.BaseAddress = new Uri("https://all.api.radio-browser.info/json/");

await using var conn = new NpgsqlConnection(cs);
await conn.OpenAsync();
Console.WriteLine("Connected! Starting geo-sweep import...");

int total = 0;
var opts = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

foreach (var cc in countries)
{
    try
    {
        var resp = await http.GetAsync($"stations/bycountrycodeexact/{cc}?hidebroken=true&order=votes&reverse=true&limit=50");
        if (!resp.IsSuccessStatusCode) { Console.Write($"\r[{cc}] HTTP {(int)resp.StatusCode}   "); continue; }

        var stations = await JsonSerializer.DeserializeAsync<List<RBStation>>(await resp.Content.ReadAsStreamAsync(), opts) ?? [];
        var geo = stations.Where(s =>
            s.GeoLat.HasValue && s.GeoLong.HasValue &&
            s.GeoLat.Value != 0 && s.GeoLong.Value != 0 &&
            Math.Abs(s.GeoLat.Value) <= 90 && Math.Abs(s.GeoLong.Value) <= 180 &&
            !string.IsNullOrEmpty(s.StationUuid) &&
            !string.IsNullOrEmpty(s.UrlResolved ?? s.Url)).ToList();

        foreach (var s in geo)
        {
            var url  = (s.UrlResolved ?? s.Url)!.Trim();
            var tags = (s.Tags ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

            await using var cmd = conn.CreateCommand();
            cmd.CommandText = @"
                INSERT INTO radio_stations
                    (external_id, name, country_code, country, city, language,
                     tags, genre, homepage, favicon_url,
                     location, lat, lon, health_status, is_active, source)
                VALUES
                    (@eid, @name, @cc, @country, @city, @lang,
                     @tags, @genre, @hp, @fav,
                     ST_SetSRID(ST_MakePoint(@lon, @lat), 4326), @lat, @lon,
                     0, true, 'radio_browser')
                ON CONFLICT (external_id) DO UPDATE SET
                    location = EXCLUDED.location,
                    lat = EXCLUDED.lat, lon = EXCLUDED.lon,
                    is_active = true, updated_at = NOW()
                RETURNING id";
            cmd.Parameters.AddWithValue("eid",     s.StationUuid!);
            cmd.Parameters.AddWithValue("name",    (s.Name ?? "").Trim());
            cmd.Parameters.AddWithValue("cc",      cc);
            cmd.Parameters.AddWithValue("country", s.Country ?? "");
            cmd.Parameters.AddWithValue("city",    s.State ?? "");
            cmd.Parameters.AddWithValue("lang",    s.Language ?? "");
            cmd.Parameters.AddWithValue("tags",    tags);
            cmd.Parameters.AddWithValue("genre",   tags.FirstOrDefault() ?? "");
            cmd.Parameters.AddWithValue("hp",      s.Homepage ?? "");
            cmd.Parameters.AddWithValue("fav",     s.Favicon ?? "");
            cmd.Parameters.AddWithValue("lat",     s.GeoLat!.Value);
            cmd.Parameters.AddWithValue("lon",     s.GeoLong!.Value);

            var sid = (Guid?)await cmd.ExecuteScalarAsync();
            if (sid == null) continue;

            await using var sc = conn.CreateCommand();
            sc.CommandText = @"
                INSERT INTO radio_streams (station_id, stream_url, codec, bitrate, is_primary)
                VALUES (@sid, @url, @codec, @br, true)
                ON CONFLICT (station_id, stream_url) DO NOTHING";
            sc.Parameters.AddWithValue("sid",   sid.Value);
            sc.Parameters.AddWithValue("url",   url);
            sc.Parameters.AddWithValue("codec", s.Codec ?? "");
            sc.Parameters.AddWithValue("br",    s.Bitrate);
            await sc.ExecuteNonQueryAsync();
            total++;
        }
        Console.Write($"\r[{cc}] +{geo.Count} | Total: {total}   ");
    }
    catch (Exception ex) { Console.WriteLine($"\n[{cc}] Error: {ex.Message[..Math.Min(60,ex.Message.Length)]}"); }
}

Console.WriteLine($"\nDone! {total} stations with GPS.");
await using var fc = conn.CreateCommand();
fc.CommandText = "SELECT COUNT(*) total, COUNT(lat) lats FROM radio_stations";
await using var fr = await fc.ExecuteReaderAsync();
await fr.ReadAsync();
Console.WriteLine($"DB: total={fr[0]} with_lat={fr[1]}");

class RBStation {
    [JsonPropertyName("stationuuid")]  public string? StationUuid { get; set; }
    [JsonPropertyName("name")]         public string? Name        { get; set; }
    [JsonPropertyName("url")]          public string? Url         { get; set; }
    [JsonPropertyName("url_resolved")] public string? UrlResolved { get; set; }
    [JsonPropertyName("homepage")]     public string? Homepage    { get; set; }
    [JsonPropertyName("favicon")]      public string? Favicon     { get; set; }
    [JsonPropertyName("country")]      public string? Country     { get; set; }
    [JsonPropertyName("state")]        public string? State       { get; set; }
    [JsonPropertyName("language")]     public string? Language    { get; set; }
    [JsonPropertyName("tags")]         public string? Tags        { get; set; }
    [JsonPropertyName("codec")]        public string? Codec       { get; set; }
    [JsonPropertyName("bitrate")]      public int     Bitrate     { get; set; }
    [JsonPropertyName("geo_lat")]      public double? GeoLat      { get; set; }
    [JsonPropertyName("geo_long")]     public double? GeoLong     { get; set; }
}
