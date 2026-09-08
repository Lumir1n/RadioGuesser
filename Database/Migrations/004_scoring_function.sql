-- Migration 004: Server-side scoring and distance functions

-- ── calculate_score ───────────────────────────────────────────────────────────
-- Authoritative score calculation — mirrors ScoringService.cs logic.
-- Called server-side after a round ends.

CREATE OR REPLACE FUNCTION calculate_score(distance_metres DOUBLE PRECISION)
RETURNS INTEGER
LANGUAGE plpgsql
IMMUTABLE
AS $$
DECLARE
    max_score    INTEGER := 5000;
    decay_k      DOUBLE PRECISION := 0.00082;
    distance_km  DOUBLE PRECISION;
    raw_score    DOUBLE PRECISION;
BEGIN
    IF distance_metres <= 0 THEN
        RETURN max_score;
    END IF;

    distance_km := distance_metres / 1000.0;
    raw_score   := max_score * EXP(-decay_k * distance_km);

    RETURN GREATEST(0, ROUND(raw_score)::INTEGER);
END;
$$;

-- ── compute_round_result ──────────────────────────────────────────────────────
-- Given a round_id and a guess_id, compute the distance and score
-- using PostGIS ST_Distance (geography for metre-accurate results).

CREATE OR REPLACE FUNCTION compute_round_result(p_guess_id UUID)
RETURNS VOID
LANGUAGE plpgsql
AS $$
DECLARE
    v_guess_location   GEOMETRY;
    v_station_location GEOMETRY;
    v_distance_m       DOUBLE PRECISION;
    v_score            INTEGER;
    v_station_id       UUID;
    v_round_id         UUID;
BEGIN
    -- Load guess location and round info
    SELECT pg.guess_location, gr.station_id, pg.round_id
    INTO v_guess_location, v_station_id, v_round_id
    FROM player_guesses pg
    JOIN game_rounds gr ON gr.id = pg.round_id
    WHERE pg.id = p_guess_id;

    IF v_guess_location IS NULL THEN
        RAISE EXCEPTION 'Guess % has no location', p_guess_id;
    END IF;

    -- Load station location
    SELECT location INTO v_station_location
    FROM radio_stations
    WHERE id = v_station_id;

    IF v_station_location IS NULL THEN
        RAISE EXCEPTION 'Station % has no location', v_station_id;
    END IF;

    -- ST_Distance with geography cast gives metres (great-circle)
    v_distance_m := ST_Distance(
        v_guess_location::GEOGRAPHY,
        v_station_location::GEOGRAPHY
    );

    v_score := calculate_score(v_distance_m);

    -- Write back authoritative result
    UPDATE player_guesses
    SET distance_metres = v_distance_m,
        score           = v_score,
        is_locked       = TRUE
    WHERE id = p_guess_id;
END;
$$;
