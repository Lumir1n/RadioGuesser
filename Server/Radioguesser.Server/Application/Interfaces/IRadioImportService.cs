// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Application.Interfaces;

public interface IRadioImportService
{
    /// <summary>Import stations from the configured provider into the database.</summary>
    Task<int> ImportStationsAsync(int limit = 1000, CancellationToken ct = default);

    /// <summary>Import stations for a single country.</summary>
    Task<int> ImportByCountryAsync(string countryCode, CancellationToken ct = default);
}
