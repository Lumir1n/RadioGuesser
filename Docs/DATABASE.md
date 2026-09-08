# RadioGuesser — Database

## Platform

**Supabase** — PostgreSQL 15 + PostGIS + Supabase Auth + Supabase Storage

## Applying Migrations

Run migrations in order via the Supabase SQL editor or `psql`:

```
Database/Migrations/001_enable_postgis.sql
Database/Migrations/002_create_radio_tables.sql
Database/Migrations/003_create_game_tables.sql
Database/Migrations/004_scoring_function.sql
```

## Schema Overview

### radio_stations

Core station catalog. Location stored as `GEOMETRY(Point, 4326)` — WGS84.

| Column | Type | Notes |
|---|---|---|
| id | UUID PK | Internal ID |
| external_id | TEXT UNIQUE | Radio Browser UUID |
| name | TEXT | Station name |
| country_code | CHAR(2) | ISO 3166-1 alpha-2 |
| location | GEOMETRY(Point, 4326) | **NEVER sent to client during round** |
| health_status | SMALLINT | 0=unknown 1=active 2=temp 3=failed 4=blocked |
| is_active | BOOLEAN | Used for station selection |

### radio_streams

Separate from station identity. One station → many streams.

| Column | Type | Notes |
|---|---|---|
| station_id | UUID FK | References radio_stations |
| stream_url | TEXT | Direct HTTP/HTTPS stream |
| codec | TEXT | MP3 / AAC / OGG |
| bitrate | INT | kbps |
| is_primary | BOOLEAN | One primary per station |
| health_status | SMALLINT | Updated by RadioHealthChecker |

### game_rounds

Each round has a `public_token` (opaque, safe to send to clients) instead of the actual `station_id`.

| Column | Notes |
|---|---|
| public_token | Sent to clients — reveals nothing about location |
| station_id | Server only — never replicated to clients |
| stream_url | Sent to clients for direct playback |

### player_guesses

| Column | Notes |
|---|---|
| guess_location | GEOMETRY(Point, 4326) — client's map click |
| distance_metres | Computed via ST_Distance (PostGIS geography) |
| score | Computed via calculate_score() function |

## Key PostGIS Functions

```sql
-- Authoritative distance: metres, great-circle
SELECT ST_Distance(
    guess_location::GEOGRAPHY,
    station_location::GEOGRAPHY
) FROM ...;

-- Compute and store result for a guess
SELECT compute_round_result('guess-uuid-here');

-- Score from distance
SELECT calculate_score(327000);  -- ~3842 for 327 km
```

## Security

- `radio_stations.location` is protected by Row Level Security
- Station coordinates are readable only by the `service_role` (backend)
- Authenticated users can only read `public_token`, `stream_url`, `display_name`

## Planned Future Tables

```
users / profiles / characters / character_loadouts
cosmetics / player_inventory
countries / country_geometries / regions / cities
ratings / mmr_history / leaderboards
seasons / achievements / player_achievements
friends / friend_requests / blocks
lobbies / matches / match_players
statistics / reports
```
