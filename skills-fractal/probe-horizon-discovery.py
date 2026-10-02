"""Read-only discovery probe: reader knows only project, never a horizon ID."""
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests/windows"))
from mcp_stdio import McpServer

parser = argparse.ArgumentParser()
parser.add_argument("--project", default="C-Users-corre-Documents-harness-kit")
args = parser.parse_args()
output = ROOT / "docs/temenos-tests" / ("discovery-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
output.mkdir()
events = []
report = {"project": args.project, "reader_inputs": ["project"],
          "reads_human_files": False, "reads_cache_files": False,
          "mutations": False, "checks": {}}
with McpServer(str(Path.home() / ".local/bin/codebase-memory-mcp.exe"), cwd=str(ROOT)) as client:
    client.initialize(timeout=40)
    schemas = client.tools_list(timeout=40)
    (output / "tool-schemas.json").write_text(json.dumps(schemas, ensure_ascii=False, indent=2), encoding="utf-8")
    report["horizon_tools"] = [{"name": t["name"], "description": t["description"]}
                               for t in schemas if "horizon" in (t["name"] + " " + t["description"]).lower()]
    catalog_tools = [t for t in schemas if re.search(r"(?:list|catalog|discover|find|search).*horizon|horizon.*(?:list|catalog|discover)", t["name"], re.I)]
    report["catalog_tool_candidates"] = [t["name"] for t in catalog_tools]
    def call(name, arguments):
        response = client.call_tool(name, arguments, timeout=45)
        events.append({"tool": name, "arguments": arguments, "response": response})
        (output / "mcp-transcript.json").write_text(json.dumps(events, ensure_ascii=False, indent=2), encoding="utf-8")
        text, error = client.tool_text(response)
        if error or not isinstance(response.get("result"), dict) or response["result"].get("isError"):
            raise RuntimeError(f"{name}: {response}")
        return json.loads(text)

    catalog = []
    if "list_horizons" in {t["name"] for t in schemas}:
        offset = 0
        while True:
            page = call("list_horizons", {"project": args.project, "offset": offset, "limit": 2})
            if page.get("partial"):
                raise RuntimeError("Partial catalog must not pass complete discovery")
            catalog.extend(page["horizons"])
            if not page["has_more"]:
                break
            if not page["returned"]:
                raise RuntimeError("Paging made no progress")
            offset += page["returned"]
        ids = [item["horizon_id"] for item in catalog]
        report["checks"]["stable_pagination"] = ids == sorted(set(ids)) and len(ids) == page["total"]
        repeated = call("list_horizons", {"project": args.project, "offset": 0, "limit": 2})
        report["checks"]["repeated_page_is_stable"] = repeated["horizons"] == catalog[:2]
        rejected = []
        for invalid in ({}, {"project": args.project, "limit": 0},
                        {"project": args.project, "status": "INVALID"}, {"project": args.project, "offset": -1}):
            response = client.call_tool("list_horizons", invalid, timeout=45)
            events.append({"tool": "list_horizons", "arguments": invalid, "response": response, "expected_error": True})
            rejected.append(bool((response.get("result") or {}).get("isError")))
        report["checks"]["invalid_parameters_rejected"] = all(rejected)
        # Negative exact-match check: this is a read-only lookup, not a fake project creation.
        other = call("list_horizons", {"project": args.project + "-not-this-project"})
        report["checks"]["project_isolation"] = other["total"] == 0
        report["catalog"] = catalog
        candidates = [item for item in catalog if item.get("context_preview")]
        report["human_selection_required"] = len(candidates) > 1
        if candidates:
            # Deterministic technical probe, not an implicit human selection of context.
            selected = candidates[-1]["horizon_id"]
            content = call("search_graph", {"project": args.project, "active_horizons": [selected], "limit": 100, "format": "json"})
            graph = call("query_graph", {"project": args.project, "active_horizons": [selected],
                                        "query": "MATCH (f:File) RETURN f.path LIMIT 1", "format": "json"})
            report["recovered_discovered_horizon"] = selected
            report["checks"]["discovered_content_recovered"] = bool(content.get("active_horizon_overlays"))
            report["checks"]["discovered_relations_recovered"] = bool(graph.get("horizon_edges"))
            payloads = {}
            for item in content.get("active_horizon_overlays", []):
                label, _, payload = item["payload"].partition(":")
                if label.startswith("Fractal"):
                    payloads.setdefault(label, []).append(json.loads(payload))
            required_labels = {"FractalTemenos", "FractalContext", "FractalDecision", "FractalQuestion", "FractalConsequence", "FractalEvidence"}
            report["checks"]["context_and_memory_categories_recovered"] = required_labels.issubset(payloads)
            report["checks"]["revision_relation_recovered"] = any(e["type"] == "SUPERSEDES" for e in graph.get("horizon_edges", []))
    calls = [
        ("list_projects", {"detail": "stats", "format": "json"}),
        ("search_graph", {"project": args.project, "label": "FractalTemenos", "limit": 100, "format": "json"}),
        ("search_graph", {"project": args.project, "name_pattern": "(?i)temenos|horizon|fractal", "limit": 100, "format": "json"}),
        ("search_graph", {"project": args.project, "query": "temenos", "limit": 100, "format": "json"}),
        ("query_graph", {"project": args.project, "query": "MATCH (t:FractalTemenos) RETURN t LIMIT 100", "format": "json"}),
    ]
    for name, arguments in calls:
        call(name, arguments)
    # Only IDs supplied by actual MCP responses count as discoverable.
    found = sorted(set(re.findall(r"h_fractal_real_\d{8}_\d{6}", json.dumps(events))))
    report["discovered_test_horizons"] = found
    recovered = report["checks"].get("discovered_content_recovered") and report["checks"].get("discovered_relations_recovered")
    report["result"] = "PASSED_REAL_DISCOVERY_AND_RECOVERY" if found and recovered and all(report["checks"].values()) else "NOT_DISCOVERABLE_WITH_CURRENT_MCP"
    report["checks"]["explicit_catalog_exposed"] = bool(catalog_tools)
    report["checks"]["project_only_discovers_test_horizon"] = bool(found)
    report["stderr"] = client.stderr_text()
(output / "mcp-transcript.json").write_text(json.dumps(events, ensure_ascii=False, indent=2), encoding="utf-8")
(output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({"output": str(output), **report}, ensure_ascii=False, indent=2))
