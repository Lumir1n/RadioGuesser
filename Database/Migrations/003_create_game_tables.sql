-- Migration 003: Core game tables

-- ── games ─────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS games (
    id              UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    mode            SMALLINT    NOT NULL DEFAULT 0,    -- 0=solo 1=multiplayer 2=ranked 3=explorer
    status          SMALLINT    NOT NULL DEFAULT 0,    -- 0=waiting 1=active 2=finished 3=abandoned
    total_rounds    INT         NOT NULL DEFAULT 5,
    seed            TEXT        NOT NULL DEFAULT '',   -- Server-side seed for reproducibility

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    started_at      TIMESTAMPTZ,
    ended_at        TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_games_status ON games (status);

-- ── game_rounds ───────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS game_rounds (
    id              UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    game_id         UUID        NOT NULL REFERENCES games(id) ON DELETE CASCADE,
    round_number    INT         NOT NULL,
    station_id      UUID        NOT NULL REFERENCES radio_stations(id),

    -- Opaque token sent to clients — does NOT reveal station location
    public_token    TEXT        NOT NULL UNIQUE DEFAULT gen_random_uuid()::TEXT,
    stream_url      TEXT        NOT NULL DEFAULT '',
    display_name    TEXT        NOT NULL DEFAULT 'LIVE RADIO',

    status          SMALLINT    NOT NULL DEFAULT 0,    -- 0=pending 1=active 2=finished
    started_at      TIMESTAMPTZ,
    ended_at        TIMESTAMPTZ,
    duration_seconds REAL       NOT NULL DEFAULT 120
);

CREATE INDEX IF NOT EXISTS idx_game_rounds_game_id ON game_rounds (game_id);
CREATE INDEX IF NOT EXISTS idx_game_rounds_public_token ON game_rounds (public_token);

-- ── game_players ──────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS game_players (
    id              UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    game_id         UUID        NOT NULL REFERENCES games(id) ON DELETE CASCADE,
    user_id         UUID        NOT NULL,
    total_score     INT         NOT NULL DEFAULT 0,
    rank            INT         NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_game_players_game_id  ON game_players (game_id);
CREATE INDEX IF NOT EXISTS idx_game_players_user_id  ON game_players (user_id);

-- ── player_guesses ────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS player_guesses (
    id               UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    round_id         UUID        NOT NULL REFERENCES game_rounds(id) ON DELETE CASCADE,
    player_id        UUID        NOT NULL,

    -- PostGIS point for the player's click on the map
    guess_location   GEOMETRY(Point, 4326),

    -- Server-side computed via ST_Distance
    distance_metres  DOUBLE PRECISION NOT NULL DEFAULT 0,
    score            INT         NOT NULL DEFAULT 0,
    is_locked        BOOLEAN     NOT NULL DEFAULT FALSE,

    submitted_at     TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_player_guesses_round_id  ON player_guesses (round_id);
CREATE INDEX IF NOT EXISTS idx_player_guesses_player_id ON player_guesses (player_id);
CREATE INDEX IF NOT EXISTS idx_player_guesses_guess_loc
    ON player_guesses USING GIST (guess_location);
