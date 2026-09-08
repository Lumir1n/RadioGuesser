// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Domain.Entities;

public sealed class GameRound
{
    public Guid      Id           { get; set; } = Guid.NewGuid();
    public Guid      GameId       { get; set; }
    public int       RoundNumber  { get; set; }
    public Guid      StationId    { get; set; }
    public string    PublicToken  { get; set; } = string.Empty;  // Opaque — sent to clients
    public string    StreamUrl    { get; set; } = string.Empty;  // Sent to clients
    public string    DisplayName  { get; set; } = "LIVE RADIO";  // Sent to clients

    public RoundStatus Status     { get; set; } = RoundStatus.Pending;
    public DateTime?   StartedAt  { get; set; }
    public DateTime?   EndedAt    { get; set; }
    public float       DurationSeconds { get; set; } = 120f;

    // Navigation
    public Game?                    Game    { get; set; }
    public RadioStation?            Station { get; set; }
    public ICollection<PlayerGuess> Guesses { get; set; } = [];
}
