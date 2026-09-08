// Copyright RadioGuesser. All Rights Reserved.

using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.Extensions.Logging;
using NetTopologySuite.Geometries;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Domain.Entities;

namespace Radioguesser.Server.Infrastructure.Radio;

/// <summary>
/// Fetches radio stations from the Radio Browser community API and normalises
/// them into the internal RadioStation model.
///
/// External schema is never exposed outside this class.
/// If Radio Browser changes their API, only this file needs updating.
/// </summary>
public sealed class RadioBrowserProvider : IRadioCatalogProvider
{
    public string ProviderName => "radio_browser";

    private readonly IHttpClientFactory _http;
    private readonly ILogger<RadioBrowserProvider> _log;

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public RadioBrowserProvider(IHttpClientFactory http, ILogger<RadioBrowserProvider> log)
    {
        _http = http;
        _log  = log;
    }

    // ── IRadioCatalogProvider ─────────────────────────────────────────────────

    public async Task<IReadOnlyList<RadioStation>> FetchStationsAsync(
        int offset, int limit, CancellationToken ct = default)
    {
        _log.LogInformation("Fetching {Limit} stations at offset {Offset} from Radio Browser",
            limit, offset);

        var url = $"stations?limit={limit}&offset={offset}&hidebroken=true&order=votes&reverse=true";
        return await FetchAndNormalize(url, ct);
    }

    public async Task<IReadOnlyList<RadioStation>> FetchStationsByCountryAsync(
        string countryCode, CancellationToken ct = default)
    {
        _log.LogInformation("Fetching stations for country {Country} from Radio Browser", countryCode);

        var url = $"stations/bycountrycodeexact/{Uri.EscapeDataString(countryCode)}?hidebroken=true&order=votes&reverse=true";
        return await FetchAndNormalize(url, ct);
    }

    // ── Internal ──────────────────────────────────────────────────────────────

    private async Task<IReadOnlyList<RadioStation>> FetchAndNormalize(
        string relativeUrl, CancellationToken ct)
    {
        using var client = _http.CreateClient("RadioBrowser");

        HttpResponseMessage response;
        try
        {
            response = await client.GetAsync(relativeUrl, ct);
            response.EnsureSuccessStatusCode();
        }
        catch (Exception ex)
        {
            _log.LogError(ex, "Radio Browser HTTP request failed: {Url}", relativeUrl);
            return [];
        }

        await using var stream = await response.Content.ReadAsStreamAsync(ct);
        var dtos = await JsonSerializer.DeserializeAsync<List<RadioBrowserStationDto>>(stream, JsonOpts, ct)
                   ?? [];

        _log.LogDebug("Radio Browser returned {Count} stations", dtos.Count);

        var result = new List<RadioStation>(dtos.Count);
        foreach (var dto in dtos)
        {
            var station = Normalize(dto);
            if (station is not null)
                result.Add(station);
        }

        return result;
    }

    /// <summary>
    /// Maps a raw Radio Browser DTO to the internal RadioStation entity.
    /// Stations without a valid stream URL are discarded.
    /// </summary>
    private RadioStation? Normalize(RadioBrowserStationDto dto)
    {
        if (string.IsNullOrWhiteSpace(dto.UrlResolved) &&
            string.IsNullOrWhiteSpace(dto.Url))
        {
            return null;
        }

        var streamUrl = dto.UrlResolved ?? dto.Url ?? string.Empty;

        // Build PostGIS point if coordinates are present and valid
        Point? location = null;
        if (dto.GeoLat is double lat && dto.GeoLong is double lon
            && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180
            && !(lat == 0 && lon == 0))  // Discard stations that haven't set coordinates
        {
            location = new Point(lon, lat) { SRID = 4326 };
        }

        var tags = (dto.Tags ?? string.Empty)
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

        var station = new RadioStation
        {
            ExternalId   = dto.StationUuid ?? string.Empty,
            Name         = dto.Name?.Trim() ?? string.Empty,
            CountryCode  = dto.CountryCode?.ToUpperInvariant() ?? string.Empty,
            Country      = dto.Country ?? string.Empty,
            City         = dto.State ?? string.Empty,   // Radio Browser uses "state" for region/city
            Language     = dto.Language ?? string.Empty,
            Tags         = tags,
            Genre        = tags.FirstOrDefault() ?? string.Empty,
            Homepage     = dto.Homepage ?? string.Empty,
            FaviconUrl   = dto.Favicon ?? string.Empty,
            Location     = location,
            IsActive     = true,
            HealthStatus = RadioHealthStatus.Unknown,
            Source       = ProviderName,
        };

        station.Streams.Add(new RadioStream
        {
            StreamUrl  = streamUrl,
            Codec      = dto.Codec ?? string.Empty,
            Bitrate    = dto.Bitrate,
            IsPrimary  = true,
        });

        return station;
    }

    // ── DTO — Radio Browser API schema ────────────────────────────────────────
    // Isolated here so external schema changes never leak into the domain.

    private sealed class RadioBrowserStationDto
    {
        [JsonPropertyName("stationuuid")]   public string?  StationUuid { get; set; }
        [JsonPropertyName("name")]          public string?  Name        { get; set; }
        [JsonPropertyName("url")]           public string?  Url         { get; set; }
        [JsonPropertyName("url_resolved")]  public string?  UrlResolved { get; set; }
        [JsonPropertyName("homepage")]      public string?  Homepage    { get; set; }
        [JsonPropertyName("favicon")]       public string?  Favicon     { get; set; }
        [JsonPropertyName("country")]       public string?  Country     { get; set; }
        [JsonPropertyName("countrycode")]   public string?  CountryCode { get; set; }
        [JsonPropertyName("state")]         public string?  State       { get; set; }
        [JsonPropertyName("language")]      public string?  Language    { get; set; }
        [JsonPropertyName("tags")]          public string?  Tags        { get; set; }
        [JsonPropertyName("codec")]         public string?  Codec       { get; set; }
        [JsonPropertyName("bitrate")]       public int      Bitrate     { get; set; }
        [JsonPropertyName("geo_lat")]       public double?  GeoLat      { get; set; }
        [JsonPropertyName("geo_long")]      public double?  GeoLong     { get; set; }
        [JsonPropertyName("lastchecktime")] public string?  LastCheckTime { get; set; }
        [JsonPropertyName("lastcheckok")]   public int      LastCheckOk { get; set; }
    }
}
