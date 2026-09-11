"""
E2E Scenario: Physical Refactoring and Two-Tier Anchor Admission Gate.
Validates Task 04: Tolerance for benign comment/header shifts vs rejection on breaking AST drift.
"""

import unittest
import sqlite3
from pathlib import Path

from tests.e2e.sandbox_environment import TestSandboxEnvironment
from tests.e2e.fixtures.specimens import TwoTierRefactorSpecimen
from tests.e2e.fixtures.base_seeder import seed_base_graph, seed_horizon_db
from tests.e2e.admission_verifier import (
    verify_two_tier_anchor,
    execute_admission_promotion,
    AdmissionStatus
)


class TestRefactoringAdmissionE2E(unittest.TestCase):

    def test_two_tier_anchor_verification_and_refactoring(self):
        with TestSandboxEnvironment() as sandbox:
            project_name = sandbox.project_name
            specimen = TwoTierRefactorSpecimen(sandbox.project_dir)

            byte_start, byte_len, ast_hash = specimen.initial_anchor
            expected_text = specimen.target_symbol

            # 1. Unchanged File: Tier 1 Fast Match
            ok, tier = verify_two_tier_anchor(
                file_path=specimen.file_path,
                byte_start=byte_start,
                byte_len=byte_len,
                expected_text=expected_text,
                ast_signature_hash=ast_hash
            )
            self.assertTrue(ok)
            self.assertEqual(tier, "TIER_1_OFFSET_MATCH")

            # 2. Benign Shift: 15 comment lines added to top of file
            shift_bytes = specimen.apply_benign_comment_shift(comment_lines=15)
            self.assertGreater(shift_bytes, 0)

            # Verification should pass via Tier 2 (AST match despite offset shift)
            ok, tier = verify_two_tier_anchor(
                file_path=specimen.file_path,
                byte_start=byte_start,  # Old offset!
                byte_len=byte_len,
                expected_text=expected_text,
                ast_signature_hash=ast_hash
            )
            self.assertTrue(ok, "Benign comment shift should be admitted")
            self.assertEqual(tier, "TIER_2_AST_MATCH")

            # 3. Breaking Mutation: Function signature changed to double add(...)
            specimen.apply_breaking_mutation()

            ok, status = verify_two_tier_anchor(
                file_path=specimen.file_path,
                byte_start=byte_start,
                byte_len=byte_len,
                expected_text=expected_text,
                ast_signature_hash=ast_hash
            )
            self.assertFalse(ok, "Breaking AST drift should be rejected")
            self.assertEqual(status, AdmissionStatus.ANCHOR_DRIFT)

    def test_admission_promotion_consolidation_and_drift_rejection(self):
        with TestSandboxEnvironment() as sandbox:
            project_name = sandbox.project_name
            specimen = TwoTierRefactorSpecimen(sandbox.project_dir)
            byte_start, byte_len, ast_hash = specimen.initial_anchor

            # Seed Base Graph
            seed_base_graph(
                db_path=str(sandbox.base_db_path),
                project_name=project_name,
                root_path=str(sandbox.project_dir),
                nodes=[
                    {
                        "name": "add",
                        "qualified_name": "src/calc.c#add",
                        "label": "Function",
                        "file_path": "src/calc.c",
                        "start_line": 3,
                        "end_line": 5
                    }
                ]
            )

            # Seed Horizon proposing Multiply function
            h_id = f"horizon_calc_{sandbox.run_id}"
            h_db = sandbox.register_horizon(h_id)
            seed_horizon_db(
                db_path=str(h_db),
                horizon_id=h_id,
                client_pid=2001,
                nodes=[
                    {
                        "cbm_uri": f"cbm://{project_name}/src/calc.c#Multiply",
                        "label": "Function",
                        "status": "PROPOSED",
                        "is_dangling": False,
                        "properties": {"derived_from": "add"}
                    }
                ]
            )

            anchors = [
                {
                    "file_path": str(specimen.file_path),
                    "byte_start": byte_start,
                    "byte_len": byte_len,
                    "expected_text": specimen.target_symbol,
                    "ast_signature_hash": ast_hash
                }
            ]

            # Case A: Benign Shift Promotion -> Admitted and Consolidated
            specimen.apply_benign_comment_shift(10)
            status, err = execute_admission_promotion(
                base_db_path=sandbox.base_db_path,
                horizon_db_path=h_db,
                project_name=project_name,
                anchors=anchors
            )
            self.assertEqual(status, AdmissionStatus.ADMITTED)
            self.assertIsNone(err)

            # Assert symbol consolidated into Base Graph
            b_conn = sqlite3.connect(sandbox.base_db_path)
            cur = b_conn.cursor()
            cur.execute("SELECT name, qualified_name FROM nodes WHERE project = ?", (project_name,))
            rows = cur.fetchall()
            b_conn.close()
            node_names = [r[0] for r in rows]
            self.assertIn("add", node_names)
            self.assertIn("Multiply", node_names)

            # Assert Horizon status updated to PROMOTED
            h_conn = sqlite3.connect(h_db)
            cur = h_conn.cursor()
            cur.execute("SELECT status FROM horizon_metadata WHERE horizon_id = ?", (h_id,))
            h_status = cur.fetchone()[0]
            h_conn.close()
            self.assertEqual(h_status, "PROMOTED")

            # Case B: Breaking Mutation -> Rejected with ANCHOR_DRIFT
            specimen.apply_breaking_mutation()
            # New horizon proposing another symbol
            h2_id = f"horizon_calc2_{sandbox.run_id}"
            h2_db = sandbox.register_horizon(h2_id)
            seed_horizon_db(
                db_path=str(h2_db),
                horizon_id=h2_id,
                client_pid=2002,
                nodes=[
                    {
                        "cbm_uri": f"cbm://{project_name}/src/calc.c#Divide",
                        "label": "Function",
                        "status": "PROPOSED"
                    }
                ]
            )

            status2, err2 = execute_admission_promotion(
                base_db_path=sandbox.base_db_path,
                horizon_db_path=h2_db,
                project_name=project_name,
                anchors=anchors
            )
            self.assertEqual(status2, AdmissionStatus.ANCHOR_DRIFT)
            self.assertIn("ANCHOR_DRIFT", err2)

            # Assert Divide was NOT added to Base Graph
            b_conn = sqlite3.connect(sandbox.base_db_path)
            cur = b_conn.cursor()
            cur.execute("SELECT name FROM nodes WHERE project = ? AND name = 'Divide'", (project_name,))
            self.assertIsNone(cur.fetchone())
            b_conn.close()


if __name__ == "__main__":
    unittest.main()
