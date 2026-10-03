"""Isolated owner-exit and daemon-restart persistence test for CBM horizons."""
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import time
from datetime import datetime, timezone

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tests" / "windows"))
from mcp_stdio import McpServer

BINARY = Path.home() / ".local" / "bin" / "codebase-memory-mcp.exe"
SOURCE_CACHE = Path.home() / ".cache" / "codebase-memory-mcp"
PROJECT = "C-Users-corre-Documents-harness-kit"
PROJECT_DB = SOURCE_CACHE / (PROJECT + ".db")
OUT_BASE = Path(__file__).resolve().parent


def digest(data):
    return hashlib.sha256(data).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def daemon_status(env):
    p = subprocess.run(
        [str(BINARY), "daemon", "status"], cwd=str(REPO), env=env,
        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20
    )
    output = p.stdout + p.stderr
    match = re.search(r"\bpid:\s*(\d+)", output)
    return {"active": p.returncode == 0 and "daemon: active" in output,
            "returncode": p.returncode, "pid": int(match.group(1)) if match else None,
            "output": output}


def stop_isolated(env):
    before = daemon_status(env)
    if not before["active"]:
        return {"before": before, "stop": None, "after": before}
    p = subprocess.run(
        [str(BINARY), "daemon", "stop"], cwd=str(REPO), env=env,
        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30
    )
    stop_result = {"returncode": p.returncode, "output": p.stdout + p.stderr}
    deadline = time.monotonic() + 15
    after = daemon_status(env)
    while after["active"] and time.monotonic() < deadline:
        time.sleep(0.25)
        after = daemon_status(env)
    return {"before": before, "stop": stop_result, "after": after}


def walk_json(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from walk_json(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk_json(child)
    elif isinstance(value, str):
        candidate = value
        prefix, separator, body = value.partition(":")
        if separator and prefix in {"FractalArtifact", "FractalArtifactPart"}:
            candidate = body
        try:
            decoded = json.loads(candidate)
        except (json.JSONDecodeError, TypeError):
            return
        if isinstance(decoded, (dict, list)):
            yield from walk_json(decoded)


def find_node(overlays, uri, symbol):
    matches = [item for item in overlays if item.get("uri") == uri]
    for item in matches:
        for obj in walk_json(item):
            if obj.get("id") == symbol or obj.get("symbol") == symbol:
                return obj
    raise AssertionError("node payload missing after MCP retrieval: " + symbol)


def run():
    if os.name != "nt":
        raise RuntimeError("This test uses the installed Windows MCP binary.")
    if not BINARY.is_file() or not PROJECT_DB.is_file():
        raise FileNotFoundError("Installed MCP binary or HarnessKit graph DB is missing.")

    run_id = datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + str(os.getpid())
    out = OUT_BASE / run_id
    cache = SOURCE_CACHE.parent / ("retention-test-cache-" + run_id)
    runtime = SOURCE_CACHE / "fractal-runtime" / ("retention-test-runtime-" + run_id)
    out.mkdir(parents=True, exist_ok=False)
    cache.mkdir(parents=True, exist_ok=False)
    runtime.mkdir(parents=True, exist_ok=False)
    test_cwd = out / "isolated-cwd"
    test_cwd.mkdir(exist_ok=False)
    report_path = out / "report.json"
    events = []
    report = {
        "run_id": run_id,
        "started_at_utc": datetime.now(timezone.utc).isoformat(),
        "result": "RUNNING",
        "project": PROJECT,
        "scope": "isolated SQLite snapshot of the real HarnessKit base graph; no writes to shared cache",
        "binary": str(BINARY),
        "binary_sha256": digest(BINARY.read_bytes()),
        "source_graph_db": str(PROJECT_DB),
        "source_graph_db_sha256": digest(PROJECT_DB.read_bytes()),
        "test_cache_dir": str(cache),
        "test_runtime_dir": str(runtime),
        "mcp_reader_reads_human_artifact": False,
        "events": events,
        "checks": {},
    }

    def check(name, passed, detail=None):
        report["checks"][name] = {"passed": bool(passed), "detail": detail}
        save(report_path, report)
        if not passed:
            raise AssertionError(name)

    def call(client, name, args):
        raw = client.call_tool(name, args, timeout=90)
        text, error = McpServer.tool_text(raw)
        if raw.get("error") or error or (raw.get("result") or {}).get("isError"):
            raise RuntimeError(name + " failed: " + repr(raw))
        value = json.loads(text or "{}")
        if isinstance(value, dict) and (value.get("isError") or value.get("success") is False):
            raise RuntimeError(name + " returned failure: " + repr(value))
        if name == "search_graph":
            response = value.get("active_horizon_overlays", [])
            captured = {"overlay_count": len(response), "overlays": response}
        elif name == "query_graph":
            captured = {"horizon_nodes": value.get("horizon_nodes", []),
                        "horizon_edges": value.get("horizon_edges", [])}
        else:
            captured = value
        events.append({"tool": name, "arguments": args, "response": captured})
        save(out / "mcp-transcript.json", events)
        return value

    env = dict(os.environ)
    env["CBM_CACHE_DIR"] = str(cache)
    env["CBM_RUNTIME_DIR"] = str(runtime)
    default_status = daemon_status(os.environ.copy())
    report["shared_daemon_before"] = default_status
    isolated_status = daemon_status(env)
    check("test_runtime_has_no_existing_daemon",
          not isolated_status["active"] and isolated_status["pid"] is None,
          isolated_status)

    # A SQLite online backup snapshots the base graph without touching its WAL.
    source = sqlite3.connect(PROJECT_DB.resolve().as_uri() + "?mode=ro", uri=True)
    target_db = cache / PROJECT_DB.name
    target = sqlite3.connect(str(target_db))
    source.backup(target)
    integrity = target.execute("PRAGMA integrity_check").fetchone()[0]
    target.close()
    source.close()
    report["snapshot_graph_db"] = {
        "path": str(target_db), "bytes": target_db.stat().st_size,
        "sha256": digest(target_db.read_bytes()), "integrity_check": integrity,
    }
    check("snapshot_integrity", integrity == "ok", report["snapshot_graph_db"])

    writer = None
    reader = None
    try:
        writer = McpServer(str(BINARY), extra_env=env, cwd=str(test_cwd))
        writer.start()
        init_a = writer.initialize(timeout=90)
        events.append({"client": "writer", "initialize": init_a})
        status_a = daemon_status(env)
        check("isolated_writer_daemon_started",
              status_a["active"] and status_a["pid"] != default_status["pid"], status_a)
        writer_pid = status_a["pid"]
        report["writer_daemon_pid"] = writer_pid

        project_list = call(writer, "list_projects", {"detail": "stats", "format": "json"})
        projects = project_list.get("projects", [])
        project_names = [p.get("name") for p in projects]
        check("only_snapshot_project_visible", project_names == [PROJECT], project_names)

        horizon_id = "h_retention_restart_" + run_id.replace("-", "_")
        artifact_id = "ART-RETENTION-" + run_id.replace("-", "")
        exact_text = "retention_probe|owner_exit|daemon_restart|" + horizon_id
        split = len(exact_text) // 2
        contents = [exact_text[:split], exact_text[split:]]
        part_symbols = ["AP-01", "AP-02"]
        part_ids = [artifact_id + "-P01", artifact_id + "-P02"]
        base_uri = f"cbm://{PROJECT}/docs/temenos/{horizon_id}/retention-probe.md#"
        root_sha = digest(exact_text.encode("utf-8"))

        nodes = [{
            "symbol": "AR-01",
            "type": "FractalArtifact",
            "cbm_uri": base_uri + "AR-01",
            "description": json.dumps({
                "id": "AR-01", "artifact_id": artifact_id, "temenos_id": horizon_id,
                "part_count": 2, "part_ids": part_ids, "join_policy": "direct",
                "byte_count": len(exact_text.encode("utf-8")), "sha256": root_sha,
            }, separators=(",", ":")),
        }]
        for i, (symbol, part_id, content) in enumerate(zip(part_symbols, part_ids, contents), 1):
            payload = {
                "id": symbol, "artifact_id": artifact_id, "temenos_id": horizon_id,
                "part_id": part_id, "part_index": i, "total_parts": 2,
                "exact_content": content, "byte_count": len(content.encode("utf-8")),
                "sha256": digest(content.encode("utf-8")),
            }
            nodes.append({
                "symbol": symbol, "type": "FractalArtifactPart",
                "cbm_uri": base_uri + symbol,
                "description": json.dumps(payload, separators=(",", ":")),
            })
        edges = [
            {"source": "AR-01", "target": "AP-01", "type": "CONTAINS_PART"},
            {"source": "AP-01", "target": "AP-02", "type": "NEXT_PART"},
            {"source": "AR-01", "target": "AP-02", "type": "CONTAINS_PART"},
        ]
        spec = {"nodes": nodes, "edges": edges}
        fence = chr(96) * 3
        document = (
            f"# Retention probe {horizon_id}\n\n"
            "## Scope\n"
            "Isolated HarnessKit graph snapshot; owner exit and daemon restart.\n\n"
            "## Artifact\n"
            f"Artifact ID: {artifact_id}\n"
            f"Expected SHA-256: {root_sha}\n"
            f"Exact text: {exact_text}\n\n"
            "## Graph\n" + fence + "tactical-spec\n"
            + json.dumps(spec, ensure_ascii=False, indent=2)
            + "\n" + fence + "\n"
        )
        max_line = max(len(line.encode("utf-8")) for line in document.splitlines())
        check("specification_within_parser_line_bound", max_line <= 480, max_line)
        (out / "artifact.txt").write_text(exact_text, encoding="utf-8", newline="")
        (out / "horizon-spec.md").write_text(document, encoding="utf-8", newline="")
        report.update({"horizon_id": horizon_id, "artifact_id": artifact_id,
                       "artifact_bytes": len(exact_text.encode("utf-8")),
                       "artifact_sha256": root_sha})

        created = call(writer, "create_horizon", {"horizon_id": horizon_id, "project": PROJECT})
        check("horizon_created_in_snapshot", created.get("horizon_id") == horizon_id, created)
        check("recorded_owner_is_test_daemon", created.get("client_pid") == writer_pid, created)

        compiled = call(writer, "sync_horizon_spec", {
            "horizon_id": horizon_id,
            "project": PROJECT,
            "file_path": f"docs/temenos/{horizon_id}/revisoes/r001.md",
            "content": document,
        })
        check("three_nodes_and_three_edges_compiled",
              compiled.get("nodes_compiled") == 3 and compiled.get("edges_compiled") == 3,
              compiled)

        valid_a = call(writer, "validate_scope_horizon", {
            "horizon_id": horizon_id, "strict_connectivity": True,
        })
        check("strict_validation_before_exit", valid_a.get("status") == "VALID", valid_a)

        catalog_a = call(writer, "list_horizons", {
            "project": PROJECT, "status": "ALL", "limit": 100, "offset": 0,
        })
        entry_a = next((x for x in catalog_a.get("horizons", [])
                        if x.get("horizon_id") == horizon_id), None)
        check("catalog_says_owner_alive_before_exit",
              entry_a is not None and entry_a.get("client_pid") == writer_pid
              and entry_a.get("owner_alive") is True, entry_a)

        writer.close()
        writer = None
        deadline = time.monotonic() + 15
        status_after_close = daemon_status(env)
        while status_after_close["active"] and time.monotonic() < deadline:
            time.sleep(0.25)
            status_after_close = daemon_status(env)
        shutdown_mode = "automatic_after_last_client_exit"
        if status_after_close["active"]:
            stopped = stop_isolated(env)
            report["isolated_daemon_stop"] = stopped
            status_after_close = stopped["after"]
            shutdown_mode = "explicit_stop_on_isolated_endpoint"
        report["owner_exit_shutdown_mode"] = shutdown_mode
        check("owner_daemon_exited_before_reader", not status_after_close["active"],
              status_after_close)

        horizon_db = cache / "horizons" / (horizon_id + ".db")
        offline = sqlite3.connect(horizon_db.resolve().as_uri() + "?mode=ro", uri=True)
        meta = offline.execute(
            "SELECT client_pid,status,created_at,last_heartbeat,based_on_seq "
            "FROM horizon_metadata LIMIT 1"
        ).fetchone()
        node_count = offline.execute("SELECT count(*) FROM symbolic_nodes").fetchone()[0]
        edge_count = offline.execute("SELECT count(*) FROM virtual_edges").fetchone()[0]
        db_integrity = offline.execute("PRAGMA integrity_check").fetchone()[0]
        offline.close()
        after_exit = {
            "db_sha256": digest(horizon_db.read_bytes()), "integrity_check": db_integrity,
            "client_pid": meta[0], "status": meta[1], "created_at": meta[2],
            "last_heartbeat": meta[3], "based_on_seq": meta[4],
            "symbolic_nodes": node_count, "virtual_edges": edge_count,
            "daemon_status": status_after_close,
        }
        save(out / "horizon-after-owner-exit.json", after_exit)
        check("sqlite_horizon_persisted_after_owner_exit",
              meta[0] == writer_pid and node_count >= 3 and edge_count >= 3
              and db_integrity == "ok", after_exit)
        report["offline_horizon_snapshot"] = after_exit

        reader = McpServer(str(BINARY), extra_env=env, cwd=str(test_cwd))
        reader.start()
        events.append({"client": "reader", "initialize": reader.initialize(timeout=90)})
        status_b = daemon_status(env)
        check("new_daemon_process_started",
              status_b["active"] and status_b["pid"] != writer_pid, status_b)
        report["reader_daemon_pid"] = status_b["pid"]

        catalog_b = call(reader, "list_horizons", {
            "project": PROJECT, "status": "ALL", "limit": 100, "offset": 0,
        })
        entry_b = next((x for x in catalog_b.get("horizons", [])
                        if x.get("horizon_id") == horizon_id), None)
        check("restarted_catalog_finds_dead_owner_horizon",
              entry_b is not None and entry_b.get("client_pid") == writer_pid
              and entry_b.get("owner_alive") is False
              and entry_b.get("status") == "ACTIVE", entry_b)

        search_b = call(reader, "search_graph", {
            "project": PROJECT, "active_horizons": [horizon_id],
            "limit": 100, "format": "json",
        })
        overlays = search_b.get("active_horizon_overlays", [])
        root = find_node(overlays, base_uri + "AR-01", "AR-01")
        parts = [find_node(overlays, base_uri + symbol, symbol) for symbol in part_symbols]
        parts.sort(key=lambda item: item["part_index"])
        reconstructed = "".join(part["exact_content"] for part in parts)
        check("mcp_reconstructs_exact_artifact_after_restart",
              reconstructed == exact_text
              and root.get("sha256") == digest(reconstructed.encode("utf-8"))
              and root.get("byte_count") == len(reconstructed.encode("utf-8")),
              {"reconstructed": reconstructed, "root": root, "parts": parts})

        query_b = call(reader, "query_graph", {
            "project": PROJECT, "active_horizons": [horizon_id],
            "query": "MATCH (f:File) RETURN f.path LIMIT 1",
            "format": "json", "max_rows": 10,
        })
        expected = {
            (base_uri + "AR-01", base_uri + "AP-01", "CONTAINS_PART"),
            (base_uri + "AP-01", base_uri + "AP-02", "NEXT_PART"),
            (base_uri + "AR-01", base_uri + "AP-02", "CONTAINS_PART"),
        }
        def edge_key(edge):
            return (edge.get("source") or edge.get("source_uri"),
                    edge.get("target") or edge.get("target_uri"),
                    edge.get("type") or edge.get("edge_type"))
        actual = {edge_key(edge) for edge in query_b.get("horizon_edges", [])}
        check("mcp_recovers_all_edges_after_restart", expected.issubset(actual), sorted(map(str, actual)))

        valid_b = call(reader, "validate_scope_horizon", {
            "horizon_id": horizon_id, "strict_connectivity": True,
        })
        check("strict_validation_after_restart", valid_b.get("status") == "VALID", valid_b)

        reader.close()
        reader = None
        final_status = daemon_status(env)
        if final_status["active"]:
            shutdown = stop_isolated(env)
            report["final_isolated_daemon_shutdown"] = shutdown
            final_status = shutdown["after"]
        report["isolated_daemon_final_status"] = final_status
        shared_after = daemon_status(os.environ.copy())
        report["shared_daemon_after"] = shared_after
        check("shared_daemon_unchanged",
              shared_after["pid"] == default_status["pid"], shared_after)
        live_project_hash_after = digest(PROJECT_DB.read_bytes())
        report["source_graph_db_sha256_after"] = live_project_hash_after
        check("live_harnesskit_graph_database_unchanged",
              live_project_hash_after == report["source_graph_db_sha256"],
              {"before": report["source_graph_db_sha256"], "after": live_project_hash_after})

        report["result"] = "PASSED_OWNER_EXIT_AND_DAEMON_RESTART_IN_ISOLATED_REAL_GRAPH_SNAPSHOT"
        report["limitations"] = [
            "The test used a private SQLite online-backup copy of the real HarnessKit base graph, not the shared live cache.",
            "It proves immediate recovery after the owning daemon exits and a new daemon loads the same persistent test cache.",
            "It does not test TTL expiry, device restart, application upgrade, or retention in the shared production cache.",
        ]
        report["completed_at_utc"] = datetime.now(timezone.utc).isoformat()
        save(report_path, report)
        print(json.dumps({"result": report["result"], "evidence": str(out),
                          "horizon_id": horizon_id, "writer_daemon_pid": writer_pid,
                          "reader_daemon_pid": status_b["pid"], "artifact_sha256": root_sha}))
        return 0
    except Exception as exc:
        report["result"] = "FAILED"
        report["error"] = repr(exc)
        report["completed_at_utc"] = datetime.now(timezone.utc).isoformat()
        save(report_path, report)
        raise
    finally:
        if writer is not None:
            writer.close()
        if reader is not None:
            reader.close()
        try:
            current = daemon_status(env)
            if current["active"]:
                report["final_isolated_daemon_shutdown"] = stop_isolated(env)
                save(report_path, report)
        except Exception as exc:
            report["shutdown_error"] = repr(exc)
            save(report_path, report)


if __name__ == "__main__":
    raise SystemExit(run())


