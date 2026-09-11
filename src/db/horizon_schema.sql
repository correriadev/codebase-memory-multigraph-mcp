-- horizon_schema.sql: Ephemeral SQLite schema for Cognitive Horizons (Overlay Layer)

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS horizon_metadata (
    horizon_id TEXT PRIMARY KEY,
    client_pid INTEGER NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('ACTIVE', 'PROMOTED', 'DISCARDED')),
    created_at INTEGER NOT NULL,
    last_heartbeat INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS symbolic_nodes (
    cbm_uri TEXT PRIMARY KEY,
    label TEXT NOT NULL,
    epistemic_status TEXT NOT NULL DEFAULT 'PROPOSED' CHECK(epistemic_status IN ('PROPOSED', 'ACCEPTED', 'CONTESTED', 'SHADOWED')),
    is_dangling INTEGER NOT NULL DEFAULT 0,
    code_snippet TEXT,
    created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_symbolic_nodes_label ON symbolic_nodes(label);
CREATE INDEX IF NOT EXISTS idx_symbolic_nodes_status ON symbolic_nodes(epistemic_status);

CREATE TABLE IF NOT EXISTS virtual_edges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_uri TEXT NOT NULL,
    target_uri TEXT NOT NULL,
    edge_type TEXT NOT NULL,
    origin_horizon TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    UNIQUE(source_uri, target_uri, edge_type)
);

CREATE INDEX IF NOT EXISTS idx_virtual_edges_source ON virtual_edges(source_uri);
CREATE INDEX IF NOT EXISTS idx_virtual_edges_target ON virtual_edges(target_uri);
