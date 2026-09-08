// Copyright RadioGuesser. All Rights Reserved.
// Alternative data access via Supabase REST API (PostgREST)
// Used when direct PostgreSQL connection is not configured.

using System.Net.Http.Headers;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Radioguesser.Server.Infrastructure.Persistence;

/// <summary>
/// Fetches game data via Supabase PostgREST REST API.
/// This is a fallback for development when the direct DB connection
/// is not yet configured. The main production path uses EF Core + Npgsql.
/// </summary>
public sealed class SupabaseGameRepository
{
    private readonly HttpClient _http;
    private readonly ILogger<SupabaseGameRepository> _log;

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public SupabaseGameRepository(IHttpClientFactory factory, ILogger<SupabaseGameRepository> log)
    {
        _http = factory.CreateClient("Supabase");
        _log  = log;
    }

    /// <summary>Get a random active station with GPS coordinates via Supabase REST.</summary>
    public async Task<SupabaseStation?> GetRandomPlayableStationAsync(CancellationToken ct = default)
    {
        // Use public_radio_stations view (safe — no location column)
        // For the actual coordinates we use a separate RPC call with service role
        var response = await _http.GetAsync(
            $"rest/v1/rpc/get_random_playable_station",
            ct);

        if (!response.IsSuccessStatusCode)
        {
            _log.LogWarning("Supabase REST: {Status}", response.StatusCode);
            return null;
        }

        var content = await response.Content.ReadAsStreamAsync(ct);
        return await JsonSerializer.DeserializeAsync<SupabaseStation>(content, JsonOpts, ct);
    }
}

public sealed class SupabaseStation
{
    public Guid    Id          { get; set; }
    public string  Name        { get; set; } = string.Empty;
    public string  CountryCode { get; set; } = string.Empty;
    public string  Country     { get; set; } = string.Empty;
    public string  StreamUrl   { get; set; } = string.Empty;
    public double  Lat         { get; set; }
    public double  Lon         { get; set; }
}
