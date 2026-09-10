// Copyright RadioGuesser. All Rights Reserved.

using Microsoft.AspNetCore.Mvc;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Npgsql;
using Radioguesser.Server.Application.Interfaces;

namespace Radioguesser.Server.Api.Controllers;

[ApiController]
[Route("api/v1/games")]
public sealed class GamesController : ControllerBase
{
    private readonly IScoringService          _scoring;
    private readonly IHttpClientFactory       _http;
    private readonly IConfiguration           _cfg;
    private readonly NpgsqlDataSource         _db;
    private readonly ILogger<GamesController> _log;

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public GamesController(IScoringService s, IHttpClientFactory h, IConfiguration c, NpgsqlDataSource db, ILogger<GamesController> l)
    { _scoring = s; _http = h; _cfg = c; _db = db; _log = l; }

    // ─── POST /api/v1/games/solo/round ────────────────────────────────────────

    [HttpPost("solo/round")]
    public async Task<IActionResult> StartSoloRound(
        [FromBody] StartRoundRequest req, CancellationToken ct = default)
    {
        try
        {
            await using var conn = await OpenDbAsync(ct);

            // Get random station with coordinates via direct SQL
            string id = "", name = "", cc = "", country = "", streamUrl = "";
            double lat = 0, lon = 0;

            await using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = @"
                    SELECT rs.id::text, rs.name, rs.country_code, rs.country,
                           rs.lat, rs.lon,
                           rst.stream_url
                    FROM radio_stations rs
                    JOIN radio_streams rst ON rst.station_id = rs.id AND rst.is_primary = true
                    WHERE rs.is_active = true
                      AND rs.lat IS NOT NULL
                      AND rs.lon IS NOT NULL
                      AND rs.health_status NOT IN (3, 4)
                    ORDER BY RANDOM()
                    LIMIT 1";

                await using var reader = await cmd.ExecuteReaderAsync(ct);
                if (!await reader.ReadAsync(ct))
                    return StatusCode(503, new { error = "No stations with coordinates. Import stations first." });

                id = reader.GetString(0);  name    = reader.GetString(1);
                cc = reader.GetString(2);  country = reader.GetString(3);
                lat = reader.GetDouble(4); lon     = reader.GetDouble(5);
                streamUrl = reader.GetString(6);
            }

            var token  = Guid.NewGuid().ToString("N");
            var gameId = Guid.NewGuid();
            var roundId = Guid.NewGuid();

            await using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = @"
                    INSERT INTO games (id, mode, status, total_rounds, seed, started_at)
                    VALUES (@gid, 0, 1, @tr, @seed, NOW());
                    INSERT INTO game_rounds (id, game_id, round_number, station_id, public_token,
                                            stream_url, display_name, status, started_at, duration_seconds)
                    VALUES (@rid, @gid, @rn, @sid::uuid, @token,
                            @url, 'LIVE RADIO', 1, NOW(), 120);";
                cmd.Parameters.AddWithValue("gid",  gameId);
                cmd.Parameters.AddWithValue("rid",  roundId);
                cmd.Parameters.AddWithValue("tr",   req.TotalRounds);
                cmd.Parameters.AddWithValue("seed", Guid.NewGuid().ToString("N"));
                cmd.Parameters.AddWithValue("rn",   req.RoundNumber);
                cmd.Parameters.AddWithValue("sid",  id);
                cmd.Parameters.AddWithValue("token", token);
                cmd.Parameters.AddWithValue("url",  streamUrl);
                await cmd.ExecuteNonQueryAsync(ct);
            }

            _log.LogInformation("Solo round {R}/{T} — {Name} ({CC}) stream={Url}",
                req.RoundNumber, req.TotalRounds, name, cc, streamUrl);

            return Ok(new { roundToken=token, streamUrl, displayName="LIVE RADIO",
                            roundNumber=req.RoundNumber, totalRounds=req.TotalRounds, durationSeconds=120 });
        }
        catch (Exception ex)
        {
            _log.LogError(ex, "StartSoloRound failed");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    // ─── POST /api/v1/games/solo/guess ────────────────────────────────────────

    [HttpPost("solo/guess")]
    public async Task<IActionResult> SubmitSoloGuess(
        [FromBody] SubmitGuessRequest req, CancellationToken ct = default)
    {
        if (req.Latitude < -90 || req.Latitude > 90 || req.Longitude < -180 || req.Longitude > 180)
            return BadRequest(new { error = "Invalid coordinates" });

        try
        {
            await using var conn = await OpenDbAsync(ct);

            // Get station coords via PostGIS distance in one query
            double actualLat=0, actualLon=0, distanceM=0;
            int score=0;
            string stationName="", country="";
            Guid roundId = Guid.Empty;

            await using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = @"
                    SELECT gr.id,
                           rs.lat, rs.lon,
                           rs.name, rs.country,
                           ST_Distance(
                               ST_SetSRID(ST_MakePoint(@lon, @lat), 4326)::geography,
                               rs.location::geography
                           ) AS dist_m
                    FROM game_rounds gr
                    JOIN radio_stations rs ON rs.id = gr.station_id
                    WHERE gr.public_token = @token AND gr.status = 1
                    LIMIT 1";
                cmd.Parameters.AddWithValue("token", req.RoundToken);
                cmd.Parameters.AddWithValue("lat",   req.Latitude);
                cmd.Parameters.AddWithValue("lon",   req.Longitude);

                await using var reader = await cmd.ExecuteReaderAsync(ct);
                if (!await reader.ReadAsync(ct))
                    return NotFound(new { error = "Round not found or already completed" });

                roundId      = reader.GetGuid(0);
                actualLat    = reader.GetDouble(1);
                actualLon    = reader.GetDouble(2);
                stationName  = reader.GetString(3);
                country      = reader.GetString(4);
                distanceM    = reader.GetDouble(5);
                score        = _scoring.CalculateScore(distanceM);
            }

            // Persist and close
            await using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = @"
                    INSERT INTO player_guesses (round_id, player_id, distance_metres, score, is_locked, submitted_at)
                    VALUES (@rid, '00000000-0000-0000-0000-000000000000'::uuid, @dist, @score, true, NOW());
                    UPDATE game_rounds SET status = 2, ended_at = NOW() WHERE id = @rid;";
                cmd.Parameters.AddWithValue("rid",   roundId);
                cmd.Parameters.AddWithValue("dist",  distanceM);
                cmd.Parameters.AddWithValue("score", score);
                await cmd.ExecuteNonQueryAsync(ct);
            }

            _log.LogInformation("Guess — {Dist:F1}km score={Score} station={Name}", distanceM/1000, score, stationName);

            return Ok(new {
                guessLat=req.Latitude, guessLon=req.Longitude,
                actualLat, actualLon,
                distanceKm=distanceM/1000.0, score, maxScore=_scoring.MaxScore,
                stationName, country });
        }
        catch (Exception ex)
        {
            _log.LogError(ex, "SubmitSoloGuess failed");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    private async Task<NpgsqlConnection> OpenDbAsync(CancellationToken ct)
        => await _db.OpenConnectionAsync(ct);

    private static StringContent Json(object o) =>
        new(JsonSerializer.Serialize(o), Encoding.UTF8, "application/json");
}

public record StartRoundRequest(int RoundNumber = 1, int TotalRounds = 5);
public record SubmitGuessRequest(string RoundToken, double Latitude, double Longitude);
