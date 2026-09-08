// Copyright RadioGuesser. All Rights Reserved.

using Microsoft.AspNetCore.Mvc;
using Radioguesser.Server.Application.Interfaces;

namespace Radioguesser.Server.Api.Controllers;

[ApiController]
[Route("api/v1/radio")]
public sealed class RadioController : ControllerBase
{
    private readonly IRadioImportService _importService;
    private readonly ILogger<RadioController> _log;

    public RadioController(IRadioImportService importService, ILogger<RadioController> log)
    {
        _importService = importService;
        _log           = log;
    }

    /// <summary>
    /// Trigger a station import from Radio Browser.
    /// TODO: Secure with admin role in production.
    /// </summary>
    [HttpPost("import")]
    public async Task<IActionResult> Import(
        [FromQuery] int limit = 500,
        CancellationToken ct  = default)
    {
        _log.LogInformation("Radio import triggered via API (limit={Limit})", limit);
        var count = await _importService.ImportStationsAsync(limit, ct);
        return Ok(new { imported = count });
    }

    /// <summary>Import stations for a specific country.</summary>
    [HttpPost("import/country/{countryCode}")]
    public async Task<IActionResult> ImportByCountry(
        string countryCode,
        CancellationToken ct = default)
    {
        var count = await _importService.ImportByCountryAsync(countryCode.ToUpperInvariant(), ct);
        return Ok(new { country = countryCode, imported = count });
    }
}
