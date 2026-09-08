// Copyright RadioGuesser. All Rights Reserved.

using Radioguesser.Server.Application.Interfaces;

namespace Radioguesser.Server.Application.Services;

/// <summary>
/// Default scoring: exponential decay from MaxScore based on distance.
/// Algorithm is replaceable by swapping IScoringService — never hardcoded in gameplay.
/// </summary>
public sealed class ScoringService : IScoringService
{
    public int MaxScore => 5000;

    // Decay constant — tune via playtesting
    // Score = MaxScore * e^(-k * distanceKm)
    // At 25 km  ≈ 4900   → k ≈ 0.00082
    // At 500 km ≈ 3300   → k ≈ 0.00082 (fits)
    private const double DecayConstant = 0.00082;

    public int CalculateScore(double distanceMetres)
    {
        if (distanceMetres <= 0) return MaxScore;

        var distanceKm = distanceMetres / 1000.0;
        var raw = MaxScore * Math.Exp(-DecayConstant * distanceKm);
        return Math.Max(0, (int)Math.Round(raw));
    }
}
