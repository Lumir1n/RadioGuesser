// Copyright RadioGuesser. All Rights Reserved.

using Microsoft.EntityFrameworkCore;
using Radioguesser.Server.Domain.Entities;

namespace Radioguesser.Server.Infrastructure.Persistence;

public sealed class RadioguesserDbContext : DbContext
{
    public RadioguesserDbContext(DbContextOptions<RadioguesserDbContext> options) : base(options) { }

    public DbSet<RadioStation>  RadioStations  => Set<RadioStation>();
    public DbSet<RadioStream>   RadioStreams   => Set<RadioStream>();
    public DbSet<Game>          Games          => Set<Game>();
    public DbSet<GameRound>     GameRounds     => Set<GameRound>();
    public DbSet<GamePlayer>    GamePlayers    => Set<GamePlayer>();
    public DbSet<PlayerGuess>   PlayerGuesses  => Set<PlayerGuess>();

    protected override void OnModelCreating(ModelBuilder model)
    {
        base.OnModelCreating(model);

        // ── RadioStation ──────────────────────────────────────────────────────
        model.Entity<RadioStation>(e =>
        {
            e.ToTable("radio_stations");
            e.HasKey(x => x.Id);
            e.HasIndex(x => x.ExternalId).IsUnique();
            e.HasIndex(x => x.CountryCode);
            e.HasIndex(x => x.IsActive);
            e.Property(x => x.Tags).HasColumnType("text[]");
            // PostGIS geography column
            e.Property(x => x.Location).HasColumnType("geometry(Point, 4326)");
        });

        // ── RadioStream ───────────────────────────────────────────────────────
        model.Entity<RadioStream>(e =>
        {
            e.ToTable("radio_streams");
            e.HasKey(x => x.Id);
            e.HasOne(x => x.Station)
             .WithMany(s => s.Streams)
             .HasForeignKey(x => x.StationId)
             .OnDelete(DeleteBehavior.Cascade);
        });

        // ── Game ──────────────────────────────────────────────────────────────
        model.Entity<Game>(e =>
        {
            e.ToTable("games");
            e.HasKey(x => x.Id);
        });

        // ── GameRound ─────────────────────────────────────────────────────────
        model.Entity<GameRound>(e =>
        {
            e.ToTable("game_rounds");
            e.HasKey(x => x.Id);
            e.HasIndex(x => x.PublicToken).IsUnique();
            e.HasOne(x => x.Game)
             .WithMany(g => g.Rounds)
             .HasForeignKey(x => x.GameId)
             .OnDelete(DeleteBehavior.Cascade);
        });

        // ── GamePlayer ────────────────────────────────────────────────────────
        model.Entity<GamePlayer>(e =>
        {
            e.ToTable("game_players");
            e.HasKey(x => x.Id);
            e.HasOne(x => x.Game)
             .WithMany(g => g.Players)
             .HasForeignKey(x => x.GameId)
             .OnDelete(DeleteBehavior.Cascade);
        });

        // ── PlayerGuess ───────────────────────────────────────────────────────
        model.Entity<PlayerGuess>(e =>
        {
            e.ToTable("player_guesses");
            e.HasKey(x => x.Id);
            e.Property(x => x.GuessLocation).HasColumnType("geometry(Point, 4326)");
            e.HasOne(x => x.Round)
             .WithMany(r => r.Guesses)
             .HasForeignKey(x => x.RoundId)
             .OnDelete(DeleteBehavior.Cascade);
        });
    }
}
