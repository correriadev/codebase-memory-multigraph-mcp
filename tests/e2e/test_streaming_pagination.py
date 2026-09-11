"""
E2E Scenario: Streaming K-Way Merge Pagination and Node Shadowing.
Validates Task 06: Multi-step pagination (LIMIT/OFFSET) across Base and multiple Horizons
with deterministic key ordering, O(K) streaming merge, and node shadowing.
"""

import unittest
import json
from pathlib import Path

from tests.e2e.sandbox_environment import TestSandboxEnvironment
from tests.e2e.fixtures.base_seeder import seed_base_graph, seed_horizon_db
from tests.e2e.federated_engine import execute_federated_search


class TestStreamingPaginationE2E(unittest.TestCase):

    def test_kway_merge_pagination_and_node_shadowing(self):
        with TestSandboxEnvironment() as sandbox:
            project_name = sandbox.project_name

            # 1. Base Graph: 20 symbols (sym_00 to sym_19)
            base_nodes = []
            for i in range(20):
                key = f"pkg/math.sym_{i:02d}"
                base_nodes.append({
                    "name": f"sym_{i:02d}",
                    "qualified_name": key,
                    "label": "Function",
                    "properties": {"origin": "BASE"}
                })
            seed_base_graph(
                db_path=str(sandbox.base_db_path),
                project_name=project_name,
                root_path=str(sandbox.project_dir),
                nodes=base_nodes
            )

            # 2. Horizon 1: Shadows sym_05 and sym_06 with modified definitions
            h1_id = f"h1_shadow_{sandbox.run_id}"
            h1_db = sandbox.register_horizon(h1_id)
            seed_horizon_db(
                db_path=str(h1_db),
                horizon_id=h1_id,
                client_pid=3001,
                nodes=[
                    {
                        "cbm_uri": "pkg/math.sym_05",  # Colliding key -> shadows BASE
                        "label": "Function",
                        "status": "PROPOSED",
                        "properties": {"origin": "H1", "modified": True}
                    },
                    {
                        "cbm_uri": "pkg/math.sym_06",  # Colliding key -> shadows BASE
                        "label": "Function",
                        "status": "PROPOSED",
                        "properties": {"origin": "H1", "modified": True}
                    }
                ]
            )

            # 3. Horizon 2: Introduces 5 new symbols (sym_20 to sym_24)
            h2_id = f"h2_new_{sandbox.run_id}"
            h2_db = sandbox.register_horizon(h2_id)
            h2_nodes = []
            for i in range(20, 25):
                key = f"pkg/math.sym_{i:02d}"
                h2_nodes.append({
                    "cbm_uri": key,
                    "label": "Function",
                    "status": "PROPOSED",
                    "properties": {"origin": "H2"}
                })
            seed_horizon_db(
                db_path=str(h2_db),
                horizon_id=h2_id,
                client_pid=3002,
                nodes=h2_nodes
            )

            # 4. Multi-step pagination: fetch pages of size 5
            page_size = 5
            all_emitted_records = []
            seen_in_pages = []

            for page_idx in range(6):
                offset = page_idx * page_size
                page = execute_federated_search(
                    cache_dir=sandbox.cache_dir,
                    project_name=project_name,
                    pattern="pkg/math",
                    active_horizons=[h1_id, h2_id],
                    limit=page_size,
                    offset=offset
                )

                if page_idx < 5:
                    self.assertEqual(len(page), page_size, f"Page {page_idx} should have {page_size} items")
                else:
                    self.assertEqual(len(page), 0, "Page 5 should be empty after 25 total records")

                # Verify monotonic ordering within the page
                page_keys = [r["key"] for r in page]
                sorted_page_keys = sorted(page_keys)
                self.assertEqual(page_keys, sorted_page_keys, f"Page {page_idx} keys must be sorted")

                seen_in_pages.append(page_keys)
                all_emitted_records.extend(page)

            # 5. Assertions on concatenated result set
            self.assertEqual(len(all_emitted_records), 25, "Total emitted records must be exactly 25")

            # Check no duplicates across page boundaries
            all_keys = [r["key"] for r in all_emitted_records]
            unique_keys = set(all_keys)
            self.assertEqual(len(all_keys), len(unique_keys), "Must contain zero duplicate records across pages")

            # Verify Shadowing: sym_05 and sym_06 must have origin H1, NOT BASE
            rec_05 = next(r for r in all_emitted_records if r["key"] == "pkg/math.sym_05")
            rec_06 = next(r for r in all_emitted_records if r["key"] == "pkg/math.sym_06")
            self.assertEqual(rec_05["origin"], h1_id, "sym_05 must be shadowed by H1")
            self.assertEqual(rec_06["origin"], h1_id, "sym_06 must be shadowed by H1")
            props_05 = json.loads(rec_05["properties"]) if isinstance(rec_05["properties"], str) else rec_05["properties"]
            self.assertTrue(props_05.get("modified"), "sym_05 must carry H1 properties")

            # Verify Horizon 2 additions are included
            rec_24 = next(r for r in all_emitted_records if r["key"] == "pkg/math.sym_24")
            self.assertEqual(rec_24["origin"], h2_id, "sym_24 must come from H2")


if __name__ == "__main__":
    unittest.main()
