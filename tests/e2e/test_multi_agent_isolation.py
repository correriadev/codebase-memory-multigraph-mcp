"""
E2E Scenario: Multi-Agent Horizon Isolation and Overlay Separation.
Validates Task 03: Two virtual agents working concurrently with private horizons.
"""

import unittest
from tests.e2e.sandbox_environment import TestSandboxEnvironment
from tests.e2e.fixtures.base_seeder import seed_base_graph, seed_horizon_db
from tests.e2e.mcp_process_driver import MCPProcessSession
from tests.e2e.federated_engine import execute_federated_search


class TestMultiAgentIsolationE2E(unittest.TestCase):

    def test_multi_agent_horizon_isolation(self):
        with TestSandboxEnvironment() as sandbox:
            project_name = sandbox.project_name

            # 1. Seed Base Graph with OrderHandler
            seed_base_graph(
                db_path=str(sandbox.base_db_path),
                project_name=project_name,
                root_path=str(sandbox.project_dir),
                nodes=[
                    {
                        "name": "OrderHandler",
                        "qualified_name": "pkg/orders.OrderHandler",
                        "label": "Function",
                        "file_path": "pkg/orders.py",
                        "start_line": 10,
                        "end_line": 25
                    }
                ]
            )

            # 2. Seed Horizon A (Agent A proposing NewFeatureService)
            h_a_id = f"horizon_a_{sandbox.run_id}"
            h_a_db = sandbox.register_horizon(h_a_id)
            seed_horizon_db(
                db_path=str(h_a_db),
                horizon_id=h_a_id,
                client_pid=1001,
                nodes=[
                    {
                        "cbm_uri": f"cbm://{project_name}/pkg/orders.py#NewFeatureService",
                        "label": "Function",
                        "status": "PROPOSED",
                        "is_dangling": False,
                        "properties": {"author": "Agent_A"}
                    }
                ]
            )

            # 3. Seed Horizon B (Agent B proposing AlternativeService)
            h_b_id = f"horizon_b_{sandbox.run_id}"
            h_b_db = sandbox.register_horizon(h_b_id)
            seed_horizon_db(
                db_path=str(h_b_db),
                horizon_id=h_b_id,
                client_pid=1002,
                nodes=[
                    {
                        "cbm_uri": f"cbm://{project_name}/pkg/orders.py#AlternativeService",
                        "label": "Function",
                        "status": "PROPOSED",
                        "is_dangling": False,
                        "properties": {"author": "Agent_B"}
                    }
                ]
            )

            # 4. Launch real MCP Server via Subprocess and verify Protocol Compliance
            with MCPProcessSession(env=sandbox.env_vars) as session:
                init_resp = session.initialize(timeout=5.0)
                self.assertTrue(init_resp.is_success(), f"Init error: {init_resp.error}")

                tools_resp = session.list_tools(timeout=5.0)
                self.assertTrue(tools_resp.is_success(), f"Tools error: {tools_resp.error}")

                # Base query over real MCP stdio
                base_query = session.call_tool(
                    name="search_graph",
                    arguments={"project": project_name, "query": "OrderHandler"}
                )
                self.assertTrue(base_query.is_success(), f"Base query error: {base_query.error}")
                self.assertIn("OrderHandler", str(base_query.result))

            # 5. Assert Multi-Agent Isolation Semantics
            # Base only: only OrderHandler
            res_base = execute_federated_search(
                cache_dir=sandbox.cache_dir,
                project_name=project_name,
                pattern="Service",
                active_horizons=[]
            )
            self.assertEqual(len(res_base), 0, "Base alone should contain zero Service symbols")

            # Agent A query with active_horizons: [h_a_id]
            res_a = execute_federated_search(
                cache_dir=sandbox.cache_dir,
                project_name=project_name,
                pattern="",
                active_horizons=[h_a_id]
            )
            keys_a = [r["key"] for r in res_a]
            self.assertIn("pkg/orders.OrderHandler", keys_a)
            self.assertIn(f"cbm://{project_name}/pkg/orders.py#NewFeatureService", keys_a)
            self.assertNotIn(f"cbm://{project_name}/pkg/orders.py#AlternativeService", keys_a)

            # Agent B query with active_horizons: [h_b_id]
            res_b = execute_federated_search(
                cache_dir=sandbox.cache_dir,
                project_name=project_name,
                pattern="",
                active_horizons=[h_b_id]
            )
            keys_b = [r["key"] for r in res_b]
            self.assertIn("pkg/orders.OrderHandler", keys_b)
            self.assertIn(f"cbm://{project_name}/pkg/orders.py#AlternativeService", keys_b)
            self.assertNotIn(f"cbm://{project_name}/pkg/orders.py#NewFeatureService", keys_b)


if __name__ == "__main__":
    unittest.main()
