// Copyright RadioGuesser. All Rights Reserved.

using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.Extensions.Logging;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Domain.Entities;

namespace Radioguesser.Server.Application.Services;

/// <summary>
/// Imports stations from IRadioCatalogProvider into Supabase via REST API.
/// Does not require a direct PostgreSQL connection.
/// </summary>
public sealed class RadioImportService : IRadioImportService
{
    private readonly IRadioCatalogProvider  _provider;
    private readonly IHttpClientFactory     _http;
    private readonly ILogger<RadioImportService> _log;

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNamingPolicy        = JsonNamingPolicy.SnakeCaseLower,
        DefaultIgnoreCondition      = JsonIgnoreCondition.WhenWritingNull,
    };

    public RadioImportService(
        IRadioCatalogProvider       provider,
        IHttpClientFactory          http,
        ILogger<RadioImportService> log)
    {
        _provider = provider;
        _http     = http;
        _log      = log;
    }

    public async Task<int> ImportStationsAsync(int limit = 1000, CancellationToken ct = default)
    {
        _log.LogInformation("Importing {Limit} stations from {Provider}", limit, _provider.ProviderName);
        var stations = await _provider.FetchStationsAsync(0, limit, ct);
        return await UpsertAsync(stations, ct);
    }

    public async Task<int> ImportByCountryAsync(string countryCode, CancellationToken ct = default)
    {
        _log.LogInformation("Importing stations for {Country}", countryCode);
        var stations = await _provider.FetchStationsByCountryAsync(countryCode, ct);
        return await UpsertAsync(stations, ct);
    }

    private async Task<int> UpsertAsync(IReadOnlyList<RadioStation> stations, CancellationToken ct)
    {
        var client = _http.CreateClient("Supabase");
        int upserted = 0;

        foreach (var station in stations)
        {
            // Build a safe DTO — no geometry type, just lat/lon for the RPC
            var dto = new
            {
                external_id   = station.ExternalId,
                name          = station.Name,
                country_code  = station.CountryCode,
                country       = station.Country,
                city          = station.City,
                language      = station.Language,
                tags          = station.Tags,
                genre         = station.Genre,
                homepage      = station.Homepage,
                favicon_url   = station.FaviconUrl,
                health_status = 0,
                is_active     = true,
                source        = station.Source,
            };

            var resp = await client.PostAsync(
                "rest/v1/radio_stations?on_conflict=external_id",
                new StringContent(JsonSerializer.Serialize(dto, JsonOpts),
                    System.Text.Encoding.UTF8, "application/json"),
                ct);

            if (!resp.IsSuccessStatusCode)
            {
                _log.LogWarning("Upsert failed for {Station}: {Status}", station.Name, resp.StatusCode);
                continue;
            }

            upserted++;
        }

        _log.LogInformation("Upserted {Count}/{Total} stations", upserted, stations.Count);
        return upserted;
    }
}
