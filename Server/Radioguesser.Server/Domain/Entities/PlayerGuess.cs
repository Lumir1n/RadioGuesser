// Copyright RadioGuesser. All Rights Reserved.

using NetTopologySuite.Geometries;

namespace Radioguesser.Server.Domain.Entities;

public sealed class PlayerGuess
{
    public Guid   Id          { get; set; } = Guid.NewGuid();
    public Guid   RoundId     { get; set; }
    public Guid   PlayerId    { get; set; }

    /// <summary>PostGIS POINT of the player's guess.</summary>
    public Point? GuessLocation { get; set; }

    /// <summary>
    /// Distance in metres from guess to actual station location.
    /// Computed server-side via ST_Distance (PostGIS).
    /// </summary>
    public double DistanceMetres { get; set; }

    public int    Score       { get; set; }
    public bool   IsLocked    { get; set; } = false;
    public DateTime SubmittedAt { get; set; } = DateTime.UtcNow;

    // Navigation
    public GameRound? Round { get; set; }
}
