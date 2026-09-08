// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Domain.Entities;

public sealed class Game
{
    public Guid      Id        { get; set; } = Guid.NewGuid();
    public GameMode  Mode      { get; set; } = GameMode.Solo;
    public GameStatus Status   { get; set; } = GameStatus.WaitingToStart;
    public int       TotalRounds{ get; set; } = 5;
    public string    Seed      { get; set; } = string.Empty;

    public DateTime  CreatedAt { get; set; } = DateTime.UtcNow;
    public DateTime? StartedAt { get; set; }
    public DateTime? EndedAt   { get; set; }

    // Navigation
    public ICollection<GameRound>  Rounds  { get; set; } = [];
    public ICollection<GamePlayer> Players { get; set; } = [];
}
