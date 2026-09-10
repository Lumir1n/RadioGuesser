// Copyright RadioGuesser. All Rights Reserved.

using Serilog;
using Npgsql;
using System.Net;
using System.Net.Sockets;
using Radioguesser.Server.Application.Interfaces;
using Radioguesser.Server.Application.Services;
using Radioguesser.Server.Infrastructure.Radio;

Log.Logger = new LoggerConfiguration()
    .WriteTo.Console()
    .CreateBootstrapLogger();

try
{
    var builder = WebApplication.CreateBuilder(args);

    builder.Host.UseSerilog((ctx, services, cfg) =>
        cfg.ReadFrom.Configuration(ctx.Configuration)
           .ReadFrom.Services(services)
           .Enrich.FromLogContext()
           .WriteTo.Console());

    // ── PostgreSQL via Npgsql (direct, IPv6-aware) ────────────────────────────
    var rawConnStr = builder.Configuration.GetConnectionString("DefaultConnection")
        ?? throw new InvalidOperationException("DefaultConnection not configured");

    // Resolve hostname to IPv6 if available (Supabase uses IPv6)
    var connStr = await ResolveToIpv6ConnectionString(rawConnStr);
    Log.Information("DB connection resolved: {ConnStr}", MaskPassword(connStr));

    var dsBuilder = new NpgsqlDataSourceBuilder(connStr);
    // Don't use NetTopologySuite — we use plain lat/lon doubles, not geometry objects
    var dataSource = dsBuilder.Build();

    // Clear any cached connection pools
    NpgsqlConnection.ClearAllPools();
    builder.Services.AddSingleton(dataSource);

    // ── Application services ──────────────────────────────────────────────────
    builder.Services.AddScoped<IRadioCatalogProvider, RadioBrowserProvider>();
    builder.Services.AddScoped<IRadioImportService,   RadioImportService>();
    builder.Services.AddSingleton<IScoringService,    ScoringService>();

    builder.Services.AddControllers();
    builder.Services.AddEndpointsApiExplorer();

    builder.Services.AddCors(opts =>
        opts.AddDefaultPolicy(p => p.AllowAnyOrigin().AllowAnyHeader().AllowAnyMethod()));

    builder.Services.AddHttpClient("RadioBrowser", client =>
    {
        client.BaseAddress = new Uri(
            builder.Configuration["RadioBrowser:BaseUrl"]
            ?? "https://all.api.radio-browser.info/json/");
        client.DefaultRequestHeaders.Add("User-Agent", "Radioguesser/1.0");
    });

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

    // Verify DB connection on startup
    try
    {
        await using var testConn = await dataSource.OpenConnectionAsync();
        await using var testCmd  = testConn.CreateCommand();
        testCmd.CommandText = "SELECT COUNT(*) FROM radio_stations WHERE lat IS NOT NULL";
        var stationCount = await testCmd.ExecuteScalarAsync();
        Log.Information("DB connected. Stations with GPS: {Count}", stationCount);
    }
    catch (Exception ex)
    {
        Log.Warning(ex, "DB startup check failed — game endpoints will be unavailable");
    }

    app.UseSerilogRequestLogging();
    app.UseCors();
    app.MapControllers();
    app.MapGet("/health", () => Results.Ok(new { status = "healthy", timestamp = DateTime.UtcNow }));

    Log.Information("RadioGuesser backend starting on port 5296...");
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

// ── Helper: resolve hostname to IPv6 address in connection string ─────────────
static async Task<string> ResolveToIpv6ConnectionString(string connStr)
{
    try
    {
        var builder = new NpgsqlConnectionStringBuilder(connStr);
        var hostEntry = await Dns.GetHostEntryAsync(builder.Host);

        // Prefer IPv6 first (Supabase uses IPv6), fallback to IPv4
        var ipv6 = hostEntry.AddressList.FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetworkV6);
        var ipv4 = hostEntry.AddressList.FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetwork);
        var target = ipv6 ?? ipv4;

        if (target != null)
        {
            builder.Host = target.ToString();
            Log.Information("Resolved {Original} → {IP}", connStr.Split(';')[0], target);
            return builder.ConnectionString;
        }
    }
    catch (Exception ex)
    {
        Log.Warning(ex, "DNS resolution failed, using original connection string");
    }
    return connStr;
}

static string MaskPassword(string connStr)
{
    var b = new NpgsqlConnectionStringBuilder(connStr);
    b.Password = "***";
    return b.ConnectionString;
}
