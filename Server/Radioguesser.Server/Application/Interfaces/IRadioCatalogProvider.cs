// Copyright RadioGuesser. All Rights Reserved.

using Radioguesser.Server.Domain.Entities;

namespace Radioguesser.Server.Application.Interfaces;

/// <summary>
/// Abstraction over external radio station data sources.
/// Implementations: RadioBrowserProvider, RadioGardenProvider, etc.
/// </summary>
public interface IRadioCatalogProvider
{
    string ProviderName { get; }

    /// <summary>Fetch a page of stations from the external source.</summary>
    Task<IReadOnlyList<RadioStation>> FetchStationsAsync(
        int offset, int limit, CancellationToken ct = default);

    /// <summary>Fetch stations for a specific country (ISO 3166-1 alpha-2).</summary>
    Task<IReadOnlyList<RadioStation>> FetchStationsByCountryAsync(
        string countryCode, CancellationToken ct = default);
}
