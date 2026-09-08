// Copyright RadioGuesser. All Rights Reserved.

using Microsoft.AspNetCore.Mvc;
using System.Text.Json;
using System.Text.Json.Serialization;
using Radioguesser.Server.Application.Interfaces;

namespace Radioguesser.Server.Api.Controllers;

[ApiController]
[Route("api/v1/games")]
public sealed class GamesController : ControllerBase
{
    private readonly IScoringService         _scoring;
    private readonly IHttpClientFactory      _http;
    private readonly IConfiguration          _cfg;
    private readonly ILogger<GamesController> _log;

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public GamesController(
        IScoringService          scoring,
        IHttpClientFactory       http,
        IConfiguration           cfg,
        ILogger<GamesController> log)
    {
        _scoring = scoring;
        _http    = http;
        _cfg     = cfg;
        _log     = log;
    }

    // ── POST /api/v1/games/solo/round ─────────────────────────────────────────
    // Returns a safe round descriptor. Station location is NEVER included.

    [HttpPost("solo/round")]
    public async Task<IActionResult> StartSoloRound(
        [FromBody] StartRoundRequest req,
        CancellationToken ct = default)
    {
        // Get a random station with coordinates via Supabase RPC
        var client = _http.CreateClient("Supabase");
        var resp = await client.PostAsync("rest/v1/rpc/get_random_playable_station",
            new StringContent("{}", System.Text.Encoding.UTF8, "application/json"), ct);

        if (!resp.IsSuccessStatusCode)
        {
            _log.LogWarning("Supabase RPC failed: {Status}", resp.StatusCode);
            return StatusCode(503, new { error = "No stations available" });
        }

        var json  = await resp.Content.ReadAsStringAsync(ct);
        var rows  = JsonSerializer.Deserialize<List<SupabaseStationRow>>(json, JsonOpts) ?? [];

        if (rows.Count == 0)
        {
            return StatusCode(503, new { error = "No stations available. Import stations first." });
        }

        var station = rows[0];

        // Create a game + round record via Supabase insert
        var gameId    = Guid.NewGuid().ToString();
        var roundId   = Guid.NewGuid().ToString();
        var token     = Guid.NewGuid().ToString("N");

        // Insert game
        await client.PostAsync("rest/v1/games",
            JsonContent(new {
                id = gameId, mode = 0, status = 1, total_rounds = req.TotalRounds,
                seed = Guid.NewGuid().ToString("N"), started_at = DateTime.UtcNow
            }), ct);

        // Insert round — station_id and true location stored server-side only
        await client.PostAsync("rest/v1/game_rounds",
            JsonContent(new {
                id = roundId, game_id = gameId,
                round_number = req.RoundNumber, station_id = station.Id,
                public_token = token, stream_url = station.StreamUrl,
                display_name = "LIVE RADIO", status = 1,
                started_at = DateTime.UtcNow, duration_seconds = 120
            }), ct);

        _log.LogInformation("Solo round {R}/{T} created — token={Token} station={Name} ({CC})",
            req.RoundNumber, req.TotalRounds, token, station.Name, station.CountryCode);

        return Ok(new
        {
            roundToken      = token,
            streamUrl       = station.StreamUrl,
            displayName     = "LIVE RADIO",
            roundNumber     = req.RoundNumber,
            totalRounds     = req.TotalRounds,
            durationSeconds = 120,
            // Station location intentionally absent
        });
    }

    // ── POST /api/v1/games/solo/guess ─────────────────────────────────────────

    [HttpPost("solo/guess")]
    public async Task<IActionResult> SubmitSoloGuess(
        [FromBody] SubmitGuessRequest req,
        CancellationToken ct = default)
    {
        if (req.Latitude < -90 || req.Latitude > 90 ||
            req.Longitude < -180 || req.Longitude > 180)
        {
            return BadRequest(new { error = "Invalid coordinates" });
        }

        // Call process_solo_guess RPC — does PostGIS distance calc + persists result
        var client = _http.CreateClient("Supabase");
        var resp = await client.PostAsync("rest/v1/rpc/process_solo_guess",
            JsonContent(new {
                p_round_token = req.RoundToken,
                p_guess_lat   = req.Latitude,
                p_guess_lon   = req.Longitude,
            }), ct);

        if (!resp.IsSuccessStatusCode)
        {
            var err = await resp.Content.ReadAsStringAsync(ct);
            _log.LogWarning("process_solo_guess failed: {Status} {Body}", resp.StatusCode, err);
            return StatusCode(500, new { error = "Could not process guess" });
        }

        var json  = await resp.Content.ReadAsStringAsync(ct);
        var rows  = JsonSerializer.Deserialize<List<GuessResultRow>>(json, JsonOpts) ?? [];

        if (rows.Count == 0)
            return NotFound(new { error = "Round not found or already completed" });

        var r = rows[0];

        _log.LogInformation("Solo guess — distance={Dist:F1}km score={Score}",
            r.DistanceKm, r.Score);

        return Ok(new
        {
            guessLat    = r.GuessLat,
            guessLon    = r.GuessLon,
            actualLat   = r.ActualLat,
            actualLon   = r.ActualLon,
            distanceKm  = r.DistanceKm,
            score       = r.Score,
            maxScore    = r.MaxScore,
            stationName = r.StationName,
            country     = r.Country,
        });
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private static HttpContent JsonContent(object obj) =>
        new StringContent(
            JsonSerializer.Serialize(obj),
            System.Text.Encoding.UTF8,
            "application/json");

    // ── DTOs ──────────────────────────────────────────────────────────────────

    private sealed class SupabaseStationRow
    {
        [JsonPropertyName("id")]          public string Id          { get; set; } = "";
        [JsonPropertyName("name")]        public string Name        { get; set; } = "";
        [JsonPropertyName("country_code")]public string CountryCode { get; set; } = "";
        [JsonPropertyName("country")]     public string Country     { get; set; } = "";
        [JsonPropertyName("stream_url")]  public string StreamUrl   { get; set; } = "";
        [JsonPropertyName("codec")]       public string Codec       { get; set; } = "";
        [JsonPropertyName("bitrate")]     public int    Bitrate     { get; set; }
        [JsonPropertyName("lat")]         public double Lat         { get; set; }
        [JsonPropertyName("lon")]         public double Lon         { get; set; }
    }

    private sealed class GuessResultRow
    {
        [JsonPropertyName("guess_lat")]    public double GuessLat    { get; set; }
        [JsonPropertyName("guess_lon")]    public double GuessLon    { get; set; }
        [JsonPropertyName("actual_lat")]   public double ActualLat   { get; set; }
        [JsonPropertyName("actual_lon")]   public double ActualLon   { get; set; }
        [JsonPropertyName("distance_km")]  public double DistanceKm  { get; set; }
        [JsonPropertyName("score")]        public int    Score       { get; set; }
        [JsonPropertyName("max_score")]    public int    MaxScore    { get; set; }
        [JsonPropertyName("station_name")] public string StationName { get; set; } = "";
        [JsonPropertyName("country")]      public string Country     { get; set; } = "";
    }
}

// ── Request DTOs ──────────────────────────────────────────────────────────────

public record StartRoundRequest(int RoundNumber = 1, int TotalRounds = 5);
public record SubmitGuessRequest(string RoundToken, double Latitude, double Longitude);
