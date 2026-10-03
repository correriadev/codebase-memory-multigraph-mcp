"""Read-only MCP verification after owner exit; resumes one isolated test horizon."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from datetime import datetime, timezone

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tests" / "windows"))
from mcp_stdio import McpServer

BINARY = Path.home() / ".local" / "bin" / "codebase-memory-mcp.exe"
RUN_DIR = Path(__file__).resolve().parent / "20261002-160437-16824"
PRIOR = json.loads((RUN_DIR / "report.json").read_text(encoding="utf-8"))
PROJECT = PRIOR["project"]
HORIZON = PRIOR["horizon_id"]
CACHE = Path(PRIOR["test_cache_dir"])
RUNTIME = Path(PRIOR["test_runtime_dir"])
ARTIFACT_ID = PRIOR["artifact_id"]
EXPECTED_SHA = PRIOR["artifact_sha256"]
EXPECTED_BYTES = PRIOR["artifact_bytes"]
PART_SYMBOLS = ["AP-01", "AP-02"]
BASE_URI = f"cbm://{PROJECT}/docs/temenos/{HORIZON}/retention-probe.md#"
OUT = RUN_DIR
EVENTS = []
RESULT = {"started_at_utc": datetime.now(timezone.utc).isoformat(),
          "project": PROJECT, "horizon_id": HORIZON,
          "cache_dir": str(CACHE), "runtime_dir": str(RUNTIME),
          "expected_sha256": EXPECTED_SHA, "expected_bytes": EXPECTED_BYTES,
          "mcp_reader_reads_human_artifact": False, "checks": {}}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write(path, obj):
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def check(key, ok, detail=None):
    RESULT["checks"][key] = {"passed": bool(ok), "detail": detail}
    write(OUT / "post-restart-reader.json", RESULT)
    if not ok:
        raise AssertionError(key)


def status(env):
    p = subprocess.run([str(BINARY), "daemon", "status"], cwd=str(REPO), env=env,
                       capture_output=True, text=True, encoding="utf-8", errors="replace",
                       timeout=20)
    text = p.stdout + p.stderr
    match = re.search(r"\bpid:\s*(\d+)", text)
    return {"active": p.returncode == 0 and "daemon: active" in text,
            "pid": int(match.group(1)) if match else None, "output": text,
            "returncode": p.returncode}


def call(client, name, args):
    raw = client.call_tool(name, args, timeout=90)
    text, error = McpServer.tool_text(raw)
    if raw.get("error") or error or (raw.get("result") or {}).get("isError"):
        raise RuntimeError(name + " failed: " + repr(raw))
    value = json.loads(text or "{}")
    if isinstance(value, dict) and (value.get("isError") or value.get("success") is False):
        raise RuntimeError(name + " returned failure: " + repr(value))
    if name == "search_graph":
        compact = {"overlays": value.get("active_horizon_overlays", [])}
    elif name == "query_graph":
        compact = {"horizon_nodes": value.get("horizon_nodes", []),
                   "horizon_edges": value.get("horizon_edges", [])}
    else:
        compact = value
    EVENTS.append({"tool": name, "arguments": args, "response": compact})
    write(OUT / "post-restart-mcp-transcript.json", EVENTS)
    return value


def find_node(overlays, uri, symbol):
    for item in overlays:
        if item.get("uri") != uri:
            continue
        payload = item.get("payload")
        if not isinstance(payload, str):
            continue
        prefix, separator, body = payload.partition(":")
        if not separator:
            continue
        try:
            obj = json.loads(body)
        except json.JSONDecodeError:
            continue
        if obj.get("id") == symbol and obj.get("artifact_id") == ARTIFACT_ID:
            RESULT.setdefault("payload_prefixes", {})[symbol] = prefix
            return obj
    raise AssertionError("missing " + symbol + " payload")


def stop_if_active(env):
    before = status(env)
    if not before["active"]:
        return {"before": before, "stop": None, "after": before}
    proc = subprocess.run([str(BINARY), "daemon", "stop"], cwd=str(REPO), env=env,
                          capture_output=True, text=True, encoding="utf-8",
                          errors="replace", timeout=30)
    stop_result = {"returncode": proc.returncode, "output": proc.stdout + proc.stderr}
    deadline = time.monotonic() + 15
    after = status(env)
    while after["active"] and time.monotonic() < deadline:
        time.sleep(0.25)
        after = status(env)
    return {"before": before, "stop": stop_result, "after": after}


def main():
    env = dict(os.environ)
    env["CBM_CACHE_DIR"] = str(CACHE)
    env["CBM_RUNTIME_DIR"] = str(RUNTIME)
    initial = status(env)
    check("isolated_test_daemon_was_down_before_reader", not initial["active"], initial)

    live_db = Path.home() / ".cache" / "codebase-memory-mcp" / (PROJECT + ".db")
    live_hash_before = sha(live_db.read_bytes())
    expected_live_hash = PRIOR["source_graph_db_sha256"]
    check("shared_harnesskit_db_unchanged_since_writer_snapshot",
          live_hash_before == expected_live_hash,
          {"expected": expected_live_hash, "actual": live_hash_before})

    reader_cwd = OUT / "reader-cwd"
    reader_cwd.mkdir(exist_ok=True)
    reader = None
    try:
        reader = McpServer(str(BINARY), extra_env=env, cwd=str(reader_cwd))
        reader.start()
        EVENTS.append({"client": "reader", "initialize": reader.initialize(timeout=90)})
        new_daemon = status(env)
        old_pid = PRIOR["writer_daemon_pid"]
        check("new_daemon_process_after_owner_exit",
              new_daemon["active"] and new_daemon["pid"] != old_pid,
              {"old_owner_pid": old_pid, "reader_daemon": new_daemon})
        RESULT["reader_daemon_pid"] = new_daemon["pid"]

        catalog = call(reader, "list_horizons", {
            "project": PROJECT, "status": "ALL", "limit": 100, "offset": 0,
        })
        entry = next((x for x in catalog.get("horizons", [])
                      if x.get("horizon_id") == HORIZON), None)
        check("catalog_finds_active_horizon_with_dead_owner",
              entry is not None and entry.get("status") == "ACTIVE"
              and entry.get("client_pid") == old_pid
              and entry.get("owner_alive") is False, entry)

        search = call(reader, "search_graph", {
            "project": PROJECT, "active_horizons": [HORIZON],
            "limit": 100, "format": "json",
        })
        overlays = search.get("active_horizon_overlays", [])
        root = find_node(overlays, BASE_URI + "AR-01", "AR-01")
        parts = [find_node(overlays, BASE_URI + symbol, symbol) for symbol in PART_SYMBOLS]
        parts.sort(key=lambda x: x["part_index"])
        recovered = "".join(x["exact_content"] for x in parts)
        recovered_bytes = recovered.encode("utf-8")
        check("mcp_recovers_exact_artifact_bytes",
              sha(recovered_bytes) == EXPECTED_SHA
              and len(recovered_bytes) == EXPECTED_BYTES
              and root.get("sha256") == EXPECTED_SHA
              and root.get("byte_count") == EXPECTED_BYTES,
              {"sha256": sha(recovered_bytes), "bytes": len(recovered_bytes),
               "root": root, "parts": parts})

        query = call(reader, "query_graph", {
            "project": PROJECT, "active_horizons": [HORIZON],
            "query": "MATCH (f:File) RETURN f.path LIMIT 1",
            "format": "json", "max_rows": 10,
        })
        expected_edges = {
            (BASE_URI + "AR-01", BASE_URI + "AP-01", "CONTAINS_PART"),
            (BASE_URI + "AP-01", BASE_URI + "AP-02", "NEXT_PART"),
            (BASE_URI + "AR-01", BASE_URI + "AP-02", "CONTAINS_PART"),
        }
        actual_edges = set()
        for edge in query.get("horizon_edges", []):
            actual_edges.add((
                edge.get("source") or edge.get("source_uri"),
                edge.get("target") or edge.get("target_uri"),
                edge.get("type") or edge.get("edge_type"),
            ))
        check("mcp_recovers_all_three_edges",
              expected_edges.issubset(actual_edges), sorted(map(str, actual_edges)))

        validation = call(reader, "validate_scope_horizon", {
            "horizon_id": HORIZON, "strict_connectivity": True,
        })
        check("strict_validation_after_restart", validation.get("status") == "VALID", validation)
    finally:
        if reader is not None:
            reader.close()

    stopped = stop_if_active(env)
    RESULT["test_daemon_after_reader_close"] = stopped["after"]
    check("isolated_reader_daemon_stopped", not stopped["after"]["active"], stopped)
    shared_after = status(os.environ.copy())
    shared_before = PRIOR["shared_daemon_before"]
    check("shared_daemon_pid_unchanged",
          shared_after.get("pid") == shared_before.get("pid"), shared_after)
    live_hash_after = sha(live_db.read_bytes())
    check("live_harnesskit_database_hash_unchanged",
          live_hash_after == expected_live_hash, live_hash_after)

    RESULT["result"] = "PASSED_OWNER_EXIT_AND_DAEMON_RESTART"
    RESULT["notes"] = [
        "Writer daemon shut down after its last MCP client exited; the on-disk horizon DB remained integrity_check=ok.",
        "A new daemon PID reopened the same private test cache and MCP recovered exact artifact bytes, all edges, and strict validation.",
        "Test used a private SQLite snapshot of the real HarnessKit graph; it did not stop or write the live shared daemon/cache.",
        "The first reader assertion failed because payloads are type-prefixed strings; this corrected reader parsed that envelope and completed the read-only checks.",
        "No TTL-expiry interval was tested.",
    ]
    RESULT["completed_at_utc"] = datetime.now(timezone.utc).isoformat()
    write(OUT / "post-restart-reader.json", RESULT)
    print(json.dumps({"result": RESULT["result"], "horizon_id": HORIZON,
                      "writer_pid": PRIOR["writer_daemon_pid"],
                      "reader_pid": RESULT["reader_daemon_pid"],
                      "sha256": EXPECTED_SHA, "evidence": str(OUT)}))


if __name__ == "__main__":
    main()

