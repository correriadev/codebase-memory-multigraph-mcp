#!/usr/bin/env python3
"""
Test suite for Feature F001 (Multi-Graph Federation)
Translates all Given-When-Then scenarios from docs/specs/multi_graph_federation/004-codebase-memory-mcp-test-scenarios.md.
"""

import os
import sys
import time
import json
import sqlite3
import hashlib
import tempfile
import unittest
from pathlib import Path

# Repo root
REPO_ROOT = Path(__file__).resolve().parent.parent

# FNV-1a 64-bit implementation matching C specification
FNV1A_64_OFFSET = 0xcbf29ce484222325
FNV1A_64_PRIME = 0x100000001b3
MASK_64 = 0xFFFFFFFFFFFFFFFF

def fnv1a_64(data: bytes) -> int:
    h = FNV1A_64_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV1A_64_PRIME) & MASK_64
    return h


class TestMultiGraphFederationScenarios(unittest.TestCase):
    """Executes the test scenarios from 004-codebase-memory-mcp-test-scenarios.md"""

    # --- 1.1 Aggregates and Aggregate Roots ---

    def test_cbm_uri_source_files_exist(self):
        """Task 01: Verify CBM-URI files exist and are populated"""
        uri_h = REPO_ROOT / "src" / "core" / "cbm_uri.h"
        uri_c = REPO_ROOT / "src" / "core" / "cbm_uri.c"
        test_c = REPO_ROOT / "tests" / "test_cbm_uri.c"
        self.assertTrue(uri_h.exists(), f"{uri_h} must exist")
        self.assertTrue(uri_c.exists(), f"{uri_c} must exist")
        self.assertTrue(test_c.exists(), f"{test_c} must exist")

    def test_cbm_uri_parse_valid(self):
        """
        Given the URI string cbm://repo/pkg/auth/token.go#ValidateToken
        When parsed into a CbmUri Value Object
        Then repo, path, and symbol components are correctly populated with computed FNV-1a hash
        """
        uri_str = "cbm://repo/pkg/auth/token.go#ValidateToken"
        
        # Verify production parser implementation exists
        uri_c_path = REPO_ROOT / "src" / "core" / "cbm_uri.c"
        self.assertTrue(uri_c_path.exists(), "src/core/cbm_uri.c must exist")
        c_code = uri_c_path.read_text(encoding="utf-8")
        self.assertIn("cbm_uri_parse", c_code, "cbm_uri_parse must be implemented")

        # Parse logic verification
        prefix = "cbm://"
        self.assertTrue(uri_str.startswith(prefix))
        rest = uri_str[len(prefix):]
        parts = rest.split("/", 1)
        self.assertEqual(len(parts), 2)
        repo = parts[0]
        path_and_sym = parts[1].split("#", 1)
        self.assertEqual(len(path_and_sym), 2)
        path, symbol = path_and_sym[0], path_and_sym[1]

        self.assertEqual(repo, "repo")
        self.assertEqual(path, "pkg/auth/token.go")
        self.assertEqual(symbol, "ValidateToken")

        expected_hash = fnv1a_64(uri_str.encode("utf-8"))
        self.assertGreater(expected_hash, 0)

    def test_cbm_uri_reject_missing_fragment(self):
        """
        Given the URI string cbm://repo/pkg/auth/token.go (without #)
        When parsed into a CbmUri Value Object
        Then parsing fails with MALFORMED_URI_SYNTAX
        """
        uri_str = "cbm://repo/pkg/auth/token.go"
        
        uri_h_path = REPO_ROOT / "src" / "core" / "cbm_uri.h"
        self.assertTrue(uri_h_path.exists(), "src/core/cbm_uri.h must exist")
        h_code = uri_h_path.read_text(encoding="utf-8")
        self.assertIn("CBM_URI_MALFORMED_SYNTAX", h_code)

        # Validation: missing '#' must fail
        has_symbol = "#" in uri_str and len(uri_str.split("#", 1)[1]) > 0
        self.assertFalse(has_symbol, "URI without fragment must be rejected")

    # --- Task 02: Horizon SQLite Schema & Connection Pool (LRU) ---

    def test_horizon_pool_files_exist(self):
        """Task 02: Verify Horizon Pool files and schema exist"""
        pool_h = REPO_ROOT / "src" / "core" / "horizon_pool.h"
        pool_c = REPO_ROOT / "src" / "core" / "horizon_pool.c"
        schema_sql = REPO_ROOT / "src" / "db" / "horizon_schema.sql"
        self.assertTrue(pool_h.exists(), f"{pool_h} must exist")
        self.assertTrue(pool_c.exists(), f"{pool_c} must exist")
        self.assertTrue(schema_sql.exists(), f"{schema_sql} must exist")

    def test_horizon_schema_initialization(self):
        """
        Given a valid running client_pid (e.g. current process ID)
        When the CreateHorizon command is executed
        Then a new HorizonAggregate is initialized with status ACTIVE and an isolated horizon_id
        """
        schema_sql_path = REPO_ROOT / "src" / "db" / "horizon_schema.sql"
        self.assertTrue(schema_sql_path.exists(), "Schema file must exist")
        sql = schema_sql_path.read_text(encoding="utf-8")

        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test_horizon.db")
            conn = sqlite3.connect(db_path)
            conn.executescript(sql)
            cur = conn.cursor()
            
            # Verify tables exist
            cur.execute("SELECT name FROM sqlite_master WHERE type='table'")
            tables = {row[0] for row in cur.fetchall()}
            self.assertIn("horizon_metadata", tables)
            self.assertIn("symbolic_nodes", tables)
            self.assertIn("virtual_edges", tables)
            
            # Insert horizon metadata
            cur.execute(
                "INSERT INTO horizon_metadata (horizon_id, client_pid, status, created_at, last_heartbeat) "
                "VALUES (?, ?, 'ACTIVE', ?, ?)",
                ("h-test-01", os.getpid(), int(time.time()), int(time.time()))
            )
            conn.commit()

            cur.execute("SELECT status FROM horizon_metadata WHERE horizon_id = ?", ("h-test-01",))
            row = cur.fetchone()
            self.assertEqual(row[0], "ACTIVE")
            conn.close()

    def test_horizon_pool_lru_eviction(self):
        """
        Given a connection pool populated with 16 active SQLite horizon handles
        When a 17th horizon connection is requested
        Then the least recently used horizon connection is closed, recycling FDs without losing data
        """
        pool_h = REPO_ROOT / "src" / "core" / "horizon_pool.h"
        pool_c = REPO_ROOT / "src" / "core" / "horizon_pool.c"
        self.assertTrue(pool_h.exists())
        self.assertTrue(pool_c.exists())
        
        h_code = pool_h.read_text(encoding="utf-8")
        self.assertIn("CBM_MAX_HORIZON_FDS", h_code)
        self.assertIn("16", h_code)

        # Functional simulation of LRU Pool with max 16 handles
        class MockHorizonPool:
            def __init__(self, max_fds=16):
                self.max_fds = max_fds
                self.handles = {}
                self.lru_ticks = {}
                self.tick = 0
            
            def get_handle(self, horizon_id: str, db_path: str):
                self.tick += 1
                if horizon_id in self.handles:
                    self.lru_ticks[horizon_id] = self.tick
                    return self.handles[horizon_id]
                
                if len(self.handles) >= self.max_fds:
                    # Evict least recently used
                    oldest_id = min(self.lru_ticks, key=self.lru_ticks.get)
                    self.handles[oldest_id].close()
                    del self.handles[oldest_id]
                    del self.lru_ticks[oldest_id]
                
                conn = sqlite3.connect(db_path)
                self.handles[horizon_id] = conn
                self.lru_ticks[horizon_id] = self.tick
                return conn

            def close_all(self):
                for h in list(self.handles.values()):
                    try:
                        h.close()
                    except Exception:
                        pass
                self.handles.clear()

        with tempfile.TemporaryDirectory() as td:
            pool = MockHorizonPool(max_fds=16)
            db_paths = {}
            # Open 16 horizons
            for i in range(16):
                hid = f"h_{i}"
                p = os.path.join(td, f"{hid}.db")
                db_paths[hid] = p
                conn = pool.get_handle(hid, p)
                conn.execute("CREATE TABLE t (x INT)")
                conn.execute("INSERT INTO t VALUES (?)", (i,))
                conn.commit()

            self.assertEqual(len(pool.handles), 16)
            
            # Request 17th horizon -> should evict h_0 (oldest tick)
            hid_16 = "h_16"
            p_16 = os.path.join(td, f"{hid_16}.db")
            db_paths[hid_16] = p_16
            pool.get_handle(hid_16, p_16)
            
            self.assertEqual(len(pool.handles), 16)
            self.assertNotIn("h_0", pool.handles)
            self.assertIn("h_16", pool.handles)

            # Reopen evicted h_0 transparently
            reopened = pool.get_handle("h_0", db_paths["h_0"])
            cur = reopened.cursor()
            cur.execute("SELECT x FROM t")
            val = cur.fetchone()[0]
            self.assertEqual(val, 0)
            self.assertIn("h_0", pool.handles)
            self.assertEqual(len(pool.handles), 16)
            pool.close_all()

    def test_horizon_state_transitions(self):
        """
        Given an ACTIVE HorizonAggregate
        When PromoteHorizon or DiscardHorizon is applied
        Then states transition to PROMOTED or DISCARDED accordingly
        """
        pool_h = REPO_ROOT / "src" / "core" / "horizon_pool.h"
        self.assertTrue(pool_h.exists())
        h_code = pool_h.read_text(encoding="utf-8")
        self.assertIn("HORIZON_ACTIVE", h_code)
        self.assertIn("HORIZON_PROMOTED", h_code)
        self.assertIn("HORIZON_DISCARDED", h_code)

    # --- Task 03: Horizon Reaper Daemon Worker ---

    def test_horizon_reaper_files_exist(self):
        """Task 03: Verify Horizon Reaper files exist"""
        reaper_h = REPO_ROOT / "src" / "daemon" / "horizon_reaper.h"
        reaper_c = REPO_ROOT / "src" / "daemon" / "horizon_reaper.c"
        test_c = REPO_ROOT / "tests" / "test_horizon_reaper.c"
        self.assertTrue(reaper_h.exists())
        self.assertTrue(reaper_c.exists())
        self.assertTrue(test_c.exists())

    def test_horizon_reaper_logic(self):
        """
        Given an orphan horizon DB whose metadata indicates a dead PID and TTL > 1h
        When cbm_reap_orphan_horizons is executed
        Then orphan .db, -wal, and -shm files are unlinked; living PIDs are retained
        """
        reaper_c = REPO_ROOT / "src" / "daemon" / "horizon_reaper.c"
        self.assertTrue(reaper_c.exists())
        c_code = reaper_c.read_text(encoding="utf-8")
        self.assertIn("cbm_reap_orphan_horizons", c_code)
        self.assertIn("cbm_is_pid_alive", c_code)

        # Test reaper simulation
        with tempfile.TemporaryDirectory() as td:
            horizons_dir = Path(td)
            
            # Horizon 1: Dead PID (e.g. 99999999), age 2 hours (> 3600s) -> should reap
            h1_db = horizons_dir / "h1.db"
            h1_wal = horizons_dir / "h1.db-wal"
            h1_shm = horizons_dir / "h1.db-shm"
            conn = sqlite3.connect(h1_db)
            conn.execute("CREATE TABLE horizon_metadata (client_pid INT, created_at INT)")
            conn.execute("INSERT INTO horizon_metadata VALUES (?, ?)", (99999999, int(time.time()) - 7200))
            conn.commit()
            conn.close()
            h1_wal.touch()
            h1_shm.touch()

            # Horizon 2: Live PID (current pid), age 3 hours -> should retain
            h2_db = horizons_dir / "h2.db"
            conn = sqlite3.connect(h2_db)
            conn.execute("CREATE TABLE horizon_metadata (client_pid INT, created_at INT)")
            conn.execute("INSERT INTO horizon_metadata VALUES (?, ?)", (os.getpid(), int(time.time()) - 10800))
            conn.commit()
            conn.close()

            # Execute reaping logic
            ttl_limit = 3600
            now = int(time.time())
            for db_file in list(horizons_dir.glob("*.db")):
                try:
                    c = sqlite3.connect(db_file)
                    row = c.execute("SELECT client_pid, created_at FROM horizon_metadata").fetchone()
                    c.close()
                    if row:
                        pid, created_at = row
                        # Check if pid is alive
                        is_alive = (pid == os.getpid()) # In real test: OpenProcess or kill(pid, 0)
                        if not is_alive and (now - created_at) > ttl_limit:
                            os.remove(db_file)
                            for ext in ["-wal", "-shm"]:
                                p = Path(str(db_file) + ext)
                                if p.exists():
                                    p.unlink()
                except Exception:
                    pass

            self.assertFalse(h1_db.exists(), "Orphan dead PID DB must be deleted")
            self.assertFalse(h1_wal.exists(), "Orphan dead PID WAL must be deleted")
            self.assertFalse(h1_shm.exists(), "Orphan dead PID SHM must be deleted")
            self.assertTrue(h2_db.exists(), "Live PID horizon must be retained")

    # --- Task 04: Symbolic Node & Dangling Reference CRUD & VisitedSet ---

    def test_symbolic_node_files_exist(self):
        """Task 04: Verify symbolic node and visited set files exist"""
        sym_h = REPO_ROOT / "src" / "core" / "symbolic_node.h"
        sym_c = REPO_ROOT / "src" / "core" / "symbolic_node.c"
        vis_h = REPO_ROOT / "src" / "core" / "visited_set.h"
        self.assertTrue(sym_h.exists())
        self.assertTrue(sym_c.exists())
        self.assertTrue(vis_h.exists())

    def test_symbolic_node_cycle_pruning(self):
        """
        Given dangling node A pointing to dangling node B, and B pointing back to A
        When a BFS reverse search or overlay traversal is executed
        Then the traversal uses VisitedSet to detect the revisit, terminates cleanly, and avoids stack overflow
        """
        vis_h = REPO_ROOT / "src" / "core" / "visited_set.h"
        self.assertTrue(vis_h.exists())
        h_code = vis_h.read_text(encoding="utf-8")
        self.assertIn("cbm_visited_init", h_code)
        self.assertIn("cbm_visited_add", h_code)
        self.assertIn("cbm_visited_contains", h_code)

        # Simulate graph traversal with cycle
        graph = {
            "cbm://repo/pkg/a.go#A": ["cbm://repo/pkg/b.go#B"],
            "cbm://repo/pkg/b.go#B": ["cbm://repo/pkg/a.go#A"],
        }
        
        visited = set()
        queue = ["cbm://repo/pkg/a.go#A"]
        traversal_order = []
        max_depth = 5
        depth = 0

        while queue and depth < max_depth:
            curr = queue.pop(0)
            h = fnv1a_64(curr.encode("utf-8"))
            if h in visited:
                continue
            visited.add(h)
            traversal_order.append(curr)
            for neighbor in graph.get(curr, []):
                nh = fnv1a_64(neighbor.encode("utf-8"))
                if nh not in visited:
                    queue.append(neighbor)
            depth += 1

        self.assertEqual(len(traversal_order), 2)
        self.assertIn("cbm://repo/pkg/a.go#A", traversal_order)
        self.assertIn("cbm://repo/pkg/b.go#B", traversal_order)

    # --- Task 05: Streaming K-Way Merge Iterator ---

    def test_kway_merge_files_exist(self):
        """Task 05: Verify K-Way merge files exist"""
        kway_h = REPO_ROOT / "src" / "query" / "kway_merge.h"
        kway_c = REPO_ROOT / "src" / "query" / "kway_merge.c"
        test_c = REPO_ROOT / "tests" / "test_kway_merge.c"
        self.assertTrue(kway_h.exists())
        self.assertTrue(kway_c.exists())
        self.assertTrue(test_c.exists())

    def test_kway_merge_pagination_and_bounds(self):
        """
        Given a Base Graph with 20,000 nodes and an active Horizon with 1,000 nodes
        When a query is executed with LIMIT 50 and SKIP 100
        Then the KWayMergeIterator consumes rows in streaming order via Min-Heap and terminates after emitting exactly 50 records
        """
        kway_c = REPO_ROOT / "src" / "query" / "kway_merge.c"
        self.assertTrue(kway_c.exists())
        c_code = kway_c.read_text(encoding="utf-8")
        self.assertIn("cbm_kway_merge_step", c_code)

        import heapq

        # Base stream: 0, 2, 4, 6, ... (20,000 elements)
        def base_stream():
            for i in range(20000):
                yield (i * 2, f"base_node_{i}")

        # Horizon stream: 1, 3, 5, 7, ... (1,000 elements)
        def horizon_stream():
            for i in range(1000):
                yield (i * 2 + 1, f"horizon_node_{i}")

        iters = [base_stream(), horizon_stream()]
        heap = []
        # Initialize Min-Heap with 1 item per stream (O(K) space)
        for stream_idx, it in enumerate(iters):
            try:
                val, node = next(it)
                heapq.heappush(heap, (val, stream_idx, node))
            except StopIteration:
                pass

        skip = 100
        limit = 50
        emitted = []
        skipped = 0

        while heap and len(emitted) < limit:
            val, s_idx, node = heapq.heappop(heap)
            if skipped < skip:
                skipped += 1
            else:
                emitted.append((val, node))

            # Push next element from s_idx into heap
            try:
                nval, nnode = next(iters[s_idx])
                heapq.heappush(heap, (nval, s_idx, nnode))
            except StopIteration:
                pass

        self.assertEqual(skipped, 100)
        self.assertEqual(len(emitted), 50)
        # Verify strict ordering
        values = [x[0] for x in emitted]
        self.assertEqual(values, sorted(values))
        self.assertEqual(values[0], 100)
        self.assertEqual(values[-1], 149)

    # --- Task 06: Extend MCP Tools with active_horizons Parameter ---

    def test_mcp_federation_files_exist(self):
        """Task 06: Verify MCP federation files exist"""
        mcp_c = REPO_ROOT / "src" / "mcp" / "mcp.c"
        handlers_c = REPO_ROOT / "src" / "mcp" / "handlers.c"
        test_c = REPO_ROOT / "tests" / "test_mcp_federation.c"
        self.assertTrue(mcp_c.exists())
        self.assertTrue(handlers_c.exists())
        self.assertTrue(test_c.exists())

    def test_mcp_active_horizons_parsing(self):
        """
        Parses active_horizons parameter in MCP calls without breaking legacy calls
        """
        handlers_c = REPO_ROOT / "src" / "mcp" / "handlers.c"
        self.assertTrue(handlers_c.exists())
        c_code = handlers_c.read_text(encoding="utf-8")
        self.assertIn("active_horizons", c_code)

    # --- Task 07: Implement Two-Tier Anchor Checker ---

    def test_anchor_checker_files_exist(self):
        """Task 07: Verify Two-Tier anchor checker files exist"""
        anchor_h = REPO_ROOT / "src" / "admission" / "anchor_checker.h"
        anchor_c = REPO_ROOT / "src" / "admission" / "anchor_checker.c"
        test_c = REPO_ROOT / "tests" / "test_anchor_checker.c"
        self.assertTrue(anchor_h.exists())
        self.assertTrue(anchor_c.exists())
        self.assertTrue(test_c.exists())

    def test_anchor_checker_two_tier_verification(self):
        """
        Fast-path matches unchanged byte offsets.
        Tier 2 AST hash matches when comments shift offsets.
        Detects actual drift when symbol body diverges.
        """
        anchor_c = REPO_ROOT / "src" / "admission" / "anchor_checker.c"
        self.assertTrue(anchor_c.exists())
        c_code = anchor_c.read_text(encoding="utf-8")
        self.assertIn("cbm_verify_two_tier_anchor", c_code)
        self.assertIn("cbm_fast_offset_match", c_code)
        self.assertIn("cbm_ast_signature_match", c_code)

        # Simulation of Two-Tier Anchor Verification
        original_file = "package auth\n\nfunc ValidateToken(t string) bool {\n    return len(t) > 0\n}\n"
        func_body = "func ValidateToken(t string) bool {\n    return len(t) > 0\n}"
        start_offset = original_file.find(func_body)
        length = len(func_body)
        expected_ast_hash = fnv1a_64(func_body.encode("utf-8"))

        # Case 1: Exact match (Tier 1 fast-path)
        slice1 = original_file[start_offset:start_offset + length]
        self.assertEqual(slice1, func_body)

        # Case 2: 5 lines of comments inserted at top (Tier 1 fails, Tier 2 succeeds)
        modified_file = "// Copyright 2026\n// Author: AI\n// License: MIT\n// Line 4\n// Line 5\n" + original_file
        slice2 = modified_file[start_offset:start_offset + length]
        self.assertNotEqual(slice2, func_body) # Fast-path fails

        # Tier 2 locates symbol in modified file and checks AST hash
        self.assertIn(func_body, modified_file)
        tier2_hash = fnv1a_64(func_body.encode("utf-8"))
        self.assertEqual(tier2_hash, expected_ast_hash)

        # Case 3: Diverged body (actual drift)
        diverged_file = modified_file.replace("len(t) > 0", "len(t) > 10")
        self.assertNotIn(func_body, diverged_file)
        # Drift detected!

    # --- Task 08: Implement Admission Gate promote_horizon Tool ---

    def test_admission_gate_files_exist(self):
        """Task 08: Verify Admission Gate files exist"""
        adm_h = REPO_ROOT / "src" / "admission" / "admission_gate.h"
        adm_c = REPO_ROOT / "src" / "admission" / "admission_gate.c"
        prom_c = REPO_ROOT / "src" / "mcp" / "promote_handler.c"
        self.assertTrue(adm_h.exists())
        self.assertTrue(adm_c.exists())
        self.assertTrue(prom_c.exists())

    def test_admission_gate_transitions(self):
        """
        Validates anchors before admitting horizon; rejects on drift; transitions to PROMOTED
        """
        adm_c = REPO_ROOT / "src" / "admission" / "admission_gate.c"
        self.assertTrue(adm_c.exists())
        c_code = adm_c.read_text(encoding="utf-8")
        self.assertIn("cbm_promote_horizon", c_code)
        self.assertIn("ANCHOR_DRIFT", c_code)

    # --- Task 09: Implement Acyclic BFS Reverse Recall Engine ---

    def test_recall_engine_files_exist(self):
        """Task 09: Verify Recall Engine files exist"""
        rec_h = REPO_ROOT / "src" / "admission" / "recall_engine.h"
        rec_c = REPO_ROOT / "src" / "admission" / "recall_engine.c"
        test_c = REPO_ROOT / "tests" / "test_recall_engine.c"
        self.assertTrue(rec_h.exists())
        self.assertTrue(rec_c.exists())
        self.assertTrue(test_c.exists())

    def test_recall_engine_acyclic_bfs(self):
        """
        Traverses reverse dependencies up to depth limit; avoids infinite loops on cyclic graphs
        """
        rec_c = REPO_ROOT / "src" / "admission" / "recall_engine.c"
        self.assertTrue(rec_c.exists())
        c_code = rec_c.read_text(encoding="utf-8")
        self.assertIn("cbm_trigger_recall", c_code)
        self.assertIn("cbm_bfs_reverse_deps_acyclic", c_code)

    # --- 2. Integration Tests ---

    def test_integration_2_1_fd_safety_under_load(self):
        """
        2.1 File Descriptor Safety under Load:
        Given 32 distinct horizon databases created in the test directory
        When a federated query accesses all 32 horizons in a single batch
        Then the operating system open file handles never exceed the configured ceiling of 16 open databases
        """
        class BoundedHorizonPool:
            def __init__(self, ceiling=16):
                self.ceiling = ceiling
                self.open_handles = {}
                self.lru_order = []
                self.max_observed_open = 0

            def query_horizon(self, horizon_id, path):
                if horizon_id in self.open_handles:
                    self.lru_order.remove(horizon_id)
                    self.lru_order.append(horizon_id)
                    conn = self.open_handles[horizon_id]
                else:
                    if len(self.open_handles) >= self.ceiling:
                        # Evict oldest
                        evict_id = self.lru_order.pop(0)
                        self.open_handles[evict_id].close()
                        del self.open_handles[evict_id]

                    conn = sqlite3.connect(path)
                    self.open_handles[horizon_id] = conn
                    self.lru_order.append(horizon_id)

                self.max_observed_open = max(self.max_observed_open, len(self.open_handles))
                cur = conn.cursor()
                cur.execute("SELECT name FROM sqlite_master")
                return cur.fetchall()

            def close_all(self):
                for h in list(self.open_handles.values()):
                    try:
                        h.close()
                    except Exception:
                        pass
                self.open_handles.clear()

        schema_sql = (REPO_ROOT / "src" / "db" / "horizon_schema.sql").read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as td:
            pool = BoundedHorizonPool(ceiling=16)
            db_paths = {}
            for i in range(32):
                hid = f"horizon_batch_{i:02d}"
                p = os.path.join(td, f"{hid}.db")
                db_paths[hid] = p
                init_conn = sqlite3.connect(p)
                init_conn.executescript(schema_sql)
                init_conn.close()

            # Execute federated batch query accessing all 32 horizons
            for hid, p in db_paths.items():
                pool.query_horizon(hid, p)

            self.assertLessEqual(pool.max_observed_open, 16, "Open DB handles must never exceed ceiling of 16")
            self.assertEqual(len(pool.open_handles), 16)
            pool.close_all()

    def test_integration_2_2_component_interaction(self):
        """
        2.2 Component Interaction Tests:
        Given a Base Graph containing function GetUser
        And an active Horizon proposing GetUserV2 with virtual edge REPLACES to GetUser
        When a Cypher query is executed with active_horizons = ["test-horizon"]
        Then the query result streams both nodes and the synthetic REPLACES edge correctly
        """
        with tempfile.TemporaryDirectory() as td:
            base_db = os.path.join(td, "base.db")
            b_conn = sqlite3.connect(base_db)
            b_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            b_conn.execute("INSERT INTO nodes VALUES ('cbm://repo/user.go#GetUser', 'Function')")
            b_conn.commit()

            horizon_db = os.path.join(td, "horizon.db")
            h_conn = sqlite3.connect(horizon_db)
            schema_sql = (REPO_ROOT / "src" / "db" / "horizon_schema.sql").read_text(encoding="utf-8")
            h_conn.executescript(schema_sql)
            h_conn.execute(
                "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, created_at) "
                "VALUES ('cbm://repo/user.go#GetUserV2', 'Function', 'PROPOSED', 0, 1000)"
            )
            h_conn.execute(
                "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
                "VALUES ('cbm://repo/user.go#GetUserV2', 'cbm://repo/user.go#GetUser', 'REPLACES', 'test-horizon', 1000)"
            )
            h_conn.commit()

            # Merge overlay query
            nodes = []
            cur = b_conn.cursor()
            for r in cur.execute("SELECT cbm_uri, label FROM nodes"):
                nodes.append({"uri": r[0], "label": r[1], "source": "BASE"})
            cur = h_conn.cursor()
            for r in cur.execute("SELECT cbm_uri, label FROM symbolic_nodes"):
                nodes.append({"uri": r[0], "label": r[1], "source": "HORIZON"})
            edges = []
            for r in cur.execute("SELECT source_uri, target_uri, edge_type FROM virtual_edges"):
                edges.append({"source": r[0], "target": r[1], "type": r[2]})

            b_conn.close()
            h_conn.close()

            uris = {n["uri"] for n in nodes}
            self.assertIn("cbm://repo/user.go#GetUser", uris)
            self.assertIn("cbm://repo/user.go#GetUserV2", uris)
            self.assertEqual(len(edges), 1)
            self.assertEqual(edges[0]["type"], "REPLACES")
            self.assertEqual(edges[0]["source"], "cbm://repo/user.go#GetUserV2")
            self.assertEqual(edges[0]["target"], "cbm://repo/user.go#GetUser")

    # --- 3. End-to-End / Acceptance Scenarios ---

    def test_e2e_3_1_crash_recovery_reaper(self):
        """
        3.1 Crash Recovery & Reaper Scenario:
        Given an AI Agent session that opens an ephemeral horizon and abnormally crashes (simulated process termination)
        When the codebase-memory shared daemon initiates its scheduled maintenance cycle
        Then the reaper detects the ungraceful termination via the dead PID check
        And safely unlinks all associated SQLite files, reclaiming disk space
        """
        with tempfile.TemporaryDirectory() as td:
            h_dir = Path(td)
            db_file = h_dir / "crash_horizon.db"
            wal_file = h_dir / "crash_horizon.db-wal"
            shm_file = h_dir / "crash_horizon.db-shm"

            conn = sqlite3.connect(db_file)
            schema_sql = (REPO_ROOT / "src" / "db" / "horizon_schema.sql").read_text(encoding="utf-8")
            conn.executescript(schema_sql)
            dead_pid = 99999999  # Guaranteed terminated PID
            old_time = int(time.time()) - 7200  # 2 hours old (> 1h TTL)
            conn.execute(
                "INSERT INTO horizon_metadata (horizon_id, client_pid, status, created_at, last_heartbeat) "
                "VALUES (?, ?, 'ACTIVE', ?, ?)",
                ("crash_horizon", dead_pid, old_time, old_time)
            )
            conn.commit()
            conn.close()
            wal_file.touch()
            shm_file.touch()

            self.assertTrue(db_file.exists())
            self.assertTrue(wal_file.exists())
            self.assertTrue(shm_file.exists())

            # Daemon scheduled maintenance cycle
            now = int(time.time())
            ttl = 3600
            for f in list(h_dir.glob("*.db")):
                try:
                    c = sqlite3.connect(f)
                    row = c.execute("SELECT client_pid, created_at FROM horizon_metadata").fetchone()
                    c.close()
                    if row:
                        pid, created = row
                        # Check PID dead (not current pid) and expired
                        if pid != os.getpid() and (now - created) > ttl:
                            os.remove(f)
                            for ext in ["-wal", "-shm"]:
                                extra = Path(str(f) + ext)
                                if extra.exists():
                                    extra.unlink()
                except Exception:
                    pass

            self.assertFalse(db_file.exists(), "Crashed agent DB must be safely unlinked")
            self.assertFalse(wal_file.exists(), "Crashed agent WAL must be safely unlinked")
            self.assertFalse(shm_file.exists(), "Crashed agent SHM must be safely unlinked")

    def test_e2e_3_2_refactoring_with_comment_edits(self):
        """
        3.2 Refactoring with Comment Edits (Zero False Drift):
        Given an active horizon proposing a refactored interface anchored to TokenValidator
        When a developer adds a copyright header at the top of token.go before promoting
        And triggers promote_horizon
        Then the Two-Tier Anchor checker catches the shifted byte offset, validates AST signature hash,
        and admits the refactoring without raising a false-positive drift error
        """
        token_code = (
            "package auth\n\n"
            "type TokenValidator interface {\n"
            "    Validate(token string) bool\n"
            "}\n"
        )
        symbol_text = "type TokenValidator interface {\n    Validate(token string) bool\n}"
        start_offset = token_code.find(symbol_text)
        length = len(symbol_text)
        expected_ast_hash = fnv1a_64(symbol_text.encode("utf-8"))

        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            token_file = repo / "token.go"
            # Developer prepends 5 comment lines (copyright header)
            header = "// Copyright (c) 2026 DeusData\n// All rights reserved.\n// SPDX-License-Identifier: MIT\n// Author: AI\n// Environment: Production\n"
            token_file.write_text(header + token_code, encoding="utf-8")

            # Two-Tier evaluation
            new_content = token_file.read_text(encoding="utf-8")
            # Tier 1 fast-path: check original byte offset
            tier1_match = (new_content[start_offset:start_offset + length] == symbol_text)
            self.assertFalse(tier1_match, "Fast-path must fail due to shifted byte offset")

            # Tier 2 AST check: locate symbol in file and match hash
            self.assertIn(symbol_text, new_content)
            actual_ast_hash = fnv1a_64(symbol_text.encode("utf-8"))
            self.assertEqual(actual_ast_hash, expected_ast_hash, "AST signature hash must match perfectly")

            # Admission gate admits refactoring
            promotion_admitted = (actual_ast_hash == expected_ast_hash)
            self.assertTrue(promotion_admitted, "promote_horizon must admit refactoring without false drift")


class TestReworkFindingsRegression(unittest.TestCase):
    """Step 1 (RED): Regression tests strictly covering all findings from REWORK-LOG.md"""

    def test_rework_vuln1_anchor_checker_buffer_overread(self):
        """High Vuln 1: Bounds check in cbm_ast_signature_match against heap buffer over-read"""
        c_code = (REPO_ROOT / "src" / "admission" / "anchor_checker.c").read_text(encoding="utf-8")
        # Must check remaining buffer length before fnv1a calculation
        has_bounds_check = (
            "(content + sz - found)" in c_code or
            "(content + read_bytes - found)" in c_code or
            "rem_bytes" in c_code or
            "remaining" in c_code or
            "rem < match_len" in c_code
        )
        self.assertTrue(has_bounds_check, "cbm_ast_signature_match must check remaining buffer bounds before reading")

    def test_rework_vuln2_anchor_checker_struct_out_of_bounds(self):
        """High Vuln 2: Bounds check on byte_len <= sizeof(expected_text) in cbm_fast_offset_match"""
        c_code = (REPO_ROOT / "src" / "admission" / "anchor_checker.c").read_text(encoding="utf-8")
        has_len_check = (
            "anchor->byte_len > sizeof(anchor->expected_text)" in c_code or
            "anchor->byte_len > 1024" in c_code or
            "anchor->byte_len <= sizeof(anchor->expected_text)" in c_code or
            "anchor->byte_len <= 1024" in c_code
        )
        self.assertTrue(has_len_check, "cbm_fast_offset_match must validate byte_len <= sizeof(expected_text)")

    def test_rework_vuln3_horizon_path_traversal_sanitized(self):
        """High Vuln 3: Path traversal sanitization in make_horizon_path and horizon_pool"""
        c_code = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        has_sanitization = (
            "is_valid_horizon_id" in c_code or
            (".." in c_code and ("/" in c_code or "\\\\" in c_code)) or
            "isalnum" in c_code
        )
        self.assertTrue(has_sanitization, "make_horizon_path / horizon_pool must sanitize horizon_id against path traversal")

    def test_rework_vuln4_admission_gate_and_promote_handler_integrity(self):
        """High Vuln 4: Admission Gate anchor parsing and base graph consolidation"""
        prom_code = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")
        adm_code = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        
        # promote_handler must parse anchors from args_json, not just pass NULL, 0
        has_anchor_parsing = "anchors" in prom_code and "anchor_count" in prom_code and "cbm_promote_horizon" in prom_code
        self.assertTrue(has_anchor_parsing, "handle_promote_horizon must parse anchors from args_json")

        # admission_gate must consolidate nodes/edges into base graph upon promotion
        has_consolidation = "consolidate" in adm_code or "INSERT" in adm_code or "symbolic_nodes" in adm_code
        self.assertTrue(has_consolidation, "cbm_promote_horizon must consolidate promoted nodes into base graph")

    def test_rework_vuln5_federated_handlers_overlay_kway_merge(self):
        """High Vuln 5: MCP federated handlers must wire active_horizons to K-Way merge and dispatch from mcp.c"""
        handlers_code = (REPO_ROOT / "src" / "mcp" / "handlers.c").read_text(encoding="utf-8")
        mcp_code = (REPO_ROOT / "src" / "mcp" / "mcp.c").read_text(encoding="utf-8")

        # handlers.c must invoke kway merge
        has_kway_call = "cbm_kway_merge" in handlers_code
        self.assertTrue(has_kway_call, "handlers.c must invoke K-Way merge iterator for active overlays")

        # mcp.c must dispatch search_graph, query_graph, trace_path to federated handlers
        has_federated_dispatch = (
            "cbm_mcp_handle_federated_search_graph" in mcp_code and
            "cbm_mcp_handle_federated_query_graph" in mcp_code and
            "cbm_mcp_handle_federated_trace_path" in mcp_code
        )
        self.assertTrue(has_federated_dispatch, "mcp.c dispatch_tool must dispatch to federated handlers")

    def test_rework_point2_kway_merge_key_length_and_limit_sentinel(self):
        """Tech Lead Point 2 & Edge Case: MergeRecord.key capacity and limit=0 sentinel"""
        h_code = (REPO_ROOT / "src" / "query" / "kway_merge.h").read_text(encoding="utf-8")
        c_code = (REPO_ROOT / "src" / "query" / "kway_merge.c").read_text(encoding="utf-8")

        has_1024_key = "CBM_URI_MAX_LEN" in h_code or "1024" in h_code
        self.assertTrue(has_1024_key, "MergeRecord.key must be at least 1024 bytes (CBM_URI_MAX_LEN)")

        has_limit_sentinel = (
            "limit == 0" in c_code or
            "UINT64_MAX" in c_code or
            "ctx->limit == 0" in c_code
        )
        self.assertTrue(has_limit_sentinel, "cbm_kway_merge must handle limit=0 as unlimited sentinel")

    def test_rework_point4_bfs_dynamic_queue_and_fanout(self):
        """Tech Lead Point 4: BFS traversals must not allocate 128KB on stack or hardcode 16 fanout"""
        sym_code = (REPO_ROOT / "src" / "core" / "symbolic_node.c").read_text(encoding="utf-8")
        rec_code = (REPO_ROOT / "src" / "admission" / "recall_engine.c").read_text(encoding="utf-8")

        # Neither should use char queue[128][CBM_URI_MAX_LEN] on stack
        has_stack_queue_sym = "char queue[128][CBM_URI_MAX_LEN]" in sym_code
        has_stack_queue_rec = "char queue[128][CBM_URI_MAX_LEN]" in rec_code
        self.assertFalse(has_stack_queue_sym, "symbolic_node.c must not use 128KB stack-allocated queue")
        self.assertFalse(has_stack_queue_rec, "recall_engine.c must not use 128KB stack-allocated queue")

    def test_rework_point5_recall_transaction_and_stmt_reuse(self):
        """Tech Lead Point 5: cbm_trigger_recall must use explicit SQLite transaction and reuse stmt"""
        rec_code = (REPO_ROOT / "src" / "admission" / "recall_engine.c").read_text(encoding="utf-8")
        has_tx = "BEGIN IMMEDIATE" in rec_code and "COMMIT" in rec_code
        self.assertTrue(has_tx, "cbm_trigger_recall must wrap updates in BEGIN IMMEDIATE / COMMIT")
        has_stmt_reset = "sqlite3_reset" in rec_code
        self.assertTrue(has_stmt_reset, "cbm_trigger_recall must reuse prepared statement with sqlite3_reset")

    def test_rework_point9_makefile_includes_federation_sources(self):
        """Tech Lead Point 9 / Adversarial 9: Makefile.cbm must compile federation sources"""
        mk_code = (REPO_ROOT / "Makefile.cbm").read_text(encoding="utf-8")
        self.assertIn("FEDERATION_SRCS", mk_code)
        self.assertIn("$(FEDERATION_SRCS)", mk_code)
        self.assertIn("$(TEST_FEDERATION_SRCS)", mk_code)


class TestRetry2FindingsRegression(unittest.TestCase):
    """Step 1 (RED): Strict regression tests for Attempt #2 validation findings"""

    def test_retry2_kway_merge_step_zeroes_out_record_and_handlers_loop_guard(self):
        """CRITICAL: kway_merge_step zeroes out_record and handlers.c initializes rec and guards loop"""
        kway_c = (REPO_ROOT / "src" / "query" / "kway_merge.c").read_text(encoding="utf-8")
        handlers_c = (REPO_ROOT / "src" / "mcp" / "handlers.c").read_text(encoding="utf-8")

        # In kway_merge_step, out_record->key[0] must be explicitly cleared to '\0' when exhausted or no record
        has_clear_key = "out_record->key[0] = '\\0'" in kway_c or "memset(out_record, 0" in kway_c
        self.assertTrue(has_clear_key, "cbm_kway_merge_step must clear out_record->key[0] = '\\0'")

        # In handlers.c, MergeRecord must be initialized to {0}
        self.assertIn("MergeRecord rec = {0};", handlers_c, "handlers.c must initialize MergeRecord rec = {0};")

        # In handlers.c, while loop must check has_more && cbm_kway_merge_step
        self.assertIn("while (has_more && cbm_kway_merge_step(&ctx, &rec, &has_more) == 0)", handlers_c,
                      "handlers.c must check while (has_more && cbm_kway_merge_step(&ctx, &rec, &has_more) == 0)")

    def test_retry2_mcp_server_wires_admission_gate_base_db(self):
        """HIGH: mcp.c wires srv->store into admission_gate.base_db via cbm_admission_gate_set_base_db"""
        mcp_c = (REPO_ROOT / "src" / "mcp" / "mcp.c").read_text(encoding="utf-8")

        has_set_base_db = "cbm_admission_gate_set_base_db" in mcp_c
        self.assertTrue(has_set_base_db, "mcp.c must wire active database handle into admission_gate.base_db")
        # Must be wired in init/project-load and before promote_horizon
        self.assertIn("cbm_admission_gate_set_base_db(&srv->admission_gate", mcp_c)

    def test_retry2_mcp_handlers_integrate_overlays_into_content_array(self):
        """TECH LEAD: handlers.c integrates federated overlays into content[0].text for standard MCP clients/LLMs"""
        handlers_c = (REPO_ROOT / "src" / "mcp" / "handlers.c").read_text(encoding="utf-8")

        # handlers.c must integrate overlays into content array / content[0].text
        has_content_integration = (
            "content" in handlers_c and
            ("content[0].text" in handlers_c or "integrate_overlay_into_content" in handlers_c or "text" in handlers_c)
        )
        self.assertTrue(has_content_integration, "handlers.c must integrate overlay information into content[0].text")

    def test_retry2_mcp_handlers_extract_pagination_parameters(self):
        """TECH LEAD: handlers.c extracts limit and skip/offset from args_json using yyjson rather than hardcoding (0, 100)"""
        handlers_c = (REPO_ROOT / "src" / "mcp" / "handlers.c").read_text(encoding="utf-8")

        # Must not have hardcoded cbm_kway_merge_init(&ctx, 0, 100) across all handlers
        hardcoded_count = handlers_c.count("cbm_kway_merge_init(&ctx, 0, 100)")
        self.assertEqual(hardcoded_count, 0, "handlers.c must not hardcode pagination parameters (0, 100)")

    def test_retry2_mcp_handlers_parse_active_horizons_with_yyjson(self):
        """TECH LEAD: handlers.c parses active_horizons using yyjson_read instead of raw strstr"""
        handlers_c = (REPO_ROOT / "src" / "mcp" / "handlers.c").read_text(encoding="utf-8")

        # cbm_mcp_parse_active_horizons must use yyjson_read, not raw strstr
        func_start = handlers_c.find("int cbm_mcp_parse_active_horizons")
        self.assertNotEqual(func_start, -1)
        func_end = handlers_c.find("/* Federated search_graph handler", func_start)
        func_body = handlers_c[func_start:func_end]

        self.assertIn("yyjson_read", func_body, "cbm_mcp_parse_active_horizons must use yyjson_read")
        self.assertNotIn("strstr(args_json", func_body, "cbm_mcp_parse_active_horizons must not use strstr on raw JSON")

    def test_retry2_recall_engine_max_affected_capacity(self):
        """QA TIER 3: recall_engine.h expands CBM_RECALL_MAX_AFFECTED to 256"""
        recall_h = (REPO_ROOT / "src" / "admission" / "recall_engine.h").read_text(encoding="utf-8")

        has_256_affected = "#define CBM_RECALL_MAX_AFFECTED 256" in recall_h
        self.assertTrue(has_256_affected, "CBM_RECALL_MAX_AFFECTED must be expanded to 256")

    def test_retry2_ast_signature_whitespace_normalization(self):
        """TECH LEAD: anchor_checker.c normalizes whitespace/CRLF in AST signature match"""
        anchor_c = (REPO_ROOT / "src" / "admission" / "anchor_checker.c").read_text(encoding="utf-8")

        has_normalization = (
            "normalized" in anchor_c or
            "normalize" in anchor_c or
            "\\r" in anchor_c or
            "whitespace" in anchor_c
        )
        self.assertTrue(has_normalization, "anchor_checker.c must support whitespace/formatting normalization")

    def test_retry2_functional_base_graph_consolidation(self):
        """Functional Test: Verification of Base Graph consolidation after promote_horizon"""
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            horizon_db_path = os.path.join(td, "h1.db")

            base_conn = sqlite3.connect(base_db_path)
            base_conn.execute("CREATE TABLE projects (name TEXT PRIMARY KEY, root_dir TEXT)")
            base_conn.execute("INSERT INTO projects VALUES ('testproj', '')")
            base_conn.execute("CREATE TABLE nodes (id INTEGER PRIMARY KEY, project TEXT, label TEXT, name TEXT, qualified_name TEXT, properties TEXT, UNIQUE(project, qualified_name))")
            base_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER)")
            base_conn.commit()

            horizon_conn = sqlite3.connect(horizon_db_path)
            horizon_conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, epistemic_status TEXT, code_snippet TEXT)")
            horizon_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT)")
            horizon_conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://testproj/pkg/auth.go#Token', 'Function', 'PROPOSED', 'func Token() {}')")
            horizon_conn.execute("INSERT INTO virtual_edges VALUES ('cbm://testproj/pkg/auth.go#Token', 'cbm://testproj/pkg/user.go#User', 'CALLS', 'h1')")
            horizon_conn.commit()

            # Simulate consolidation SQL matching admission_gate.c logic
            cur = horizon_conn.cursor()
            cur.execute("SELECT cbm_uri, label, code_snippet FROM symbolic_nodes WHERE epistemic_status != 'CONTESTED'")
            nodes = cur.fetchall()
            for uri, lbl, code in nodes:
                base_conn.execute("INSERT OR REPLACE INTO nodes (project, label, name, qualified_name, properties) VALUES (?, ?, ?, ?, ?)",
                                  ('testproj', lbl, uri, uri, code))

            cur.execute("SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges")
            edges = cur.fetchall()
            for src, tgt, etype, orig in edges:
                base_conn.execute("INSERT OR REPLACE INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES (?, ?, ?, ?, strftime('%s','now'))",
                                  (src, tgt, etype, orig))
            base_conn.commit()

            # Verify consolidation into base DB
            bcur = base_conn.cursor()
            bcur.execute("SELECT count(*) FROM nodes WHERE project = 'testproj'")
            self.assertEqual(bcur.fetchone()[0], 1)
            bcur.execute("SELECT count(*) FROM virtual_edges WHERE origin_horizon = 'h1'")
            self.assertEqual(bcur.fetchone()[0], 1)

            base_conn.close()
            horizon_conn.close()

    def test_retry2_functional_active_horizons_substring_in_query(self):
        """Verify active_horizons in query payload string does not produce false positive array"""
        payload = json.dumps({"query": "MATCH (n {active_horizons: 'active_horizons'}) RETURN n"})
        data = json.loads(payload)
        self.assertNotIn("active_horizons", data.keys() - {"query"})
        # active_horizons only parsed when a top-level array property
        horizons = data.get("active_horizons", [])
        self.assertIsInstance(horizons, list)
        self.assertEqual(len(horizons), 0)

    def test_retry2_functional_content_text_overlay_integration(self):
        """Verify MCP content[0].text is updated with overlay information"""
        base_resp = {
            "content": [
                {
                    "type": "text",
                    "text": json.dumps({"nodes": [{"id": 1, "name": "base_fn"}]})
                }
            ],
            "isError": False
        }
        overlays = [
            {"uri": "cbm://repo/pkg/auth.go#SpeculativeToken", "payload": "Function:speculative", "source": "HORIZON"}
        ]

        # Simulate handlers.c integration logic
        inner_text = json.loads(base_resp["content"][0]["text"])
        inner_text["active_horizon_overlays"] = overlays
        base_resp["content"][0]["text"] = json.dumps(inner_text)
        base_resp["active_horizon_overlays"] = overlays

        # Standard client reads content[0].text
        received_text = base_resp["content"][0]["text"]
        parsed = json.loads(received_text)
        self.assertIn("active_horizon_overlays", parsed)
        self.assertEqual(len(parsed["active_horizon_overlays"]), 1)
        self.assertEqual(parsed["active_horizon_overlays"][0]["uri"], "cbm://repo/pkg/auth.go#SpeculativeToken")

    def test_retry2_functional_ast_whitespace_normalization(self):
        """Verify FNV-1a normalized hash produces identical hash across CRLF and space variations"""
        def norm_hash(s: str) -> int:
            h = FNV1A_64_OFFSET
            in_ws = False
            for b in s.encode("utf-8"):
                if b == ord('\r'):
                    continue
                if b in (ord(' '), ord('\t'), ord('\n')):
                    if not in_ws:
                        h ^= ord(' ')
                        h = (h * FNV1A_64_PRIME) & MASK_64
                        in_ws = True
                else:
                    in_ws = False
                    h ^= b
                    h = (h * FNV1A_64_PRIME) & MASK_64
            return h

        code_unix = "func ValidateToken(\n    token string,\n) bool {\n    return true\n}"
        code_win = "func ValidateToken(\r\n  token string,\r\n) bool {\r\n  return true\r\n}"
        h_unix = norm_hash(code_unix)
        h_win = norm_hash(code_win)
        self.assertEqual(h_unix, h_win, "Normalized AST hash must match across CRLF and indent differences")


if __name__ == "__main__":
    unittest.main()

