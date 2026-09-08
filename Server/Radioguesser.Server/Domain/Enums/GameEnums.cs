// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Domain.Entities;

public enum GameMode
{
    Solo = 0,
    Multiplayer = 1,
    Ranked = 2,
    WorldExplorer = 3,
}

public enum GameStatus
{
    WaitingToStart = 0,
    Active = 1,
    Finished = 2,
    Abandoned = 3,
}

public enum RoundStatus
{
    Pending = 0,
    Active = 1,
    Finished = 2,
}
