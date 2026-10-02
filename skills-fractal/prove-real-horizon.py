"""Exercise the installed MCP against an existing project and a new horizon only."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tests" / "windows"))
from mcp_stdio import McpServer

parser = argparse.ArgumentParser()
parser.add_argument("--binary", type=Path, default=Path.home() / ".local/bin/codebase-memory-mcp.exe")
parser.add_argument("--project", default="C-Users-corre-Documents-harness-kit")
parser.add_argument("--runtime", type=Path)
parser.add_argument("--probe", action="store_true")
args = parser.parse_args()
stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
output = REPO / "docs" / "temenos-tests" / ("real-" + stamp)
output.mkdir(parents=True, exist_ok=False)
events = []
clients = []
report = {"started_at": datetime.now(timezone.utc).isoformat(), "binary": str(args.binary),
          "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
          "project": args.project, "output": str(output), "checks": {},
          "scope": "Existing real project; new horizon; no index, promotion or base mutation",
          "reader_inputs": ["project", "horizon_id"], "reader_reads_document": False}

def save():
    (output / "mcp-transcript.json").write_text(json.dumps(events, ensure_ascii=False, indent=2), encoding="utf-8")
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

def start(label):
    extra_env = {"CBM_RUNTIME_DIR": str(args.runtime)} if args.runtime else None
    client = McpServer(str(args.binary), cwd=str(REPO), extra_env=extra_env)
    client.start()
    clients.append((label, client))
    initialized = client.initialize(timeout=40)
    events.append({"client": label, "method": "initialize", "response": initialized})
    return client

def call(client, label, name, arguments):
    response = client.call_tool(name, arguments, timeout=45)
    events.append({"time": datetime.now(timezone.utc).isoformat(), "client": label,
                   "tool": name, "arguments": arguments, "response": response})
    save()
    result = response.get("result")
    if not isinstance(result, dict) or response.get("error") or result.get("isError"):
        raise RuntimeError(f"{name} refused: {response}")
    text, error = client.tool_text(response)
    if error:
        raise RuntimeError(str(error))
    value = json.loads(text)
    if value.get("isError") or value.get("success") is False:
        raise RuntimeError(f"{name} failed: {value}")
    return value

def overlay_search(client, label, horizon):
    value = call(client, label, "search_graph", {"project": args.project,
        "active_horizons": [horizon], "limit": 100, "format": "json"})
    return {item["uri"]: item["payload"] for item in value.get("active_horizon_overlays", [])}

def overlay_query(client, label, horizon):
    return call(client, label, "query_graph", {"project": args.project,
        "active_horizons": [horizon], "query": "MATCH (f:File) RETURN f.path LIMIT 1", "format": "json"})

def check(name, condition, detail=None):
    report["checks"][name] = {"passed": bool(condition), "detail": detail}
    save()
    if not condition:
        raise AssertionError(name)

try:
    writer = start("A")
    tools = writer.tools_list(timeout=40)
    (output / "tool-schemas.json").write_text(json.dumps(tools, ensure_ascii=False, indent=2), encoding="utf-8")
    required = {"list_projects", "list_horizons", "create_horizon", "sync_horizon_spec", "validate_scope_horizon", "search_graph", "query_graph"}
    check("required_tools", required.issubset({tool["name"] for tool in tools}))
    catalog = call(writer, "A", "list_projects", {"detail": "stats", "format": "json"})
    projects = {p["name"]: p for p in catalog.get("projects", [])}
    check("existing_real_project", args.project in projects)
    report["project_catalog_entry"] = projects[args.project]
    if args.probe:
        report["result"] = "PROBE_PASSED"
        save()
        print(json.dumps({"output": str(output), "result": report["result"], "project": projects[args.project]}, ensure_ascii=False), flush=True)
    else:
        # Read human evidence only in writer A; B receives no source content as input.
        source = Path(projects[args.project]["root_path"]) / "docs/temenos/t-20261001-1754/registro.md"
        original = source.read_text(encoding="utf-8")
        check("real_decision_evidence", 'essa abordagem esta boa' in original and 'docs/backlog' in original)
        report["source"] = {"path": str(source), "sha256": hashlib.sha256(original.encode()).hexdigest()}
        horizon = "h_fractal_real_" + stamp.replace("-", "_")
        created = call(writer, "A", "create_horizon", {"horizon_id": horizon, "project": args.project})
        horizon = created["horizon_id"]
        report["horizon_id"] = horizon
        report["horizon_owner_pid"] = created.get("client_pid")
        save()
        print("Created real horizon: " + horizon, flush=True)
        catalog = call(writer, "A", "list_horizons", {"project": args.project, "limit": 500})
        check("empty_horizon_has_explicit_project_binding", any(item["horizon_id"] == horizon and
              item["association_source"] == "explicit_project" for item in catalog["horizons"]))
        base = f"cbm://{args.project}/docs/temenos/{horizon}/registro.md#"
        def node(symbol, label, **fields):
            payload = json.dumps({"id": symbol, "temenos_id": horizon, **fields}, ensure_ascii=False, separators=(",", ":"))
            if len(payload.encode("utf-8")) > 3000:
                raise ValueError("Payload too large for current overlay transport")
            return {"symbol": symbol, "type": label, "cbm_uri": base + symbol, "description": payload}
        nodes = [
            node("T-01", "FractalTemenos", context=args.project, cycle="ABERTO", purpose="Prova de memoria da decisao real", evidence="E-01"),
            node("C-01", "FractalContext", project=args.project, knowledge="VERIFICADO", evidence="list_projects: 4457 nos, 14274 arestas"),
            node("D-03-R1", "FractalDecision", predicate="Persistir cards em docs/backlog/*.md com frontmatter", knowledge="DECLARADO", destination="CONSOLIDADO_LOCAL", author="humano/Antigravity", evidence="E-01", implementation="NAO_VERIFICADA"),
            node("Q-03-R1", "FractalQuestion", predicate="Qual o proximo passo operacional para implementacao?", knowledge="DECLARADO", destination="MANTIDO_ABERTO", author="Antigravity", evidence="E-01"),
            node("K-01-R1", "FractalConsequence", predicate="Markdown pode manter rastreabilidade Git e transparencia humana", knowledge="HIPOTESE", destination="MANTIDO_ABERTO", author="Antigravity", evidence="E-01"),
            node("E-01", "FractalEvidence", source="docs/temenos/t-20261001-1754/registro.md", items="D-03,Q-03", human_quote="essa abordagem esta boa", source_revision=5)
        ]
        edges = [
            {"source": "T-01", "target": "C-01", "type": "IN_CONTEXT"},
            {"source": "T-01", "target": "D-03-R1", "type": "CONTAINS"},
            {"source": "T-01", "target": "Q-03-R1", "type": "CONTAINS"},
            {"source": "D-03-R1", "target": "K-01-R1", "type": "HAS_CONSEQUENCE"},
            {"source": "Q-03-R1", "target": "D-03-R1", "type": "CONCERNS"},
            {"source": "D-03-R1", "target": "E-01", "type": "SUPPORTED_BY"},
            {"source": "Q-03-R1", "target": "E-01", "type": "SUPPORTED_BY"},
            {"source": "K-01-R1", "target": "E-01", "type": "SUPPORTED_BY"}
        ]
        def sync(revision):
            document = f"# Temenos {horizon}\n\n## Revisao {revision}\nContexto: {args.project}. Decisao real de persistencia em Markdown; implementacao nao verificada.\n\n## Grafo\n```tactical-spec\n" + json.dumps({"nodes": nodes, "edges": edges}, ensure_ascii=False, indent=2) + "\n```\n"
            if max(len(line.encode("utf-8")) for line in document.splitlines()) > 480:
                raise ValueError("Physical line exceeds installed parser compatibility bound")
            path = output / f"r{revision:03d}.md"
            path.write_text(document, encoding="utf-8")
            logical = f"docs/temenos/{horizon}/revisoes/r{revision:03d}.md"
            compiled = call(writer, "A", "sync_horizon_spec", {"horizon_id": horizon,
                "project": args.project, "file_path": logical, "content": document})
            check(f"compiled_revision_{revision}", compiled.get("nodes_compiled") == len(nodes) and compiled.get("edges_compiled") == len(edges), compiled)
            valid = call(writer, "A", "validate_scope_horizon", {"horizon_id": horizon, "strict_connectivity": True})
            check(f"connected_revision_{revision}", valid.get("status") == "VALID", valid)
        sync(1)
        before = call(writer, "A", "list_horizons", {"project": args.project, "limit": 500})
        before_item = next(item for item in before["horizons"] if item["horizon_id"] == horizon)
        for tool, attempt in (
            ("create_horizon", {"horizon_id": horizon, "project": args.project + "-conflict", "based_on_seq": "must_not_replace"}),
            ("sync_horizon_spec", {"horizon_id": horizon, "project": args.project + "-conflict", "file_path": "conflict.md", "content": "# Must not compile"}),
        ):
            refused = writer.call_tool(tool, attempt, timeout=45)
            events.append({"client": "A", "tool": tool, "arguments": attempt, "response": refused, "expected_error": True})
            check(tool + "_project_conflict_rejected", bool((refused.get("result") or {}).get("isError")))
        after = call(writer, "A", "list_horizons", {"project": args.project, "limit": 500})
        after_item = next(item for item in after["horizons"] if item["horizon_id"] == horizon)
        check("conflicting_calls_preserve_catalog_metadata", before_item == after_item)
        reader = start("B")
        recovered = overlay_search(reader, "B", horizon)
        check("parallel_content_recovery", all(base + n["symbol"] in recovered and n["description"] in recovered[base + n["symbol"]] for n in nodes))
        queried = overlay_query(reader, "B", horizon)
        recovered_edges = {(e["source"], e["target"], e["type"]) for e in queried.get("horizon_edges", [])}
        expected_edges = {(base + e["source"], base + e["target"], e["type"]) for e in edges}
        check("parallel_relation_recovery", expected_edges.issubset(recovered_edges), sorted(queried.keys()))
        nodes.append(node("D-03-R2", "FractalDecision", predicate="Persistir cards em Markdown; a viabilidade segue nao verificada", knowledge="DECLARADO", destination="CONSOLIDADO_LOCAL", author="agente/prova-tecnica", evidence="E-01", supersedes="D-03-R1", revision_kind="Reexpressao para teste; nao e nova decisao humana"))
        edges.extend([{"source": "D-03-R2", "target": "D-03-R1", "type": "SUPERSEDES"},
                      {"source": "T-01", "target": "D-03-R2", "type": "CONTAINS"},
                      {"source": "D-03-R2", "target": "E-01", "type": "SUPPORTED_BY"}])
        sync(2)
        updated = overlay_search(reader, "B", horizon)
        check("reader_sees_revision_without_restart", nodes[-1]["description"] in updated.get(base + "D-03-R2", ""))
        check("prior_revision_preserved", nodes[2]["description"] in updated.get(base + "D-03-R1", ""))
        updated_graph = overlay_query(reader, "B", horizon)
        revision_edges = {(e["source"], e["target"], e["type"]) for e in updated_graph.get("horizon_edges", [])}
        check("revision_relation_recovered", (base + "D-03-R2", base + "D-03-R1", "SUPERSEDES") in revision_edges)
        writer.close()
        report["writer_stopped"] = True
        after_close = overlay_search(reader, "B", horizon)
        check("immediate_recovery_after_writer_exit", base + "D-03-R2" in after_close)
        reader.close()
        fresh = start("C")
        after_restart = overlay_search(fresh, "C", horizon)
        check("fresh_client_recovery", all(n["description"] in after_restart.get(base + n["symbol"], "") for n in nodes))
        report["result"] = "PASSED_BOUNDED_REAL_GRAPH_PROOF"
        report["not_proven"] = ["Recovery after orphan TTL/daemon lifecycle", "Automatic horizon discovery", "Full Cypher filtering over horizon", "Host UI invocation", "Institutional promotion"]
        print(json.dumps({"output": str(output), "horizon_id": horizon, "result": report["result"]}, ensure_ascii=False), flush=True)
except Exception as exc:
    report["result"] = "FAILED"
    report["error"] = repr(exc)
    print(json.dumps({"output": str(output), "result": "FAILED", "error": repr(exc)}, ensure_ascii=False), flush=True)
    raise
finally:
    for label, client in clients:
        client.close()
        (output / (label + "-stderr.log")).write_text(client.stderr_text(), encoding="utf-8")
    save()
