// Copyright RadioGuesser. All Rights Reserved.

using Serilog;
using Radioguesser.Server.Infrastructure.Persistence;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Application.Services;
using Radioguesser.Server.Infrastructure.Radio;
using Microsoft.EntityFrameworkCore;

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

    // ── Database ──────────────────────────────────────────────────────────────
    builder.Services.AddDbContext<RadioguesserDbContext>(options =>
        options.UseNpgsql(
            builder.Configuration.GetConnectionString("DefaultConnection"),
            npgsql => npgsql.UseNetTopologySuite()));

    // ── Redis ─────────────────────────────────────────────────────────────────
    builder.Services.AddStackExchangeRedisCache(options =>
        options.Configuration = builder.Configuration.GetConnectionString("Redis"));

    // ── Auth (JWT via Supabase) ────────────────────────────────────────────────
    builder.Services
        .AddAuthentication("Bearer")
        .AddJwtBearer("Bearer", options =>
        {
            options.Authority = builder.Configuration["Supabase:Url"];
            options.Audience  = "authenticated";
            options.RequireHttpsMetadata = !builder.Environment.IsDevelopment();
        });
    builder.Services.AddAuthorization();

    // ── Application services ──────────────────────────────────────────────────
    builder.Services.AddScoped<IRadioCatalogProvider, RadioBrowserProvider>();
    builder.Services.AddScoped<IRadioImportService,   RadioImportService>();
    builder.Services.AddScoped<IScoringService,        ScoringService>();

    // ── Controllers ───────────────────────────────────────────────────────────
    builder.Services.AddControllers();
    builder.Services.AddEndpointsApiExplorer();

    // ── CORS (development only — tighten for production) ─────────────────────
    builder.Services.AddCors(opts =>
        opts.AddDefaultPolicy(p => p.AllowAnyOrigin().AllowAnyHeader().AllowAnyMethod()));

    // ── HttpClient (for Radio Browser API) ───────────────────────────────────
    builder.Services.AddHttpClient("RadioBrowser", client =>
    {
        client.BaseAddress = new Uri(
            builder.Configuration["RadioBrowser:BaseUrl"] ?? "https://all.api.radio-browser.info/json/");
        client.DefaultRequestHeaders.Add("User-Agent", "Radioguesser/1.0");
    });

    var app = builder.Build();

    app.UseSerilogRequestLogging();
    app.UseCors();
    app.UseAuthentication();
    app.UseAuthorization();
    app.MapControllers();

    // ── Health check endpoint ─────────────────────────────────────────────────
    app.MapGet("/health", () => Results.Ok(new { status = "healthy", timestamp = DateTime.UtcNow }));

    Log.Information("RadioGuesser server starting on {Urls}", app.Urls);
    app.Run();
}
catch (Exception ex)
{
    Log.Fatal(ex, "RadioGuesser server failed to start");
    throw;
}
finally
{
    Log.CloseAndFlush();
}
