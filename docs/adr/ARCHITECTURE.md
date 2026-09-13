---
doc_type: adr
domain: architecture
stack: [C, SQLite, Python, JSON-RPC]
node_id: "adr:architecture"
tags: [architecture, design-patterns, multi-graph-federation]
edges:
  - relation: references
    target: "adr:tests"
updated: 2026-09-13
---
# Project Architecture

## OVERVIEW
Layered architecture providing persistent codebase knowledge indexing, multi-graph cognitive horizon federation, and Model Context Protocol (MCP) JSON-RPC tool endpoints over SQLite storage.

## FOLDER STRUCTURE
<folder_structure>
```
[project_root]/
├── src/                          # Core C11 codebase implementing indexing and federation
│   ├── core/                     # Canonical CBM-URI, LRU connection pools, and symbolic nodes
│   ├── query/                    # Cypher execution, k-way merge iterators, and min-heap
│   ├── admission/                # Two-tier AST anchor verification, admission gate, and recall
│   ├── daemon/                   # Background reaper daemon and project lock coordination
│   ├── db/                       # SQLite schema definitions and migrations
│   ├── mcp/                      # JSON-RPC protocol server and federated tool handlers
│   ├── pipeline/                 # Tree-sitter parsing and index generation pipeline
│   └── store/                    # Base graph SQLite persistent storage
├── tests/                        # C unit/integration suites and Python regression tests
└── docs/                         # Technical documentation, ADRs, and feature specifications
    ├── adr/                      # Architectural Decision Records and baseline protocols
    └── feature/                  # Feature domain documentation and source routing
```
</folder_structure>

## LAYERS
- **Transport / Protocol**: MCP JSON-RPC stdio server dispatching tool invocations with `active_horizons` parameters.
- **Federation & Admission**: `AdmissionGate`, `TwoTierAnchor` verification against AST, and `EpistemicRecallService`.
- **Query & Merging**: Streaming `KWayMergeIterator` over ordered cursors combining Base Graph and active horizons.
- **Core & Domain**: `CbmUri` addressing, `SymbolicNode` management, and LRU bounded `HorizonConnectionPool`.
- **Persistence & Daemons**: Base SQLite graph, per-horizon SQLite WAL databases, and `HorizonReaperService`.

## MODULES
| Module | Responsibility | Location |
|---|---|---|
| Core Addressing & Pools | Canonical CBM-URI parsing, FNV-1a hashing, and LRU SQLite connection management | `src/core/` |
| Query & Federation | Streaming K-Way merge iterator with min-heap and Cypher federation | `src/query/` |
| Admission & Recall | Two-tier AST anchor checking, promotion gates, and acyclic reverse recall | `src/admission/` |
| MCP Server & Tools | JSON-RPC dispatch, tool handlers, and federated parameter parsing | `src/mcp/` |
| Daemon & Reaping | Stale horizon cleanup, OS PID verification, and daemon coordination | `src/daemon/` |
| Storage & Schema | SQLite table schemas, migration scripts, and persistent node/edge storage | `src/store/`, `src/db/` |

## PATTERNS
<code_patterns>
# REQUIRED: Pool-managed SQLite handle acquisition with LRU eviction
sqlite3 *db = NULL;
int rc = cbm_horizon_pool_get(&pool, horizon_id, &db);
if (rc == CBM_URI_OK) {
    /* Execute queries against horizon SQLite database */
}

# FORBIDDEN: Direct unmanaged SQLite handle creation outside pool
sqlite3 *db = NULL;
sqlite3_open_v2("unmanaged.db", &db, SQLITE_OPEN_READWRITE, NULL);

# REQUIRED: Dynamic disk database attachment during horizon promotion
sqlite3 *orig_db = gate->base_db;
gate->base_db = project_disk_db;
cbm_admission_gate_admit(gate, horizon_id, anchors, count);
gate->base_db = orig_db;

# FORBIDDEN: Consolidating promoted horizon state into ephemeral in-memory databases
gate->base_db = cbm_store_open_memory(); // Promoted nodes lost on process exit

# REQUIRED: Cycle-pruned traversal with 64-bit FNV-1a VisitedSet
VisitedSet visited = cbm_visited_init();
cbm_visited_add(&visited, uri.hash);

# FORBIDDEN: Unbounded recursive graph traversal without visited guards
traverse_dependencies(node); // Vulnerable to stack overflow on cyclic references
</code_patterns>

## INTEGRATIONS
| External Service / Component | Purpose | Connection / Authentication Method |
|---|---|---|
| SQLite (WAL Mode) | Persistent Base Graph and isolated horizon databases | Native C sqlite3 API with memory-mapped I/O and WAL journaling |
| Model Context Protocol (MCP) | LLM client communication and tool interface | JSON-RPC 2.0 via standard I/O streams (`stdio`) |
| Tree-sitter Runtime | Concrete Syntax Tree parsing and AST symbol signature hashing | In-memory C runtime binding with vendored grammars |
| Host Operating System | Client PID liveness check for daemon reaper | `OpenProcess` (Windows) / `kill(pid, 0)` (POSIX) |
| Host OS & WSL2 Boundary | Cross-platform path canonicalization and inode unlinking | `cbm_path_within_root`, `install -m 755` bypassing `ETXTBSY` |

<!-- DOCUMENT MAP: omitted — this baseline ADR has exactly 1 edge. The ## REFERENCES section below carries the relation. Include ## DOCUMENT MAP with Mermaid graph TD only when 2+ edges exist. -->

## REFERENCES

- [**README.md**](../README.md): Main documentation index.
- [**TESTS.md**](./TESTS.md): Testing strategies, test suites, and execution commands.
