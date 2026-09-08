// Copyright RadioGuesser. All Rights Reserved.

using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Domain.Entities;
using Radioguesser.Server.Infrastructure.Persistence;

namespace Radioguesser.Server.Application.Services;

public sealed class RadioImportService : IRadioImportService
{
    private readonly IRadioCatalogProvider  _provider;
    private readonly RadioguesserDbContext  _db;
    private readonly ILogger<RadioImportService> _log;

    public RadioImportService(
        IRadioCatalogProvider  provider,
        RadioguesserDbContext  db,
        ILogger<RadioImportService> log)
    {
        _provider = provider;
        _db       = db;
        _log      = log;
    }

    public async Task<int> ImportStationsAsync(int limit = 1000, CancellationToken ct = default)
    {
        _log.LogInformation("Starting station import from {Provider} (limit={Limit})",
            _provider.ProviderName, limit);

        var stations = await _provider.FetchStationsAsync(0, limit, ct);
        return await UpsertStationsAsync(stations, ct);
    }

    public async Task<int> ImportByCountryAsync(string countryCode, CancellationToken ct = default)
    {
        _log.LogInformation("Importing stations for country {Country} from {Provider}",
            countryCode, _provider.ProviderName);

        var stations = await _provider.FetchStationsByCountryAsync(countryCode, ct);
        return await UpsertStationsAsync(stations, ct);
    }

    private async Task<int> UpsertStationsAsync(
        IReadOnlyList<RadioStation> incoming, CancellationToken ct)
    {
        var upserted = 0;

        foreach (var station in incoming)
        {
            var existing = await _db.RadioStations
                .Include(s => s.Streams)
                .FirstOrDefaultAsync(s => s.ExternalId == station.ExternalId, ct);

            if (existing is null)
            {
                _db.RadioStations.Add(station);
                upserted++;
            }
            else
            {
                // Update fields that may have changed
                existing.Name        = station.Name;
                existing.CountryCode = station.CountryCode;
                existing.Country     = station.Country;
                existing.City        = station.City;
                existing.Language    = station.Language;
                existing.Tags        = station.Tags;
                existing.Genre       = station.Genre;
                existing.Homepage    = station.Homepage;
                existing.FaviconUrl  = station.FaviconUrl;
                existing.Location    = station.Location;
                existing.UpdatedAt   = DateTime.UtcNow;

                // Upsert primary stream
                var primaryStream = station.Streams.FirstOrDefault(s => s.IsPrimary);
                if (primaryStream is not null)
                {
                    var existingStream = existing.Streams.FirstOrDefault(s => s.IsPrimary);
                    if (existingStream is null)
                        existing.Streams.Add(primaryStream);
                    else
                        existingStream.StreamUrl = primaryStream.StreamUrl;
                }

                upserted++;
            }
        }

        await _db.SaveChangesAsync(ct);
        _log.LogInformation("Import complete — {Count} stations upserted", upserted);
        return upserted;
    }
}
