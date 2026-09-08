// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Domain.Entities;

public enum RadioHealthStatus
{
    Unknown = 0,
    Active = 1,
    TemporarilyUnavailable = 2,
    Failed = 3,
    Blocked = 4,
}
