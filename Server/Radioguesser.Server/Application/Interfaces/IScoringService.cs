// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Application.Interfaces;

public interface IScoringService
{
    /// <summary>
    /// Calculate score from distance in metres.
    /// Always computed server-side — never trusted from the client.
    /// </summary>
    int CalculateScore(double distanceMetres);

    /// <summary>Maximum possible score per round.</summary>
    int MaxScore { get; }
}
