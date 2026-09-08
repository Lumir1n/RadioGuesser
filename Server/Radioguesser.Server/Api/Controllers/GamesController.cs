// Copyright RadioGuesser. All Rights Reserved.

using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using NetTopologySuite.Geometries;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Domain.Entities;
using Radioguesser.Server.Infrastructure.Persistence;

namespace Radioguesser.Server.Api.Controllers;

[ApiController]
[Route("api/v1/games")]
public sealed class GamesController : ControllerBase
{
    private readonly RadioguesserDbContext _db;
    private readonly IScoringService      _scoring;
    private readonly ILogger<GamesController> _log;

    private static readonly GeometryFactory GeoFactory =
        new GeometryFactory(new PrecisionModel(), 4326);

    public GamesController(
        RadioguesserDbContext      db,
        IScoringService            scoring,
        ILogger<GamesController>   log)
    {
        _db      = db;
        _scoring = scoring;
        _log     = log;
    }

    // ── POST /api/v1/games/solo/round ─────────────────────────────────────────
    // Returns a safe round descriptor. Station location is NEVER included.

    [HttpPost("solo/round")]
    public async Task<IActionResult> StartSoloRound(
        [FromBody] StartRoundRequest req,
        CancellationToken ct = default)
    {
        // Select a random active station that has a valid stream and location
        var station = await _db.RadioStations
            .Include(s => s.Streams)
            .Where(s => s.IsActive
                     && s.Location != null
                     && s.HealthStatus != RadioHealthStatus.Blocked
                     && s.HealthStatus != RadioHealthStatus.Failed
                     && s.Streams.Any(st => st.IsPrimary && st.HealthStatus != RadioHealthStatus.Blocked))
            .OrderBy(_ => EF.Functions.Random())
            .FirstOrDefaultAsync(ct);

        if (station is null)
        {
            _log.LogWarning("No suitable station found for solo round");
            return StatusCode(503, new { error = "No stations available. Try importing stations first." });
        }

        var primaryStream = station.Streams.First(s => s.IsPrimary);

        // Create a game + round record
        var game = new Game
        {
            Mode        = GameMode.Solo,
            Status      = GameStatus.Active,
            TotalRounds = req.TotalRounds,
            StartedAt   = DateTime.UtcNow,
            Seed        = Guid.NewGuid().ToString("N"),
        };
        _db.Games.Add(game);

        var round = new GameRound
        {
            GameId          = game.Id,
            RoundNumber     = req.RoundNumber,
            StationId       = station.Id,
            PublicToken     = Guid.NewGuid().ToString("N"),  // opaque — safe to send
            StreamUrl       = primaryStream.StreamUrl,
            DisplayName     = "LIVE RADIO",
            Status          = RoundStatus.Active,
            StartedAt       = DateTime.UtcNow,
            DurationSeconds = 120f,
        };
        _db.GameRounds.Add(round);
        await _db.SaveChangesAsync(ct);

        _log.LogInformation("Solo round {Round}/{Total} created — station={StationId} token={Token}",
            req.RoundNumber, req.TotalRounds, station.Id, round.PublicToken);

        return Ok(new
        {
            roundToken      = round.PublicToken,
            streamUrl       = round.StreamUrl,
            displayName     = round.DisplayName,
            roundNumber     = round.RoundNumber,
            totalRounds     = req.TotalRounds,
            durationSeconds = round.DurationSeconds,
            // Location is intentionally absent
        });
    }

    // ── POST /api/v1/games/solo/guess ─────────────────────────────────────────
    // Receives client guess, computes distance & score server-side.

    [HttpPost("solo/guess")]
    public async Task<IActionResult> SubmitSoloGuess(
        [FromBody] SubmitGuessRequest req,
        CancellationToken ct = default)
    {
        // Validate coordinates
        if (req.Latitude  < -90 || req.Latitude  > 90 ||
            req.Longitude < -180 || req.Longitude > 180)
        {
            return BadRequest(new { error = "Invalid coordinates" });
        }

        // Look up the round by its public token
        var round = await _db.GameRounds
            .Include(r => r.Station)
            .FirstOrDefaultAsync(r => r.PublicToken == req.RoundToken
                                   && r.Status == RoundStatus.Active, ct);

        if (round is null)
        {
            return NotFound(new { error = "Round not found or already completed" });
        }

        if (round.Station?.Location is null)
        {
            _log.LogError("Station {StationId} has no location!", round.StationId);
            return StatusCode(500, new { error = "Station location unavailable" });
        }

        // Build PostGIS point for persistence
        var guessPoint  = GeoFactory.CreatePoint(new Coordinate(req.Longitude, req.Latitude));
        guessPoint.SRID = 4326;

        // Compute great-circle distance in metres using PostGIS geography cast
        // We use raw SQL because EF Core doesn't have a built-in ST_Distance binding
        var distanceMetres = 0.0;
        var conn = _db.Database.GetDbConnection();
        var wasOpen = conn.State == System.Data.ConnectionState.Open;
        if (!wasOpen) await conn.OpenAsync(ct);

        try
        {
            using var cmd = conn.CreateCommand();
            cmd.CommandText = "SELECT ST_Distance(ST_GeomFromEWKT(@guess)::geography, ST_GeomFromEWKT(@station)::geography)";

            var pGuess = cmd.CreateParameter();
            pGuess.ParameterName = "guess";
            pGuess.Value = $"SRID=4326;POINT({req.Longitude} {req.Latitude})";
            cmd.Parameters.Add(pGuess);

            var pStation = cmd.CreateParameter();
            pStation.ParameterName = "station";
            pStation.Value = $"SRID=4326;POINT({round.Station.Location.X} {round.Station.Location.Y})";
            cmd.Parameters.Add(pStation);

            distanceMetres = (double)(await cmd.ExecuteScalarAsync(ct))!;
        }
        finally
        {
            if (!wasOpen) await conn.CloseAsync();
        }

        var score = _scoring.CalculateScore(distanceMetres);

        // Persist the guess
        // For solo mode player_id is anonymous (Guid.Empty until auth is implemented)
        var guess = new PlayerGuess
        {
            RoundId         = round.Id,
            PlayerId        = Guid.Empty,
            GuessLocation   = guessPoint,
            DistanceMetres  = distanceMetres,
            Score           = score,
            IsLocked        = true,
        };
        _db.PlayerGuesses.Add(guess);

        // Close the round
        round.Status  = RoundStatus.Finished;
        round.EndedAt = DateTime.UtcNow;

        await _db.SaveChangesAsync(ct);

        _log.LogInformation("Solo guess — distance={Distance:F0}m score={Score}", distanceMetres, score);

        // Return result WITH actual coordinates (now that round is over)
        return Ok(new
        {
            guessLat    = req.Latitude,
            guessLon    = req.Longitude,
            actualLat   = round.Station.Location.Y,   // PostGIS: Y = latitude
            actualLon   = round.Station.Location.X,   // PostGIS: X = longitude
            distanceKm  = distanceMetres / 1000.0,
            score,
            maxScore    = _scoring.MaxScore,
            stationName = round.Station.Name,
            country     = round.Station.Country,
        });
    }
}

// ── Request DTOs ──────────────────────────────────────────────────────────────

public record StartRoundRequest(int RoundNumber = 1, int TotalRounds = 5);

public record SubmitGuessRequest(
    string RoundToken,
    double Latitude,
    double Longitude);
