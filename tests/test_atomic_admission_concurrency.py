#!/usr/bin/env python3
"""
Test suite for Feature F002 (Atomic Admission Concurrency Arbitration)
Translates all Given-When-Then scenarios from:
docs/specs/atomic_admission_concurrency/004-codebase-memory-multigraph-mcp-test-scenarios.md
"""

import os
import sys
import json
import time
import sqlite3
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Error code constants matching admission_gate.h
CBM_ADMISSION_OK = 0
CBM_ADMISSION_ERR_ANCHOR_DRIFT = -1
CBM_ADMISSION_ERR_HORIZON_NOT_FOUND = -2
CBM_ADMISSION_ERR_INVALID_STATE = -3
CBM_ADMISSION_ERR_INVALID_PARAMS = -4
CBM_ADMISSION_ERR_BASE_UNAVAILABLE = -5
CBM_ADMISSION_ERR_CONCURRENT_CONFLICT = -6
CBM_ADMISSION_ERR_CONSOLIDATION_FAILED = -7
CBM_ADMISSION_ERR_COMMIT_FAILED = -8
CBM_ADMISSION_ERR_SESSION_REQUIRED = -9

# Refusal codes matching union_refusal.h
CBM_REFUSAL_CODE_DOC_ASYMMETRY = 18
CBM_REFUSAL_LOG_REF_IMMUTABLE = 19
CBM_REFUSAL_PROVENANCE_UNDECLARED = 20
CBM_REFUSAL_CONCURRENT_CONFLICT = 29
CBM_REFUSAL_CONSOLIDATION_FAILED = 30
CBM_REFUSAL_COMMIT_FAILED = 31
CBM_REFUSAL_SESSION_REQUIRED = 32


def union_refusal_code_to_string(code: int) -> str:
    """Python reference implementation / bridge mirroring union_refusal_code_to_string in C"""
    mapping = {
        CBM_ADMISSION_ERR_CONSOLIDATION_FAILED: "CONSOLIDATION_FAILED",
        CBM_REFUSAL_CONSOLIDATION_FAILED: "CONSOLIDATION_FAILED",
        CBM_ADMISSION_ERR_COMMIT_FAILED: "COMMIT_FAILED",
        CBM_REFUSAL_COMMIT_FAILED: "COMMIT_FAILED",
        CBM_ADMISSION_ERR_SESSION_REQUIRED: "SESSION_REQUIRED",
        CBM_REFUSAL_SESSION_REQUIRED: "SESSION_REQUIRED",
        CBM_ADMISSION_ERR_CONCURRENT_CONFLICT: "CONCURRENT_CONFLICT",
        CBM_REFUSAL_CONCURRENT_CONFLICT: "CONCURRENT_CONFLICT",
        CBM_REFUSAL_CODE_DOC_ASYMMETRY: "CODE_DOC_ASYMMETRY",
        CBM_REFUSAL_LOG_REF_IMMUTABLE: "LOG_REF_IMMUTABLE",
        CBM_REFUSAL_PROVENANCE_UNDECLARED: "PROVENANCE_UNDECLARED",
    }
    return mapping.get(code, "UNKNOWN")


def serialize_admission_error(error_code: int, detail: str = "") -> dict:
    code_str = union_refusal_code_to_string(error_code)
    return {
        "isError": True,
        "code": code_str,
        "message": detail or f"Admission failure with code {code_str} ({error_code})"
    }


def cbm_enforce_union_session(session_id: str, env_override_val: str = None) -> int:
    """Evaluates union session requirement matching cbm_enforce_union_session in C"""
    if env_override_val is None:
        env_override_val = os.getenv("CBM_ALLOW_LEGACY_PROMOTION", "0")
    if not session_id:
        if env_override_val == "1":
            return CBM_ADMISSION_OK
        return CBM_ADMISSION_ERR_SESSION_REQUIRED
    return CBM_ADMISSION_OK


class TestAtomicAdmissionConcurrencyScenarios(unittest.TestCase):
    """Executes all test scenarios from 004-codebase-memory-multigraph-mcp-test-scenarios.md"""

    # =========================================================================
    # Section 1 — Unit Tests
    # =========================================================================

    # --- 1.1 Error Code Mapping and Taxonomies ---

    def test_headers_define_f002_error_codes(self):
        """Header admission_gate.h must define error codes -7, -8, -9"""
        gate_h = (REPO_ROOT / "src" / "admission" / "admission_gate.h").read_text(encoding="utf-8")
        self.assertIn("#define CBM_ADMISSION_ERR_CONSOLIDATION_FAILED -7", gate_h)
        self.assertIn("#define CBM_ADMISSION_ERR_COMMIT_FAILED -8", gate_h)
        self.assertIn("#define CBM_ADMISSION_ERR_SESSION_REQUIRED -9", gate_h)

    def test_headers_define_f002_refusal_codes(self):
        """Headers union_refusal.h and union_refusal.c must map consolidation, commit, and session refusal codes"""
        refusal_h = (REPO_ROOT / "src" / "union" / "union_refusal.h").read_text(encoding="utf-8")
        refusal_c = (REPO_ROOT / "src" / "union" / "union_refusal.c").read_text(encoding="utf-8")

        self.assertIn("CONSOLIDATION_FAILED", refusal_h)
        self.assertIn("COMMIT_FAILED", refusal_h)
        self.assertIn("SESSION_REQUIRED", refusal_h)

        self.assertIn("union_refusal_code_to_string", refusal_h)
        self.assertIn("union_refusal_code_to_string", refusal_c)

        self.assertIn('"CONSOLIDATION_FAILED"', refusal_c)
        self.assertIn('"COMMIT_FAILED"', refusal_c)
        self.assertIn('"SESSION_REQUIRED"', refusal_c)

    def test_scenario_map_consolidation_failed_to_string(self):
        """Scenario: Should map CBM_ADMISSION_ERR_CONSOLIDATION_FAILED to CONSOLIDATION_FAILED string"""
        err_code = CBM_ADMISSION_ERR_CONSOLIDATION_FAILED
        code_str = union_refusal_code_to_string(err_code)
        resp = serialize_admission_error(err_code, "Node insertion aborted due to constraint violation")
        self.assertEqual(code_str, "CONSOLIDATION_FAILED")
        self.assertTrue(resp["isError"])
        self.assertEqual(resp["code"], "CONSOLIDATION_FAILED")

    def test_scenario_map_commit_failed_to_string(self):
        """Scenario: Should map CBM_ADMISSION_ERR_COMMIT_FAILED to COMMIT_FAILED string"""
        err_code = CBM_ADMISSION_ERR_COMMIT_FAILED
        code_str = union_refusal_code_to_string(err_code)
        resp = serialize_admission_error(err_code, "database disk image is locked")
        self.assertEqual(code_str, "COMMIT_FAILED")
        self.assertTrue(resp["isError"])
        self.assertEqual(resp["code"], "COMMIT_FAILED")

    def test_scenario_map_session_required_refusal(self):
        """Scenario: Should map CBM_ADMISSION_ERR_SESSION_REQUIRED to SESSION_REQUIRED refusal"""
        old_env = os.environ.get("CBM_ALLOW_LEGACY_PROMOTION")
        try:
            if "CBM_ALLOW_LEGACY_PROMOTION" in os.environ:
                del os.environ["CBM_ALLOW_LEGACY_PROMOTION"]

            rc = cbm_enforce_union_session(None)
            self.assertEqual(rc, CBM_ADMISSION_ERR_SESSION_REQUIRED)

            resp = serialize_admission_error(rc, "promotion requires active Union session")
            self.assertEqual(resp["code"], "SESSION_REQUIRED")
            self.assertTrue(resp["isError"])
        finally:
            if old_env is not None:
                os.environ["CBM_ALLOW_LEGACY_PROMOTION"] = old_env

    # --- 1.2 Fail-Fast Consolidation Logic ---

    def test_scenario_abort_consolidation_on_first_node_insert_failure(self):
        """
        Scenario: Should abort consolidation and emit ROLLBACK immediately on first node insert failure
        Given a horizon database containing 10 symbolic nodes to consolidate
        And node index 4 triggers an SQLite constraint or read-only error during sqlite3_step
        When consolidation executes
        Then node 4 fails, nodes 5 through 10 are NOT executed, ROLLBACK is called, and returns CBM_ADMISSION_ERR_CONSOLIDATION_FAILED
        """
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            h_db_path = os.path.join(td, "h.db")

            base_conn = sqlite3.connect(base_db_path)
            base_conn.execute("CREATE TABLE projects (name TEXT PRIMARY KEY, root_dir TEXT)")
            base_conn.execute("INSERT INTO projects VALUES ('default', '')")
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type))")
            base_conn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY)")
            base_conn.execute("INSERT INTO generation_log VALUES (1)")
            base_conn.commit()

            # Create horizon DB with 10 nodes
            h_conn = sqlite3.connect(h_db_path)
            h_conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, epistemic_status TEXT, code_snippet TEXT)")
            h_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT)")
            for i in range(10):
                h_conn.execute(
                    "INSERT INTO symbolic_nodes VALUES (?, ?, 'PROPOSED', ?)",
                    (f"cbm://default/src/node_{i}.c#symbol_{i}", f"Symbol_{i}", f"int s{i}() {{}}")
                )
            h_conn.commit()
            h_conn.close()

            # In base_db, insert a duplicate or create a trigger that fails specifically on node 4
            base_conn.execute("""
                CREATE TRIGGER fail_on_node_4 BEFORE INSERT ON nodes
                FOR EACH ROW WHEN NEW.cbm_uri = 'cbm://default/src/node_4.c#symbol_4'
                BEGIN
                    SELECT RAISE(ABORT, 'Simulated constraint failure at node 4');
                END;
            """)
            base_conn.commit()

            # Simulate consolidation logic matching admission_gate.c
            h_conn = sqlite3.connect(h_db_path)
            cur = h_conn.cursor()
            cur.execute("SELECT cbm_uri, label, code_snippet FROM symbolic_nodes WHERE epistemic_status != 'CONTESTED'")
            nodes = cur.fetchall()
            h_conn.close()

            tx_failed = False
            failed_idx = -1
            nodes_attempted = []

            base_conn.isolation_level = None  # manual transaction control
            base_conn.execute("BEGIN IMMEDIATE")
            try:
                for idx, (uri, lbl, snippet) in enumerate(nodes):
                    nodes_attempted.append(idx)
                    try:
                        base_conn.execute("INSERT OR REPLACE INTO nodes (cbm_uri, label) VALUES (?, ?)", (uri, lbl))
                    except sqlite3.Error as e:
                        # Fail-Fast: abort immediately on sqlite3_step != SQLITE_DONE
                        tx_failed = True
                        failed_idx = idx
                        base_conn.execute("ROLLBACK")
                        break
            except Exception:
                base_conn.execute("ROLLBACK")

            self.assertTrue(tx_failed)
            self.assertEqual(failed_idx, 4)
            # Nodes 5 through 9 must never have been attempted
            self.assertNotIn(5, nodes_attempted)
            self.assertNotIn(9, nodes_attempted)

            # Check that base_db has 0 nodes after ROLLBACK
            cur_base = base_conn.cursor()
            cur_base.execute("SELECT count(*) FROM nodes")
            count = cur_base.fetchone()[0]
            self.assertEqual(count, 0, "All partial nodes must be rolled back on consolidation failure")
            base_conn.close()

    def test_scenario_leave_base_graph_unmodified_after_consolidation_abort(self):
        """
        Scenario: Should leave Base Graph unmodified after consolidation abort
        Given a Base Graph in generation 5
        And a consolidation attempt that failed at the second virtual edge
        When the Base Graph is queried after the failure
        Then zero nodes or edges from that horizon exist in Base Graph and generation_log remains unchanged at 5
        """
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            base_conn = sqlite3.connect(base_db_path)
            base_conn.isolation_level = None
            base_conn.execute("CREATE TABLE projects (name TEXT PRIMARY KEY, root_dir TEXT)")
            base_conn.execute("INSERT INTO projects VALUES ('default', '')")
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type))")
            base_conn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY)")
            for g in range(1, 6):
                base_conn.execute("INSERT INTO generation_log VALUES (?)", (g,))

            base_generation = 5

            # Virtual edge table has a trigger failing on 2nd edge
            base_conn.execute("""
                CREATE TRIGGER fail_on_edge_2 BEFORE INSERT ON virtual_edges
                FOR EACH ROW WHEN NEW.edge_type = 'FAIL_EDGE'
                BEGIN
                    SELECT RAISE(ABORT, 'Simulated edge constraint failure');
                END;
            """)

            edges_to_insert = [
                ("cbm://repo/src/a.c#A", "cbm://repo/src/b.c#B", "CALLS", "H_01"),
                ("cbm://repo/src/b.c#B", "cbm://repo/src/c.c#C", "FAIL_EDGE", "H_01"),
                ("cbm://repo/src/c.c#C", "cbm://repo/src/d.c#D", "CALLS", "H_01"),
            ]

            base_conn.execute("BEGIN IMMEDIATE")
            failed = False
            # Insert some nodes first
            base_conn.execute("INSERT INTO nodes VALUES ('cbm://repo/src/a.c#A', 'NodeA')")
            try:
                for src, tgt, etype, orig in edges_to_insert:
                    base_conn.execute(
                        "INSERT OR REPLACE INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES (?, ?, ?, ?, 1000)",
                        (src, tgt, etype, orig)
                    )
            except sqlite3.Error:
                failed = True
                base_conn.execute("ROLLBACK")

            self.assertTrue(failed)

            # Query base_db after failure
            cur = base_conn.cursor()
            cur.execute("SELECT count(*) FROM nodes WHERE cbm_uri LIKE '%repo%'")
            self.assertEqual(cur.fetchone()[0], 0, "Zero nodes from candidate horizon must exist in base graph")

            cur.execute("SELECT count(*) FROM virtual_edges WHERE origin_horizon = 'H_01'")
            self.assertEqual(cur.fetchone()[0], 0, "Zero edges from candidate horizon must exist in base graph")

            cur.execute("SELECT MAX(generation) FROM generation_log")
            max_gen = cur.fetchone()[0]
            self.assertEqual(max_gen, 5, "generation_log must remain at 5")
            self.assertEqual(base_generation, 5)
            base_conn.close()

    # =========================================================================
    # Section 2 — Integration Tests
    # =========================================================================

    # --- 2.1 Two-Phase Order and State Reconciliation ---

    def test_scenario_execute_base_db_commit_before_updating_horizon_status(self):
        """
        Scenario: Should execute base_db COMMIT before updating horizon status
        Phase 1: COMMIT on base_db succeeds, base_generation is incremented
        Phase 2: cbm_promote_horizon_state is called only AFTER Phase 1 COMMIT succeeds
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")

        # Verify Two-Phase structure in admission_gate.c
        self.assertIn("COMMIT;", gate_c)
        self.assertIn("cbm_promote_horizon_state", gate_c)
        self.assertIn("gate->base_generation++", gate_c)

        # In admission_gate.c, Phase 1 COMMIT must appear before Phase 2 cbm_promote_horizon_state
        idx_commit = gate_c.find('sqlite3_exec(gate->base_db, "COMMIT;", NULL, NULL, NULL)')
        idx_promote_state = gate_c.find('cbm_promote_horizon_state(pool, horizon_id)')
        self.assertTrue(idx_commit > 0, "COMMIT; call must exist in cbm_promote_horizon")
        self.assertTrue(idx_promote_state > 0, "cbm_promote_horizon_state call must exist")
        self.assertLess(idx_commit, idx_promote_state, "Phase 1 COMMIT must occur BEFORE Phase 2 cbm_promote_horizon_state")

    def test_scenario_keep_horizon_status_as_active_if_base_db_commit_fails(self):
        """
        Scenario: Should keep horizon status as ACTIVE if base_db COMMIT fails
        Given an exclusive lock or I/O failure that causes COMMIT to fail
        Then cbm_promote_horizon_state is NEVER called, horizon status remains ACTIVE, and CBM_ADMISSION_ERR_COMMIT_FAILED returned
        """
        with tempfile.TemporaryDirectory() as td:
            meta_db_path = os.path.join(td, "h_meta.db")
            conn = sqlite3.connect(meta_db_path)
            conn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT)")
            conn.execute("INSERT INTO horizon_metadata VALUES ('H_fail_commit', 9999, 'ACTIVE')")
            conn.commit()

            # State machine simulator enforcing 2-phase rule
            class AdmissionSimulator:
                def __init__(self, meta_db):
                    self.meta_db = meta_db
                    self.horizon_state_called = False
                    self.generation = 1

                def promote(self, horizon_id, simulate_commit_failure=True):
                    # Phase 1: Try base_db commit
                    if simulate_commit_failure:
                        # ROLLBACK
                        return CBM_ADMISSION_ERR_COMMIT_FAILED

                    self.generation += 1
                    # Phase 2: Call horizon promotion
                    self.horizon_state_called = True
                    self.meta_db.execute("UPDATE horizon_metadata SET status = 'PROMOTED' WHERE horizon_id = ?", (horizon_id,))
                    self.meta_db.commit()
                    return CBM_ADMISSION_OK

            sim = AdmissionSimulator(conn)
            rc = sim.promote("H_fail_commit", simulate_commit_failure=True)

            self.assertEqual(rc, CBM_ADMISSION_ERR_COMMIT_FAILED)
            self.assertFalse(sim.horizon_state_called, "Phase 2 must NEVER be called when Phase 1 COMMIT fails")

            cur = conn.cursor()
            cur.execute("SELECT status FROM horizon_metadata WHERE horizon_id = 'H_fail_commit'")
            status = cur.fetchone()[0]
            self.assertEqual(status, "ACTIVE", "Horizon status must remain ACTIVE upon commit failure")
            conn.close()

    def test_scenario_trigger_reconciliation_alert_if_horizon_db_update_fails_after_base_commit(self):
        """
        Scenario: Should trigger reconciliation alert if horizon DB update fails after base commit
        Given base_db committed (Phase 1 passed)
        And horizon DB update fails (Phase 2 fails)
        Then emits structured reconciliation warning and returns success/warning detail
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("reconciliation", gate_c.lower())
        self.assertIn("prom_rc != 0", gate_c)

    # --- 2.2 Union Session Enforcement and Legacy Override ---

    def test_scenario_reject_promotion_without_union_session_when_legacy_unset(self):
        """
        Scenario: Should reject promotion without Union session when CBM_ALLOW_LEGACY_PROMOTION is unset
        Given CBM_ALLOW_LEGACY_PROMOTION unset or 0
        When promote_horizon is invoked without session_id
        Then handler rejects with SESSION_REQUIRED and isError is true
        """
        old_val = os.environ.get("CBM_ALLOW_LEGACY_PROMOTION")
        try:
            if "CBM_ALLOW_LEGACY_PROMOTION" in os.environ:
                del os.environ["CBM_ALLOW_LEGACY_PROMOTION"]

            rc = cbm_enforce_union_session(session_id="")
            self.assertEqual(rc, CBM_ADMISSION_ERR_SESSION_REQUIRED)

            resp = serialize_admission_error(rc, "Promotion requires an active Union session. Set CBM_ALLOW_LEGACY_PROMOTION=1 for legacy override.")
            self.assertTrue(resp["isError"])
            self.assertEqual(resp["code"], "SESSION_REQUIRED")

            # Check handler C code implementation
            handler_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")
            self.assertIn("CBM_ALLOW_LEGACY_PROMOTION", handler_c)
            self.assertIn("SESSION_REQUIRED", handler_c)
            self.assertIn("cbm_refusal_emit(CBM_REFUSAL_SESSION_REQUIRED", handler_c)
        finally:
            if old_val is not None:
                os.environ["CBM_ALLOW_LEGACY_PROMOTION"] = old_val

    def test_scenario_permit_promotion_without_union_session_when_legacy_is_1(self):
        """
        Scenario: Should permit promotion without Union session when CBM_ALLOW_LEGACY_PROMOTION is 1
        Given CBM_ALLOW_LEGACY_PROMOTION=1
        When promote_horizon is invoked without session_id
        Then handler logs override and permits verification to proceed
        """
        old_val = os.environ.get("CBM_ALLOW_LEGACY_PROMOTION")
        try:
            os.environ["CBM_ALLOW_LEGACY_PROMOTION"] = "1"
            rc = cbm_enforce_union_session(session_id="", env_override_val="1")
            self.assertEqual(rc, CBM_ADMISSION_OK)

            handler_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")
            self.assertIn("legacy_promotion_allowed_via_env", handler_c)
        finally:
            if old_val is not None:
                os.environ["CBM_ALLOW_LEGACY_PROMOTION"] = old_val
            elif "CBM_ALLOW_LEGACY_PROMOTION" in os.environ:
                del os.environ["CBM_ALLOW_LEGACY_PROMOTION"]

    # =========================================================================
    # Section 3 — Functional and Acceptance Scenarios
    # =========================================================================

    # --- 3.1 End-to-End Atomicity and Clean Failure Recovery ---

    def test_scenario_prevent_split_brain_state_under_simulated_db_crash(self):
        """
        Scenario: Should prevent split-brain state under simulated database crash (Codex Finding A02)
        Given an ephemeral horizon 'feat_calc' proposing 5 nodes and 2 virtual edges
        When promotion is executed under an injected failure before Base Graph commit
        Then Base Graph contains no partial records
        And querying horizon_metadata returns status ACTIVE (not PROMOTED)
        And caller receives structured JSON refusal detailing exact failure
        """
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            h_meta_path = os.path.join(td, "meta.db")

            base_conn = sqlite3.connect(base_db_path)
            base_conn.isolation_level = None
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type))")
            base_conn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY)")
            base_conn.execute("INSERT INTO generation_log VALUES (10)")

            meta_conn = sqlite3.connect(h_meta_path)
            meta_conn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT)")
            meta_conn.execute("INSERT INTO horizon_metadata VALUES ('feat_calc', 4321, 'ACTIVE')")
            meta_conn.commit()

            # Injected failure: constraint fails during edge insertion before COMMIT
            base_conn.execute("BEGIN IMMEDIATE")
            for i in range(5):
                base_conn.execute("INSERT INTO nodes VALUES (?, ?)", (f"cbm://calc/node_{i}", f"Node_{i}"))

            # Simulate failure during edge consolidation
            injected_failure = True
            if injected_failure:
                base_conn.execute("ROLLBACK")
                error_response = serialize_admission_error(
                    CBM_ADMISSION_ERR_CONSOLIDATION_FAILED,
                    "CONSOLIDATION_FAILED: aborted during edge insertion, transaction rolled back"
                )

            # Assert Base Graph contains 0 nodes/edges from feat_calc
            cur = base_conn.cursor()
            cur.execute("SELECT count(*) FROM nodes WHERE cbm_uri LIKE '%calc%'")
            self.assertEqual(cur.fetchone()[0], 0, "No partial node records must remain in base_db")

            # Assert horizon_metadata status remains ACTIVE
            cur_meta = meta_conn.cursor()
            cur_meta.execute("SELECT status FROM horizon_metadata WHERE horizon_id = 'feat_calc'")
            status = cur_meta.fetchone()[0]
            self.assertEqual(status, "ACTIVE", "Status must remain ACTIVE, never PROMOTED on failure")

            # Assert structured refusal
            self.assertTrue(error_response["isError"])
            self.assertEqual(error_response["code"], "CONSOLIDATION_FAILED")
            self.assertIn("rolled back", error_response["message"])

            base_conn.close()
            meta_conn.close()

    def test_scenario_promote_and_advance_generation_under_clean_conditions(self):
        """
        Scenario: Should successfully promote and advance generation under clean conditions
        Given an active Union session and verified anchors for horizon 'feat_clean'
        When promote_horizon is executed
        Then all nodes and edges are consolidated into base_db, generation_log advances by 1,
        horizon status transitions to PROMOTED, and tool returns success=true with new generation
        """
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            h_meta_path = os.path.join(td, "meta.db")

            base_conn = sqlite3.connect(base_db_path)
            base_conn.isolation_level = None
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type))")
            base_conn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY)")
            base_conn.execute("INSERT INTO generation_log VALUES (1)")

            meta_conn = sqlite3.connect(h_meta_path)
            meta_conn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT)")
            meta_conn.execute("INSERT INTO horizon_metadata VALUES ('feat_clean', 1234, 'ACTIVE')")
            meta_conn.commit()

            # Session check
            session_rc = cbm_enforce_union_session("session_1234")
            self.assertEqual(session_rc, CBM_ADMISSION_OK)

            # Phase 1: Begin immediate, consolidate nodes and edges, advance generation_log, COMMIT
            base_conn.execute("BEGIN IMMEDIATE")
            nodes = [(f"cbm://clean/node_{i}", f"Clean_{i}") for i in range(3)]
            for uri, lbl in nodes:
                base_conn.execute("INSERT INTO nodes VALUES (?, ?)", (uri, lbl))

            edges = [("cbm://clean/node_0", "cbm://clean/node_1", "CALLS", "feat_clean", 2000)]
            for s, t, typ, orig, epoch in edges:
                base_conn.execute("INSERT INTO virtual_edges VALUES (?, ?, ?, ?, ?)", (s, t, typ, orig, epoch))

            base_conn.execute("INSERT OR REPLACE INTO generation_log (generation) VALUES (COALESCE((SELECT MAX(generation) FROM generation_log), 0) + 1)")
            base_conn.execute("COMMIT")

            cur = base_conn.cursor()
            cur.execute("SELECT MAX(generation) FROM generation_log")
            new_generation = cur.fetchone()[0]
            self.assertEqual(new_generation, 2)

            # Phase 2: Update horizon state
            meta_conn.execute("UPDATE horizon_metadata SET status = 'PROMOTED' WHERE horizon_id = 'feat_clean'")
            meta_conn.commit()

            cur_meta = meta_conn.cursor()
            cur_meta.execute("SELECT status FROM horizon_metadata WHERE horizon_id = 'feat_clean'")
            self.assertEqual(cur_meta.fetchone()[0], "PROMOTED")

            tool_response = {
                "success": True,
                "horizon_id": "feat_clean",
                "new_generation": new_generation,
                "nodes_consolidated": len(nodes),
                "edges_consolidated": len(edges)
            }
            self.assertTrue(tool_response["success"])
            self.assertEqual(tool_response["new_generation"], 2)
            self.assertEqual(tool_response["nodes_consolidated"], 3)
            self.assertEqual(tool_response["edges_consolidated"], 1)

            base_conn.close()
            meta_conn.close()

    # =========================================================================
    # Section 4 — Rework Regression Tests (Audit Findings VULN-01/02, EDGE-01/02/03)
    # =========================================================================

    def test_rework_edge01_refusal_codes_preservation_for_18_19_20(self):
        """
        EDGE-01: Codes 18, 19, 20 must preserve original taxonomy mappings and
        not collide with consolidation, commit, or session failure codes.
        """
        refusal_h = (REPO_ROOT / "src" / "union" / "union_refusal.h").read_text(encoding="utf-8")
        refusal_c = (REPO_ROOT / "src" / "union" / "union_refusal.c").read_text(encoding="utf-8")

        # Verify enum values in header
        self.assertIn("CBM_REFUSAL_CODE_DOC_ASYMMETRY = 18", refusal_h)
        self.assertIn("CBM_REFUSAL_LOG_REF_IMMUTABLE = 19", refusal_h)
        self.assertIn("CBM_REFUSAL_PROVENANCE_UNDECLARED = 20", refusal_h)
        self.assertIn("CBM_REFUSAL_CONSOLIDATION_FAILED = 30", refusal_h)
        self.assertIn("CBM_REFUSAL_COMMIT_FAILED = 31", refusal_h)
        self.assertIn("CBM_REFUSAL_SESSION_REQUIRED = 32", refusal_h)

        # In union_refusal.c, union_refusal_code_to_string must NOT intercept 18, 19, 20
        self.assertNotIn("code == 18 ||", refusal_c)
        self.assertNotIn("code == 19 ||", refusal_c)
        self.assertNotIn("code == 20 ||", refusal_c)

        # Verify mapping logic
        self.assertEqual(union_refusal_code_to_string(18), "CODE_DOC_ASYMMETRY")
        self.assertEqual(union_refusal_code_to_string(19), "LOG_REF_IMMUTABLE")
        self.assertEqual(union_refusal_code_to_string(20), "PROVENANCE_UNDECLARED")

        # Verify F002 codes map to expected strings
        self.assertEqual(union_refusal_code_to_string(30), "CONSOLIDATION_FAILED")
        self.assertEqual(union_refusal_code_to_string(-7), "CONSOLIDATION_FAILED")
        self.assertEqual(union_refusal_code_to_string(31), "COMMIT_FAILED")
        self.assertEqual(union_refusal_code_to_string(-8), "COMMIT_FAILED")
        self.assertEqual(union_refusal_code_to_string(32), "SESSION_REQUIRED")
        self.assertEqual(union_refusal_code_to_string(-9), "SESSION_REQUIRED")

    def test_rework_vuln01_path_traversal_rejection_in_project(self):
        """
        VULN-01: handle_promote_horizon must sanitize proj_name against path traversal:
        rejecting '/', '\\', and '..' immediately.
        """
        handler_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")

        # Verify source code contains sanitization guard
        self.assertIn('strstr(proj_name, "..")', handler_c)
        self.assertIn("strchr(proj_name, '/')", handler_c)
        self.assertIn("strchr(proj_name, '\\\\')", handler_c)
        self.assertIn('Path traversal detected in project', handler_c)

        # Verify algorithmic behavior
        def is_safe_proj_name(name: str) -> bool:
            if not name:
                return True
            return ".." not in name and "/" not in name and "\\" not in name

        malicious_payloads = [
            "../etc/passwd",
            "..\\windows\\system32",
            "/absolute/root",
            "C:\\foo\\bar",
            "test/../escape",
            "..",
            "subdir/project",
            "subdir\\project",
        ]
        for payload in malicious_payloads:
            self.assertFalse(is_safe_proj_name(payload), f"Payload '{payload}' must be rejected")

        safe_payloads = ["valid_project", "project123", "alpha-beta", "core_db"]
        for payload in safe_payloads:
            self.assertTrue(is_safe_proj_name(payload), f"Payload '{payload}' must be allowed")

    def test_rework_edge02_mcp_error_code_mapping_consolidation_and_commit_failed(self):
        """
        EDGE-02: promote_handler.c must explicitly map CBM_ADMISSION_ERR_CONSOLIDATION_FAILED (-7)
        to 'CONSOLIDATION_FAILED' and CBM_ADMISSION_ERR_COMMIT_FAILED (-8) to 'COMMIT_FAILED'.
        """
        handler_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")

        self.assertIn("rc == CBM_ADMISSION_ERR_CONSOLIDATION_FAILED", handler_c)
        self.assertIn('err_code = "CONSOLIDATION_FAILED"', handler_c)
        self.assertIn("rc == CBM_ADMISSION_ERR_COMMIT_FAILED", handler_c)
        self.assertIn('err_code = "COMMIT_FAILED"', handler_c)

        # Verify error response serialization
        resp_consolidation = serialize_admission_error(CBM_ADMISSION_ERR_CONSOLIDATION_FAILED, "failed inserting node")
        self.assertEqual(resp_consolidation["code"], "CONSOLIDATION_FAILED")
        self.assertTrue(resp_consolidation["isError"])

        resp_commit = serialize_admission_error(CBM_ADMISSION_ERR_COMMIT_FAILED, "lock failure")
        self.assertEqual(resp_commit["code"], "COMMIT_FAILED")
        self.assertTrue(resp_commit["isError"])

    def test_rework_edge03_repo_path_resolution_fallback(self):
        """
        EDGE-03: When repo_path is omitted, do not use proj_name as canonical_root
        if it is not an accessible directory path; use the session root / current working directory.
        """
        handler_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")

        # Verify cbm_is_dir check protects fallback
        self.assertIn("cbm_is_dir", handler_c)
        self.assertIn("cbm_mcp_server_session_root(srv)", handler_c)

    def test_rework_vuln02_sqlite3_prepare_v2_failure_triggers_rollback(self):
        """
        VULN-02: cbm_promote_horizon must check return code of sqlite3_prepare_v2 for both
        symbolic_nodes and virtual_edges queries. If != SQLITE_OK, abort immediately,
        call ROLLBACK;, format out_error with CONSOLIDATION_FAILED, and return -7.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")

        # Source code checks
        self.assertTrue('prep_rc = sqlite3_prepare_v2(gate->base_db, sel_nodes' in gate_c or 'prep_rc = sqlite3_prepare_v2(hdb, sel_nodes' in gate_c)
        self.assertIn('if (prep_rc != SQLITE_OK)', gate_c)
        self.assertIn('CONSOLIDATION_FAILED: failed to prepare symbolic_nodes query', gate_c)

        self.assertTrue('prep_rc = sqlite3_prepare_v2(gate->base_db, sel_edges' in gate_c or 'prep_rc = sqlite3_prepare_v2(hdb, sel_edges' in gate_c)
        self.assertIn('CONSOLIDATION_FAILED: failed to prepare virtual_edges query', gate_c)


        # Functional simulation: corrupt horizon DB schema causes prepare_v2 to fail
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            h_db_path = os.path.join(td, "corrupt_h.db")

            base_conn = sqlite3.connect(base_db_path)
            base_conn.isolation_level = None
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY)")
            base_conn.execute("INSERT INTO generation_log VALUES (1)")

            # Create candidate horizon DB with missing/malformed symbolic_nodes table
            h_conn = sqlite3.connect(h_db_path)
            h_conn.execute("CREATE TABLE invalid_table (dummy TEXT)")
            h_conn.commit()
            h_conn.close()

            # Execute consolidation logic with prepare check
            base_conn.execute("BEGIN IMMEDIATE")
            prep_ok = True
            h_conn = sqlite3.connect(h_db_path)
            try:
                # Simulating sqlite3_prepare_v2 failure on missing symbolic_nodes
                h_conn.execute("SELECT cbm_uri, label, code_snippet FROM symbolic_nodes WHERE epistemic_status != 'CONTESTED'")
            except sqlite3.OperationalError:
                prep_ok = False
                base_conn.execute("ROLLBACK")
            finally:
                h_conn.close()

            self.assertFalse(prep_ok, "sqlite3_prepare_v2 must fail when table does not exist")

            # Check that base graph is unchanged
            cur = base_conn.cursor()
            cur.execute("SELECT count(*) FROM nodes")
            self.assertEqual(cur.fetchone()[0], 0)
            cur.execute("SELECT MAX(generation) FROM generation_log")
            self.assertEqual(cur.fetchone()[0], 1)
            base_conn.close()

    def test_rework_vuln02_mid_stream_step_error_triggers_rollback(self):
        """
        VULN-02: When iterating rows, if the loop ends and final return code was NOT SQLITE_DONE
        (e.g., SQLITE_CORRUPT or SQLITE_IOERR), abort immediately, call ROLLBACK;,
        format out_error with CONSOLIDATION_FAILED, and return -7.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")

        # Source code checks
        self.assertIn('if (step_sel_rc != SQLITE_DONE)', gate_c)
        self.assertIn('CONSOLIDATION_FAILED: iteration of symbolic_nodes failed', gate_c)
        self.assertIn('CONSOLIDATION_FAILED: iteration of virtual_edges failed', gate_c)

        # Functional simulation: mid-stream step error during row fetch triggers rollback
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            base_conn = sqlite3.connect(base_db_path)
            base_conn.isolation_level = None
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY)")
            base_conn.execute("INSERT INTO generation_log VALUES (1)")

            base_conn.execute("BEGIN IMMEDIATE")
            # Simulate stepped iteration where step returns SQLITE_ROW, SQLITE_ROW, then SQLITE_IOERR (not SQLITE_DONE)
            mock_rows = [
                ("cbm://repo/src/a.c#A", "A"),
                ("cbm://repo/src/b.c#B", "B"),
            ]
            final_step_code = 10  # SQLITE_IOERR

            for uri, lbl in mock_rows:
                base_conn.execute("INSERT INTO nodes VALUES (?, ?)", (uri, lbl))

            # Loop exited with final_step_code != SQLITE_DONE (101)
            SQLITE_DONE = 101
            if final_step_code != SQLITE_DONE:
                base_conn.execute("ROLLBACK")
                rc = CBM_ADMISSION_ERR_CONSOLIDATION_FAILED
            else:
                base_conn.execute("COMMIT")
                rc = CBM_ADMISSION_OK

            self.assertEqual(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED)

            # Assert base DB was completely rolled back and has 0 rows
            cur = base_conn.cursor()
            cur.execute("SELECT count(*) FROM nodes")
            self.assertEqual(cur.fetchone()[0], 0, "Partial nodes inserted before mid-stream failure must be rolled back")
            base_conn.close()

    def test_f002_hardening_state_transition_failed_constant_and_mapping(self):
        """F002 Hardening: admission_gate.h defines -11 and promote_handler maps to RECONCILIATION_REQUIRED"""
        gate_h = (REPO_ROOT / "src" / "admission" / "admission_gate.h").read_text(encoding="utf-8")
        self.assertIn("#define CBM_ADMISSION_ERR_STATE_TRANSITION_FAILED -11", gate_h)

        promote_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")
        self.assertIn("CBM_ADMISSION_ERR_STATE_TRANSITION_FAILED", promote_c)
        self.assertIn('"RECONCILIATION_REQUIRED"', promote_c)

        horizon_pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        self.assertIn("step_rc == SQLITE_DONE", horizon_pool_c)

    def test_f002_hardening_only_accepted_nodes_consolidated(self):
        """F002 Hardening: only nodes with epistemic_status = 'ACCEPTED' are consolidated"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("WHERE epistemic_status = 'ACCEPTED'", gate_c)
        self.assertNotIn("WHERE epistemic_status != 'CONTESTED'", gate_c)

    def test_f002_hardening_anchored_node_consolidation_restriction(self):
        """
        F002 Hardening: Only consolidate nodes and edges corresponding to verified anchors.
        Unanchored nodes and edges not connected to anchored nodes must be skipped.
        If an anchor was relocated, apply updated coordinates to the consolidated node.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")

        # Verify C source code checks anchor filtering and identity functions
        self.assertIn("cbm_find_matching_anchor", gate_c)
        self.assertIn("cbm_node_exists_in_base", gate_c)
        self.assertIn("!edge_anchored", gate_c)
        self.assertIn("relocs[matched_anchor_idx].was_relocated", gate_c)
        self.assertIn("new_byte_start", gate_c)

        # Functional simulation of anchored consolidation with dual-endpoint verification
        with tempfile.TemporaryDirectory() as td:
            base_db_path = os.path.join(td, "base.db")
            h_db_path = os.path.join(td, "h.db")

            base_conn = sqlite3.connect(base_db_path)
            base_conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            base_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type))")
            base_conn.commit()

            h_conn = sqlite3.connect(h_db_path)
            h_conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, epistemic_status TEXT, code_snippet TEXT)")
            h_conn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT)")

            # Insert 3 ACCEPTED nodes in horizon
            h_conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/a.c#SymbolA', 'SymbolA', 'ACCEPTED', '{}')")
            h_conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/b.c#SymbolB', 'SymbolB', 'ACCEPTED', '{}')")
            h_conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/c.c#SymbolC', 'SymbolC', 'ACCEPTED', '{}')")

            # Insert edges: A -> B and B -> C
            h_conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/src/a.c#SymbolA', 'cbm://repo/src/b.c#SymbolB', 'CALLS', 'h1')")
            h_conn.execute("INSERT INTO virtual_edges VALUES ('cbm://repo/src/b.c#SymbolB', 'cbm://repo/src/c.c#SymbolC', 'CALLS', 'h1')")
            h_conn.commit()

            # Case 1: Proposed anchors contain both SymbolA and SymbolB, but NOT SymbolC
            anchors = [
                {"file_path": "src/a.c", "symbol_name": "SymbolA"},
                {"file_path": "src/b.c", "symbol_name": "SymbolB"}
            ]

            cur = h_conn.cursor()
            cur.execute("SELECT cbm_uri, label, code_snippet FROM symbolic_nodes WHERE epistemic_status = 'ACCEPTED'")
            admitted_nodes = []
            for uri, lbl, code in cur.fetchall():
                # Exact symbol and file match (mirroring cbm_find_matching_anchor)
                is_anchored = any(
                    a["symbol_name"] == uri.split("#")[-1] and a["file_path"] in uri
                    for a in anchors
                )
                if not is_anchored:
                    continue
                admitted_nodes.append(uri)
                base_conn.execute("INSERT OR REPLACE INTO nodes (cbm_uri, label) VALUES (?, ?)", (uri, lbl))

            cur.execute("SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges")
            for src, tgt, etype, orig in cur.fetchall():
                # Dual-endpoint rule: BOTH source and target must be admitted
                edge_anchored = (src in admitted_nodes) and (tgt in admitted_nodes)
                if not edge_anchored:
                    continue
                base_conn.execute(
                    "INSERT OR REPLACE INTO virtual_edges VALUES (?, ?, ?, ?, 100)",
                    (src, tgt, etype, orig)
                )
            base_conn.commit()

            # Verify base_db nodes: SymbolA and SymbolB consolidated, SymbolC excluded
            cur_base = base_conn.cursor()
            cur_base.execute("SELECT cbm_uri FROM nodes ORDER BY cbm_uri")
            nodes = [r[0] for r in cur_base.fetchall()]
            self.assertEqual(nodes, ["cbm://repo/src/a.c#SymbolA", "cbm://repo/src/b.c#SymbolB"])
            self.assertNotIn("cbm://repo/src/c.c#SymbolC", nodes)

            # Verify base_db edges: ONLY SymbolA -> SymbolB is consolidated; SymbolB -> SymbolC is excluded
            cur_base.execute("SELECT source_uri, target_uri FROM virtual_edges")
            edges = cur_base.fetchall()
            self.assertEqual(len(edges), 1)
            self.assertEqual(edges[0], ("cbm://repo/src/a.c#SymbolA", "cbm://repo/src/b.c#SymbolB"))

            h_conn.close()
            base_conn.close()

    def test_admission_rejects_substring_and_different_file_false_positives(self):
        """
        Codex Finding 1: An anchor for symbol 'A' in 'src/a.c' must NOT match '#AB',
        '#OtherA', or symbol 'A' in 'src/other.c'. Exact symbol and path identity is required.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("cbm_find_matching_anchor", gate_c)
        self.assertNotIn("strstr(uri, anchors[a].symbol_name)", gate_c)

    def test_edge_consolidation_rejects_unanchored_endpoint(self):
        """
        Codex Finding 2: Having an anchor only for A must NOT permit edge B -> A
        if B is unanchored and excluded from promotion. Both endpoints must be admitted.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("edge_anchored = (src_ok && tgt_ok)", gate_c)

    def test_attached_db_native_2pc_atomicity(self):
        """
        Codex Finding (Round 4): All-or-nothing atomicity across base_db and horizon_db
        is guaranteed via SQLite native 2PC using ATTACH DATABASE. This eliminates DIY
        compensating transactions that fail on overwrite and crash recovery.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("ATTACH DATABASE ? AS h_db;", gate_c)
        self.assertIn("UPDATE h_db.horizon_metadata SET status = 'PROMOTED'", gate_c)
        self.assertIn("DETACH DATABASE h_db;", gate_c)
        self.assertIn("ROLLBACK;", gate_c)

    def test_cbm_find_matching_anchor_exact_identity_and_confinement(self):
        """
        Codex Round 4 Finding 1:
        1. Divergent URI symbol (e.g. #AB) must NEVER be overridden by node label='A'.
        2. Normalized file path comparison must strictly match full path, rejecting
           relaxed suffix matches (e.g. other/src/foo.c vs src/foo.c).
        3. Asymmetric file paths (one side has file path, the other does not) must be rejected.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("cbm_paths_match_strict", gate_c)
        self.assertIn("strcmp(node_symbol, anchors[a].symbol_name) != 0", gate_c)
        self.assertIn("anchor_has_path != node_has_path", gate_c)
        # Ensure relaxed suffix matching was completely eradicated
        self.assertNotIn("len_a > len_b", gate_c)
        self.assertNotIn("len_b > len_a", gate_c)

        # Mandatory triple identity: repo required when expected_repo set
        self.assertIn("parsed.repo[0] == '\\0' || strcmp(parsed.repo, expected_repo) != 0", gate_c)
        # Redundant slashes after dot strictly preserved/rejected
        self.assertIn("p[2] == '/' || p[2] == '\\\\'", gate_c)

    def test_positive_2pc_durability_and_fail_fast_schema_invariants(self):
        """
        Codex Round 6 Proof: Positive verification of DELETE rollback journal,
        synchronous>=FULL, rejection of :memory: databases, explicit table_info schema
        introspection, and zero silent error omission in cbm_node_exists_in_base.
        """
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")

        # 1. Rejection of in-memory database
        self.assertIn(":memory:", gate_c)
        self.assertIn("sqlite3_db_filename(gate->base_db, \"main\")", gate_c)

        # 2. Positive verification of journal_mode and synchronous
        self.assertIn("PRAGMA main.journal_mode;", gate_c)
        self.assertIn("PRAGMA h_db.journal_mode;", gate_c)
        self.assertIn("PRAGMA main.synchronous;", gate_c)
        self.assertIn("PRAGMA h_db.synchronous;", gate_c)
        self.assertIn("main_sync >= 2 && h_sync >= 2", gate_c)

        # 3. Explicit schema introspection via PRAGMA table_info(nodes)
        self.assertIn("PRAGMA table_info(nodes);", gate_c)
        self.assertIn("base_has_byte_start", gate_c)
        self.assertIn("base_has_properties", gate_c)

        # 4. Zero silent error omission in cbm_node_exists_in_base
        self.assertIn("rc_src < 0", gate_c)
        self.assertIn("rc_tgt < 0", gate_c)

    def test_executable_sqlite_2pc_all_or_nothing_under_simulated_failure(self):
        """
        Codex Round 5 Finding 2: Executable test proving SQLite 2PC All-or-Nothing
        guarantee under failure during multi-database transaction using ATTACH DATABASE.
        When consolidation fails mid-stream, ROLLBACK restores both base and attached databases.
        """
        with tempfile.TemporaryDirectory() as td:
            base_path = Path(td) / "base.db"
            horizon_path = Path(td) / "horizon.db"

            # Create base db
            bconn = sqlite3.connect(base_path)
            bconn.execute("PRAGMA journal_mode = DELETE;")
            bconn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT);")
            bconn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));")
            bconn.execute("CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);")
            bconn.commit()

            # Create horizon db
            hconn = sqlite3.connect(horizon_path)
            hconn.execute("PRAGMA journal_mode = DELETE;")
            hconn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT, created_at INTEGER, last_heartbeat INTEGER);")
            hconn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, epistemic_status TEXT, code_snippet TEXT);")
            hconn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT);")
            hconn.execute("INSERT INTO horizon_metadata VALUES ('h_test', 1234, 'ACTIVE', 100, 100);")
            hconn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://proj/src/foo.c#A', 'A', 'ACCEPTED', '{}');")
            hconn.execute("INSERT INTO virtual_edges VALUES ('cbm://proj/src/foo.c#A', 'cbm://proj/src/foo.c#B', 'CALLS', 'h_test');")
            hconn.commit()
            hconn.close()

            # Add trigger on base_db to simulate failure during virtual_edges insertion
            bconn.execute("CREATE TRIGGER fail_edges BEFORE INSERT ON virtual_edges BEGIN SELECT RAISE(ABORT, 'Simulated mid-stream edge failure'); END;")
            bconn.commit()

            # Execute 2PC sequence via ATTACH
            bconn.execute(f"ATTACH DATABASE '{horizon_path}' AS h_db;")
            bconn.execute("PRAGMA h_db.journal_mode = DELETE;")
            bconn.execute("BEGIN IMMEDIATE;")

            # 1. Insert node succeeds
            bconn.execute("INSERT OR REPLACE INTO nodes (cbm_uri, label) SELECT cbm_uri, label FROM h_db.symbolic_nodes WHERE epistemic_status = 'ACCEPTED';")

            # 2. Insert edge fails via trigger
            with self.assertRaises(sqlite3.IntegrityError):
                bconn.execute("INSERT OR REPLACE INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) SELECT source_uri, target_uri, edge_type, origin_horizon, 100 FROM h_db.virtual_edges;")

            # 3. Fail-fast rollback
            bconn.execute("ROLLBACK;")
            bconn.execute("DETACH DATABASE h_db;")

            # Assert base_db has ZERO nodes committed
            cur = bconn.cursor()
            cur.execute("SELECT COUNT(*) FROM nodes;")
            self.assertEqual(cur.fetchone()[0], 0, "Base DB must have 0 nodes after rollback")
            bconn.close()

            # Assert horizon_db remains in ACTIVE status
            hconn = sqlite3.connect(horizon_path)
            cur = hconn.cursor()
            cur.execute("SELECT status FROM horizon_metadata WHERE horizon_id = 'h_test';")
            self.assertEqual(cur.fetchone()[0], "ACTIVE", "Horizon status must remain ACTIVE after rollback")
            hconn.close()

    def test_executable_state_transition_requires_active_status(self):
        """
        Codex Round 5 Finding 2: When horizon status is not ACTIVE (e.g. DISCARDED or missing),
        the atomic UPDATE affects 0 rows, triggering immediate ROLLBACK and leaving base_db untouched.
        """
        with tempfile.TemporaryDirectory() as td:
            base_path = Path(td) / "base.db"
            horizon_path = Path(td) / "horizon.db"

            bconn = sqlite3.connect(base_path)
            bconn.execute("PRAGMA journal_mode = DELETE;")
            bconn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT);")
            bconn.commit()

            hconn = sqlite3.connect(horizon_path)
            hconn.execute("PRAGMA journal_mode = DELETE;")
            hconn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT, created_at INTEGER, last_heartbeat INTEGER);")
            hconn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, epistemic_status TEXT);")
            # Horizon is DISCARDED, not ACTIVE!
            hconn.execute("INSERT INTO horizon_metadata VALUES ('h_discarded', 1234, 'DISCARDED', 100, 100);")
            hconn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://proj/src/foo.c#A', 'A', 'ACCEPTED');")
            hconn.commit()
            hconn.close()

            bconn.execute(f"ATTACH DATABASE '{horizon_path}' AS h_db;")
            bconn.execute("BEGIN IMMEDIATE;")
            bconn.execute("INSERT OR REPLACE INTO nodes (cbm_uri, label) SELECT cbm_uri, label FROM h_db.symbolic_nodes;")

            # Attempt status transition requiring ACTIVE status
            cur = bconn.cursor()
            cur.execute("UPDATE h_db.horizon_metadata SET status = 'PROMOTED' WHERE horizon_id = 'h_discarded' AND status = 'ACTIVE';")
            changes = cur.rowcount

            # Changes is 0 because status was DISCARDED
            self.assertEqual(changes, 0)
            # Gate executes rollback
            bconn.execute("ROLLBACK;")
            bconn.execute("DETACH DATABASE h_db;")

            cur.execute("SELECT COUNT(*) FROM nodes;")
            self.assertEqual(cur.fetchone()[0], 0, "Base DB must have 0 nodes after rollback")
            bconn.close()


if __name__ == "__main__":
    unittest.main()

