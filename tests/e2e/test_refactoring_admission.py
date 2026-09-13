"""
E2E Scenario: Physical Refactoring and Two-Tier Anchor Admission Gate.
Validates Task 04: Real MCP server communication over stdio, verifying Two-Tier anchors,
tolerance for benign comment shifts vs rejection on breaking AST drift, and Base Graph consolidation.
"""

import unittest
import sqlite3
import json
from pathlib import Path

from tests.e2e.sandbox_environment import TestSandboxEnvironment
from tests.e2e.fixtures.harness_specimen import HarnessKitSpecimen
from tests.e2e.fixtures.base_seeder import seed_base_graph, seed_horizon_db
from tests.e2e.mcp_process_driver import MCPProcessSession


class TestRefactoringAdmissionE2E(unittest.TestCase):

    def test_real_mcp_two_tier_anchor_verification_and_refactoring(self):
        """
        Validates Two-Tier anchor verification directly through the compiled codebase-memory-mcp binary:
        1. Exact match (Tier 1 fast-path) -> PROMOTED
        2. Benign comment shift (Tier 2 AST fallback) -> PROMOTED
        3. Breaking mutation -> ANCHOR_DRIFT rejection
        """
        with TestSandboxEnvironment() as sandbox:
            specimen = HarnessKitSpecimen(sandbox.project_dir)
            byte_start, byte_len, ast_hash = specimen.initial_anchor

            with MCPProcessSession(env=sandbox.env_vars, cwd=str(sandbox.project_dir)) as session:
                init_resp = session.initialize(timeout=15.0)
                self.assertTrue(init_resp.is_success(), f"Init failed: {init_resp.error}")

                anchor_payload = {
                    "file_path": specimen.rel_path,
                    "symbol_name": "IAgentRunner",
                    "expected_text": specimen.target_symbol,
                    "byte_start": byte_start,
                    "byte_len": byte_len,
                    "ast_signature_hash": ast_hash
                }

                # 1. Unchanged File: Tier 1 Fast Match -> PROMOTED
                resp1 = session.call_tool(
                    name="promote_horizon",
                    arguments={
                        "horizon_id": f"feat_runner_t1_{sandbox.run_id}",
                        "repo_path": str(sandbox.project_dir),
                        "anchors": [anchor_payload]
                    },
                    timeout=15.0
                )
                self.assertTrue(resp1.is_success(), f"Promotion 1 failed: {resp1.error or resp1.result}")
                res1_text = str(resp1.result)
                self.assertIn("PROMOTED", res1_text)

                # 2. Benign Shift: 15 comment lines added to top of file
                specimen.apply_benign_comment_shift(comment_lines=15)
                # Keep original byte_start to force Tier 1 miss and Tier 2 AST fallback
                resp2 = session.call_tool(
                    name="promote_horizon",
                    arguments={
                        "horizon_id": f"feat_runner_t2_{sandbox.run_id}",
                        "repo_path": str(sandbox.project_dir),
                        "anchors": [anchor_payload]
                    },
                    timeout=15.0
                )
                self.assertTrue(resp2.is_success(), f"Promotion 2 failed: {resp2.error or resp2.result}")
                res2_text = str(resp2.result)
                self.assertIn("PROMOTED", res2_text)

                # 3. Breaking Mutation: Function / interface altered
                specimen.apply_breaking_mutation()
                resp3 = session.call_tool(
                    name="promote_horizon",
                    arguments={
                        "horizon_id": f"feat_runner_t3_{sandbox.run_id}",
                        "repo_path": str(sandbox.project_dir),
                        "anchors": [anchor_payload]
                    },
                    timeout=15.0
                )
                res3_text = str(resp3.result) if resp3.result else str(resp3.error)
                self.assertIn("ANCHOR_DRIFT", res3_text)

    def test_real_mcp_path_resolution_variants(self):
        """
        Validates path resolution under real-world multi-OS conventions:
        - Relative path with repo_path
        - Absolute path directly in file_path
        """
        with TestSandboxEnvironment() as sandbox:
            specimen = HarnessKitSpecimen(sandbox.project_dir)

            with MCPProcessSession(env=sandbox.env_vars, cwd=str(sandbox.project_dir)) as session:
                init_resp = session.initialize(timeout=15.0)
                self.assertTrue(init_resp.is_success())

                # Variant A: Relative path + repo_path
                resp_a = session.call_tool(
                    name="promote_horizon",
                    arguments={
                        "horizon_id": f"h_path_a_{sandbox.run_id}",
                        "repo_path": str(sandbox.project_dir),
                        "anchors": [{
                            "file_path": specimen.rel_path,
                            "symbol_name": "IAgentRunner",
                            "expected_text": specimen.target_symbol
                        }]
                    }
                )
                self.assertTrue(resp_a.is_success())
                self.assertIn("PROMOTED", str(resp_a.result))

                # Variant B: Absolute path directly in file_path
                resp_b = session.call_tool(
                    name="promote_horizon",
                    arguments={
                        "horizon_id": f"h_path_b_{sandbox.run_id}",
                        "anchors": [{
                            "file_path": str(specimen.file_path),
                            "symbol_name": "IAgentRunner",
                            "expected_text": specimen.target_symbol
                        }]
                    }
                )
                self.assertTrue(resp_b.is_success())
                self.assertIn("PROMOTED", str(resp_b.result))

    def test_real_mcp_base_graph_consolidation(self):
        """
        Validates that upon promotion via real MCP tool call, symbolic nodes
        and virtual edges are consolidated into the Base Graph database.
        """
        with TestSandboxEnvironment() as sandbox:
            specimen = HarnessKitSpecimen(sandbox.project_dir)
            project_name = sandbox.project_name

            # Seed Base Graph with IAgentRunner
            seed_base_graph(
                db_path=str(sandbox.base_db_path),
                project_name=project_name,
                root_path=str(sandbox.project_dir),
                nodes=[
                    {
                        "name": "IAgentRunner",
                        "qualified_name": "sdk/src/agent-runner/IAgentRunner.ts#IAgentRunner",
                        "label": "Interface",
                        "file_path": "sdk/src/agent-runner/IAgentRunner.ts",
                        "start_line": 3,
                        "end_line": 7
                    }
                ]
            )

            # Seed Horizon proposing OpenCodeSDKRunner with virtual edge to IAgentRunner
            h_id = f"horizon_opencode_{sandbox.run_id}"
            h_db = sandbox.register_horizon(h_id)
            seed_horizon_db(
                db_path=str(h_db),
                horizon_id=h_id,
                client_pid=4001,
                nodes=[
                    {
                        "cbm_uri": f"cbm://{project_name}/sdk/src/agent-runner/opencode-sdk/OpenCodeSDKRunner.ts#OpenCodeSDKRunner",
                        "label": "Class",
                        "status": "PROPOSED",
                        "is_dangling": False,
                        "code_snippet": "export class OpenCodeSDKRunner implements IAgentRunner {}"
                    }
                ],
                virtual_edges=[
                    {
                        "source_uri": f"cbm://{project_name}/sdk/src/agent-runner/opencode-sdk/OpenCodeSDKRunner.ts#OpenCodeSDKRunner",
                        "target_uri": f"cbm://{project_name}/sdk/src/agent-runner/IAgentRunner.ts#IAgentRunner",
                        "edge_type": "IMPLEMENTS"
                    }
                ]
            )

            with MCPProcessSession(env=sandbox.env_vars, cwd=str(sandbox.project_dir)) as session:
                session.initialize(timeout=15.0)

                # Promote horizon via real MCP JSON-RPC
                prom_resp = session.call_tool(
                    name="promote_horizon",
                    arguments={
                        "horizon_id": h_id,
                        "project": project_name,
                        "repo_path": str(sandbox.project_dir),
                        "anchors": [{
                            "file_path": specimen.rel_path,
                            "symbol_name": "IAgentRunner",
                            "expected_text": specimen.target_symbol
                        }]
                    }
                )
                self.assertTrue(prom_resp.is_success(), f"Promotion failed: {prom_resp.error or prom_resp.result}")
                self.assertIn("PROMOTED", str(prom_resp.result))

            # Verify consolidated nodes in Base Graph
            b_conn = sqlite3.connect(sandbox.base_db_path)
            cur = b_conn.cursor()
            cur.execute("SELECT name FROM nodes WHERE project = ?", (project_name,))
            node_names = [r[0] for r in cur.fetchall()]
            self.assertIn("IAgentRunner", node_names)
            self.assertIn("OpenCodeSDKRunner", node_names)

            # Verify consolidated virtual edges in Base Graph
            cur.execute("SELECT source_uri, target_uri, edge_type FROM virtual_edges WHERE origin_horizon = ?", (h_id,))
            edge_rows = cur.fetchall()
            b_conn.close()
            self.assertEqual(len(edge_rows), 1)
            self.assertEqual(edge_rows[0][2], "IMPLEMENTS")


if __name__ == "__main__":
    unittest.main()
