"""Two-process black-box test for thematic discovery and the HarnessKit UI horizon.

Run from the CBM repository root with CBM_CACHE_DIR pointing at a private CBM
cache snapshot and CBM_BINARY_PATH pointing at a freshly built CBM executable.
The writer seeds that snapshot from the MCP-exported UI horizon fixture; the
reader uses MCP only and never opens the scenario or thematic Markdown.
"""

import json
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT))

from tests.e2e.mcp_process_driver import MCPProcessSession  # noqa: E402


THEME_ID = "@harnesskit/ui-design-practices"
THEME_VERSION = "1.0.0"
THEME_PROJECT = "HarnessKit-UI-Design-Practices"
HARNESS_PROJECT = "C-Users-corre-Documents-harness-kit"
UI_HORIZON = "h_ui_visual_ideation_e2e_20261002_a1"
UI_SECTION_URI = (
    "cbm://C-Users-corre-Documents-harness-kit/"
    "docs/temenos/ui-e2e-20261002-a1/visual-ideation.md"
    "#03-visual-tokens-and-typography"
)
UI_ARTIFACT_URI = (
    "cbm://C-Users-corre-Documents-harness-kit/"
    "docs/temenos/ui-e2e-20261002-a1/visual-ideation.md"
)
UI_FIXTURE_PATH = Path(__file__).with_name("ui-horizon-fixture.json")


def payload(session, name, arguments, allow_tool_error=False):
    response = session.call_tool(name, arguments, timeout=60.0)
    if response.error:
        raise AssertionError(f"{name} JSON-RPC error: {response.error}")
    result = response.result or {}
    structured = result.get("structuredContent")
    if structured is not None:
        value = structured
    else:
        text_blocks = [
            block.get("text", "")
            for block in result.get("content", [])
            if block.get("type") == "text"
        ]
        if not text_blocks:
            raise AssertionError(f"{name} returned no text or structured content: {result}")
        try:
            value = json.loads(text_blocks[0])
        except json.JSONDecodeError:
            value = {"text": text_blocks[0]}
    if result.get("isError") or value.get("isError"):
        if allow_tool_error:
            return value
        raise AssertionError(f"{name} tool error: {value}")
    return value


def require_tools(session):
    response = session.list_tools(timeout=30.0)
    if not response.is_success():
        raise AssertionError(f"tools/list failed: {response.error}")
    names = {tool["name"] for tool in response.result.get("tools", [])}
    expected = {
        "theme_search",
        "theme_list",
        "theme_lookup",
        "theme_graph_search",
        "theme_graph_query",
        "index_repository",
        "get_graph_schema",
        "validate_provenance",
        "create_horizon",
        "search_graph",
        "query_graph",
    }
    missing = expected - names
    if missing:
        raise AssertionError(f"MCP server is missing expected tools: {sorted(missing)}")


def main():
    binary = os.environ.get("CBM_BINARY_PATH")
    if not binary:
        raise SystemExit("Set CBM_BINARY_PATH to the freshly built MCP executable.")

    catalog_entry = {
        "theme_id": THEME_ID,
        "namespace": "product-design/interface",
        "curator": "codex-e2e",
        "version": THEME_VERSION,
        "name": "HarnessKit UI Design Practices",
        "target_uri": f"cbm-project://{THEME_PROJECT}",
        "description": (
            "Curated design guidance for spacing, typography, hierarchy, components, "
            "color, responsive behavior, motion, and accessibility. The HarnessKit "
            "mapping is a synthetic test fixture, not approved product design."
        ),
        "aliases": "UI design; design system; conversational workspace; assistant UI",
        "tags": (
            "spacing layout typography hierarchy components color contrast responsive "
            "motion accessibility conversation workspace"
        ),
        "founding_provenance": "Codex E2E curation; W3C accessibility sources",
        "status": "ACTIVE",
    }

    with MCPProcessSession(binary_path=binary) as writer:
        if not writer.initialize(timeout=30.0).is_success():
            raise AssertionError("writer MCP initialization failed")
        require_tools(writer)

        indexed_theme = payload(
            writer,
            "index_repository",
            {
                "repo_path": str(REPO_ROOT / "themes" / "harnesskit-ui-design"),
                "name": THEME_PROJECT,
                "mode": "full",
                "persistence": False,
            },
        )

        ui_fixture = json.loads(UI_FIXTURE_PATH.read_text(encoding="utf-8"))
        seeded = payload(
            writer,
            "create_horizon",
            {
                "horizon_id": ui_fixture["horizon_id"],
                "project": ui_fixture["project"],
                "nodes": ui_fixture["nodes"],
                "edges": ui_fixture["edges"],
            },
        )
        if seeded.get("horizon_id") != UI_HORIZON:
            raise AssertionError(f"MCP-exported UI horizon fixture was not seeded: {seeded}")

        registered = payload(writer, "theme_register", catalog_entry)
        if registered.get("theme_id") != THEME_ID or registered.get("version") != THEME_VERSION:
            raise AssertionError(f"unexpected registration result: {registered}")

        theme_schema = payload(
            writer,
            "get_graph_schema",
            {"project": THEME_PROJECT, "format": "json", "diagnostics": "full"},
        )

        relation_query = payload(
            writer,
            "theme_graph_query",
            {
                "theme_id": THEME_ID,
                "version": THEME_VERSION,
                "query": (
                    "MATCH (s:Section)-[:SUPPORTS]->(t:Section) "
                    "WHERE s.name = 'T-02 Spacing, alignment, and layout rhythm' "
                    "RETURN t.name AS target LIMIT 10"
                ),
                "max_rows": 10,
            },
        )
        relation_rows = relation_query.get("graph_result", {}).get("rows", [])
        if not any(
            row and row[0] == "T-03 Typography and visual hierarchy" for row in relation_rows
        ):
            raise AssertionError(
                "typed topic relation was not traversable after indexing: "
                f"index={indexed_theme}; schema={theme_schema}; query={relation_query}"
            )

        initial_graph_search = payload(
            writer,
            "theme_graph_search",
            {
                "theme_id": THEME_ID,
                "version": THEME_VERSION,
                "query": "spacing alignment layout rhythm",
                "limit": 30,
            },
        )
        candidates = initial_graph_search.get("citation_candidates", [])
        spacing = next(
            (item for item in candidates if "T-02-Spacing" in item.get("qualified_name", "")),
            None,
        )
        if not spacing:
            raise AssertionError(f"spacing section was not citable: {initial_graph_search}")

        supporting = payload(
            writer,
            "theme_graph_search",
            {
                "theme_id": THEME_ID,
                "version": THEME_VERSION,
                "query": "components color responsive motion accessibility",
                "limit": 50,
            },
        ).get("citation_candidates", [])
        supporting_uris = sorted({item["node_uri"] for item in supporting if item.get("node_uri")})
        reference_uri = f"{UI_ARTIFACT_URI}#theme-ref-harnesskit-ui-design-practices-v1-0-0"
        reference_text = (
            f"theme_id={THEME_ID}; pinned_version={THEME_VERSION}; "
            f"target_uri=cbm-project://{THEME_PROJECT}; "
            f"entry_node_uri={spacing['node_uri']}; "
            f"supporting_node_uris={json.dumps(supporting_uris)}; "
            "relation=INFORMED_BY; epistemic_status=PROPOSED; product_approval=NONE"
        )
        overlay = payload(
            writer,
            "create_horizon",
            {
                "horizon_id": UI_HORIZON,
                "project": HARNESS_PROJECT,
                "nodes": [
                    {
                        "cbm_uri": reference_uri,
                        "label": f"FractalThemeReference: {THEME_ID}@{THEME_VERSION}",
                        "epistemic_status": "PROPOSED",
                        "is_dangling": False,
                        "code_snippet": reference_text,
                    }
                ],
                "edges": [
                    {
                        "source_uri": UI_SECTION_URI,
                        "target_uri": reference_uri,
                        "edge_type": "INFORMED_BY",
                    }
                ],
            },
        )
        if overlay.get("horizon_id") != UI_HORIZON:
            raise AssertionError(f"theme reference was not added to UI horizon: {overlay}")

    # A new MCP process models a new agent session. All reads below use CBM tools.
    with MCPProcessSession(binary_path=binary) as reader:
        if not reader.initialize(timeout=30.0).is_success():
            raise AssertionError("reader MCP initialization failed")
        require_tools(reader)

        discovery = payload(
            reader,
            "theme_search",
            {"query": "HarnessKit spacing motion accessibility", "namespace": "product-design/interface"},
        )
        results = discovery.get("results", [])
        if not any(item.get("theme_id") == THEME_ID for item in results):
            raise AssertionError(f"theme was not discovered after writer process closed: {discovery}")

        listed = payload(
            reader,
            "theme_list",
            {"namespace": "product-design/interface", "status": "ACTIVE"},
        )
        if not any(item.get("theme_id") == THEME_ID for item in listed.get("results", [])):
            raise AssertionError(f"theme_list missed the registered active theme: {listed}")

        lookup = payload(
            reader,
            "theme_lookup",
            {"theme_id": THEME_ID, "version": THEME_VERSION},
        )
        if lookup.get("target_uri") != f"cbm-project://{THEME_PROJECT}":
            raise AssertionError(f"pinned lookup lost its graph target: {lookup}")

        graph_search = payload(
            reader,
            "theme_graph_search",
            {
                "theme_id": THEME_ID,
                "version": THEME_VERSION,
                "query": "spacing alignment layout rhythm narrow viewport exceptions",
                "limit": 30,
            },
        )
        citations = graph_search.get("citation_candidates", [])
        spacing = next(
            (item for item in citations if "T-02-Spacing" in item.get("qualified_name", "")),
            None,
        )
        if not spacing:
            raise AssertionError(f"graph search did not recover the spacing citation: {graph_search}")

        traversed = payload(
            reader,
            "theme_graph_query",
            {
                "theme_id": THEME_ID,
                "version": THEME_VERSION,
                "query": (
                    "MATCH (f:File)-[:DEFINES]->(s:Section) "
                    "WHERE f.name = 'README.md' "
                    "RETURN s.name AS topic, s.qualified_name AS qn, s.docstring AS guidance LIMIT 30"
                ),
                "max_rows": 30,
            },
        )
        graph_result = traversed.get("graph_result", {})
        rows = graph_result.get("rows", []) if isinstance(graph_result, dict) else []
        topic_names = {row[0] for row in rows if row and isinstance(row[0], str)}
        expected_topics = {f"T-{i:02d}" for i in range(1, 11)}
        traversed_ids = {
            f"T-{i:02d}"
            for i in range(1, 11)
            if any(name.startswith(f"T-{i:02d} ") for name in topic_names)
        }
        if len(rows) < 10 or traversed_ids != expected_topics:
            raise AssertionError(f"theme graph traversal did not recover all topic nodes: {traversed}")

        recovered_topic_relation = payload(
            reader,
            "theme_graph_query",
            {
                "theme_id": THEME_ID,
                "version": THEME_VERSION,
                "query": (
                    "MATCH (s:Section)-[:SUPPORTS]->(t:Section) "
                    "WHERE s.name = 'T-02 Spacing, alignment, and layout rhythm' "
                    "RETURN t.name AS target LIMIT 10"
                ),
                "max_rows": 10,
            },
        )
        relation_rows = recovered_topic_relation.get("graph_result", {}).get("rows", [])
        if not any(
            row and row[0] == "T-03 Typography and visual hierarchy" for row in relation_rows
        ):
            raise AssertionError(
                "fresh reader could not traverse the persisted topic relation: "
                f"{recovered_topic_relation}"
            )

        citation_check = payload(
            reader,
            "validate_provenance",
            {
                "theme_id": THEME_ID,
                "pinned_version": THEME_VERSION,
                "node_uri": spacing["node_uri"],
            },
        )
        if not citation_check.get("anchor_verified"):
            raise AssertionError(f"exact citation was not verified: {citation_check}")

        wrong_anchor = payload(
            reader,
            "validate_provenance",
            {
                "theme_id": THEME_ID,
                "pinned_version": THEME_VERSION,
                "node_uri": f"cbm://{THEME_PROJECT}/README.md#missing-node",
            },
            allow_tool_error=True,
        )
        if wrong_anchor.get("code") != "ANCHOR_NOT_FOUND":
            raise AssertionError(f"unknown citation did not fail closed: {wrong_anchor}")

        recovered_reference = payload(
            reader,
            "search_graph",
            {
                "project": HARNESS_PROJECT,
                "active_horizons": [UI_HORIZON],
                "query": THEME_ID,
                "format": "json",
                "limit": 100,
            },
        )
        reference_overlays = recovered_reference.get("active_horizon_overlays", [])
        ref_overlay = next((item for item in reference_overlays if item.get("uri") == reference_uri), None)
        ref_payload = ref_overlay.get("payload", "") if ref_overlay else ""
        if not ref_payload or f"pinned_version={THEME_VERSION}" not in ref_payload:
            raise AssertionError(f"fresh session could not recover the thematic reference: {recovered_reference}")

        recovered_ui = payload(
            reader,
            "search_graph",
            {
                "project": HARNESS_PROJECT,
                "active_horizons": [UI_HORIZON],
                "query": "assistant workspace left rail context drawer composer typography",
                "format": "json",
                "limit": 5,
            },
        )
        ui_overlays = recovered_ui.get("active_horizon_overlays", [])
        ui_payloads = [item.get("payload", "") for item in ui_overlays]
        if not any("context drawer 320" in value.lower() for value in ui_payloads):
            raise AssertionError("fresh session did not recover the existing UI fixture from the horizon")

        horizon_graph = payload(
            reader,
            "query_graph",
            {
                "project": HARNESS_PROJECT,
                "active_horizons": [UI_HORIZON],
                "query": "MATCH (n) RETURN n.name LIMIT 1",
                "format": "json",
                "max_rows": 1,
            },
        )
        horizon_nodes = horizon_graph.get("horizon_nodes", [])
        horizon_edges = horizon_graph.get("horizon_edges", [])
        if not any(node.get("uri") == reference_uri for node in horizon_nodes):
            raise AssertionError(f"theme reference node missing from horizon graph: {horizon_graph}")
        if not any(
            edge.get("source") == UI_SECTION_URI
            and edge.get("target") == reference_uri
            and edge.get("type") == "INFORMED_BY"
            for edge in horizon_edges
        ):
            raise AssertionError(f"typed UI-to-theme relation missing from horizon graph: {horizon_graph}")

        print(
            json.dumps(
                {
                    "result": "PASS",
                    "writer_and_reader_processes": 2,
                    "mcp_exported_ui_fixture_seeded": True,
                    "theme_id": THEME_ID,
                    "version": THEME_VERSION,
                    "theme_project": THEME_PROJECT,
                    "theme_indexed_in_writer": True,
                    "topic_nodes_traversed": len(rows),
                    "topic_relation_recovered": "T-02 -[SUPPORTS]-> T-03",
                    "declared_topic_set": sorted(expected_topics),
                    "exact_citation_verified": spacing["node_uri"],
                    "unknown_citation_rejected": wrong_anchor.get("code"),
                    "ui_horizon": UI_HORIZON,
                    "reference_recovered_from_horizon": reference_uri,
                    "horizon_relation_recovered": "INFORMED_BY",
                    "fixture_recovered_without_source_file_read": True,
                },
                indent=2,
            )
        )


if __name__ == "__main__":
    main()
