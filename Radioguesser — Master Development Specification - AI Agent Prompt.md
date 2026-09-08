# RADIOGUESSER — MASTER DEVELOPMENT SPECIFICATION

## 0. ROLE OF THE AI AGENT

You are the lead architect, senior Unreal Engine developer, backend engineer, multiplayer engineer, database engineer, geospatial engineer, DevOps engineer, and technical designer for a large-scale desktop multiplayer game called **Radioguesser**.

Your task is to design and progressively implement the project according to this specification.

Do not treat this as a small prototype or a simple GeoGuessr clone.

The target is a scalable, production-oriented desktop game that can start as an MVP but has an architecture capable of expanding into:

- large-scale multiplayer;
- competitive ranked gameplay;
- player profiles;
- character customization;
- cosmetics;
- inventories;
- achievements;
- friends;
- social systems;
- seasons;
- leaderboards;
- statistics;
- events;
- potentially a shop/battle-pass system;
- potentially multiple game modes;
- potentially much more sophisticated geographic gameplay.

The game is called:

# RADIOGUESSER

Core concept:

> A competitive geographic discovery game in which players listen to real radio stations from around the world and attempt to determine where the radio broadcast originates.

The game combines the core intuition of GeoGuessr with real-world radio.

The central gameplay loop is:

**listen → investigate → infer location → place guess → calculate distance → receive score → compete**

Do not build a superficial prototype that will need to be completely rewritten later.

Prefer clean abstractions, modular architecture, strong typing, testability, scalability, and replaceable subsystems.

Do not optimize prematurely, but do not choose deliberately weak technologies merely because they are easier.

---

# 1. HIGH-LEVEL PRODUCT VISION

Radioguesser is a native desktop game made with Unreal Engine 5.

It is NOT a website.

It is NOT primarily a web application.

The final user experience should be a normal packaged desktop game:

- Windows executable;
- later potentially macOS/Linux;
- Steam-compatible;
- fullscreen/windowed;
- keyboard/mouse support;
- controller support should be considered in architecture.

The player should feel that they are playing a real multiplayer game, not operating a map website.

The game should contain:

### Core systems

- world map;
- radio stations;
- live internet radio playback;
- geographic guessing;
- distance calculation;
- scoring;
- rounds;
- solo mode;
- multiplayer mode;
- ranked mode;
- player profiles;
- character;
- character customization;
- cosmetics;
- statistics;
- leaderboards;
- achievements;
- friends/social features;
- lobbies;
- matchmaking.

---

# 2. FINAL TECHNOLOGY STACK

## GAME CLIENT

Use:

- Unreal Engine 5.x
- C++
- Blueprints
- UMG
- Slate where appropriate
- Enhanced Input
- Unreal Animation system
- Niagara
- MetaSounds
- Unreal Media Framework
- Electra Media Player where appropriate
- Unreal Online Services / EOS where appropriate
- dedicated-server capable architecture

C++ should be the foundation for:

- core systems;
- networking;
- game state;
- data models;
- services;
- API communication;
- radio subsystem;
- map subsystem;
- gameplay systems;
- persistence integration;
- performance-critical systems.

Blueprints should be used for:

- UI presentation;
- animation logic;
- designer-facing configuration;
- rapid gameplay iteration;
- cosmetic configuration;
- non-critical visual behavior.

Do NOT put the entire game architecture into Blueprints.

Do NOT build a Blueprint-only project.

---

# 3. BACKEND

Use:

- C#
- ASP.NET Core
- REST API
- WebSockets / SignalR where appropriate
- background workers
- PostgreSQL-compatible database access
- Redis
- Docker
- structured logging
- metrics
- health checks

The backend must be authoritative for competitive gameplay.

The client must never be trusted for:

- final score;
- actual station coordinates;
- MMR;
- ranked results;
- inventory ownership;
- purchases;
- achievements;
- match results;
- anti-cheat-sensitive state.

---

# 4. DATABASE

Use:

# Supabase

Supabase is the primary persistent-data platform.

Use:

- PostgreSQL
- PostGIS
- Supabase Auth
- Supabase Storage
- Row Level Security where appropriate
- SQL migrations
- database indexes
- database constraints
- database functions/triggers where useful

Supabase is not merely a temporary prototype database.

Design the schema as production infrastructure.

PostGIS is particularly important because the game is fundamentally geographic.

Store geographic information using appropriate geographic types rather than merely storing latitude and longitude as unrelated floating-point columns.

Examples:

- POINT
- POLYGON
- MULTIPOLYGON
- LINESTRING where needed

Potential tables:

```text
users
profiles
characters
character_loadouts
cosmetics
inventory_items
player_inventory
countries
country_geometries
regions
cities
radio_stations
radio_streams
radio_health_checks
games
game_rounds
game_players
guesses
scores
ratings
mmr_history
leaderboards
seasons
achievements
player_achievements
friends
friend_requests
blocks
lobbies
matches
match_players
statistics
reports
```

Do not create every table immediately if it is not needed.

However, design the architecture so these systems can be introduced without a database rewrite.

---

# 5. RADIO SYSTEM

Radio is one of the most important systems in the game.

The primary external source for station discovery should be:

# Radio Browser

Radio Browser is a large community radio-station database/API.

Do not assume Radio Browser is permanently stable.

Build an internal abstraction:

```text
IRadioCatalogProvider
```

and implement:

```text
RadioBrowserProvider
```

This allows future providers to be added.

Potential future providers:

```text
RadioGardenProvider
OtherRadioProvider
OfficialStationProvider
CustomStationProvider
```

Do not make the entire game directly dependent on Radio Browser's API schema.

Instead:

```text
External Radio Browser
        ↓
Radio Importer
        ↓
Normalizer
        ↓
Internal RadioStation model
        ↓
PostgreSQL
        ↓
Radioguesser
```

---

# 6. RADIO DATABASE MODEL

A radio station should have data similar to:

```text
id
external_id
name
country
country_code
city
region
latitude
longitude
location
language
languages
tags
genre
homepage
favicon
stream_url
stream_protocol
codec
bitrate
last_checked
health_status
is_active
source
created_at
updated_at
```

Separate station identity from stream endpoints.

One station may have multiple streams.

Therefore prefer:

```text
radio_stations
        ↓
radio_streams
```

rather than putting one permanent stream URL directly into the station entity.

---

# 7. RADIO HEALTH CHECKER

Radio stations frequently disappear, change URLs, change codecs, or become unavailable.

Create a backend background service:

```text
RadioHealthChecker
```

It should periodically:

1. select stations requiring verification;
2. test their streams;
3. record success/failure;
4. record latency;
5. record codec if detectable;
6. record HTTP status;
7. record last successful connection;
8. disable repeatedly failing streams;
9. retry failed streams later.

Example state:

```text
ACTIVE
TEMPORARILY_UNAVAILABLE
FAILED
BLOCKED
UNKNOWN
```

Do not randomly select dead stations during gameplay.

---

# 8. RADIO STREAM PLAYBACK

The game should preferably connect directly to the radio stream rather than proxying every audio byte through our backend.

Desired architecture:

```text
Radio Station
      ↓
Internet
      ↓
Radioguesser Client
      ↓
Unreal Audio System
```

Not:

```text
Radio Station
      ↓
Our Backend
      ↓
Every Player
```

The second architecture creates unnecessary bandwidth costs and scalability problems.

Unreal's Media Framework supports streamed media, and Electra Media Player supports internet radio streams including MP3 streams delivered via Icecast/Icycast.

Use the native Unreal media/audio infrastructure where possible.

However, do not assume every Radio Browser station will work.

Create:

```text
IRadioPlaybackService
```

with an implementation capable of:

- open stream;
- play;
- pause;
- stop;
- reconnect;
- retry;
- volume;
- mute;
- crossfade where practical;
- detect playback failure;
- expose playback state;
- expose metadata if available.

Design a fallback mechanism for streams that Unreal cannot play.

Potential fallback architecture may use a dedicated native third-party decoder/player plugin if technically and legally appropriate.

Do not implement a custom audio decoder unless required.

---

# 9. RADIO METADATA MUST NOT REVEAL THE ANSWER

During guessing gameplay, the player must not receive:

- station country;
- station coordinates;
- city;
- hidden station ID that trivially reveals location;
- metadata containing the answer.

The client may receive a temporary internal round identifier.

For example:

```json
{
    "roundId": "abc",
    "streamUrl": "...",
    "displayName": "LIVE RADIO"
}
```

but not:

```json
{
    "country": "Poland",
    "city": "Poznan",
    "latitude": 52.4,
    "longitude": 16.9
}
```

The authoritative server retains the true location.

---

# 10. RADIO GAMEPLAY

Primary gameplay:

```text
Player enters round
        ↓
Radio starts playing
        ↓
Player listens
        ↓
Player studies the map
        ↓
Player places guess
        ↓
Server receives guess
        ↓
Server calculates distance
        ↓
Server calculates score
        ↓
Round ends
        ↓
Correct location revealed
        ↓
Results shown
```

---

# 11. GAME MODES

Implement the architecture for multiple modes.

## MODE 1 — SOLO RADIO

One hidden radio station.

Player must determine the country/location.

Example:

```text
LIVE RADIO

[ PLAYING ]

Where is this radio?

WORLD MAP

[ MAKE GUESS ]
```

---

## MODE 2 — WORLD EXPLORER

Player can move around the world map and select different radio stations.

Example:

```text
Europe

● ● ● ● ● ● ●

Select a radio station
↓
Tune in
↓
Listen
```

This is an exploration mode rather than a pure competitive guessing mode.

---

## MODE 3 — CLASSIC GUESS

A hidden station is selected.

Player gets one guess.

The real location is revealed afterward.

---

## MODE 4 — MULTIPLAYER

Multiple players hear the same radio station.

All players make independent guesses.

Server calculates:

- distance;
- score;
- ranking.

---

## MODE 5 — RANKED

Competitive matchmaking.

Players have:

- MMR;
- rank;
- seasons;
- placement games;
- rating changes.

---

## FUTURE MODES

Architecture should allow:

- country-only mode;
- city mode;
- continent mode;
- timed mode;
- hardcore audio-only mode;
- team mode;
- duels;
- tournament mode;
- daily challenge;
- custom rooms;
- special events.

---

# 12. SCORING

The game should use a deterministic server-side score function.

Initial concept:

```text
Maximum score = 5000
```

Score should decrease with geographic distance.

Do not hard-code the final mathematical curve without testing.

Create:

```text
IScoringService
```

and allow the scoring algorithm to be changed independently.

Potential formula:

```text
distance
    ↓
normalized distance
    ↓
score
```

Possible score bands:

```text
< 25 km       ≈ 5000
< 100 km      ≈ 4500
< 500 km      ≈ 3500
< 1000 km     ≈ 2500
> 5000 km     ≈ very low
```

These values are placeholders.

Tune them through playtesting.

---

# 13. GEOGRAPHIC CALCULATION

Never calculate competitive scores solely on the client.

Use server-side geographic calculations.

Use PostGIS where practical.

Possible calculation:

```text
actual_station.location
        ↓
ST_Distance
        ↓
player_guess.location
        ↓
distance in meters
```

The client may show preview information, but the server is authoritative.

---

# 14. MAP SYSTEM — VERY IMPORTANT

Do NOT use Google Maps as the foundation.

Do NOT manually draw the entire world.

Use open geographic data and build our own visual representation.

Primary geographic data source:

# OpenStreetMap

The map must have our own visual identity.

The player should feel that they are looking at the Radioguesser world, not a generic OpenStreetMap website.

---

# 15. UNREAL MAP ARCHITECTURE

Investigate and prototype:

# Cesium for Unreal

Cesium for Unreal provides:

- WGS84 georeferencing;
- globe-scale coordinates;
- 3D Tiles;
- streaming geospatial content;
- terrain;
- imagery;
- geospatial positioning;
- Unreal integration.

Use Cesium as the primary candidate for geospatial infrastructure.

However:

DO NOT blindly use Cesium's default appearance.

DO NOT make the game look like a Cesium demo.

The final map must have a custom Radioguesser visual style.

---

# 16. MAP RENDERING STRATEGY

Before committing to one implementation, create a technical prototype comparing three approaches.

## APPROACH A — Cesium-based

Use:

```text
Cesium for Unreal
+
Cesium Georeference
+
3D Tiles
+
vector data / vector tile pipeline
```

Cesium currently supports vector-data workflows through its 3D Tiles/vector tile pipeline, including styling possibilities.

---

## APPROACH B — CUSTOM VECTOR MAP

Build an internal map pipeline:

```text
OpenStreetMap / Natural Earth
        ↓
Data preprocessing
        ↓
Vector tiles / optimized geographic data
        ↓
Unreal importer
        ↓
Procedural meshes / instanced geometry
        ↓
Custom materials
```

This approach gives maximum control over:

- colors;
- country borders;
- water;
- labels;
- roads;
- cities;
- radio markers;
- hover effects;
- selection;
- zoom;
- rendering.

---

## APPROACH C — RASTERIZED CUSTOM MAP

Generate our own map tiles:

```text
OSM geographic data
        ↓
Custom renderer
        ↓
PNG/WebP map tiles
        ↓
Unreal map layer
```

This is easier but less flexible.

Use it only if it provides a significant performance/stability advantage.

---

# 17. REQUIRED MAP PROTOTYPE

Before building the entire game, create a standalone Unreal map prototype that demonstrates:

1. World map.
2. Zoom.
3. Pan.
4. Country boundaries.
5. Country highlighting.
6. City labels.
7. Radio station markers.
8. Marker hover.
9. Marker click.
10. Geographic coordinate conversion.
11. Guess placement.
12. Distance line.
13. Correct location marker.
14. Smooth camera movement.

The prototype must support at least:

- Europe;
- North America;
- Asia;
- South America.

Do not build the entire world manually before proving the rendering architecture.

---

# 18. MAP VISUAL STYLE

The map should be inspired by the readability of GeoGuessr but have a unique Radioguesser identity.

Possible visual characteristics:

- clean;
- premium;
- minimal;
- slightly dark;
- high readability;
- strong country borders;
- subtle terrain;
- beautiful water;
- clear city names;
- radio station glow;
- animated radio waves;
- responsive hover states.

Do NOT copy GeoGuessr's exact visual assets.

Do NOT copy copyrighted UI.

Create an original art direction.

---

# 19. MAP LAYERS

Design the map as independent layers:

```text
Map
├── Ocean
├── Terrain
├── Country polygons
├── Country borders
├── Regions
├── Cities
├── Major cities
├── Roads
├── Radio stations
├── Radio waves
├── Player marker
├── Guess marker
├── Correct location marker
├── Route / distance line
├── UI labels
└── Special event overlays
```

Each layer should be independently enabled/disabled/configured.

---

# 20. RADIO MARKERS

In exploration mode, stations may appear as:

```text
●
```

or:

```text
🔊
```

but the final visual design should be custom.

Possible animation:

```text
●
(( ● ))
((( ● )))
```

representing radio waves.

Do not render tens of thousands of expensive individual actors.

Use:

- instanced rendering;
- spatial indexing;
- clustering;
- level-of-detail;
- tile-based loading;
- GPU-friendly rendering where appropriate.

At low zoom:

```text
Europe
    ◉ 132 stations
```

At high zoom:

```text
● ● ● ● ● ●
```

At city level:

```text
individual station markers
```

---

# 21. MAP PERFORMANCE

The map must be designed for large geographic datasets.

Do not load the entire world as thousands of individual Unreal Actors.

Use:

- spatial partitioning;
- tile-based streaming;
- World Partition where useful;
- instanced static meshes;
- hierarchical instancing;
- LOD;
- asynchronous loading;
- caching.

Unreal's PCG framework may be used for procedural world decoration or large-scale generated content where useful, but do not use PCG simply because it exists.

PCG should not become the core geographic database.

---

# 22. WORLD PARTITION

Evaluate Unreal World Partition for large-scale map/world content.

Use it where it improves:

- streaming;
- memory;
- large world management.

Do not force the entire world into one Unreal level if a tiled/streamed architecture is more appropriate.

---

# 23. CHARACTER SYSTEM

The game must have player characters.

Use Unreal's:

- skeletal meshes;
- Animation Blueprints;
- Control Rig where useful;
- IK;
- modular character components;
- animation montages;
- emotes.

Character architecture:

```text
Character
├── Body
├── Head
├── Hair
├── Face
├── Top
├── Bottom
├── Shoes
├── Accessory
├── Backpack
├── Held Item
└── Emote Set
```

Do not hard-code clothing directly into the character.

Use modular cosmetic slots.

---

# 24. CHARACTER CUSTOMIZATION

Players should eventually be able to customize:

- body;
- face;
- hair;
- clothing;
- shoes;
- accessories;
- colors;
- animations;
- emotes;
- profile cosmetics.

Customization should be data-driven.

Example:

```json
{
    "itemId": "hoodie_001",
    "slot": "top",
    "rarity": "rare",
    "mesh": "...",
    "material": "...",
    "unlockCondition": "..."
}
```

The server determines ownership.

The client renders the item.

---

# 25. INVENTORY

Create a scalable inventory architecture.

Conceptually:

```text
ItemDefinition
        ↓
PlayerInventory
        ↓
EquippedLoadout
        ↓
Character
```

Never trust the client to decide what cosmetics it owns.

---

# 26. PROFILE

Player profile should eventually include:

```text
username
display_name
avatar
character
country
level
XP
MMR
rank
wins
losses
games_played
best_score
average_score
favorite_country
favorite_station
achievements
statistics
```

---

# 27. RANKING SYSTEM

Build a competitive rating abstraction:

```text
IRatingService
```

Possible rating model:

- Elo-like;
- Glicko;
- custom MMR.

Do not permanently couple gameplay to one formula.

Start with a simple MMR system.

Potential ranks:

```text
Bronze
Silver
Gold
Platinum
Diamond
Master
Grandmaster
Radioguesser
```

These names are provisional.

Support:

- seasons;
- rating reset/soft reset;
- placement games;
- rank history;
- leaderboards.

---

# 28. MULTIPLAYER ARCHITECTURE

The multiplayer system must be authoritative.

Desired structure:

```text
CLIENT A
      \
CLIENT B ----> GAME SERVER
      /
CLIENT C
```

The game server knows:

- current station;
- true location;
- round timer;
- player guesses;
- score;
- round state;
- match state.

Clients receive only information appropriate for their current game state.

---

# 29. DEDICATED SERVER

Design the project so the gameplay server can run as an Unreal dedicated server where appropriate.

Separate:

```text
Game Client
```

from:

```text
Authoritative Game Server
```

Do not design multiplayer as peer-to-peer unless a specific feature explicitly requires it.

---

# 30. ONLINE SERVICES

Investigate and use Unreal Online Services / EOS for platform-level features where appropriate.

Potential uses:

- authentication integration;
- lobbies;
- sessions;
- friends;
- presence;
- achievements;
- stats;
- platform integration.

However, do not put all persistent game data into EOS.

Persistent game data remains in our backend/Supabase.

---

# 31. BACKEND VS EOS VS SUPABASE

Use clear responsibility boundaries.

### Unreal

Rendering and game client.

### Unreal Dedicated Server

Authoritative real-time game simulation.

### ASP.NET Core

Persistent API and application logic.

### Supabase

Persistent PostgreSQL data, Auth, Storage.

### Redis

Fast ephemeral state/cache/queues.

### EOS/Steam

Platform/social services where appropriate.

---

# 32. MATCH FLOW

Example:

```text
Player starts Ranked Match
        ↓
Authenticate
        ↓
Send matchmaking request
        ↓
Matchmaking service
        ↓
Create match
        ↓
Create game session
        ↓
Select station
        ↓
Hide true location from clients
        ↓
Start round
        ↓
Players listen
        ↓
Players submit guesses
        ↓
Server validates guesses
        ↓
Server calculates distance
        ↓
Server calculates score
        ↓
Round result
        ↓
Update match score
        ↓
Next round
        ↓
Final result
        ↓
Update MMR
        ↓
Persist statistics
```

---

# 33. ANTI-CHEAT

Assume clients are hostile.

Never trust:

- client score;
- client MMR;
- client station location;
- client inventory;
- client achievements;
- client purchase state.

Potential cheats to consider:

- inspecting network packets;
- reading hidden coordinates;
- manipulating local memory;
- modifying client data;
- spoofing guesses;
- automating radio recognition;
- extracting station metadata;
- timing manipulation.

The server should minimize sensitive data sent to clients.

---

# 34. IMPORTANT: DO NOT LEAK RADIO LOCATION

This is one of the most important anti-cheat requirements.

If the client receives:

```text
stationId
countryId
coordinates
city
```

a malicious player may extract it.

During a competitive round, send only what is required to play.

If necessary, create a temporary server-side radio session:

```text
RadioRound
    ↓
hiddenStationId
hiddenLocation
streamEndpoint
publicRoundToken
```

The client receives only:

```text
publicRoundToken
streamEndpoint
display information
```

---

# 35. RANKED DATA SECURITY

Ranked match results should be signed/validated server-side.

Do not allow a client to directly update:

```text
MMR
wins
leaderboards
achievements
inventory
```

---

# 36. CACHE

Use Redis for:

- matchmaking queues;
- active rooms;
- temporary game state;
- session state;
- rate limiting;
- leaderboard caching;
- online presence;
- short-lived radio metadata cache;
- distributed locks where required.

Do not treat Redis as permanent storage.

---

# 37. API ARCHITECTURE

Use versioned APIs.

Example:

```text
/api/v1/auth
/api/v1/profile
/api/v1/radio
/api/v1/games
/api/v1/matches
/api/v1/leaderboards
/api/v1/inventory
/api/v1/cosmetics
/api/v1/achievements
```

Use DTOs.

Do not expose database entities directly through APIs.

---

# 38. C# BACKEND STRUCTURE

Suggested architecture:

```text
Radioguesser.Server
│
├── Api
├── Application
├── Domain
├── Infrastructure
├── Persistence
├── Radio
├── Matchmaking
├── Multiplayer
├── Ratings
├── AntiCheat
├── Users
├── Inventory
├── Leaderboards
├── Achievements
└── Common
```

Use dependency inversion.

Create interfaces for important services.

Examples:

```text
IRadioCatalogProvider
IRadioHealthService
IRadioSelectionService
IScoringService
IRatingService
IMatchmakingService
IInventoryService
IAchievementService
```

---

# 39. UNREAL PROJECT STRUCTURE

Suggested:

```text
Source/
└── Radioguesser/
    ├── Core/
    ├── Game/
    ├── Map/
    ├── Radio/
    ├── Multiplayer/
    ├── Characters/
    ├── Cosmetics/
    ├── UI/
    ├── Online/
    ├── Networking/
    ├── Audio/
    ├── API/
    ├── Data/
    ├── Player/
    ├── Match/
    ├── Inventory/
    └── Utilities/
```

Content:

```text
Content/
├── Characters/
├── Cosmetics/
├── UI/
├── Maps/
├── Materials/
├── Audio/
├── VFX/
├── Data/
├── Radio/
└── Developer/
```

Use naming conventions consistently.

---

# 40. UI

Use UMG.

Main screens:

```text
Main Menu
Profile
Play
Solo
Multiplayer
Ranked
Custom Game
World Explorer
Radio Browser
Leaderboard
Inventory
Character
Customization
Settings
Friends
Achievements
Match Results
Round Results
```

The UI should be data-driven and modular.

---

# 41. GAME UI

During a round:

```text
┌──────────────────────────────────────────┐
│ ROUND 3 / 5                  00:28       │
│                                          │
│                                          │
│                 WORLD MAP                │
│                                          │
│                      ?                   │
│                                          │
│                                          │
│                                          │
├──────────────────────────────────────────┤
│ 🔊 LIVE RADIO                            │
│                                          │
│ ▶  ━━━━━━━━━━━━━━━━━━━━━━━━━━━           │
│                                          │
│               [ MAKE GUESS ]             │
└──────────────────────────────────────────┘
```

After guessing:

```text
Your Guess
      ↓
Actual Location
      ↓
Distance
      ↓
Score
```

---

# 42. AUDIO DESIGN

The radio should feel like the core of the game.

Implement:

- smooth start;
- buffering state;
- reconnect;
- volume;
- mute;
- station switching;
- optional crossfade;
- error state;
- loading state;
- audio visualization if desired.

Potential future features:

- signal distortion;
- radio tuning effect;
- transition sounds;
- station change static;
- environmental audio;
- spatial radio sources.

---

# 43. RADIO STREAM FAILURE UX

If stream fails:

```text
Connecting...
      ↓
Retry
      ↓
Failed
```

Do not leave the player with a frozen UI.

Show:

```text
Radio unavailable.
Finding another station...
```

if the game mode permits station replacement.

For ranked games, station replacement must be controlled server-side and deterministic.

---

# 44. WORLD EXPLORATION

In Explorer mode, the player can:

- pan;
- zoom;
- select stations;
- inspect countries;
- inspect cities;
- tune into radio;
- save favorites;
- discover stations.

Do not reveal hidden locations in competitive mode.

---

# 45. FAVORITES

Players can eventually save:

```text
favorite stations
favorite countries
favorite cities
favorite genres
```

This is persistent user data.

---

# 46. ACHIEVEMENTS

Examples:

```text
First Broadcast
First Perfect Guess
100 Countries
1000 Stations
Guess 10 Countries in a Row
Radio Explorer
Europe Expert
Asia Expert
World Traveler
```

Create a generic achievement system rather than hardcoding individual achievements.

---

# 47. SEASONS

Design ranked architecture for seasons.

Example:

```text
Season 1
Season 2
Season 3
```

Store:

```text
season_id
start_date
end_date
rating_rules
leaderboard
rewards
```

---

# 48. STATISTICS

Track:

- total games;
- total rounds;
- total guesses;
- average distance;
- average score;
- best score;
- country accuracy;
- continent accuracy;
- station accuracy;
- win rate;
- ranked games;
- favorite countries;
- favorite radio genres.

Do not calculate expensive statistics on every client request.

Use aggregation/background jobs where appropriate.

---

# 49. ADMIN SYSTEM

Eventually create an admin backend/dashboard.

Admin should be able to:

- inspect radio stations;
- disable bad streams;
- inspect health;
- manage countries;
- manage cosmetics;
- manage achievements;
- manage seasons;
- inspect reports;
- ban players;
- modify game configuration.

Do not expose admin APIs to ordinary users.

---

# 50. RADIO DATA PIPELINE

Create a scheduled pipeline:

```text
Radio Browser
      ↓
Import
      ↓
Normalize
      ↓
Deduplicate
      ↓
Geocode / validate
      ↓
Health Check
      ↓
Rank station quality
      ↓
Store
```

Station quality score may consider:

- stream availability;
- bitrate;
- codec;
- metadata;
- geographic accuracy;
- last successful check;
- language;
- station completeness.

---

# 51. STATION SELECTION

Create:

```text
IRadioSelectionService
```

Selection should consider:

- country;
- continent;
- game mode;
- difficulty;
- stream reliability;
- station uniqueness;
- language;
- previous usage;
- player history;
- competitive fairness.

Do not select stations randomly from the entire raw database.

---

# 52. DIFFICULTY SYSTEM

A station can have a difficulty score.

Potential factors:

Easy:

- obvious language;
- strong local identity;
- clear station identification.

Hard:

- international music;
- minimal speech;
- multilingual region;
- border region;
- generic music;
- weak geographic clues.

Future difficulty:

```text Easy
Normal
Hard
Expert
Impossible
```

---

# 53. GAME SEEDING

Every competitive round should be reproducible from a server-side seed.

Example:

```text matchSeed
roundSeed
stationSelectionSeed
```

This helps debugging and anti-cheat.

---

# 54. MAP INTERACTION

The map should support:

- click;
- drag;
- zoom;
- double-click;
- mouse wheel;
- touchpad;
- controller navigation if feasible.

Player guess should be represented by a custom marker.

---

# 55. MAP GUESS

When the player clicks:

```text
Player Guess Marker
```

show:

```text
[ CONFIRM GUESS ]
```

Do not submit accidental clicks.

After confirmation:

```text
guess submitted
```

Lock the guess.

---

# 56. RESULTS VISUALIZATION

After the round:

```text
YOUR GUESS
     ●

ACTUAL LOCATION
     ★

──────────────

Distance:
327 km

Score:
3,842 / 5,000
```

Animate the result.

Potential future:

- flight line;
- camera zoom;
- country highlight;
- city label;
- station icon;
- radio wave animation.

---

# 57. PERFORMANCE PRINCIPLES

The project should target high frame rates on normal gaming PCs.

Avoid:

- thousands of Unreal Actors for map markers;
- unnecessary Tick functions;
- synchronous HTTP;
- blocking database calls;
- loading huge datasets into RAM;
- rebuilding the entire map every frame.

Prefer:

- async operations;
- event-driven architecture;
- object pooling;
- instancing;
- spatial partitioning;
- caching;
- asynchronous loading;
- LOD;
- GPU-friendly rendering.

---

# 58. NETWORKING PRINCIPLES

Do not send unnecessary data.

Prefer:

```text
server state
     ↓
replicated minimal state
```

rather than:

```text
everything
     ↓
every client
```

Use server authority.

---

# 59. ERROR HANDLING

Every subsystem must have explicit failure states.

Examples:

```text
Radio unavailable
API unavailable
Database unavailable
Authentication failed
Matchmaking timeout
Server disconnected
Map tile unavailable
Stream timeout
Invalid guess
Session expired
```

Do not crash because a radio stream is unavailable.

---

# 60. LOGGING

Use structured logging.

Each subsystem should have categories:

```text
LogRadio
LogMap
LogMatch
LogNetworking
LogAPI
LogInventory
LogAuthentication
LogRating
```

Do not spam logs in shipping builds.

---

# 61. CONFIGURATION

Do not hardcode:

- API URLs;
- Supabase URLs;
- Redis addresses;
- radio provider URLs;
- environment secrets;
- server addresses.

Use environment/configuration systems.

Separate:

```text
Development
Staging
Production
```

---

# 62. SECRETS

Never commit:

- Supabase service-role key;
- EOS secret;
- database password;
- Redis password;
- API secret;
- private signing keys.

Use environment variables/secrets management.

---

# 63. DATABASE MIGRATIONS

Every schema change must be represented by a migration.

Do not manually alter production databases without migration tracking.

---

# 64. TESTING

Create tests for:

### Backend

- station selection;
- scoring;
- geographic calculations;
- MMR;
- inventory;
- authentication;
- permissions.

### Unreal

- round state;
- radio subsystem;
- map coordinate conversion;
- UI state;
- networking;
- player guesses.

### Integration

- client → backend;
- backend → Supabase;
- radio import;
- radio health checker;
- multiplayer flow.

---

# 65. DEVELOPMENT PHASES

Do NOT attempt to build every system simultaneously.

Use phases.

## PHASE 1 — TECHNICAL FOUNDATION

Create:

- Unreal C++ project;
- backend project;
- Supabase project;
- PostgreSQL/PostGIS;
- basic API;
- repository;
- CI;
- configuration;
- logging.

---

## PHASE 2 — RADIO PROTOTYPE

Implement:

- Radio Browser importer;
- station database;
- health checker;
- one playable stream;
- Unreal radio playback.

Goal:

```text
Open game
→ choose station
→ hear live radio
```

---

## PHASE 3 — MAP PROTOTYPE

Implement:

- Cesium for Unreal prototype;
- georeferencing;
- world map;
- custom map appearance;
- country boundaries;
- coordinate conversion;
- station marker.

Compare:

1. Cesium-based;
2. custom vector;
3. raster tile.

Choose based on:

- visual quality;
- performance;
- licensing;
- offline/online requirements;
- scalability;
- implementation complexity.

Do not commit to a map architecture without this prototype.

---

## PHASE 4 — FIRST PLAYABLE

Implement:

```text
Radio
+
Map
+
Guess
+
Distance
+
Score
```

At this point Radioguesser becomes playable.

---

## PHASE 5 — SOLO

Add:

- rounds;
- timer;
- result screen;
- score;
- difficulty;
- station selection.

---

## PHASE 6 — PLAYER

Add:

- authentication;
- profile;
- character;
- basic customization.

---

## PHASE 7 — MULTIPLAYER

Add:

- dedicated server;
- lobbies;
- sessions;
- matchmaking;
- authoritative rounds;
- replicated state.

---

## PHASE 8 — RANKED

Add:

- MMR;
- ranks;
- leaderboard;
- seasons;
- rating history.

---

## PHASE 9 — SOCIAL

Add:

- friends;
- invites;
- presence;
- parties;
- custom rooms.

---

## PHASE 10 — LIVE GAME

Add:

- achievements;
- cosmetics;
- events;
- shop if desired;
- analytics;
- moderation;
- admin tools.

---

# 66. FIRST MILESTONE

Do not start by creating hundreds of classes.

The first milestone is deliberately small:

```text
Unreal Engine project
        ↓
Cesium/map prototype
        ↓
Custom world map
        ↓
Radio Browser importer
        ↓
One working radio station
        ↓
Radio playback
        ↓
Player clicks map
        ↓
Guess is submitted
        ↓
Correct location appears
        ↓
Distance is calculated
        ↓
Score is shown
```

When this works reliably, continue.

---

# 67. CODE QUALITY RULES

Use:

- SOLID principles where appropriate;
- dependency injection on backend;
- interfaces for replaceable services;
- strong domain models;
- async APIs;
- clear ownership;
- explicit lifecycle management.

Avoid:

- giant God classes;
- global mutable state;
- duplicated logic;
- hardcoded game rules;
- database logic inside UI;
- network logic inside widgets;
- radio logic inside GameMode;
- map logic inside player controller.

---

# 68. UNREAL ARCHITECTURE PRINCIPLE

Keep Unreal subsystems separated.

Example:

```text
UGameInstance
    ↓
URadioguesserGameInstanceSubsystem
    ↓
Services

URadioSubsystem
UMapSubsystem
UBackendSubsystem
UAuthSubsystem
UPlayerProfileSubsystem
UInventorySubsystem
UMatchSubsystem
```

Do not make one giant `GameInstance` class.

---

# 69. BACKEND ARCHITECTURE PRINCIPLE

Separate:

```text
Domain
Application
Infrastructure
API
```

The domain should not depend on HTTP.

The domain should not depend directly on Supabase SDK details.

The application layer should use interfaces.

Infrastructure implements those interfaces.

---

# 70. LICENSE AND LEGAL REQUIREMENTS

This is critical.

Technical access to a radio stream does NOT automatically mean that Radioguesser has commercial rights to redistribute or use that stream in a commercial game.

Before commercial release:

- verify station-specific terms;
- verify Radio Browser terms;
- verify OpenStreetMap/ODbL obligations;
- verify map tile provider licensing;
- verify Cesium/Cesium ion terms;
- verify radio-stream usage rights;
- verify trademarks;
- avoid copying GeoGuessr copyrighted assets/UI.

For development/prototyping, use appropriate test stations and open data.

Build the architecture so radio providers can be replaced if licensing requirements change.

---

# 71. IMPORTANT RADIO LICENSE ARCHITECTURE

Do not assume:

```text Radio Browser
=
permission to commercially rebroadcast every station.
```

Radio Browser should be treated primarily as a station discovery/catalog source.

The actual stream rights belong to the relevant broadcaster/provider.

For commercial launch, create a station eligibility system:

```text
radio_station
    ↓
license_status

UNKNOWN
REVIEW_REQUIRED
APPROVED
REJECTED
```

Only approved stations should be eligible for production ranked gameplay if legal review requires this.

---

# 72. OPENSTREETMAP LICENSING

If OpenStreetMap data is used, comply with the applicable ODbL requirements.

Do not remove required attribution.

The exact map-data architecture must account for:

- attribution;
- derived databases;
- tile-provider terms;
- caching;
- redistribution.

---

# 73. DO NOT DEPEND ON A SINGLE THIRD-PARTY PROVIDER

Every major external dependency should have an abstraction.

Examples:

```text
IRadioCatalogProvider
IMapDataProvider
IAuthProvider
IOnlinePlatformProvider
IStorageProvider
```

This is important because the project should survive:

- API changes;
- provider shutdown;
- pricing changes;
- licensing changes.

---

# 74. OBSERVABILITY

Plan for:

- server metrics;
- request latency;
- radio failure rate;
- matchmaking time;
- game completion rate;
- crash reporting;
- client performance;
- backend errors.

Potential future tools:

- OpenTelemetry;
- Prometheus;
- Grafana;
- Sentry.

Do not overbuild observability in the first prototype, but keep the architecture compatible with it.

---

# 75. DEPLOYMENT

Initial:

```text
Git
Docker
CI/CD
Staging
Production
```

Potential infrastructure:

```text
Cloudflare
      ↓
ASP.NET Core
      ↓
Redis
      ↓
Supabase
```

For dedicated game servers:

```text
Game Server Fleet
```

Use containers where practical.

---

# 76. DEVELOPMENT REPOSITORY

Suggested monorepo:

```text
Radioguesser/
│
├── Game/
│   └── RadioguesserUE/
│
├── Server/
│   └── Radioguesser.Server/
│
├── Database/
│   ├── migrations/
│   └── seeds/
│
├── Tools/
│   ├── RadioImporter/
│   ├── RadioHealthChecker/
│   └── MapPipeline/
│
├── Infrastructure/
│   ├── docker/
│   └── deployment/
│
├── Docs/
│
└── README.md
```

---

# 77. DOCUMENTATION

Maintain:

```text
ARCHITECTURE.md
MAP_PIPELINE.md
RADIO_SYSTEM.md
MULTIPLAYER.md
DATABASE.md
API.md
DEPLOYMENT.md
SECURITY.md
CONTRIBUTING.md
```

Whenever architecture changes, update documentation.

---

# 78. AI AGENT DEVELOPMENT RULES

You are an AI coding agent.

Do not blindly generate massive amounts of code.

Before implementing a major subsystem:

1. inspect the repository;
2. inspect existing architecture;
3. identify dependencies;
4. create a plan;
5. implement incrementally;
6. compile;
7. test;
8. fix errors;
9. document the result.

Never overwrite working systems unnecessarily.

Do not create duplicate implementations of the same service.

Prefer extending existing abstractions.

---

# 79. DO NOT MAKE THESE ARCHITECTURAL MISTAKES

Do NOT:

- build the whole game in Blueprint;
- store competitive state only on the client;
- put the entire application into Supabase Edge Functions;
- proxy all radio audio through our server;
- hardcode station coordinates into the client;
- hardcode radio station lists;
- hardcode cosmetics into character classes;
- use Google Maps as the core map;
- build the whole world manually;
- create thousands of Unreal Actors for radio markers;
- make the client authoritative;
- expose service-role credentials;
- couple the game directly to Radio Browser API responses;
- couple gameplay directly to one map provider;
- create one giant GameInstance;
- create one giant ASP.NET controller;
- ignore licensing.

---

# 80. DESIGN PHILOSOPHY

Radioguesser should feel like:

> GeoGuessr + global radio + competitive multiplayer + character personalization.

But it should not be a literal GeoGuessr clone.

Its unique identity is:

# "Explore the world through sound."

The map tells the player:

> where you might be.

The radio tells the player:

> what the place sounds like.

The gameplay is the process of connecting those two.

---

# 81. LONG-TERM VISION

The architecture should make it possible to eventually support:

- thousands of simultaneous players;
- tens of thousands of radio stations;
- global leaderboards;
- millions of player profiles;
- seasonal ranked gameplay;
- extensive cosmetic collections;
- events;
- tournaments;
- custom maps;
- user-created challenges;
- daily challenges;
- AI-assisted radio clues;
- voice/social systems;
- advanced world exploration;
- richer 3D environments.

Do not implement all of these now.

But do not architect the MVP in a way that makes them impossible later.

---

# 82. FINAL TECHNOLOGY SUMMARY

## GAME

```text
Unreal Engine 5
C++
Blueprints
UMG
Slate
Enhanced Input
Animation System
Niagara
MetaSounds
Media Framework
Electra Media Player
```

## MAP

```text
OpenStreetMap
PostGIS
Cesium for Unreal
3D Tiles
Vector data
Custom map renderer/style
World Partition where appropriate
Instancing / LOD / streaming
```

## BACKEND

```text
C#
ASP.NET Core
REST
WebSockets / SignalR where appropriate
Background Workers
Docker
```

## DATABASE

```text
Supabase
PostgreSQL
PostGIS
Supabase Auth
Supabase Storage
RLS
SQL migrations
```

## REALTIME / PERFORMANCE

```text
Redis
Caching
Queues
Matchmaking state
Session state
Rate limiting
```

## RADIO

```text
Radio Browser
Radio catalog importer
Radio normalization
Radio health checker
Direct station streams
MP3/AAC/other supported formats
Unreal Media Framework / Electra
```

## ONLINE

```text
EOS
Steam integration
Unreal Online Services / Online Subsystem as appropriate
Dedicated servers
```

## INFRASTRUCTURE

```text
Cloudflare
Docker
CI/CD
Staging
Production
Monitoring
Logging
Metrics
```

---

# 83. FIRST TASK FOR THE AGENT

Do NOT start by building the entire game.

First:

### TASK 1

Create the repository and architecture.

### TASK 2

Create the Unreal Engine 5 C++ project.

### TASK 3

Create the ASP.NET Core backend.

### TASK 4

Create Supabase database and initial migrations.

### TASK 5

Enable PostGIS.

### TASK 6

Implement the first `RadioBrowserProvider`.

### TASK 7

Import a small controlled subset of radio stations.

### TASK 8

Implement radio health checking.

### TASK 9

Create a minimal Unreal radio playback prototype.

### TASK 10

Create a Cesium for Unreal map prototype.

### TASK 11

Create a custom Radioguesser map style.

### TASK 12

Implement coordinate conversion between geographic coordinates and Unreal world coordinates.

### TASK 13

Implement:

```text
radio
+
map
+
guess
+
correct location
+
distance
+
score
```

Only after this vertical slice works should the agent proceed to multiplayer, profiles, cosmetics, ranking, and other large systems.

---

# 84. DEFINITION OF SUCCESS FOR THE FIRST PLAYABLE BUILD

The first playable build is successful when a player can:

1. Launch `Radioguesser`.
2. See the custom world map.
3. Start a round.
4. Hear a real radio station.
5. Move/zoom around the map.
6. Place a guess.
7. Confirm the guess.
8. See the real station location.
9. See a line between the guess and actual location.
10. See geographic distance.
11. Receive a score.
12. Start another round.

This is the foundation.

Everything else should be built on top of this architecture.