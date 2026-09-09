// Copyright RadioGuesser. All Rights Reserved.

using Serilog;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Application.Services;
using Radioguesser.Server.Infrastructure.Radio;

// ── Serilog bootstrap ─────────────────────────────────────────────────────────

Log.Logger = new LoggerConfiguration()
    .WriteTo.Console()
    .CreateBootstrapLogger();

try
{
    var builder = WebApplication.CreateBuilder(args);

    // ── Logging ───────────────────────────────────────────────────────────────
    builder.Host.UseSerilog((ctx, services, cfg) =>
        cfg.ReadFrom.Configuration(ctx.Configuration)
           .ReadFrom.Services(services)
           .Enrich.FromLogContext()
           .WriteTo.Console());

    // ── Application services ──────────────────────────────────────────────────
    builder.Services.AddScoped<IRadioCatalogProvider, RadioBrowserProvider>();
    builder.Services.AddScoped<IRadioImportService,   RadioImportService>();
    builder.Services.AddSingleton<IScoringService,    ScoringService>();

    // ── Controllers ───────────────────────────────────────────────────────────
    builder.Services.AddControllers();
    builder.Services.AddEndpointsApiExplorer();

    // ── CORS ──────────────────────────────────────────────────────────────────
    builder.Services.AddCors(opts =>
        opts.AddDefaultPolicy(p => p.AllowAnyOrigin().AllowAnyHeader().AllowAnyMethod()));

    // ── HttpClient: Radio Browser ─────────────────────────────────────────────
    builder.Services.AddHttpClient("RadioBrowser", client =>
    {
        client.BaseAddress = new Uri(
            builder.Configuration["RadioBrowser:BaseUrl"]
            ?? "https://all.api.radio-browser.info/json/");
        client.DefaultRequestHeaders.Add("User-Agent", "Radioguesser/1.0");
    });

    // ── HttpClient: Supabase REST (uses service role — bypasses RLS) ──────────
    builder.Services.AddHttpClient("Supabase", client =>
    {
        var url        = builder.Configuration["Supabase:Url"]
                         ?? throw new InvalidOperationException("Supabase:Url not configured");
        var serviceKey = builder.Configuration["Supabase:ServiceRoleKey"]
                         ?? throw new InvalidOperationException("Supabase:ServiceRoleKey not configured");

        client.BaseAddress = new Uri(url.TrimEnd('/') + "/");
        client.DefaultRequestHeaders.Add("apikey",        serviceKey);
        client.DefaultRequestHeaders.Add("Authorization", $"Bearer {serviceKey}");
        client.DefaultRequestHeaders.Add("Prefer",        "return=representation");
    });

    var app = builder.Build();

    app.UseSerilogRequestLogging();
    app.UseCors();
    app.MapControllers();

    // ── Health check ──────────────────────────────────────────────────────────
    app.MapGet("/health", () => Results.Ok(new { status = "healthy", timestamp = DateTime.UtcNow }));

    Log.Information("RadioGuesser backend starting...");
    app.Run();
}
catch (Exception ex)
{
    Log.Fatal(ex, "RadioGuesser backend failed to start");
    throw;
}
finally
{
    Log.CloseAndFlush();
}
