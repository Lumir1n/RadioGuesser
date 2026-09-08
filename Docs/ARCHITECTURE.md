# RadioGuesser — Architecture

## System Overview

```
┌───────────────────────────────────────────────────────────────────┐
│                        GAME CLIENT                                │
│  Unreal Engine 5  •  C++  •  Blueprints  •  UMG                  │
│                                                                   │
│  URGGameInstance                                                  │
│  ├── URGRadioSubsystem   — stream playback state                 │
│  ├── URGMapSubsystem     — geo coordinates, guess marker         │
│  ├── URGBackendSubsystem — async HTTP to ASP.NET Core API        │
│  └── URGAuthSubsystem    — JWT token management (planned)        │
│                                                                   │
│  ARGGameMode / ARGGameState / ARGPlayerController                │
└───────────────────────────────────────────────────────────────────┘
             │ REST/WebSocket (JWT Bearer)
             ▼
┌───────────────────────────────────────────────────────────────────┐
│                      ASP.NET Core API                             │
│  /api/v1/radio  /api/v1/games  /api/v1/profile  ...              │
│                                                                   │
│  Application Layer                                                │
│  ├── IRadioCatalogProvider  → RadioBrowserProvider               │
│  ├── IRadioImportService    → RadioImportService                  │
│  └── IScoringService        → ScoringService                     │
│                                                                   │
│  Infrastructure Layer                                             │
│  ├── RadioguesserDbContext  — EF Core + NetTopologySuite          │
│  └── Redis                 — session state / rate limiting        │
└───────────────────────────────────────────────────────────────────┘
             │ EF Core / Npgsql
             ▼
┌───────────────────────────────────────────────────────────────────┐
│                       Supabase                                    │
│  PostgreSQL 15 + PostGIS                                          │
│  Supabase Auth (JWT)                                              │
│  Supabase Storage (avatars, cosmetics)                            │
│  Row Level Security                                               │
└───────────────────────────────────────────────────────────────────┘
```

## Responsibility Boundaries

| Layer | Owns |
|---|---|
| Unreal Client | Rendering, input, audio playback, UI state |
| Unreal Dedicated Server | Authoritative round simulation (Phase 7+) |
| ASP.NET Core | Persistent API, business logic, radio import |
| Supabase | PostgreSQL, Auth, Storage, RLS |
| Redis | Matchmaking queues, session state, rate limiting |

## Security Principles

- Station coordinates are **never** sent to the client during an active round
- The client receives only: `roundToken`, `streamUrl`, `displayName`
- All scores are computed server-side
- All ranked results are signed/validated server-side
- MMR, achievements, inventory are never updated by the client directly

## Development Phases

| Phase | Goal |
|---|---|
| 1 | Technical foundation (current) |
| 2 | Radio import prototype |
| 3 | Map prototype (Cesium for Unreal) |
| 4 | First playable (radio + map + guess + score) |
| 5 | Solo mode with rounds/timer |
| 6 | Player auth + profile |
| 7 | Multiplayer + dedicated server |
| 8 | Ranked / MMR |
| 9 | Social features |
| 10 | Cosmetics / achievements / events |

## Project Structure

```
RadioGuesser/
├── Source/RadioGuesser/        — Unreal C++ source
│   ├── Core/                   — GameInstance
│   ├── Game/                   — GameMode, GameState
│   ├── Player/                 — PlayerController
│   ├── Radio/                  — Radio subsystem + types
│   ├── Map/                    — Map subsystem + geo utils
│   ├── API/                    — HTTP backend subsystem
│   ├── Match/                  — Match flow (Phase 4+)
│   ├── Multiplayer/            — Dedicated server (Phase 7+)
│   └── UI/                     — UMG widget code (Phase 4+)
│
├── Server/Radioguesser.Server/ — ASP.NET Core 9 backend
│   ├── Api/                    — Controllers
│   ├── Application/            — Interfaces + Services
│   ├── Domain/                 — Entities + Enums
│   └── Infrastructure/         — EF Core, Radio providers
│
├── Database/Migrations/        — SQL migrations (apply via Supabase)
└── Docs/                       — Architecture docs
```
