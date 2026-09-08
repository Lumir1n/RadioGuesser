// Copyright RadioGuesser. All Rights Reserved.

namespace Radioguesser.Server.Domain.Entities;

/// <summary>
/// A single stream endpoint for a RadioStation.
/// One station can have multiple streams (different codecs/bitrates/mirrors).
/// </summary>
public sealed class RadioStream
{
    public Guid   Id          { get; set; } = Guid.NewGuid();
    public Guid   StationId   { get; set; }
    public string StreamUrl   { get; set; } = string.Empty;
    public string Protocol    { get; set; } = string.Empty;  // e.g. "http", "https"
    public string Codec       { get; set; } = string.Empty;  // e.g. "MP3", "AAC"
    public int    Bitrate     { get; set; } = 0;             // kbps
    public bool   IsPrimary   { get; set; } = true;

    public RadioHealthStatus HealthStatus  { get; set; } = RadioHealthStatus.Unknown;
    public DateTime?         LastCheckedAt { get; set; }
    public DateTime?         LastSuccessAt { get; set; }
    public int               FailureCount  { get; set; } = 0;
    public int?              LatencyMs     { get; set; }
    public int?              HttpStatus    { get; set; }

    public DateTime CreatedAt { get; set; } = DateTime.UtcNow;
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;

    // Navigation
    public RadioStation? Station { get; set; }
}
