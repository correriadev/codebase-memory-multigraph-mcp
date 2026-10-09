---
doc_type: feature
domain: transitive_recall_closure
stack: [C, SQLite, Python]
node_id: "feature:transitive-recall-closure"
tags: [recall-engine, transitive-closure, epistemic-recall, causal-edges, contestation]
edges:
  - relation: implements
    target: "adr:architecture"
    read: must
  - relation: tested_by
    target: "adr:tests"
    read: must
  - relation: references
    target: "feature:cross-horizon-admission"
    read: optional
    when: "Required when evaluating epistemic recall during admission conflict resolution"
updated: 2026-10-09
---
```graph
{"node_id":"feature:transitive-recall-closure","domain":"transitive_recall_closure","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/admission/recall_engine.h"],"registration_files":[],"reference_files":["src/admission/recall_engine.c"],"code_files":[],"test_files":["tests/test_transitive_recall_closure.py","tests/test_recall_engine.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:transitive-recall","type":"capability","label":"Transitive Recall Closure","definition":"Traverses reverse causal dependency graphs to transitively contest derived symbols upon root invalidation.","aliases":["epistemic-recall"]},{"id":"rule:causal-edge-filtering","type":"rule","label":"Causal Edge Filtering","definition":"Reverse dependency traversal expands only across causal derivation edges (DEPENDS_ON, DERIVED_FROM, CALLS, IMPLEMENTS, EXTENDS), ignoring non-causal edges.","aliases":["causal-filter"]},{"id":"rule:transactional-recall-consistency","type":"rule","label":"Transactional Recall Consistency","definition":"BEGIN IMMEDIATE must execute before BFS traversal, ensuring traversal and status UPDATE operate within the exact same database snapshot.","aliases":["recall-tx"]},{"id":"contract:recall-engine-api","type":"contract","label":"Recall Engine API","definition":"cbm_trigger_recall marks root and dependents CONTESTED within an atomic transaction, returning RecallReport with total metrics.","aliases":["cbm_trigger_recall"]}],"claims":[{"id":"claim:causal-traversal-boundary","subject":"capability:transitive-recall","relation":"constrained_by","object":"rule:causal-edge-filtering","statement":"cbm_bfs_reverse_deps_acyclic filters exclusively by causal derivation edges, ignoring DOC_REF and TAGGED_WITH.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/recall_engine.c","locator":"cbm_bfs_reverse_deps_acyclic: SQL WHERE edge_type IN clause","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:transactional-consistency","subject":"capability:transitive-recall","relation":"constrained_by","object":"rule:transactional-recall-consistency","statement":"cbm_trigger_recall begins transaction before BFS traversal and commits only after all UPDATE statements succeed.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/recall_engine.c","locator":"cbm_trigger_recall: BEGIN IMMEDIATE before BFS call","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:api-contract-boundary","subject":"capability:transitive-recall","relation":"exposes","object":"contract:recall-engine-api","statement":"cbm_trigger_recall populates total_affected_count and is_committed in RecallReport, rolling back on any SQLite failure.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/recall_engine.h","locator":"RecallReport struct and CBM_RECALL_ERR_TX_FAILED","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Transitive Recall Closure

Causal reverse-dependency graph traversal, cycle-protected BFS, and atomic transitive contestation cascading for invalidated knowledge nodes.

## OVERVIEW
The Transitive Recall Closure subsystem (Feature F003) maintains epistemic graph integrity when an admitted node is contested, revoked, or superseded. It performs reverse breadth-first search (BFS) over causal derivation edges, identifies all downstream dependent entities without depth limits, and atomically marks them `CONTESTED` within a single SQLite transaction.

## FOLDER STRUCTURE
<folder_structure>
```
[project_root]/
├── src/
│   └── admission/
│       ├── recall_engine.c        # Acyclic BFS traversal, causal edge filtering, and transactional updates
│       └── recall_engine.h        # RecallReport, VisitedSet, and cbm_trigger_recall API
└── tests/
    ├── test_recall_engine.c       # Native C unit tests for VisitedSet, FNV-64 collisions, and capacity
    └── test_transitive_recall_closure.py # Python regression scenarios for causal traversal and rollbacks
```
</folder_structure>

## KEY INVARIANTS

1. **Causal Edge Filtering**: Reverse traversal expands strictly across causal edges (`DEPENDS_ON`, `DERIVED_FROM`, `CALLS`, `IMPLEMENTS`, `EXTENDS`). Non-causal associations (`DOC_REF`, `TAGGED_WITH`) are ignored.
2. **Unbounded Traversal**: When `max_depth == 0`, traversal proceeds until the directed acyclic graph (DAG) is exhausted.
3. **Transactional Isolation & Consistency**: `BEGIN IMMEDIATE;` is acquired before executing the reverse BFS query. Traversal and subsequent status updates run within the identical database snapshot.
4. **All-or-Nothing Atomic Invalidation**: If any `UPDATE symbolic_nodes SET epistemic_status = 'CONTESTED'` step or the final `COMMIT;` fails, an immediate `ROLLBACK;` occurs, `is_committed` is set to `false`, and `CBM_RECALL_ERR_TX_FAILED` (-10) is returned.
5. **Dynamic Capacity & Collision Resistance**: The `VisitedSet` hash table grows dynamically beyond initial boundaries, handling synthetic FNV-64 collisions via exact URI comparison.
6. **Null DB Safety**: Invoking recall functions with `db == NULL` returns `-1` immediately with `is_committed = false`.

## HOW TO TRIGGER TRANSITIVE RECALL

### Prerequisites
1. Open persistent SQLite database connection containing `symbolic_nodes` and `virtual_edges`.
2. Valid canonical CBM-URI representing the contested root entity.

### Steps
1. Initialize a `RecallReport` structure.
2. Call `cbm_trigger_recall` with database handle, root URI, reason, and report pointer.
3. Check returned status code and report `is_committed` flag.

<code_example>
# CORRECT: Atomic recall invocation with status validation
RecallReport report = {0};
int rc = cbm_trigger_recall(db, "cbm://proj/src/core.c#func", "upstream invariant violated", &report);
if (rc == 0 && report.is_committed) {
    /* Root and all downstream causal dependents successfully marked CONTESTED */
    printf("Invalidated %zu entities\n", report.total_affected_count);
} else {
    /* Transaction rolled back; zero statuses modified */
}

# WRONG: Running BFS traversal outside transaction boundary
cbm_bfs_reverse_deps_acyclic(db, root_uri, 0, &report); // Read without transaction lock
sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL); // TOCTOU drift hazard between read and update
</code_example>

## PARAMETERS / CONFIGURATIONS

| Field / Constant | Type | Description |
|---|---|---|
| `CBM_RECALL_MAX_AFFECTED` | Constant (256) | Fixed array capacity in report struct for summary serialization |
| `CBM_RECALL_ERR_TX_FAILED` | Constant (-10) | Error return code for transaction or prepare/step failures |
| `contested_root` | `CbmUri` | Canonical URI of the initial contested entity |
| `affected_uris` | `char[256][512]` | Sample of affected dependent URIs |
| `affected_count` | `size_t` | Count of populated entries in `affected_uris` |
| `total_affected_count` | `size_t` | Total count of all downstream nodes marked `CONTESTED` |
| `is_committed` | `bool` | True if transaction successfully committed to storage |

## REFERENCES

- [**ARCHITECTURE.md**](../adr/ARCHITECTURE.md): Multi-graph federation and epistemic recall layer.
- [**TESTS.md**](../adr/TESTS.md): Testing protocol, test suites, and execution commands.
- [**atomic_admission_concurrency.md**](./atomic_admission_concurrency.md): Atomic promotion transactions.
- [**cross_horizon_admission.md**](./cross_horizon_admission.md): Admission gate integration and conflict management.
