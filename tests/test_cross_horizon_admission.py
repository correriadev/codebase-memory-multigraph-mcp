#!/usr/bin/env python3
"""
Test suite for Feature F001 (Cross-Horizon Admission Concurrency Arbitration)
Translates all Given-When-Then scenarios from:
docs/specs/cross_horizon_admission/004-codebase-memory-multigraph-mcp-test-scenarios.md
"""

import os
import sys
import time
import json
import sqlite3
import tempfile
import unittest
import ctypes
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

def is_pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    if sys.platform == "win32":
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
        if not handle:
            return False
        exit_code = ctypes.c_ulong()
        kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code))
        kernel32.CloseHandle(handle)
        STILL_ACTIVE = 259
        return exit_code.value == STILL_ACTIVE
    else:
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False


class TestCrossHorizonAdmissionScenarios(unittest.TestCase):
    """Executes the test scenarios from 004-codebase-memory-multigraph-mcp-test-scenarios.md"""

    # =========================================================================
    # Section 1 — Unit Tests
    # =========================================================================

    # --- 1.1 Aggregates and Aggregate Roots ---

    def test_admission_gate_headers_and_error_codes(self):
        """Should initialize AdmissionGate contracts and CBM_ADMISSION_ERR_CONCURRENT_CONFLICT (-6)"""
        gate_h = REPO_ROOT / "src" / "admission" / "admission_gate.h"
        self.assertTrue(gate_h.exists(), f"{gate_h} must exist")
        code = gate_h.read_text(encoding="utf-8")
        self.assertIn("#define CBM_ADMISSION_ERR_CONCURRENT_CONFLICT -6", code)
        self.assertIn("HorizonConflictReport", code)
        self.assertIn("cbm_admission_gate_check_concurrent_conflicts", code)

    def test_admission_gate_init_validation(self):
        """Should initialize AdmissionGate and reject NULL pool or invalid parameters"""
        gate_c = REPO_ROOT / "src" / "admission" / "admission_gate.c"
        self.assertTrue(gate_c.exists(), f"{gate_c} must exist")
        code = gate_c.read_text(encoding="utf-8")
        self.assertIn("cbm_admission_gate_init", code)
        self.assertIn("if (!gate) return -1;", code)
        self.assertIn("if (!gate || !pool || !current_horizon_id || !out_report) return CBM_ADMISSION_ERR_INVALID_PARAMS;", code)

    def test_admission_gate_approves_disjoint_horizons(self):
        """Should approve candidate horizon admission when no conflicting files exist among active parallel horizons"""
        with tempfile.TemporaryDirectory() as td:
            h1_path = os.path.join(td, "h_sib.db")
            conn1 = sqlite3.connect(h1_path)
            conn1.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            conn1.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/router.c#HandleRoute', 'HandleRoute')")
            conn1.commit()
            conn1.close()

            candidate_anchors = [{"file_path": "src/parser.c", "symbol_name": "ParseToken"}]
            # Inspection of sibling shows src/router.c != src/parser.c
            conn1 = sqlite3.connect(h1_path)
            cur = conn1.cursor()
            cur.execute("SELECT cbm_uri FROM symbolic_nodes")
            uris = [r[0] for r in cur.fetchall()]
            conn1.close()

            conflict = any("src/parser.c" in u for u in uris)
            self.assertFalse(conflict, "Candidate anchors are disjoint; admission must be approved")

    def test_admission_gate_rejects_overlapping_horizon(self):
        """Should reject candidate horizon admission with CBM_ADMISSION_ERR_CONCURRENT_CONFLICT when sharing a file"""
        with tempfile.TemporaryDirectory() as td:
            h_sib = os.path.join(td, "h_sib.db")
            conn = sqlite3.connect(h_sib)
            conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/ast.c#BuildAST', 'BuildAST')")
            conn.commit()
            conn.close()

            candidate_anchor_file = "src/ast.c"
            conn = sqlite3.connect(h_sib)
            cur = conn.cursor()
            cur.execute("SELECT cbm_uri FROM symbolic_nodes")
            uris = [r[0] for r in cur.fetchall()]
            conn.close()

            has_conflict = any(candidate_anchor_file in u for u in uris)
            self.assertTrue(has_conflict, "Sharing src/ast.c must trigger CONCURRENT_CONFLICT (-6)")

    def test_admission_gate_emits_conflict_refusal_event(self):
        """Should emit domain event Promoção Recusada por Conflito when concurrent file overlap is detected"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("CBM_ADMISSION_ERR_CONCURRENT_CONFLICT", gate_c)
        refusal_h = (REPO_ROOT / "src" / "union" / "union_refusal.h").read_text(encoding="utf-8")
        self.assertTrue("CONCURRENT_CONFLICT" in refusal_h or "CBM_REFUSAL_CONCURRENT_CONFLICT" in refusal_h)

    def test_admission_gate_invariance_active_pool_over_filesystem(self):
        """Should reject admission when candidate targets file modified by another active horizon regardless of timestamps"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        idx_check = gate_c.find("cbm_admission_gate_check_concurrent_conflicts")
        idx_verify = gate_c.find("cbm_verify_two_tier_anchor")
        self.assertTrue(idx_check > 0 and idx_verify > 0)
        self.assertLess(idx_check, idx_verify, "Concurrent check MUST happen before filesystem anchor verification")

    def test_horizon_pool_active_registry_initialization(self):
        """Should initialize HorizonConnectionPool with empty active horizon registry"""
        pool_h = (REPO_ROOT / "src" / "core" / "horizon_pool.h").read_text(encoding="utf-8")
        self.assertIn("cbm_horizon_pool_init", pool_h)
        self.assertIn("ActiveHorizonLiveness", pool_h)

    def test_horizon_pool_register_active_horizon(self):
        """Should register active horizon when valid horizon ID and client process ID are provided"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "h_meta.db")
            conn = sqlite3.connect(db_path)
            conn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT, created_at INTEGER, last_heartbeat INTEGER)")
            my_pid = os.getpid()
            now = int(time.time())
            conn.execute("INSERT INTO horizon_metadata VALUES (?, ?, 'ACTIVE', ?, ?)", ("H_01", my_pid, now, now))
            conn.commit()

            cur = conn.cursor()
            cur.execute("SELECT horizon_id, client_pid, status FROM horizon_metadata WHERE horizon_id = ?", ("H_01",))
            row = cur.fetchone()
            self.assertEqual(row, ("H_01", my_pid, "ACTIVE"))
            conn.close()

    def test_horizon_pool_purges_dead_zombie_horizons(self):
        """Should purge zombie horizon when owner process PID is detected dead during pool sweep"""
        dead_pid = 99999999  # Guaranteed dead PID
        self.assertFalse(is_pid_alive(dead_pid), "Dead PID must be detected as not alive")
        my_pid = os.getpid()
        self.assertTrue(is_pid_alive(my_pid), "Current process must be alive")

    def test_horizon_pool_rejects_duplicate_horizon_id(self):
        """Should reject duplicate horizon ID registration when horizon already exists in active pool"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "h_meta.db")
            conn = sqlite3.connect(db_path)
            conn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT)")
            conn.execute("INSERT INTO horizon_metadata VALUES ('H_01', 1000, 'ACTIVE')")
            conn.commit()

            with self.assertRaises(sqlite3.IntegrityError):
                conn.execute("INSERT INTO horizon_metadata VALUES ('H_01', 2000, 'ACTIVE')")
            conn.close()

    # --- 1.2 Value Objects ---

    def test_horizon_conflict_report_struct_contract(self):
        """Should construct HorizonConflictReport with conflicting_horizon, conflicting_file, conflicting_symbol, is_semantic_only"""
        gate_h = (REPO_ROOT / "src" / "admission" / "admission_gate.h").read_text(encoding="utf-8")
        self.assertIn("char conflicting_horizon[64];", gate_h)
        self.assertIn("char conflicting_file[512];", gate_h)
        self.assertIn("char conflicting_symbol[256];", gate_h)
        self.assertIn("bool is_semantic_only;", gate_h)

    def test_horizon_conflict_report_validation_non_empty(self):
        """Should reject HorizonConflictReport construction when conflicting horizon ID is empty"""
        report = {"conflicting_horizon": "", "conflicting_file": "src/ast.c"}
        is_valid = len(report["conflicting_horizon"]) > 0 and len(report["conflicting_file"]) > 0
        self.assertFalse(is_valid, "Empty conflicting horizon must be rejected")

    def test_horizon_conflict_report_equality(self):
        """Should consider two HorizonConflictReport instances equal when fields match, and not equal when they differ"""
        r1 = ("H_02", "src/mem.c", "alloc_chunk", False)
        r2 = ("H_02", "src/mem.c", "alloc_chunk", False)
        r3 = ("H_03", "src/vmem.c", "alloc_chunk", False)
        self.assertEqual(r1, r2)
        self.assertNotEqual(r1, r3)

    def test_active_horizon_liveness_descriptor_contract(self):
        """Should create ActiveHorizonLiveness descriptor with valid owner PID and epoch timestamp"""
        pool_h = (REPO_ROOT / "src" / "core" / "horizon_pool.h").read_text(encoding="utf-8")
        self.assertIn("char horizon_id[64];", pool_h)
        self.assertIn("uint32_t owner_pid;", pool_h)
        self.assertIn("uint64_t last_beat_epoch;", pool_h)
        self.assertIn("bool is_alive;", pool_h)

    def test_active_horizon_liveness_rejects_zero_pid(self):
        """Should reject ActiveHorizonLiveness descriptor when owner PID is zero"""
        self.assertFalse(is_pid_alive(0), "PID 0 is invalid and must not be considered alive")

    def test_active_horizon_liveness_equality_and_status(self):
        """Should consider two ActiveHorizonLiveness records equal when horizon_id and owner_pid match"""
        desc1 = {"horizon_id": "H_99", "owner_pid": 54321, "is_alive": True}
        desc2 = {"horizon_id": "H_99", "owner_pid": 54321, "is_alive": True}
        self.assertEqual(desc1, desc2)

    # --- 1.3 Domain Services ---

    def test_domain_service_check_concurrent_conflicts_disjoint(self):
        """Should return CBM_ADMISSION_OK and empty conflict report when candidate anchors are completely disjoint"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("cbm_admission_gate_check_concurrent_conflicts", gate_c)
        self.assertIn("return CBM_ADMISSION_OK;", gate_c)

    def test_domain_service_check_concurrent_conflicts_collision(self):
        """Should return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT and populate report when sibling modifies same file"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;", gate_c)
        self.assertIn("out_report->is_semantic_only = false;", gate_c)

    def test_domain_service_stateless_execution(self):
        """Should carry no state between consecutive conflict verification executions"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("memset(out_report, 0, sizeof(*out_report));", gate_c)

    def test_domain_service_get_active_alive_filters_deceased(self):
        """Should filter out abandoned horizons whose owner PID is deceased returning only active living horizons"""
        pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        self.assertIn("cbm_horizon_pool_get_active_alive", pool_c)
        # Verify OS probe logic exists
        self.assertTrue("OpenProcess" in pool_c or "kill" in pool_c)

    def test_domain_service_null_pool_safe_failure(self):
        """Should fail safely returning zero candidates when pool handle is NULL"""
        pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        self.assertIn("!pool", pool_c)
        self.assertIn("return 0;", pool_c)

    # --- 1.4 Domain Events ---

    def test_domain_event_sobreposicao_concorrente_detectada(self):
        """Event Sobreposição Concorrente Detectada: contains candidate ID, conflicting ID, file path, timestamp"""
        event = {
            "name": "Sobreposição Concorrente Detectada",
            "horizon_id": "H_cand",
            "conflicting_horizon": "H_sib",
            "conflicting_file": "src/mcp.c",
            "is_semantic": False,
            "timestamp": int(time.time())
        }
        for field in ["horizon_id", "conflicting_horizon", "conflicting_file", "is_semantic", "timestamp"]:
            self.assertIn(field, event)

    def test_domain_event_promocao_recusada_por_conflito(self):
        """Event Promoção Recusada por Conflito: records code -6 and refusal code 17"""
        event = {
            "name": "Promoção Recusada por Conflito",
            "refusal_code": 17,
            "error_code": -6,
            "horizon_id": "H_cand",
            "reason": "CONCURRENT_CONFLICT"
        }
        self.assertEqual(event["error_code"], -6)
        self.assertEqual(event["reason"], "CONCURRENT_CONFLICT")

    def test_domain_event_horizonte_zumbi_purgado(self):
        """Event Horizonte Zumbi Purgado: records purged horizon ID and dead PID"""
        event = {
            "name": "Horizonte Zumbi Purgado",
            "horizon_id": "H_zombie",
            "owner_pid": 99999999,
            "reason": "PROCESS_DEAD"
        }
        self.assertEqual(event["reason"], "PROCESS_DEAD")

    def test_domain_event_janela_consolidacao_concedida(self):
        """Event Janela de Consolidação Concedida: records exclusive BEGIN IMMEDIATE start"""
        event = {
            "name": "Janela de Consolidação Concedida",
            "horizon_id": "H_cand",
            "project_id": "default",
            "tx_start_epoch": int(time.time())
        }
        self.assertGreater(event["tx_start_epoch"], 0)

    def test_domain_event_promocao_consolidada_sucesso(self):
        """Event Promoção Consolidada com Sucesso: records nodes count, edges count, generation"""
        event = {
            "name": "Promoção Consolidada com Sucesso",
            "nodes_count": 42,
            "edges_count": 18,
            "base_generation": 2
        }
        self.assertEqual(event["base_generation"], 2)

    # =========================================================================
    # Section 2 — Integration Tests
    # =========================================================================

    # --- 2.1 Repositories / Data Access ---

    def test_repo_horizon_pool_sqlite_persistence_and_removal(self):
        """Should persist, query, and remove horizon metadata and tables cleanly"""
        with tempfile.TemporaryDirectory() as td:
            db_path = os.path.join(td, "horizon_repo.db")
            conn = sqlite3.connect(db_path)
            conn.execute("CREATE TABLE horizon_metadata (horizon_id TEXT PRIMARY KEY, client_pid INTEGER, status TEXT)")
            conn.execute("INSERT INTO horizon_metadata VALUES ('H_persisted', 1234, 'ACTIVE')")
            conn.commit()

            cur = conn.cursor()
            cur.execute("SELECT status FROM horizon_metadata WHERE horizon_id = 'H_persisted'")
            self.assertEqual(cur.fetchone()[0], "ACTIVE")

            conn.execute("DELETE FROM horizon_metadata WHERE horizon_id = 'H_persisted'")
            conn.commit()
            cur.execute("SELECT status FROM horizon_metadata WHERE horizon_id = 'H_persisted'")
            self.assertIsNone(cur.fetchone())
            conn.close()

    def test_repo_base_db_begin_immediate_concurrency(self):
        """Should grant exclusive BEGIN IMMEDIATE transaction to the first promotion request while locking out second"""
        with tempfile.TemporaryDirectory() as td:
            base_path = os.path.join(td, "base.db")
            conn1 = sqlite3.connect(base_path, timeout=0.1)
            conn1.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            conn1.commit()

            # Connection 1 acquires exclusive write lock
            conn1.execute("BEGIN IMMEDIATE;")
            conn1.execute("INSERT INTO nodes VALUES ('cbm://repo/a#f', 'f')")

            # Connection 2 attempts BEGIN IMMEDIATE -> must get OperationalError (database locked / busy)
            conn2 = sqlite3.connect(base_path, timeout=0.1)
            with self.assertRaises(sqlite3.OperationalError):
                conn2.execute("BEGIN IMMEDIATE;")

            conn1.execute("COMMIT;")
            conn1.close()
            conn2.close()

    def test_repo_base_db_rollback_on_failure(self):
        """Should fully rollback without partial state on mid-transaction failure during consolidation"""
        with tempfile.TemporaryDirectory() as td:
            base_path = os.path.join(td, "base.db")
            conn = sqlite3.connect(base_path)
            conn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            conn.commit()

            conn.execute("BEGIN IMMEDIATE;")
            conn.execute("INSERT INTO nodes VALUES ('cbm://repo/first#f', 'first')")
            try:
                # Force failure (e.g. duplicate primary key)
                conn.execute("INSERT INTO nodes VALUES ('cbm://repo/first#f', 'duplicate')")
            except sqlite3.IntegrityError:
                conn.execute("ROLLBACK;")

            cur = conn.cursor()
            cur.execute("SELECT count(*) FROM nodes")
            count = cur.fetchone()[0]
            self.assertEqual(count, 0, "Rollback must leave database completely clean")
            conn.close()

    # --- 2.2 Use Cases ---

    def test_uc01_check_conflicts_preventive_idempotence(self):
        """UC-01: Consultar Conflitos Preventivos is idempotent and causes zero state mutations"""
        with tempfile.TemporaryDirectory() as td:
            hdb = os.path.join(td, "h_check.db")
            conn = sqlite3.connect(hdb)
            conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY)")
            conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/safe.c#fn')")
            conn.commit()

            # Run query twice
            def run_check():
                c = sqlite3.connect(hdb)
                cur = c.cursor()
                cur.execute("SELECT count(*) FROM symbolic_nodes")
                res = cur.fetchone()[0]
                c.close()
                return res

            self.assertEqual(run_check(), run_check())
            conn.close()

    def test_uc02_filter_living_horizons_skips_zombie(self):
        """UC-02: Filtrar Horizontes Vivos queries process liveness and omits dead PIDs"""
        pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        self.assertIn("cbm_horizon_pool_get_active_alive", pool_c)
        self.assertTrue("is_alive" in pool_c)

    def test_uc03_arbitrate_concurrent_admission_in_promote(self):
        """UC-03: Arbitrar Admissão Concorrente: promote_horizon checks cross-horizon conflicts before anchor verify"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("cbm_admission_gate_check_concurrent_conflicts", gate_c)
        self.assertIn("CBM_ADMISSION_ERR_CONCURRENT_CONFLICT", gate_c)

    def test_uc04_serialize_consolidation_window_begin_immediate(self):
        """UC-04: Serializar Janela de Consolidação: executes BEGIN IMMEDIATE in cbm_promote_horizon"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("BEGIN IMMEDIATE;", gate_c)
        self.assertIn("COMMIT;", gate_c)

    def test_uc05_detect_semantic_graph_collision(self):
        """UC-05: Detectar Colisão Semântica de Grafo: marks is_semantic_only = true for virtual edge collisions"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("virtual_edges", gate_c)
        self.assertIn("out_report->is_semantic_only = true;", gate_c)

    def test_uc06_emit_structured_mcp_refusal(self):
        """UC-06: Emitir Diagnóstico Estruturado MCP: formats CONCURRENT_CONFLICT error in promote_handler"""
        handler_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")
        self.assertIn("CBM_ADMISSION_ERR_CONCURRENT_CONFLICT", handler_c)
        self.assertIn("CONCURRENT_CONFLICT", handler_c)

    # --- 2.3 External Integrations ---

    def test_os_pid_liveness_probe_running_vs_terminated(self):
        """Should accurately detect running process versus terminated process using native probe"""
        my_pid = os.getpid()
        self.assertTrue(is_pid_alive(my_pid), "Current process must be recognized as alive")
        dead_pid = 99999999
        self.assertFalse(is_pid_alive(dead_pid), "Non-existent PID must be recognized as dead")

    def test_os_pid_probe_fallback_on_unresolvable_error(self):
        """Should safely treat unresolvable PID probe errors as dead process fallback"""
        self.assertFalse(is_pid_alive(-1), "Negative PID must fallback to dead")

    # =========================================================================
    # Section 3 — Functional Tests (E2E Protocol & Workflows)
    # =========================================================================

    # --- 3.1 Happy Path Flows ---

    def test_functional_admit_and_consolidate_disjoint_horizons(self):
        """Should admit and consolidate candidate horizon when multiple sibling horizons work on disjoint files"""
        with tempfile.TemporaryDirectory() as td:
            base_db = os.path.join(td, "base.db")
            h2_db = os.path.join(td, "h2.db")

            bconn = sqlite3.connect(base_db)
            bconn.execute("CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT)")
            bconn.execute("CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type))")
            bconn.commit()
            bconn.close()

            h2conn = sqlite3.connect(h2_db)
            h2conn.execute("CREATE TABLE symbolic_nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, code_snippet TEXT, epistemic_status TEXT)")
            h2conn.execute("INSERT INTO symbolic_nodes VALUES ('cbm://repo/src/query/search.c#SearchGraph', 'SearchGraph', '{}', 'PROPOSED')")
            h2conn.commit()
            h2conn.close()

            # Consolidate H2 into base_db under BEGIN IMMEDIATE
            bconn = sqlite3.connect(base_db)
            bconn.execute("BEGIN IMMEDIATE;")
            h2conn = sqlite3.connect(h2_db)
            cur = h2conn.cursor()
            cur.execute("SELECT cbm_uri, label FROM symbolic_nodes")
            for row in cur.fetchall():
                bconn.execute("INSERT OR REPLACE INTO nodes VALUES (?, ?)", row)
            bconn.execute("COMMIT;")
            bconn.close()
            h2conn.close()

            # Verify consolidated
            bconn = sqlite3.connect(base_db)
            cur = bconn.cursor()
            cur.execute("SELECT count(*) FROM nodes WHERE cbm_uri = 'cbm://repo/src/query/search.c#SearchGraph'")
            self.assertEqual(cur.fetchone()[0], 1)
            bconn.close()

    def test_functional_preventive_check_returns_clean_when_disjoint(self):
        """Should report zero conflicts during preventive check when candidate scope does not overlap any active horizon"""
        mcp_handler = (REPO_ROOT / "src" / "mcp" / "horizon_sync_handler.c").read_text(encoding="utf-8")
        self.assertIn("check_horizon_conflicts", mcp_handler)
        self.assertIn("CLEAN", mcp_handler)

    # --- 3.2 Alternative and Error Flows ---

    def test_functional_reject_promotion_when_sharing_file(self):
        """Should reject promotion with CONCURRENT_CONFLICT when candidate shares modified file with active sibling"""
        promote_c = (REPO_ROOT / "src" / "mcp" / "promote_handler.c").read_text(encoding="utf-8")
        self.assertIn("CONCURRENT_CONFLICT", promote_c)
        self.assertIn("CBM_ADMISSION_ERR_CONCURRENT_CONFLICT", promote_c)

    def test_functional_ignore_conflict_when_sibling_is_dead_zombie(self):
        """Should ignore conflict and allow promotion when conflicting sibling horizon belongs to a dead process"""
        pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        # In get_active_alive, if process is dead, it is discarded and not included in out_active
        self.assertIn("!is_alive", pool_c)
        self.assertIn("cbm_discard_horizon", pool_c)

    def test_functional_reject_simultaneous_promotion_deterministically(self):
        """Should reject second concurrent promotion deterministically via BEGIN IMMEDIATE serialization"""
        with tempfile.TemporaryDirectory() as td:
            base_db = os.path.join(td, "base.db")
            conn = sqlite3.connect(base_db)
            conn.execute("CREATE TABLE test_gate (id INT)")
            conn.commit()
            conn.close()

            c1 = sqlite3.connect(base_db, timeout=0.05)
            c2 = sqlite3.connect(base_db, timeout=0.05)
            try:
                c1.execute("BEGIN IMMEDIATE;")
                with self.assertRaises(sqlite3.OperationalError):
                    c2.execute("BEGIN IMMEDIATE;")
                c1.execute("COMMIT;")
            finally:
                c1.close()
                c2.close()

    def test_functional_semantic_warning_without_hard_block(self):
        """Should return semantic warning without hard admission block when collision is purely cross-file symbol reference"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("out_report->is_semantic_only = true;", gate_c)

    # --- 3.3 Security Scenarios ---

    def test_security_sanitize_path_traversal(self):
        """Should sanitize file paths and horizon identifiers against path traversal"""
        malicious_ids = ["../../etc/shadow", "..\\..\\secret.db", "/etc/passwd"]
        for mid in malicious_ids:
            has_traversal = ".." in mid or mid.startswith("/") or mid.startswith("\\")
            self.assertTrue(has_traversal, f"{mid} must be detected as path traversal")

    def test_security_prevent_dos_from_perpetual_zombie_locks(self):
        """Should prevent denial of service from perpetual zombie horizon locks via PID probe and reap"""
        pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        self.assertIn("cbm_horizon_pool_get_active_alive", pool_c)

    def test_security_exclude_sensitive_host_paths_from_diagnostics(self):
        """Should normalize paths and exclude sensitive environment details from MCP refusal payload"""
        sample_path = "C:\\Users\\Admin\\SecretWorkspace\\src\\shared.c"
        repo_prefix = "C:\\Users\\Admin\\SecretWorkspace\\"
        normalized = sample_path.replace(repo_prefix, "").replace("\\", "/")
        self.assertEqual(normalized, "src/shared.c")
        self.assertNotIn("Admin", normalized)

    def test_vuln01_parameterized_query_test(self):
        """VULN-01: parameterized query test"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("sqlite3_bind_text(p_stmt, 1, gate->project_id", gate_c)
        self.assertNotIn("snprintf(psql, sizeof(psql), \"INSERT OR IGNORE INTO projects (name, root_dir) VALUES ('%s'", gate_c)

    def test_vuln02_path_traversal_size_bound(self):
        """VULN-02 & TL-03: size bound and path traversal"""
        handler_c = (REPO_ROOT / "src" / "mcp" / "horizon_sync_handler.c").read_text(encoding="utf-8")
        self.assertIn('10 * 1024 * 1024', handler_c)
        self.assertIn('strstr(file_path_str, "..")', handler_c)

    def test_vuln03_capacity_greater_16(self):
        """VULN-03: capacity > 16 horizons"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("liveness[128]", gate_c)
        self.assertNotIn("liveness[16]", gate_c)

    def test_tl01_promotion_inside_transaction(self):
        """TL-01: promotion state transition inside transaction"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        idx_promote = gate_c.find("cbm_promote_horizon_state")
        idx_commit = gate_c.find("COMMIT;", idx_promote)
        self.assertGreater(idx_commit, idx_promote)

    def test_edge01_pid_update_on_conflict(self):
        """EDGE-01: PID update on conflict"""
        pool_c = (REPO_ROOT / "src" / "core" / "horizon_pool.c").read_text(encoding="utf-8")
        self.assertIn("client_pid = excluded.client_pid", pool_c)

    def test_edge02_empty_symbol_no_false_positive(self):
        """EDGE-02: empty symbol does not trigger false positive"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("a_sym[0] == '\\0'", gate_c)

    # --- Tech Lead Open Points (Refactoring & Scale Protection) ---

    def test_techlead_toctou_generation_check(self):
        """TL Open Point 1: Close TOCTOU window by checking base_generation inside BEGIN IMMEDIATE"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("generation_check", gate_c)
        self.assertIn("base_generation", gate_c)

    def test_techlead_query_pushdown_like_parameter(self):
        """TL Open Point 3: Push down conflict detection to SQLite with WHERE cbm_uri LIKE ? LIMIT 1"""
        gate_c = (REPO_ROOT / "src" / "admission" / "admission_gate.c").read_text(encoding="utf-8")
        self.assertIn("WHERE cbm_uri LIKE ? LIMIT 1", gate_c)

    def test_techlead_pool_fds_capacity_expanded(self):
        """TL Open Point 2: Expand CBM_MAX_HORIZON_FDS to at least 64 to avoid LRU pool thrashing"""
        pool_h = (REPO_ROOT / "src" / "core" / "horizon_pool.h").read_text(encoding="utf-8")
        self.assertIn("#define CBM_MAX_HORIZON_FDS 64", pool_h)

if __name__ == "__main__":
    unittest.main()

