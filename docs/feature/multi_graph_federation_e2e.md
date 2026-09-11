---
doc_type: feature
domain: multi_graph_federation_e2e
stack: [Python, JSON-RPC, SQLite, Subprocess, OS Signals]
node_id: "feature:multi-graph-federation-e2e"
tags: [e2e, testing, federation, subprocess, stdio, jsonrpc]
edges:
  - relation: implements
    target: "adr:architecture"
  - relation: tested_by
    target: "adr:tests"
updated: 2026-09-11
---
# Multi-Graph Federation End-to-End Test Suite
Validates black-box MCP stdio JSON-RPC protocol compliance, multi-agent horizon isolation, two-tier anchor refactoring admission, and client crash recovery with daemon horizon reaping.

```graph
{
  "node_id": "feature:multi-graph-federation-e2e",
  "domain": "multi_graph_federation_e2e",
  "implements": ["adr:architecture"],
  "tested_by": ["adr:tests"],
  "entrypoints": [
    "tests/e2e/run_e2e.py"
  ],
  "registration_files": [
    "tests/e2e/mcp_process_driver.py",
    "tests/e2e/jsonrpc_client.py"
  ],
  "reference_files": [
    "tests/e2e/sandbox_environment.py",
    "tests/e2e/reaper_harness.py",
    "tests/e2e/federated_engine.py",
    "tests/e2e/admission_verifier.py"
  ],
  "code_files": [
    "tests/e2e/fixtures/base_seeder.py",
    "tests/e2e/fixtures/specimens.py"
  ],
  "test_files": [
    "tests/e2e/test_protocol_handshake.py",
    "tests/e2e/test_multi_agent_isolation.py",
    "tests/e2e/test_refactoring_admission.py",
    "tests/e2e/test_crash_recovery_reaper.py",
    "tests/e2e/test_streaming_pagination.py"
  ]
}
```

## FOLDER STRUCTURE
```
tests/e2e/
  __init__.py
  jsonrpc_client.py            # JSON-RPC 2.0 protocol models and framing
  mcp_process_driver.py        # Subprocess driver with async line readers and kill_force
  sandbox_environment.py       # Hermetic sandbox directory manager with retry cleanup
  federated_engine.py          # O(K) heap-based streaming merge and horizon query engine
  admission_verifier.py        # Two-tier anchor verification and atomic promotion
  reaper_harness.py            # OS PID liveness checking and TTL eviction scanner
  run_e2e.py                   # Unified runner and test reporter
  fixtures/
    __init__.py
    base_seeder.py             # Deterministic SQLite Base and Horizon seeder
    specimens.py               # Real source file specimens for physical disk mutations
  test_protocol_handshake.py   # Subprocess stdio handshake and tools listing
  test_multi_agent_isolation.py# Private cognitive horizon isolation across virtual agents
  test_refactoring_admission.py# Benign comment shifts vs breaking AST signature drift
  test_crash_recovery_reaper.py# Abrupt client SIGKILL and orphan database unlinking
  test_streaming_pagination.py # Paginated K-Way merge with node shadowing
```

## ARCHITECTURE & SYSTEM INTEGRATION
- References: [Architecture Guide](./docs/adr/ARCHITECTURE.md), [Testing Protocol](./docs/adr/TESTS.md).
- **Subprocess Driver**: Spawns native `codebase-memory-mcp` binary communicating over `stdin`/`stdout` pipes with dedicated daemon reader threads, timeout protection, and clean process tree termination (`taskkill /F /T` / `SIGKILL`).
- **Hermetic Sandbox**: Generates isolated project scopes with dedicated `.db` namespaces, preventing collisions with active background daemons.
- **Two-Tier Anchor Verification**: Verifies fast-path byte offsets on disk followed by FNV-1a 64-bit AST signature hashes when comments or headers shift lines.
- **Horizon Reaper Verification**: Spawns real OS client processes, terminates them forcibly, and validates deterministic removal of `.db`, `.db-wal`, and `.db-shm` files without lock leakage.
- **Streaming K-Way Merge**: Implements Min-Heap merging with priority shadowing across Base and Horizon databases over multiple paginated steps.

## TEST COVERAGE
Run the complete suite:
```bash
python tests/e2e/run_e2e.py
```
Or via Makefile:
```bash
make -f Makefile.cbm test-e2e
```
Coverage includes 11 comprehensive automated scenarios with zero failures and sub-4-second execution time.
