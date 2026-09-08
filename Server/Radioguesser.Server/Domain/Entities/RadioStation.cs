// Copyright RadioGuesser. All Rights Reserved.

using NetTopologySuite.Geometries;

namespace Radioguesser.Server.Domain.Entities;

/// <summary>
/// Internal normalised radio station record.
/// Maps to the radio_stations table in PostgreSQL.
/// </summary>
public sealed class RadioStation
{
    public Guid   Id         { get; set; } = Guid.NewGuid();
    public string ExternalId { get; set; } = string.Empty;  // Radio Browser UUID
    public string Name       { get; set; } = string.Empty;
    public string CountryCode{ get; set; } = string.Empty;  // ISO 3166-1 alpha-2
    public string Country    { get; set; } = string.Empty;
    public string City       { get; set; } = string.Empty;
    public string Region     { get; set; } = string.Empty;
    public string Language   { get; set; } = string.Empty;
    public string[] Tags     { get; set; } = [];
    public string Genre      { get; set; } = string.Empty;
    public string Homepage   { get; set; } = string.Empty;
    public string FaviconUrl { get; set; } = string.Empty;

    /// <summary>
    /// PostGIS POINT geometry — authoritative location.
    /// NEVER sent to clients during active gameplay.
    /// </summary>
    public Point? Location { get; set; }

    public RadioHealthStatus HealthStatus { get; set; } = RadioHealthStatus.Unknown;
    public bool   IsActive    { get; set; } = false;
    public string Source      { get; set; } = "radio_browser";

    public DateTime CreatedAt { get; set; } = DateTime.UtcNow;
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;

    // Navigation
    public ICollection<RadioStream> Streams { get; set; } = [];
}
