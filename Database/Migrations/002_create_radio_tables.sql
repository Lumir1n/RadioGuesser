-- Migration 002: Radio station catalog tables

-- ── radio_stations ────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS radio_stations (
    id              UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    external_id     TEXT        NOT NULL,   -- Radio Browser UUID or other provider ID
    name            TEXT        NOT NULL,
    country_code    CHAR(2)     NOT NULL DEFAULT '',   -- ISO 3166-1 alpha-2
    country         TEXT        NOT NULL DEFAULT '',
    city            TEXT        NOT NULL DEFAULT '',
    region          TEXT        NOT NULL DEFAULT '',
    language        TEXT        NOT NULL DEFAULT '',
    tags            TEXT[]      NOT NULL DEFAULT '{}',
    genre           TEXT        NOT NULL DEFAULT '',
    homepage        TEXT        NOT NULL DEFAULT '',
    favicon_url     TEXT        NOT NULL DEFAULT '',

    -- PostGIS geography point (WGS84)
    -- NEVER sent to clients during active gameplay
    location        GEOMETRY(Point, 4326),

    health_status   SMALLINT    NOT NULL DEFAULT 0,    -- 0=unknown 1=active 2=temp_unavail 3=failed 4=blocked
    is_active       BOOLEAN     NOT NULL DEFAULT FALSE,
    source          TEXT        NOT NULL DEFAULT 'radio_browser',

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX IF NOT EXISTS uq_radio_stations_external_id
    ON radio_stations (external_id);

CREATE INDEX IF NOT EXISTS idx_radio_stations_country_code
    ON radio_stations (country_code);

CREATE INDEX IF NOT EXISTS idx_radio_stations_is_active
    ON radio_stations (is_active);

-- Spatial index on location
CREATE INDEX IF NOT EXISTS idx_radio_stations_location
    ON radio_stations USING GIST (location);

-- ── radio_streams ─────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS radio_streams (
    id              UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    station_id      UUID        NOT NULL REFERENCES radio_stations(id) ON DELETE CASCADE,
    stream_url      TEXT        NOT NULL,
    protocol        TEXT        NOT NULL DEFAULT '',   -- http / https / hls etc.
    codec           TEXT        NOT NULL DEFAULT '',   -- MP3 / AAC / OGG etc.
    bitrate         INT         NOT NULL DEFAULT 0,    -- kbps
    is_primary      BOOLEAN     NOT NULL DEFAULT TRUE,

    health_status   SMALLINT    NOT NULL DEFAULT 0,
    last_checked_at TIMESTAMPTZ,
    last_success_at TIMESTAMPTZ,
    failure_count   INT         NOT NULL DEFAULT 0,
    latency_ms      INT,
    http_status     INT,

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_radio_streams_station_id
    ON radio_streams (station_id);

CREATE INDEX IF NOT EXISTS idx_radio_streams_health_status
    ON radio_streams (health_status);

-- ── Trigger: auto-update updated_at ──────────────────────────────────────────

CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER LANGUAGE plpgsql AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$;

CREATE OR REPLACE TRIGGER trg_radio_stations_updated_at
    BEFORE UPDATE ON radio_stations
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

CREATE OR REPLACE TRIGGER trg_radio_streams_updated_at
    BEFORE UPDATE ON radio_streams
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();
