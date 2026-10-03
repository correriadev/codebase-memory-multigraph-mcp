"""Smoke the installed Windows MCP executable against the real theme graph.

Run after building a Windows executable with the updated pipeline. This starts
an independent stdio MCP process, reindexes the local theme source in the
user's Windows cache, and verifies discovery plus a typed topic traversal.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT))

from tests.e2e.mcp_process_driver import MCPProcessSession  # noqa: E402


THEME_ID = "@harnesskit/ui-design-practices"
THEME_VERSION = "1.0.0"
THEME_PROJECT = "HarnessKit-UI-Design-Practices"
THEME_SOURCE = REPO_ROOT / "themes" / "harnesskit-ui-design"
RELATION_QUERY = (
    "MATCH (s:Section)-[:SUPPORTS]->(t:Section) "
    "WHERE s.name = 'T-02 Spacing, alignment, and layout rhythm' "
    "RETURN t.name AS target LIMIT 10"
)
RELATION_TYPES = {
    "APPLIES_TO",
    "CONSTRAINS",
    "MOTION_SPECIFIED_BY",
    "SPECIALIZES",
    "SUPPORTED_BY",
    "SUPPORTS",
    "USES",
    "VERIFIED_WITH",
}
THEME_CATALOG_ENTRY = {
    "theme_id": THEME_ID,
    "namespace": "product-design/interface",
    "curator": "codex-e2e",
    "version": THEME_VERSION,
    "name": "HarnessKit UI Design Practices",
    "target_uri": f"cbm-project://{THEME_PROJECT}",
    "description": (
        "Curated practices for spacing, typography, hierarchy, components, color, "
        "responsive behavior, motion, and accessibility."
    ),
    "aliases": "UI design; design system; assistant UI",
    "tags": "spacing typography hierarchy components color motion accessibility",
    "founding_provenance": "Codex E2E curation; W3C accessibility sources",
    "status": "ACTIVE",
}


def payload(session, name, arguments):
    response = session.call_tool(name, arguments, timeout=180.0)
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
            raise AssertionError(f"{name} returned no result: {result}")
        value = json.loads(text_blocks[0])
    if result.get("isError") or value.get("isError"):
        raise AssertionError(f"{name} tool error: {value}")
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", help="Updated codebase-memory-mcp.exe")
    parser.add_argument(
        "--cache",
        help="CBM cache to use (default: a temporary isolated cache)",
    )
    parser.add_argument(
        "--runtime",
        help="CBM runtime parent (default: CBM's normal account runtime)",
    )
    args = parser.parse_args()

    temporary_root = (
        tempfile.TemporaryDirectory(
            prefix="cbm-theme-mcp-smoke-",
            dir=os.environ.get("LOCALAPPDATA"),
        )
        if not args.cache
        else None
    )
    root = Path(args.cache) if args.cache else Path(temporary_root.name)
    cache = root / "cache" if temporary_root else root
    runtime = Path(args.runtime) if args.runtime else (root / "runtime" if temporary_root else None)
    cache.mkdir(parents=True, exist_ok=True)
    env = {"CBM_CACHE_DIR": str(cache)}
    if runtime:
        runtime.mkdir(parents=True, exist_ok=True)
        env["CBM_RUNTIME_DIR"] = str(runtime)
    try:
        with MCPProcessSession(
            binary_path=args.binary,
            env=env,
            cwd=str(REPO_ROOT),
        ) as session:
            if not session.initialize(timeout=120.0).is_success():
                raise AssertionError("Windows MCP initialization failed")
            tools = session.list_tools(timeout=30.0)
            if not tools.is_success():
                raise AssertionError(f"tools/list failed: {tools.error}")
            names = {tool["name"] for tool in tools.result.get("tools", [])}
            expected = {
                "theme_search",
                "theme_graph_search",
                "theme_graph_query",
                "index_repository",
                "get_graph_schema",
            }
            if missing := expected - names:
                raise AssertionError(f"updated MCP schema is missing: {sorted(missing)}")

            indexed = payload(
                session,
                "index_repository",
                {
                    "repo_path": str(THEME_SOURCE),
                    "name": THEME_PROJECT,
                    "mode": "full",
                    "persistence": False,
                },
            )
            if indexed.get("status") != "indexed":
                raise AssertionError(f"theme source was not indexed: {indexed}")

            existing_theme = payload(session, "theme_lookup", {"theme_id": THEME_ID})
            if (
                existing_theme.get("status") == "ACTIVE"
                and existing_theme.get("version") == THEME_VERSION
            ):
                catalog_action = "reused existing immutable version"
            else:
                registered = payload(session, "theme_register", THEME_CATALOG_ENTRY)
                if registered.get("theme_id") != THEME_ID or registered.get("version") != THEME_VERSION:
                    raise AssertionError(f"theme catalog registration failed: {registered}")
                catalog_action = "registered"

            schema = payload(
                session,
                "get_graph_schema",
                {"project": THEME_PROJECT, "format": "json", "diagnostics": "full"},
            )
            edge_counts = {edge["type"]: edge["count"] for edge in schema.get("edge_types", [])}
            relation_count = sum(edge_counts.get(edge_type, 0) for edge_type in RELATION_TYPES)
            if relation_count != 32:
                raise AssertionError(
                    f"expected 32 declared topic edges, found {relation_count}: {edge_counts}"
                )

            traversal = payload(
                session,
                "theme_graph_query",
                {
                    "theme_id": THEME_ID,
                    "version": THEME_VERSION,
                    "query": RELATION_QUERY,
                    "max_rows": 10,
                },
            )
            rows = traversal.get("graph_result", {}).get("rows", [])
            if not any(row and row[0] == "T-03 Typography and visual hierarchy" for row in rows):
                raise AssertionError(f"typed graph traversal failed: {traversal}")

            print(
                json.dumps(
                    {
                        "result": "PASS",
                        "mcp_tools": sorted(expected),
                        "project": THEME_PROJECT,
                        "catalog": catalog_action,
                        "topic_nodes": schema.get("node_labels", []),
                        "typed_topic_edges": relation_count,
                        "traversal": "T-02 -[SUPPORTS]-> T-03",
                        "cache": os.path.abspath(cache),
                        "runtime": os.path.abspath(runtime) if runtime else "CBM default account runtime",
                    },
                    indent=2,
                )
            )
    finally:
        # MCP clients leave the detached account daemon alive. Stop only the
        # daemon under this test's private runtime; never touch the user's live one.
        subprocess.run(
            [args.binary, "daemon", "stop"],
            cwd=str(REPO_ROOT),
            env={**os.environ, **env},
            capture_output=True,
            timeout=30.0,
            check=False,
        )
        if temporary_root:
            temporary_root.cleanup()


if __name__ == "__main__":
    main()
