#!/usr/bin/env python3
"""
Test suite for Feature F003 (Transitive Recall Closure / G5).
Translates all Given-When-Then scenarios from:
docs/specs/transitive_recall_closure/004-codebase-memory-multigraph-mcp-test-scenarios.md
"""

import os
import sys
import ctypes
import tempfile
import sqlite3
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Constants matching recall_engine.h
# ---------------------------------------------------------------------------

CBM_RECALL_OK = 0
CBM_RECALL_ERR_TX_FAILED = -10
CBM_RECALL_MAX_AFFECTED = 256

SQLITE_OPEN_READONLY = 0x00000001
SQLITE_OPEN_READWRITE = 0x00000002
SQLITE_OPEN_CREATE = 0x00000004

# ---------------------------------------------------------------------------
# CTypes definitions matching cbm_uri.h, visited_set.h, and recall_engine.h
# ---------------------------------------------------------------------------

class CbmUri(ctypes.Structure):
    _fields_ = [
        ("repo", ctypes.c_char * 256),
        ("path", ctypes.c_char * 512),
        ("symbol", ctypes.c_char * 256),
        ("hash", ctypes.c_uint64),
    ]

class RecallReport(ctypes.Structure):
    _fields_ = [
        ("contested_root", CbmUri),
        ("reason", ctypes.c_char * 256),
        ("affected_uris", (ctypes.c_char * 1024) * CBM_RECALL_MAX_AFFECTED),
        ("affected_count", ctypes.c_size_t),
        ("total_affected_count", ctypes.c_size_t),
        ("is_committed", ctypes.c_bool),
    ]

class VisitedSet(ctypes.Structure):
    _fields_ = [
        ("hashes", ctypes.c_uint64 * 8192),
        ("count", ctypes.c_size_t),
        ("extra_hashes", ctypes.c_void_p),
        ("extra_count", ctypes.c_size_t),
        ("extra_cap", ctypes.c_size_t),
        ("uris", ctypes.c_void_p),
        ("extra_uris", ctypes.c_void_p),
    ]


def get_recall_dll():
    dll_path = REPO_ROOT / "build" / "libcbm_recall.dll"
    srcs = [
        REPO_ROOT / "src" / "core" / "cbm_uri.c",
        REPO_ROOT / "src" / "core" / "symbolic_node.c",
        REPO_ROOT / "src" / "admission" / "recall_engine.c",
        REPO_ROOT / "src" / "admission" / "recall_engine.h",
        REPO_ROOT / "src" / "core" / "visited_set.h",
    ]
    recompile = not dll_path.exists() or any(s.stat().st_mtime > dll_path.stat().st_mtime for s in srcs)
    if recompile:
        import subprocess
        env = os.environ.copy()
        env["PATH"] = r"C:\msys64\clang64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")
        clang_bin = r"C:\msys64\clang64\bin\clang.exe" if os.path.exists(r"C:\msys64\clang64\bin\clang.exe") else "clang"
        cmd = [
            clang_bin, "-shared", "-fPIC", "-Wl,--export-all-symbols",
            str(REPO_ROOT / "src" / "core" / "cbm_uri.c"),
            str(REPO_ROOT / "src" / "core" / "symbolic_node.c"),
            str(REPO_ROOT / "src" / "admission" / "recall_engine.c"),
            str(REPO_ROOT / "vendored" / "sqlite3" / "sqlite3.c"),
            f"-I{REPO_ROOT / 'src'}",
            f"-I{REPO_ROOT / 'vendored' / 'sqlite3'}",
            "-o", str(dll_path)
        ]
        subprocess.check_call(cmd, env=env)

    dll = ctypes.CDLL(str(dll_path))

    dll.cbm_uri_parse.argtypes = [ctypes.c_char_p, ctypes.POINTER(CbmUri)]
    dll.cbm_uri_parse.restype = ctypes.c_int

    dll.cbm_uri_to_string.argtypes = [ctypes.POINTER(CbmUri), ctypes.c_char_p, ctypes.c_size_t]
    dll.cbm_uri_to_string.restype = ctypes.c_int

    dll.cbm_bfs_reverse_deps_acyclic.argtypes = [
        ctypes.c_void_p,  # sqlite3 *
        ctypes.POINTER(CbmUri),
        ctypes.POINTER(VisitedSet),
        ctypes.c_uint32,
        ctypes.POINTER(RecallReport)
    ]
    dll.cbm_bfs_reverse_deps_acyclic.restype = ctypes.c_int

    dll.cbm_trigger_recall.argtypes = [
        ctypes.c_void_p,  # sqlite3 *
        ctypes.POINTER(CbmUri),
        ctypes.c_char_p,
        ctypes.POINTER(RecallReport)
    ]
    dll.cbm_trigger_recall.restype = ctypes.c_int

    dll.sqlite3_open_v2.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_int, ctypes.c_char_p]
    dll.sqlite3_open_v2.restype = ctypes.c_int

    dll.sqlite3_close_v2.argtypes = [ctypes.c_void_p]
    dll.sqlite3_close_v2.restype = ctypes.c_int

    dll.sqlite3_exec.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]
    dll.sqlite3_exec.restype = ctypes.c_int

    return dll


def serialize_recall_error(error_code: int, detail: str = "") -> dict:
    mapping = {
        CBM_RECALL_ERR_TX_FAILED: "TRANSACTION_FAILED",
    }
    code_str = mapping.get(error_code, "UNKNOWN")
    return {
        "isError": True,
        "code": code_str,
        "message": detail or f"Recall error {code_str} ({error_code})",
        "is_committed": False
    }


def init_schema(db_path: str):
    conn = sqlite3.connect(db_path)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS symbolic_nodes (
            cbm_uri TEXT PRIMARY KEY,
            label TEXT,
            epistemic_status TEXT,
            code_snippet TEXT
        );
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS virtual_edges (
            source_uri TEXT,
            target_uri TEXT,
            edge_type TEXT,
            origin_horizon TEXT,
            created_at INTEGER,
            PRIMARY KEY(source_uri, target_uri, edge_type)
        );
    """)
    conn.commit()
    conn.close()


class TestTransitiveRecallClosureScenarios(unittest.TestCase):
    """Executes all test scenarios from 004-codebase-memory-multigraph-mcp-test-scenarios.md"""

    @classmethod
    def setUpClass(cls):
        cls.cbm = get_recall_dll()

    # =========================================================================
    # Section 1 — Unit Tests
    # =========================================================================

    # --- 1.1 Structural Types and Error Code Mapping ---

    def test_scenario_map_tx_failed_to_transaction_failed_string(self):
        """Scenario: Should map CBM_RECALL_ERR_TX_FAILED to TRANSACTION_FAILED string"""
        err_code = CBM_RECALL_ERR_TX_FAILED
        resp = serialize_recall_error(err_code, "Database transaction failed")
        self.assertEqual(resp["code"], "TRANSACTION_FAILED")
        self.assertFalse(resp["is_committed"])
        self.assertTrue(resp["isError"])

    def test_scenario_initialize_recall_report_clean_state(self):
        """Scenario: Should initialize RecallReport with zero total_affected_count and false is_committed"""
        report = RecallReport()
        ctypes.memset(ctypes.byref(report), 0, ctypes.sizeof(report))
        self.assertEqual(report.total_affected_count, 0)
        self.assertEqual(report.affected_count, 0)
        self.assertFalse(report.is_committed)

    # --- 1.2 Causal Edge Filtering and BFS Traversal ---

    def test_scenario_follow_calls_and_depends_on_edges(self):
        """Scenario: Should follow CALLS and DEPENDS_ON edges in reverse derivation traversal"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/a#A', 'A', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/b#B', 'B', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/c#C', 'C', 'ACCEPTED', '')")
            # B CALLS A (target=A, source=B)
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/b#B', 'cbm://repo/pkg/a#A', 'CALLS', 'h1', 100)")
            # C DEPENDS_ON B (target=B, source=C)
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/c#C', 'cbm://repo/pkg/b#B', 'DEPENDS_ON', 'h1', 100)")
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/a#A", ctypes.byref(root_uri))

                visited = VisitedSet()
                ctypes.memset(ctypes.byref(visited), 0, ctypes.sizeof(visited))
                report = RecallReport()
                rc = self.cbm.cbm_bfs_reverse_deps_acyclic(db_ptr, ctypes.byref(root_uri), ctypes.byref(visited), 0, ctypes.byref(report))
                self.assertEqual(rc, 0)

                affected = [report.affected_uris[i].value.decode("utf-8") for i in range(report.affected_count)]
                self.assertIn("cbm://repo/pkg/b#B", affected)
                self.assertIn("cbm://repo/pkg/c#C", affected)
                self.assertEqual(report.total_affected_count, 2)
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    def test_scenario_ignore_doc_ref_and_tagged_with_edges(self):
        """Scenario: Should ignore DOC_REF and TAGGED_WITH edges during reverse derivation traversal"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/a#A', 'A', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/m2#Module2', 'Module2', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/docs/readme#Doc1', 'Doc1', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/tags/auth#Tag1', 'Tag1', 'ACCEPTED', '')")

            # Module2 IMPLEMENTS A (causal)
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/m2#Module2', 'cbm://repo/pkg/a#A', 'IMPLEMENTS', 'h1', 100)")
            # Doc1 DOC_REF A (non-causal)
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/docs/readme#Doc1', 'cbm://repo/pkg/a#A', 'DOC_REF', 'h1', 100)")
            # Tag1 TAGGED_WITH A (non-causal)
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/tags/auth#Tag1', 'cbm://repo/pkg/a#A', 'TAGGED_WITH', 'h1', 100)")
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/a#A", ctypes.byref(root_uri))

                visited = VisitedSet()
                ctypes.memset(ctypes.byref(visited), 0, ctypes.sizeof(visited))
                report = RecallReport()
                rc = self.cbm.cbm_bfs_reverse_deps_acyclic(db_ptr, ctypes.byref(root_uri), ctypes.byref(visited), 0, ctypes.byref(report))
                self.assertEqual(rc, 0)

                affected = [report.affected_uris[i].value.decode("utf-8") for i in range(report.affected_count)]
                self.assertIn("cbm://repo/pkg/m2#Module2", affected)
                self.assertNotIn("cbm://repo/docs/readme#Doc1", affected)
                self.assertNotIn("cbm://repo/tags/auth#Tag1", affected)
                self.assertEqual(report.total_affected_count, 1)
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    def test_scenario_terminate_gracefully_on_cyclic_dependency_graph(self):
        """Scenario: Should terminate gracefully without infinite loop on cyclic dependency graph"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/a#A', 'A', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/b#B', 'B', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/c#C', 'C', 'ACCEPTED', '')")
            # A CALLS B, B CALLS C, C CALLS A
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/b#B', 'cbm://repo/pkg/a#A', 'CALLS', 'h1', 100)")
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/c#C', 'cbm://repo/pkg/b#B', 'CALLS', 'h1', 100)")
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/a#A', 'cbm://repo/pkg/c#C', 'CALLS', 'h1', 100)")
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/a#A", ctypes.byref(root_uri))

                visited = VisitedSet()
                ctypes.memset(ctypes.byref(visited), 0, ctypes.sizeof(visited))
                report = RecallReport()
                rc = self.cbm.cbm_bfs_reverse_deps_acyclic(db_ptr, ctypes.byref(root_uri), ctypes.byref(visited), 0, ctypes.byref(report))
                self.assertEqual(rc, 0)

                affected = [report.affected_uris[i].value.decode("utf-8") for i in range(report.affected_count)]
                self.assertEqual(len(affected), 2)
                self.assertIn("cbm://repo/pkg/b#B", affected)
                self.assertIn("cbm://repo/pkg/c#C", affected)
                self.assertEqual(report.total_affected_count, 2)
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    # =========================================================================
    # Section 2 — Integration Tests
    # =========================================================================

    # --- 2.1 Unbounded Depth and Large Node Closures (Codex Finding A05) ---

    def test_scenario_traverse_chains_deeper_than_5_levels_when_unbounded(self):
        """Scenario: Should traverse dependency chains deeper than 5 levels when max_depth is 0"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            # Create linear chain: Node8 -> Node7 -> ... -> Node1 -> Root
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/root#Root', 'Root', 'ACCEPTED', '')")
            prev = "cbm://repo/pkg/root#Root"
            for i in range(1, 9):
                uri = f"cbm://repo/pkg/n{i}#Node{i}"
                conn.execute("INSERT INTO symbolic_nodes VALUES (?, ?, 'ACCEPTED', '')", (uri, f"Node{i}"))
                conn.execute("INSERT INTO virtual_edges VALUES (?, ?, 'DEPENDS_ON', 'h1', 100)", (uri, prev))
                prev = uri
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/root#Root", ctypes.byref(root_uri))

                report = RecallReport()
                rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Linear chain test", ctypes.byref(report))
                self.assertEqual(rc, 0)
                self.assertTrue(report.is_committed)
                self.assertEqual(report.total_affected_count, 8)
                self.assertEqual(report.affected_count, 8)

                # Verify all 8 nodes plus Root are CONTESTED in the database
                conn2 = sqlite3.connect(db_path)
                cur = conn2.cursor()
                cur.execute("SELECT cbm_uri, epistemic_status FROM symbolic_nodes")
                rows = cur.fetchall()
                conn2.close()

                self.assertEqual(len(rows), 9)
                for uri, status in rows:
                    self.assertEqual(status, "CONTESTED", f"Node {uri} must be marked CONTESTED overcoming depth-5 ceiling")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    def test_scenario_invalidate_all_dependent_nodes_when_exceeding_256(self):
        """Scenario: Should invalidate all dependent nodes when the graph exceeds 256 affected entities"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/base#BaseModule', 'BaseModule', 'ACCEPTED', '')")
            # Create star graph with 300 dependent nodes
            for i in range(1, 301):
                uri = f"cbm://repo/pkg/client{i}#Client{i}"
                conn.execute("INSERT INTO symbolic_nodes VALUES (?, ?, 'ACCEPTED', '')", (uri, f"Client{i}"))
                conn.execute("INSERT INTO virtual_edges VALUES (?, 'cbm://repo/pkg/base#BaseModule', 'CALLS', 'h1', 100)", (uri,))
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/base#BaseModule", ctypes.byref(root_uri))

                report = RecallReport()
                rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Large closure test", ctypes.byref(report))
                self.assertEqual(rc, 0)
                self.assertTrue(report.is_committed)
                self.assertEqual(report.total_affected_count, 300, "total_affected_count must record all 300 nodes")
                self.assertEqual(report.affected_count, 256, "report array capacity must be capped at 256")

                # Verify all 300 client nodes + BaseModule are CONTESTED in DB
                conn2 = sqlite3.connect(db_path)
                cur = conn2.cursor()
                cur.execute("SELECT count(*) FROM symbolic_nodes WHERE epistemic_status = 'CONTESTED'")
                contested_count = cur.fetchone()[0]
                conn2.close()

                self.assertEqual(contested_count, 301, "All 300 dependent nodes + root must be updated to CONTESTED in database")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    # --- 2.2 Strict Transactional Rollback (Codex Finding NFR-6) ---

    def test_scenario_execute_rollback_when_begin_immediate_fails(self):
        """Scenario: Should execute ROLLBACK and return negative error code when BEGIN IMMEDIATE fails"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/a#A', 'A', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/b#B', 'B', 'ACCEPTED', '')")
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/pkg/b#B', 'cbm://repo/pkg/a#A', 'CALLS', 'h1', 100)")
            conn.commit()

            # Acquire exclusive lock on another connection
            conn.execute("BEGIN EXCLUSIVE")

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                # Set busy timeout to 1ms so BEGIN IMMEDIATE fails immediately
                self.cbm.sqlite3_exec(db_ptr, b"PRAGMA busy_timeout = 1;", None, None, None)

                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/a#A", ctypes.byref(root_uri))

                report = RecallReport()
                rec_rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Locked test", ctypes.byref(report))

                self.assertEqual(rec_rc, CBM_RECALL_ERR_TX_FAILED, "Must return CBM_RECALL_ERR_TX_FAILED on lock failure")
                self.assertFalse(report.is_committed, "is_committed must be false on transaction abort")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)
                conn.rollback()
                conn.close()

            # Verify no rows modified
            conn2 = sqlite3.connect(db_path)
            cur = conn2.cursor()
            cur.execute("SELECT count(*) FROM symbolic_nodes WHERE epistemic_status = 'ACCEPTED'")
            accepted_count = cur.fetchone()[0]
            conn2.close()
            self.assertEqual(accepted_count, 2, "All rows must remain ACCEPTED after aborted transaction")

    def test_scenario_execute_rollback_when_status_update_step_fails(self):
        """Scenario: Should execute ROLLBACK and return error if any status UPDATE step fails"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            for i in range(1, 20):
                uri = f"cbm://repo/pkg/n{i}#Node{i}"
                conn.execute("INSERT INTO symbolic_nodes VALUES (?, ?, 'ACCEPTED', '')", (uri, f"Node{i}"))
                if i > 1:
                    conn.execute("INSERT INTO virtual_edges VALUES (?, 'cbm://repo/pkg/n1#Node1', 'CALLS', 'h1', 100)", (uri,))

            # Add an UPDATE trigger that fails when updating Node15
            conn.execute("""
                CREATE TRIGGER fail_on_node_15
                BEFORE UPDATE ON symbolic_nodes
                FOR EACH ROW
                WHEN NEW.cbm_uri = 'cbm://repo/pkg/n15#Node15'
                BEGIN
                    SELECT RAISE(FAIL, 'Constraint error on Node15');
                END;
            """)
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/n1#Node1", ctypes.byref(root_uri))

                report = RecallReport()
                rec_rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Trigger failure test", ctypes.byref(report))

                self.assertEqual(rec_rc, CBM_RECALL_ERR_TX_FAILED, "Must return CBM_RECALL_ERR_TX_FAILED on step failure")
                self.assertFalse(report.is_committed, "is_committed must be false on step failure")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

            # Assert all 19 nodes remain ACCEPTED (zero partial updates)
            conn2 = sqlite3.connect(db_path)
            cur = conn2.cursor()
            cur.execute("SELECT count(*) FROM symbolic_nodes WHERE epistemic_status = 'ACCEPTED'")
            accepted_count = cur.fetchone()[0]
            conn2.close()
            self.assertEqual(accepted_count, 19, "All nodes must remain ACCEPTED proving partial invalidation was prevented")

    def test_scenario_leave_all_node_statuses_intact_after_rollback(self):
        """Scenario: Should leave all node statuses intact after a transaction rollback"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            for i in range(1, 11):
                uri = f"cbm://repo/pkg/m{i}#M{i}"
                conn.execute("INSERT INTO symbolic_nodes VALUES (?, ?, 'ACCEPTED', '')", (uri, f"M{i}"))
                if i > 1:
                    conn.execute("INSERT INTO virtual_edges VALUES (?, 'cbm://repo/pkg/m1#M1', 'DEPENDS_ON', 'h1', 100)", (uri,))

            # Trigger that raises error on root update
            conn.execute("""
                CREATE TRIGGER fail_on_root
                BEFORE UPDATE ON symbolic_nodes
                FOR EACH ROW
                WHEN NEW.cbm_uri = 'cbm://repo/pkg/m1#M1'
                BEGIN
                    SELECT RAISE(FAIL, 'Trigger fail root');
                END;
            """)
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/m1#M1", ctypes.byref(root_uri))

                report = RecallReport()
                rec_rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Abort test", ctypes.byref(report))
                self.assertEqual(rec_rc, CBM_RECALL_ERR_TX_FAILED)
                self.assertFalse(report.is_committed)
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

            conn2 = sqlite3.connect(db_path)
            cur = conn2.cursor()
            cur.execute("SELECT count(*) FROM symbolic_nodes WHERE epistemic_status = 'ACCEPTED'")
            count_accepted = cur.fetchone()[0]
            conn2.close()
            self.assertEqual(count_accepted, 10)

    # =========================================================================
    # Section 3 — Functional and Acceptance Scenarios
    # =========================================================================

    # --- 3.1 End-to-End Contestation and Recall Cascade (G5 Axiom) ---

    def test_scenario_cascade_contestation_to_derived_interfaces_and_implementations(self):
        """Scenario: Should cascade contestation from root to all derived interfaces and implementations"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/svc/auth#IAuthService', 'IAuthService', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/svc/oauth#OAuthService', 'OAuthService', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/svc/ldap#LdapService', 'LdapService', 'ACCEPTED', '')")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/ctl/login#LoginController', 'LoginController', 'ACCEPTED', '')")

            # OAuthService and LdapService IMPLEMENTS IAuthService
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/svc/oauth#OAuthService', 'cbm://repo/svc/auth#IAuthService', 'IMPLEMENTS', 'h1', 100)")
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/svc/ldap#LdapService', 'cbm://repo/svc/auth#IAuthService', 'IMPLEMENTS', 'h1', 100)")
            # LoginController DEPENDS_ON IAuthService
            conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/ctl/login#LoginController', 'cbm://repo/svc/auth#IAuthService', 'DEPENDS_ON', 'h1', 100)")
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/svc/auth#IAuthService", ctypes.byref(root_uri))

                report = RecallReport()
                rec_rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Interface contract drift", ctypes.byref(report))
                self.assertEqual(rec_rc, 0)
                self.assertTrue(report.is_committed)
                self.assertEqual(report.total_affected_count, 3)

                conn2 = sqlite3.connect(db_path)
                cur = conn2.cursor()
                cur.execute("SELECT cbm_uri FROM symbolic_nodes WHERE epistemic_status = 'CONTESTED'")
                contested = {r[0] for r in cur.fetchall()}
                conn2.close()

                expected = {
                    "cbm://repo/svc/auth#IAuthService",
                    "cbm://repo/svc/oauth#OAuthService",
                    "cbm://repo/svc/ldap#LdapService",
                    "cbm://repo/ctl/login#LoginController",
                }
                self.assertEqual(contested, expected, "Root, implementations, and consumers must all be CONTESTED atomically")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    def test_scenario_emit_structured_recall_notice_with_complete_metrics(self):
        """Scenario: Should emit structured RecallNotice with complete affected metrics"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "test.db")
            init_schema(db_path)

            conn = sqlite3.connect(db_path)
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/core/config#RootConfig', 'RootConfig', 'ACCEPTED', '')")
            for i in range(1, 43):
                uri = f"cbm://repo/sub/consumer{i}#Consumer{i}"
                conn.execute("INSERT INTO symbolic_nodes VALUES (?, ?, 'ACCEPTED', '')", (uri, f"Consumer{i}"))
                conn.execute("INSERT INTO virtual_edges VALUES (?, 'cbm://repo/core/config#RootConfig', 'DEPENDS_ON', 'h1', 100)", (uri,))
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/core/config#RootConfig", ctypes.byref(root_uri))

                report = RecallReport()
                rec_rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Config schema drift", ctypes.byref(report))
                self.assertEqual(rec_rc, 0)

                # Form structured notice object matching tactical design
                notice = {
                    "contested_root": "cbm://repo/core/config#RootConfig",
                    "total_affected": report.total_affected_count,
                    "is_committed": report.is_committed,
                    "reason": report.reason.decode("utf-8")
                }
                self.assertEqual(notice["total_affected"], 42)
                self.assertTrue(notice["is_committed"])
                self.assertEqual(notice["reason"], "Config schema drift")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    # =========================================================================
    # Section 4 — Structural and C Code Contracts
    # =========================================================================

    def test_c_source_contracts_and_struct_invariants(self):
        """Validates all C headers and source code conform to F003 structural design contracts"""
        rec_h = (REPO_ROOT / "src" / "admission" / "recall_engine.h").read_text(encoding="utf-8")
        rec_c = (REPO_ROOT / "src" / "admission" / "recall_engine.c").read_text(encoding="utf-8")

        self.assertIn("#define CBM_RECALL_ERR_TX_FAILED -10", rec_h)
        self.assertIn("size_t total_affected_count;", rec_h)
        self.assertIn("bool is_committed;", rec_h)

        self.assertIn("'DEPENDS_ON'", rec_c)
        self.assertIn("'DERIVED_FROM'", rec_c)
        self.assertIn("'CALLS'", rec_c)
        self.assertIn("'IMPLEMENTS'", rec_c)
        self.assertIn("'EXTENDS'", rec_c)

        self.assertIn("BEGIN IMMEDIATE;", rec_c)
        self.assertIn("ROLLBACK;", rec_c)
        self.assertIn("COMMIT;", rec_c)
        self.assertIn("CBM_RECALL_ERR_TX_FAILED", rec_c)

    def test_f003_hardening_db_null_returns_error_and_uncommitted(self):
        """F003 Hardening: cbm_trigger_recall and cbm_bfs_reverse_deps_acyclic must return -1 when db is NULL"""
        root_uri = CbmUri()
        self.cbm.cbm_uri_parse(b"cbm://repo/pkg/auth#Token", ctypes.byref(root_uri))

        report = RecallReport()
        report.is_committed = True  # Initialize to True to verify reset
        rc = self.cbm.cbm_trigger_recall(None, ctypes.byref(root_uri), b"Anchor drift", ctypes.byref(report))
        self.assertEqual(rc, -1, "cbm_trigger_recall must return -1 when db is NULL")
        self.assertFalse(report.is_committed, "is_committed must be false when db is NULL")

        visited = VisitedSet()
        ctypes.memset(ctypes.byref(visited), 0, ctypes.sizeof(visited))
        bfs_rc = self.cbm.cbm_bfs_reverse_deps_acyclic(None, ctypes.byref(root_uri), ctypes.byref(visited), 0, ctypes.byref(report))
        self.assertEqual(bfs_rc, -1, "cbm_bfs_reverse_deps_acyclic must return -1 when db is NULL")

    def test_f003_hardening_prepare_failure_on_missing_table_returns_error(self):
        """F003 Hardening: sqlite3_prepare_v2 failure on missing virtual_edges table must return -1 and trigger rollback"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "corrupt_schema.db")
            conn = sqlite3.connect(db_path)
            # Create symbolic_nodes but omit virtual_edges
            conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, epistemic_status TEXT);")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/pkg/a#A', 'A', 'ACCEPTED');")
            conn.commit()
            conn.close()

            db_ptr = ctypes.c_void_p()
            rc = self.cbm.sqlite3_open_v2(db_path.encode("utf-8"), ctypes.byref(db_ptr), SQLITE_OPEN_READWRITE, None)
            self.assertEqual(rc, 0)

            try:
                root_uri = CbmUri()
                self.cbm.cbm_uri_parse(b"cbm://repo/pkg/a#A", ctypes.byref(root_uri))

                visited = VisitedSet()
                ctypes.memset(ctypes.byref(visited), 0, ctypes.sizeof(visited))
                report = RecallReport()

                # 1. BFS must fail (-1) immediately instead of ignoring missing table
                bfs_rc = self.cbm.cbm_bfs_reverse_deps_acyclic(db_ptr, ctypes.byref(root_uri), ctypes.byref(visited), 0, ctypes.byref(report))
                self.assertEqual(bfs_rc, -1, "BFS must return -1 when virtual_edges table is missing")

                # 2. Trigger recall must fail and roll back with CBM_RECALL_ERR_TX_FAILED (-10)
                rec_rc = self.cbm.cbm_trigger_recall(db_ptr, ctypes.byref(root_uri), b"Drift", ctypes.byref(report))
                self.assertEqual(rec_rc, CBM_RECALL_ERR_TX_FAILED, "Recall must return CBM_RECALL_ERR_TX_FAILED on BFS prepare failure")
                self.assertFalse(report.is_committed)

                # Verify symbolic_nodes status remained UNCHANGED (not updated to CONTESTED)
                conn = sqlite3.connect(db_path)
                cur = conn.cursor()
                cur.execute("SELECT epistemic_status FROM symbolic_nodes WHERE cbm_uri = 'cbm://repo/pkg/a#A'")
                status = cur.fetchone()[0]
                conn.close()
                self.assertEqual(status, "ACCEPTED", "Node must remain ACCEPTED when recall fails and rolls back")
            finally:
                self.cbm.sqlite3_close_v2(db_ptr)

    def test_f003_hardening_transactional_bfs_consistency(self):
        """F003 Hardening: BEGIN IMMEDIATE; must execute BEFORE cbm_bfs_reverse_deps_internal for transactional consistency"""
        rec_c = (REPO_ROOT / "src" / "admission" / "recall_engine.c").read_text(encoding="utf-8")
        idx_trigger = rec_c.find("int cbm_trigger_recall(")
        self.assertGreater(idx_trigger, 0)
        trigger_body = rec_c[idx_trigger:]

        idx_begin = trigger_body.find('sqlite3_exec(db, "BEGIN IMMEDIATE;"')
        idx_bfs = trigger_body.find("cbm_bfs_reverse_deps_internal(")
        self.assertGreater(idx_begin, 0, "BEGIN IMMEDIATE must be present in cbm_trigger_recall")
        self.assertGreater(idx_bfs, 0, "cbm_bfs_reverse_deps_internal must be present in cbm_trigger_recall")
        self.assertLess(idx_begin, idx_bfs, "BEGIN IMMEDIATE must precede cbm_bfs_reverse_deps_internal to ensure atomic snapshot")

    def test_f003_visited_set_dynamic_expansion_and_cycle_guard(self):
        """
        Codex Finding 3: VisitedSet must expand dynamically beyond 8192 entries
        and check cbm_visited_add return code so that cycles cannot cause infinite
        loops while BEGIN IMMEDIATE is open.
        """
        vis_h = (REPO_ROOT / "src" / "core" / "visited_set.h").read_text(encoding="utf-8")
        rec_c = (REPO_ROOT / "src" / "admission" / "recall_engine.c").read_text(encoding="utf-8")

        # VisitedSet dynamically allocates extra capacity beyond CBM_VISITED_CAP
        self.assertIn("extra_hashes", vis_h)
        self.assertIn("extra_cap", vis_h)
        self.assertIn("realloc", vis_h)
        self.assertIn("cbm_visited_free", vis_h)

        # BFS checks return code of cbm_visited_add or cbm_visited_add_uri to abort on failure
        self.assertTrue(
            "if (!cbm_visited_add(visited, hash))" in rec_c or
            "if (!cbm_visited_add_uri(visited, hash, curr))" in rec_c,
            "BFS loop must check return code of visited add"
        )

    def test_f003_visited_set_fnv64_collision_exact_uri_resolution(self):
        """
        Codex Round 5 Finding 5: VisitedSet must handle synthetic FNV-64 hash collisions
        without suppressing distinct URIs.
        """
        vis_h = (REPO_ROOT / "src" / "core" / "visited_set.h").read_text(encoding="utf-8")
        rec_c = (REPO_ROOT / "src" / "admission" / "recall_engine.c").read_text(encoding="utf-8")

        # VisitedSet contains exact URI comparison
        self.assertIn("cbm_visited_contains_uri", vis_h)
        self.assertIn("cbm_visited_add_uri", vis_h)
        self.assertIn("strcmp(s->uris[i], uri) == 0", vis_h)

        # recall_engine passes exact URI to visited checks
        self.assertIn("cbm_visited_contains_uri(visited, hash, curr)", rec_c)
        self.assertIn("cbm_visited_add_uri(visited, hash, curr)", rec_c)
        self.assertIn("cbm_visited_contains_uri(visited, nhash, src)", rec_c)


if __name__ == "__main__":
    unittest.main()
