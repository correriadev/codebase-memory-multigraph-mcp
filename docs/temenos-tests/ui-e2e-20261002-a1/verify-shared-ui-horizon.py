"""Read-only fresh-process recovery check against the shared CBM cache.

This reads theme and UI horizon data through MCP only. It never opens the
HarnessKit scenario Markdown or writes horizon nodes/edges.
"""

import argparse
import json
import os
import subprocess
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
REFERENCE_URI = (
    "cbm://C-Users-corre-Documents-harness-kit/"
    "docs/temenos/ui-e2e-20261002-a1/visual-ideation.md"
    "#theme-ref-harnesskit-ui-design-practices-v1-0-0"
)
EXPECTED_TOOLS = {
    "theme_search",
    "theme_list",
    "theme_lookup",
    "theme_graph_search",
    "theme_graph_query",
    "index_repository",
    "get_graph_schema",
    "validate_provenance",
    "search_graph",
    "query_graph",
}
TOPIC_RELATION_TYPES = {
    "APPLIES_TO",
    "CONSTRAINS",
    "MOTION_SPECIFIED_BY",
    "SPECIALIZES",
    "SUPPORTED_BY",
    "SUPPORTS",
    "USES",
    "VERIFIED_WITH",
}


def payload(session, name, arguments, allow_tool_error=False):
    response = session.call_tool(name, arguments, timeout=90.0)
    if response.error:
        raise AssertionError(f"{name} JSON-RPC error: {response.error}")
    result = response.result or {}
    structured = result.get("structuredContent")
    if structured is not None:
        value = structured
    else:
        blocks = [
            block.get("text", "")
            for block in result.get("content", [])
            if block.get("type") == "text"
        ]
        if not blocks:
            raise AssertionError(f"{name} returned no result: {result}")
        value = json.loads(blocks[0])
    if result.get("isError") or value.get("isError"):
        if allow_tool_error:
            return value
        raise AssertionError(f"{name} tool error: {value}")
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", help="Updated codebase-memory-mcp.exe")
    parser.add_argument(
        "--cache",
        default=str(Path.home() / ".cache" / "codebase-memory-mcp"),
        help="Shared CBM cache (default: current user's cache)",
    )
    args = parser.parse_args()
    env = {"CBM_CACHE_DIR": args.cache}

    try:
        with MCPProcessSession(
            binary_path=args.binary,
            env=env,
            cwd=str(REPO_ROOT),
        ) as session:
            if not session.initialize(timeout=120.0).is_success():
                raise AssertionError("fresh MCP initialization failed")
            listed_tools = session.list_tools(timeout=30.0)
            if not listed_tools.is_success():
                raise AssertionError(f"tools/list failed: {listed_tools.error}")
            tool_names = {item["name"] for item in listed_tools.result.get("tools", [])}
            if missing := EXPECTED_TOOLS - tool_names:
                raise AssertionError(f"new toolset is incomplete: {sorted(missing)}")

            discovery = payload(
                session,
                "theme_search",
                {"query": "HarnessKit spacing motion accessibility", "namespace": "product-design/interface"},
            )
            if not any(item.get("theme_id") == THEME_ID for item in discovery.get("results", [])):
                raise AssertionError(f"thematic discovery failed: {discovery}")

            theme = payload(session, "theme_lookup", {"theme_id": THEME_ID})
            if theme.get("version") != THEME_VERSION or theme.get("status") != "ACTIVE":
                raise AssertionError(f"the registered theme pin was not recovered: {theme}")

            schema = payload(
                session,
                "get_graph_schema",
                {"project": THEME_PROJECT, "format": "json", "diagnostics": "full"},
            )
            edge_counts = {edge["type"]: edge["count"] for edge in schema.get("edge_types", [])}
            typed_relation_count = sum(edge_counts.get(kind, 0) for kind in TOPIC_RELATION_TYPES)
            if typed_relation_count != 32:
                raise AssertionError(
                    f"shared theme graph should have 32 typed topic links, got {edge_counts}"
                )

            citation_result = payload(
                session,
                "theme_graph_search",
                {
                    "theme_id": THEME_ID,
                    "version": THEME_VERSION,
                    "query": "spacing alignment layout rhythm narrow viewport exceptions",
                    "limit": 30,
                },
            )
            citations = citation_result.get("citation_candidates", [])
            spacing = next(
                (item for item in citations if "T-02-Spacing" in item.get("qualified_name", "")),
                None,
            )
            if not spacing:
                raise AssertionError(f"spacing citation not recovered: {citation_result}")

            relation_result = payload(
                session,
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
            relation_rows = relation_result.get("graph_result", {}).get("rows", [])
            if not any(row and row[0] == "T-03 Typography and visual hierarchy" for row in relation_rows):
                raise AssertionError(f"typed topic link was not recovered: {relation_result}")

            reference_result = payload(
                session,
                "search_graph",
                {
                    "project": HARNESS_PROJECT,
                    "active_horizons": [UI_HORIZON],
                    "query": THEME_ID,
                    "format": "json",
                    "limit": 100,
                },
            )
            reference_overlays = reference_result.get("active_horizon_overlays", [])
            reference = next(
                (item for item in reference_overlays if item.get("uri") == REFERENCE_URI),
                None,
            )
            reference_payload = reference.get("payload", "") if reference else ""
            if f"pinned_version={THEME_VERSION}" not in reference_payload:
                raise AssertionError(f"UI horizon lost its theme reference: {reference_result}")

            ui_result = payload(
                session,
                "search_graph",
                {
                    "project": HARNESS_PROJECT,
                    "active_horizons": [UI_HORIZON],
                    "query": "assistant workspace left rail context drawer composer typography",
                    "format": "json",
                    "limit": 100,
                },
            )
            ui_payloads = [item.get("payload", "") for item in ui_result.get("active_horizon_overlays", [])]
            recovered_ui = next((value for value in ui_payloads if "context drawer 320" in value.lower()), None)
            if not recovered_ui:
                raise AssertionError(f"UI specification was not recovered from the horizon: {ui_result}")

            horizon_result = payload(
                session,
                "query_graph",
                {
                    "project": HARNESS_PROJECT,
                    "active_horizons": [UI_HORIZON],
                    "query": "MATCH (n) RETURN n.name LIMIT 1",
                    "format": "json",
                    "max_rows": 1,
                },
            )
            horizon_nodes = horizon_result.get("horizon_nodes", [])
            horizon_edges = horizon_result.get("horizon_edges", [])
            if not any(node.get("uri") == REFERENCE_URI for node in horizon_nodes):
                raise AssertionError(f"theme reference node missing: {horizon_result}")
            if not any(
                edge.get("source") == UI_SECTION_URI
                and edge.get("target") == REFERENCE_URI
                and edge.get("type") == "INFORMED_BY"
                for edge in horizon_edges
            ):
                raise AssertionError(f"UI-to-theme INFORMED_BY edge missing: {horizon_result}")

            print(
                json.dumps(
                    {
                        "result": "PASS",
                        "process": "fresh Windows MCP process",
                        "theme_discovered": THEME_ID,
                        "theme_version": THEME_VERSION,
                        "typed_topic_edges_in_shared_cache": typed_relation_count,
                        "topic_relation": "T-02 -[SUPPORTS]-> T-03",
                        "citation": spacing["node_uri"],
                        "ui_horizon": UI_HORIZON,
                        "theme_reference_recovered": REFERENCE_URI,
                        "horizon_relation": "INFORMED_BY",
                        "ui_specification_recovered_without_source_file_read": True,
                        "ui_overlay_matches": len(ui_payloads),
                        "horizon_nodes": len(horizon_nodes),
                        "horizon_edges": len(horizon_edges),
                        "cache": os.path.abspath(args.cache),
                    },
                    indent=2,
                )
            )
    finally:
        # Stop only the default-runtime daemon this script may have started.
        subprocess.run(
            [args.binary, "daemon", "stop"],
            cwd=str(REPO_ROOT),
            env={**os.environ, **env},
            capture_output=True,
            timeout=30.0,
            check=False,
        )


if __name__ == "__main__":
    main()
