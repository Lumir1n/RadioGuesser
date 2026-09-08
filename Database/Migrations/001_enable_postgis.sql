-- Migration 001: Enable PostGIS extension
-- PostGIS is the foundation for all geographic queries in RadioGuesser.

CREATE EXTENSION IF NOT EXISTS postgis;
CREATE EXTENSION IF NOT EXISTS postgis_topology;

-- Verify
SELECT PostGIS_Version();
