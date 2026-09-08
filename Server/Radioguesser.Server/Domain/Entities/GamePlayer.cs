// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Domain.Entities;

public sealed class GamePlayer
{
    public Guid   Id       { get; set; } = Guid.NewGuid();
    public Guid   GameId   { get; set; }
    public Guid   UserId   { get; set; }
    public int    TotalScore { get; set; } = 0;
    public int    Rank     { get; set; } = 0;

    // Navigation
    public Game?  Game     { get; set; }
}
