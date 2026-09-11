"""
Base Graph and Horizon Database Seeder for E2E Tests.
Populates deterministic SQLite tables for Base and Horizon databases.
"""

import os
import json
import time
import sqlite3
from pathlib import Path
from typing import Any, Dict, List, Optional


BASE_GRAPH_SCHEMA = """
CREATE TABLE IF NOT EXISTS projects (
  name TEXT PRIMARY KEY,
  indexed_at TEXT NOT NULL,
  root_path TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS file_hashes (
  project TEXT NOT NULL REFERENCES projects(name) ON DELETE CASCADE,
  rel_path TEXT NOT NULL,
  sha256 TEXT NOT NULL,
  mtime_ns INTEGER NOT NULL DEFAULT 0,
  size INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (project, rel_path)
);
CREATE TABLE IF NOT EXISTS nodes (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  project TEXT NOT NULL REFERENCES projects(name) ON DELETE CASCADE,
  label TEXT NOT NULL,
  name TEXT NOT NULL,
  qualified_name TEXT NOT NULL,
  file_path TEXT DEFAULT '',
  start_line INTEGER DEFAULT 0,
  end_line INTEGER DEFAULT 0,
  properties TEXT DEFAULT '{}',
  UNIQUE(project, qualified_name)
);
CREATE TABLE IF NOT EXISTS edges (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  project TEXT NOT NULL REFERENCES projects(name) ON DELETE CASCADE,
  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  type TEXT NOT NULL,
  properties TEXT DEFAULT '{}',
  url_path_gen TEXT GENERATED ALWAYS AS (json_extract(properties,'$.url_path')),
  local_name_gen TEXT GENERATED ALWAYS AS (CASE WHEN type='IMPORTS' THEN coalesce(json_extract(properties,'$.local_name'),'') ELSE '' END),
  UNIQUE(source_id, target_id, type, local_name_gen)
);
"""

HORIZON_SCHEMA = """
CREATE TABLE IF NOT EXISTS horizon_metadata (
    horizon_id TEXT PRIMARY KEY,
    owner_session_id TEXT NOT NULL,
    client_pid INTEGER NOT NULL,
    status TEXT NOT NULL DEFAULT 'ACTIVE',
    created_at INTEGER NOT NULL,
    last_heartbeat INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS symbolic_nodes (
    cbm_uri TEXT PRIMARY KEY,
    label TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'PROPOSED',
    is_dangling INTEGER NOT NULL DEFAULT 0,
    properties TEXT DEFAULT '{}',
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS virtual_edges (
    source_uri TEXT NOT NULL,
    target_uri TEXT NOT NULL,
    edge_type TEXT NOT NULL,
    origin_horizon TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    PRIMARY KEY (source_uri, target_uri, edge_type)
);
"""


def seed_base_graph(
    db_path: str,
    project_name: str,
    root_path: str,
    nodes: Optional[List[Dict[str, Any]]] = None,
    edges: Optional[List[Dict[str, Any]]] = None
) -> None:
    """Creates and seeds a Base Graph SQLite database."""
    os.makedirs(os.path.dirname(os.path.abspath(db_path)), exist_ok=True)
    conn = sqlite3.connect(db_path)
    try:
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.executescript(BASE_GRAPH_SCHEMA)

        now_iso = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        conn.execute(
            "INSERT OR REPLACE INTO projects (name, indexed_at, root_path) VALUES (?, ?, ?)",
            (project_name, now_iso, root_path)
        )

        id_map = {}
        if nodes:
            for n in nodes:
                cur = conn.execute(
                    """INSERT OR REPLACE INTO nodes 
                       (project, label, name, qualified_name, file_path, start_line, end_line, properties)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                    (
                        project_name,
                        n.get("label", "Function"),
                        n.get("name", "sym"),
                        n["qualified_name"],
                        n.get("file_path", ""),
                        n.get("start_line", 1),
                        n.get("end_line", 10),
                        json.dumps(n.get("properties", {}))
                    )
                )
                id_map[n["qualified_name"]] = cur.lastrowid

        if edges:
            for e in edges:
                src_id = id_map.get(e["source"])
                tgt_id = id_map.get(e["target"])
                if src_id and tgt_id:
                    conn.execute(
                        """INSERT OR IGNORE INTO edges 
                           (project, source_id, target_id, type, properties)
                           VALUES (?, ?, ?, ?, ?)""",
                        (
                            project_name,
                            src_id,
                            tgt_id,
                            e.get("type", "CALLS"),
                            json.dumps(e.get("properties", {}))
                        )
                    )
        conn.commit()
    finally:
        conn.close()


def seed_horizon_db(
    db_path: str,
    horizon_id: str,
    client_pid: int,
    status: str = "ACTIVE",
    nodes: Optional[List[Dict[str, Any]]] = None,
    virtual_edges: Optional[List[Dict[str, Any]]] = None
) -> None:
    """Creates and seeds a private Horizon SQLite database."""
    os.makedirs(os.path.dirname(os.path.abspath(db_path)), exist_ok=True)
    conn = sqlite3.connect(db_path)
    try:
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.executescript(HORIZON_SCHEMA)

        now_epoch = int(time.time())
        conn.execute(
            """INSERT OR REPLACE INTO horizon_metadata 
               (horizon_id, owner_session_id, client_pid, status, created_at, last_heartbeat)
               VALUES (?, ?, ?, ?, ?, ?)""",
            (horizon_id, f"session_{horizon_id}", client_pid, status, now_epoch, now_epoch)
        )

        if nodes:
            for n in nodes:
                conn.execute(
                    """INSERT OR REPLACE INTO symbolic_nodes 
                       (cbm_uri, label, status, is_dangling, properties, created_at)
                       VALUES (?, ?, ?, ?, ?, ?)""",
                    (
                        n["cbm_uri"],
                        n.get("label", "Function"),
                        n.get("status", "PROPOSED"),
                        1 if n.get("is_dangling", False) else 0,
                        json.dumps(n.get("properties", {})),
                        now_epoch
                    )
                )

        if virtual_edges:
            for ve in virtual_edges:
                conn.execute(
                    """INSERT OR REPLACE INTO virtual_edges 
                       (source_uri, target_uri, edge_type, origin_horizon, created_at)
                       VALUES (?, ?, ?, ?, ?)""",
                    (
                        ve["source_uri"],
                        ve["target_uri"],
                        ve.get("edge_type", "VIRTUAL_CALLS"),
                        horizon_id,
                        now_epoch
                    )
                )
        conn.commit()
    finally:
        conn.close()
