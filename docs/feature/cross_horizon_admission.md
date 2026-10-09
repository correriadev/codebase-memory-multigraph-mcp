---
doc_type: feature
domain: cross_horizon_admission
stack: [C, SQLite, Python, JSON-RPC]
node_id: "feature:cross-horizon-admission"
tags: [admission-gate, concurrency, horizon-pool, two-tier-anchors, conflict-arbitration]
edges:
  - relation: implements
    target: "adr:architecture"
    read: must
  - relation: tested_by
    target: "adr:tests"
    read: must
  - relation: references
    target: "feature:multi-graph-federation"
    read: must
  - relation: references
    target: "feature:ast-anchor-isolation"
    read: optional
    when: "Required when inspecting two-tier AST anchor verification and relocation"
  - relation: references
    target: "feature:atomic-admission-concurrency"
    read: optional
    when: "Required when inspecting two-phase promotion ordering and error codes"
  - relation: references
    target: "feature:transitive-recall-closure"
    read: optional
    when: "Required when inspecting epistemic contestation and reverse dependency recall"
updated: 2026-10-09
---
```graph
{"node_id":"feature:cross-horizon-admission","domain":"cross_horizon_admission","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/promote_handler.c","src/mcp/horizon_sync_handler.c"],"registration_files":["src/mcp/mcp.c","src/mcp/horizon_sync_handler.h"],"reference_files":["src/admission/admission_gate.h","src/core/horizon_pool.h"],"code_files":["src/admission/admission_gate.c","src/core/horizon_pool.c","src/union/union_refusal.c","src/union/union_refusal.h"],"test_files":["tests/test_cross_horizon_admission.py"]}
```

# Cross-Horizon Admission & Concurrency Arbitration
Arbitrates concurrent promotion requests between parallel ephemeral horizons, shifts merge conflict anticipation left, and serializes Base Graph consolidation under atomic SQLite transactions.

## OVERVIEW
The Cross-Horizon Admission subsystem extends the Admission Gate into a multi-horizon coexistence arbiter. It verifies that candidate anchors do not collide with files or symbols actively modified by living sibling horizons, purges abandoned zombie sessions via OS process liveness probing, and enforces atomic consolidation using `BEGIN IMMEDIATE` serialization on the persistent Base Graph.

## FOLDER STRUCTURE
```
[project_root]/
├── src/
│   ├── admission/
│   │   ├── admission_gate.c       # Concurrency scanning, conflict reports, atomic consolidation
│   │   ├── admission_gate.h       # CBM_ADMISSION_ERR_CONCURRENT_CONFLICT (-6), HorizonConflictReport
│   │   ├── anchor_checker.c       # Two-tier anchor verification against filesystem
│   │   └── anchor_checker.h       # Anchor verification definitions
│   ├── core/
│   │   ├── horizon_pool.c         # Process liveness probe, zombie pruning, active horizon retrieval
│   │   └── horizon_pool.h         # ActiveHorizonLiveness descriptor and connection pool declarations
│   ├── mcp/
│   │   ├── horizon_sync_handler.c # check_horizon_conflicts tool handler and bounded spec sync
│   │   ├── mcp.c                  # Tool dispatch registry
│   │   └── promote_handler.c      # CONCURRENT_CONFLICT JSON-RPC refusal formatting
│   └── union/
│       ├── union_refusal.c        # Refusal taxonomy string mapping
│       └── union_refusal.h        # CBM_REFUSAL_CONCURRENT_CONFLICT (17) definition
└── tests/
    └── test_cross_horizon_admission.py # Unit, integration, and E2E scenario validation suite
```

## INTEGRATION POINTS
- **`promote_horizon`**: Prior to executing Two-Tier anchor verification, invokes `cbm_admission_gate_check_concurrent_conflicts`. Returns `CBM_ADMISSION_ERR_CONCURRENT_CONFLICT` (-6) upon physical or semantic overlap.
- **`validate_scope_horizon`**: Accepts optional boolean argument `check_conflicts` to return active sibling collisions non-destructively.
- **`check_horizon_conflicts`**: Dedicated MCP inspection tool returning structured `CLEAN` or `CONFLICT` diagnoses.

## KEY INVARIANTS
1. **Liveness Verification**: Sibling horizons are inspected only if their registered owner PID is confirmed alive by host OS probes (`kill(pid, 0)` on POSIX / `OpenProcess` on Windows). Dead processes are purged as zombies.
2. **Shift-Left Collision Block**: Physical file overlaps trigger immediate promotion rejection before filesystem verification or Base Graph mutation.
3. **Atomic Promotion Serialization & TOCTOU Protection**: Base Graph consolidation operates strictly under `BEGIN IMMEDIATE;` ... `COMMIT;` transaction locks. State transition to `PROMOTED` occurs within this transactional boundary only after successful table updates. Crucially, immediately upon acquiring the lock, a TOCTOU `generation_check` queries `SELECT MAX(generation) FROM generation_log`. If the generation has drifted during anchor verification, the transaction immediately executes `ROLLBACK;` and returns `CBM_ADMISSION_ERR_CONCURRENT_CONFLICT`.
4. **Query Pushdown Collision Filtering**: Sibling collision analysis offloads file and symbol pattern searches directly to SQLite (`SELECT ... WHERE cbm_uri LIKE ? LIMIT 1`) with parameter binding and short-circuiting, eliminating in-memory $O(N \times M)$ scan loops.
5. **Connection Pool LRU Capacity**: Connection pool file descriptor capacity is expanded to `CBM_MAX_HORIZON_FDS = 64` (from baseline 16) to prevent handle thrashing across concurrent multi-horizon agent sessions.
6. **Input Sanitization**: Project identifiers are bound using parameterized SQL statements; file paths are verified against directory traversal (`..`) and clamped to 10 MB maximum buffer size.

## REFERENCES
- [**ARCHITECTURE.md**](../adr/ARCHITECTURE.md): Multi-graph federation and layered system architecture.
- [**TESTS.md**](../adr/TESTS.md): Test execution protocol, suites, and coverage standards.
- [**multi_graph_federation.md**](./multi_graph_federation.md): Epistemic horizons and AST anchor verification.
- [**ast_anchor_isolation.md**](./ast_anchor_isolation.md): Two-Tier AST anchor verification and relocation.
- [**atomic_admission_concurrency.md**](./atomic_admission_concurrency.md): Fail-fast consolidation and two-phase promotion.
- [**transitive_recall_closure.md**](./transitive_recall_closure.md): Reverse causal BFS and atomic contestation.
