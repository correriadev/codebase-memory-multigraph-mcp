---
doc_type: feature
domain: atomic_admission_concurrency
stack: [C, SQLite, Python, JSON-RPC]
node_id: "feature:atomic-admission-concurrency"
tags: [atomic-admission, concurrency, fail-fast, two-phase-promotion, union-sessions]
edges:
  - relation: implements
    target: "adr:architecture"
    read: must
  - relation: tested_by
    target: "adr:tests"
    read: must
  - relation: references
    target: "feature:cross-horizon-admission"
    read: must
  - relation: references
    target: "feature:ast-anchor-isolation"
    read: optional
    when: "Required when reviewing anchor validation ordering in admission"
updated: 2026-10-09
---
```graph
{"node_id":"feature:atomic-admission-concurrency","domain":"atomic_admission_concurrency","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/promote_handler.c"],"registration_files":["src/union/union_refusal.h","src/union/union_refusal.c"],"reference_files":["src/admission/admission_gate.h"],"code_files":["src/admission/admission_gate.c"],"test_files":["tests/test_atomic_admission_concurrency.py","tests/test_cross_horizon_admission.py","tests/test_anchor_checker.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:atomic-promotion","type":"capability","label":"Atomic Horizon Promotion","definition":"Atomically consolidates verified symbolic nodes and virtual edges into the base graph under strict two-phase commit ordering.","aliases":["2pc-promotion"]},{"id":"rule:two-phase-reconciled-order","type":"rule","label":"Two-Phase Reconciled Commit Order","definition":"Phase 1 COMMIT on base_db must succeed before Phase 2 transitions horizon status to PROMOTED in horizon_metadata.","aliases":["2pc-order"]},{"id":"rule:fail-fast-step-validation","type":"rule","label":"Fail-Fast Step Validation","definition":"Every sqlite3_prepare_v2 and sqlite3_step call during node/edge consolidation must be validated, triggering immediate ROLLBACK on any failure.","aliases":["consolidation-check"]},{"id":"contract:admission-error-codes","type":"contract","label":"Admission Error Taxonomy","definition":"Standardized admission error codes: CONSOLIDATION_FAILED (-7), COMMIT_FAILED (-8), SESSION_REQUIRED (-9), and STATE_TRANSITION_FAILED (-11).","aliases":["f002-errors"]}],"claims":[{"id":"claim:fail-fast-consolidation","subject":"capability:atomic-promotion","relation":"constrained_by","object":"rule:fail-fast-step-validation","statement":"cbm_promote_horizon aborts immediately with ROLLBACK and returns CBM_ADMISSION_ERR_CONSOLIDATION_FAILED (-7) if statement prepare or step fails.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/admission_gate.c","locator":"cbm_promote_horizon: sqlite3_step validation loops","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:two-phase-order","subject":"capability:atomic-promotion","relation":"constrained_by","object":"rule:two-phase-reconciled-order","statement":"Phase 1 COMMIT executes on base_db before Phase 2 cbm_promote_horizon_state is called, preventing partial base writes.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/admission_gate.c","locator":"cbm_promote_horizon: two-phase commit sequence","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:error-contract-boundary","subject":"capability:atomic-promotion","relation":"exposes","object":"contract:admission-error-codes","statement":"cbm_promote_horizon maps failure modes to distinct negative return codes and corresponding JSON-RPC refusal strings.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/admission_gate.h","locator":"CBM_ADMISSION_ERR_* constants","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Atomic Admission & Concurrency Arbitration

Transactional two-phase promotion, fail-fast consolidation validation, and Union session enforcement for horizon changesets.

## OVERVIEW
The Atomic Admission & Concurrency Arbitration subsystem (Feature F002) guarantees all-or-nothing transactional atomicity when promoting changes from ephemeral horizons to the persistent Base Knowledge Graph. It eliminates split-brain corruption via Two-Phase Reconciled commit ordering, validates every SQLite consolidation step, and enforces active Union sessions for mutating promotions.

## FOLDER STRUCTURE
<folder_structure>
```
[project_root]/
├── src/
│   ├── admission/
│   │   ├── admission_gate.c       # Fail-fast node/edge consolidation and two-phase promotion logic
│   │   └── admission_gate.h       # CBM_ADMISSION_ERR_* definitions (-7, -8, -9, -11)
│   ├── mcp/
│   │   └── promote_handler.c      # JSON-RPC tool handler, session gate, and path sanitization
│   └── union/
│       ├── union_refusal.c        # Refusal taxonomy string conversions
│       └── union_refusal.h        # CBM_REFUSAL_* definitions (30, 31, 32)
└── tests/
    ├── test_anchor_checker.c      # Native C multi-dimensional rollback tests
    ├── test_atomic_admission_concurrency.py # Python regression scenarios for atomicity contracts
    └── test_cross_horizon_admission.py      # Concurrency and session enforcement tests
```
</folder_structure>

## KEY INVARIANTS

1. **Two-Phase Reconciled Commit Ordering**:
   - **Phase 1**: Execute `COMMIT;` on `base_db`. On failure, call `ROLLBACK;` and return `CBM_ADMISSION_ERR_COMMIT_FAILED` (-8). Increment `base_generation` only on success.
   - **Phase 2**: Call `cbm_promote_horizon_state(pool, horizon_id)` to update horizon status to `PROMOTED` in `horizon_metadata`.
   - **Invariant**: If Phase 1 fails, Phase 2 is never called and the horizon state remains `ACTIVE`.
2. **Fail-Fast Consolidation Validation**: Every `sqlite3_prepare_v2` and `sqlite3_step` invocation during node and edge consolidation is checked. Any failure triggers an immediate `ROLLBACK;` and returns `CBM_ADMISSION_ERR_CONSOLIDATION_FAILED` (-7). Zero partial nodes or edges leak into `base_db`.
3. **Anchored Consolidation Scoping**: Only `symbolic_nodes` and `virtual_edges` associated with verified anchors are merged into the Base Graph; unanchored entities are excluded.
4. **Relocation Offset Propagation**: Relocated anchor coordinates are written to the consolidated Base Graph nodes.
5. **Union Session Enforcement**: Promotions require an active Union session unless explicitly overridden by `CBM_ALLOW_LEGACY_PROMOTION=1`.
6. **Project Parameter Sanitization**: Project identifiers are bound via prepared statements; path traversal characters (`/`, `\`, `..`) are rejected.

## HOW TO EXECUTE ATOMIC PROMOTION

### Prerequisites
1. Validated candidate horizon with accepted symbolic nodes.
2. Verified `TwoTierAnchor` descriptors.
3. Active Union session opened via `union_session_open`.

### Steps
1. Call `cbm_admission_gate_check_concurrent_conflicts` to verify no overlapping sibling locks exist.
2. Call `cbm_verify_two_tier_anchor` for all candidate anchors.
3. Call `cbm_promote_horizon` to perform the atomic two-phase consolidation.

<code_example>
# CORRECT: Atomic promotion with error inspection
char err_buf[512] = {0};
int rc = cbm_promote_horizon(&gate, &pool, repo_root, horizon_id, anchors, count, err_buf, sizeof(err_buf));
if (rc == CBM_ADMISSION_OK) {
    /* Phase 1 committed and Phase 2 marked horizon PROMOTED */
} else {
    /* base_db rolled back; zero partial state persisted; horizon remains ACTIVE */
}

# WRONG: Updating horizon status before base database commit
cbm_horizon_set_status(pool, horizon_id, "PROMOTED"); // Out of order: risks split-brain if base commit fails
sqlite3_exec(gate->base_db, "COMMIT;", NULL, NULL, NULL);
</code_example>

## PARAMETERS / CONFIGURATIONS

| Error Code | Refusal String | Value | Meaning |
|---|---|---|---|
| `CBM_ADMISSION_ERR_CONSOLIDATION_FAILED` | `CONSOLIDATION_FAILED` | -7 / 30 | Prepare or step failure during node/edge consolidation |
| `CBM_ADMISSION_ERR_COMMIT_FAILED` | `COMMIT_FAILED` | -8 / 31 | SQLite transaction commit failed on base graph |
| `CBM_ADMISSION_ERR_SESSION_REQUIRED` | `SESSION_REQUIRED` | -9 / 32 | Missing active Union session without legacy override |
| `CBM_ADMISSION_ERR_STATE_TRANSITION_FAILED` | `RECONCILIATION_REQUIRED` | -11 | Base committed but metadata update failed |

## REFERENCES

- [**ARCHITECTURE.md**](../adr/ARCHITECTURE.md): Multi-graph federation and transactional boundary.
- [**TESTS.md**](../adr/TESTS.md): Testing protocol, test suites, and execution commands.
- [**ast_anchor_isolation.md**](./ast_anchor_isolation.md): Two-Tier AST anchor verification.
- [**cross_horizon_admission.md**](./cross_horizon_admission.md): Concurrency conflict arbitration and liveness checks.
