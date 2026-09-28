# 004 — Test Scenarios: Specification
## Projeto: `codebase-memory-mcp` | Domínio: `multi_graph_federation`

---

## 1. Unit Tests

### 1.1 Aggregates and Aggregate Roots

#### HorizonAggregate & ConnectionPool
* **Creation & Pool Boundaries:**
  - [ ] **Should create HorizonAggregate successfully when client_pid is valid**
    - **Given** a valid running `client_pid` (e.g. current process ID)
    - **When** the `CreateHorizon` command is executed
    - **Then** a new `HorizonAggregate` is initialized with status `ACTIVE` and an isolated `horizon_id`
  - [ ] **Should evict oldest idle connection via LRU when open connections exceed CBM_MAX_HORIZON_FDS**
    - **Given** a connection pool populated with 16 active SQLite horizon handles
    - **When** a 17th horizon connection is requested
    - **Then** the least recently used horizon connection is closed (`sqlite3_close_v2`), recycling its file descriptors without losing on-disk data
  - [ ] **Should transparently reopen evicted horizon upon new query request**
    - **Given** an evicted horizon whose handle was closed by LRU policy
    - **When** an overlay query targets that horizon ID
    - **Then** the connection pool reopens the database file and executes the query normally

* **Commands and State Transitions:**
  - [ ] **Should transition to PROMOTED when promotion succeeds**
    - **Given** an `ACTIVE` `HorizonAggregate` with verified Two-Tier anchors
    - **When** the `PromoteHorizon` command is applied
    - **Then** the horizon transitions to `PROMOTED` state and emits `HorizonPromotedEvent`
  - [ ] **Should transition to DISCARDED when discard is requested**
    - **Given** an `ACTIVE` `HorizonAggregate`
    - **When** the `DiscardHorizon` command is applied
    - **Then** the horizon transitions to `DISCARDED` state and its backing storage is removed

#### SymbolicNode & Dangling Nodes
* **Acyclic Safety:**
  - [ ] **Should prune cyclic traversal when dangling nodes reference each other circularly**
    - **Given** dangling node A pointing to dangling node B, and B pointing back to A
    - **When** a BFS reverse search or overlay traversal is executed
    - **Then** the traversal uses `VisitedSet` to detect the revisit, terminates cleanly, and avoids stack overflow or infinite loops

---

### 1.2 Value Objects

#### CbmUri
* **Validation & Interning:**
  - [ ] **Should parse valid CbmUri string successfully**
    - **Given** the URI string `cbm://repo/pkg/auth/token.go#ValidateToken`
    - **When** parsed into a `CbmUri` Value Object
    - **Then** `repo`, `path`, and `symbol` components are correctly populated with computed FNV-1a hash
  - [ ] **Should reject CbmUri missing symbol fragment**
    - **Given** the URI string `cbm://repo/pkg/auth/token.go` (without `#`)
    - **When** parsed into a `CbmUri` Value Object
    - **Then** parsing fails with `MALFORMED_URI_SYNTAX`

#### TwoTierAnchor
* **Resilience against Offset Shift:**
  - [ ] **Should accept anchor via fast-path when byte offset matches exactly**
    - **Given** a `TwoTierAnchor` where byte range 100..150 matches the file exactly
    - **When** `VerifyTwoTierAnchorService` evaluates the anchor
    - **Then** verification succeeds on Tier 1 (fast-path) without calling AST parsing
  - [ ] **Should accept anchor via Tier 2 (AST Signature) when byte offset shifted due to inserted comments**
    - **Given** a file where 5 lines of comments were added at the top, shifting the byte offset
    - **And** the symbol's AST signature and body content remain completely identical
    - **When** `VerifyTwoTierAnchorService` evaluates the anchor
    - **Then** Tier 1 fails, Tier 2 locates the symbol on the AST, validates identical `ast_signature_hash`, and returns true without flagging false drift

---

### 1.3 Domain Services

#### KWayMergeIterator (Streaming Pagination)
* **Execution & Memory Bound:**
  - [ ] **Should paginate results in O(K) memory without materializing full dataset**
    - **Given** a Base Graph with 20,000 nodes and an active Horizon with 1,000 nodes
    - **When** a query is executed with `LIMIT 50` and `SKIP 100`
    - **Then** the `KWayMergeIterator` consumes rows in streaming order via Min-Heap and terminates after emitting exactly 50 records, keeping heap memory bounded by the number of active horizons ($K$)

#### HorizonReaperService
* **Garbage Collection:**
  - [ ] **Should reap horizon files when client PID is dead and TTL is exceeded**
    - **Given** an orphan horizon database file whose metadata indicates a dead PID (verified via `OpenProcess` / `kill 0`) and creation time older than 1 hour
    - **When** `cbm_reap_orphan_horizons` is executed by the daemon worker
    - **Then** the orphan `.db`, `-wal` and `-shm` files are deleted from the disk and an audit event is logged
  - [ ] **Should retain horizon files when client PID is still alive regardless of age**
    - **Given** an active long-running horizon belonging to a live client PID
    - **When** the reaper runs
    - **Then** the horizon is left untouched

---

## 2. Integration Tests

### 2.1 File Descriptor Safety under Load
* **Should never exceed CBM_MAX_HORIZON_FDS during parallel horizon queries:**
  - **Given** 32 distinct horizon databases created in the test directory
  - **When** a federated query accesses all 32 horizons in a single batch
  - **Then** the operating system open file handles for `.db`, `-wal` and `-shm` never exceed the configured ceiling of 48 handles (16 open databases)

### 2.2 Component Interaction Tests
* **Should merge Base Graph and Horizon nodes seamlessly with streaming overlays:**
  - **Given** a Base Graph containing function `GetUser`
  - **And** an active Horizon proposing `GetUserV2` with virtual edge `REPLACES` to `GetUser`
  - **When** a Cypher query is executed with `active_horizons = ["test-horizon"]`
  - **Then** the query result streams both nodes and the synthetic `REPLACES` edge correctly

---

## 3. End-to-End / Acceptance Scenarios

### 3.1 Crash Recovery & Reaper Scenario
* **Given** an AI Agent session that opens an ephemeral horizon and abnormally crashes (simulated process termination)
* **When** the codebase-memory shared daemon initiates its scheduled maintenance cycle
* **Then** the reaper detects the ungraceful termination via the dead PID check
* **And** safely unlinks all associated SQLite files, reclaiming disk space without administrator intervention

### 3.2 Refactoring with Comment Edits (Zero False Drift)
* **Given** an active horizon proposing a refactored interface anchored to `TokenValidator`
* **When** a developer adds a copyright header at the top of `token.go` before promoting
* **And** triggers `promote_horizon`
* **Then** the Two-Tier Anchor checker catches the shifted byte offset, validates the AST signature hash successfully, and admits the refactoring without raising a false-positive drift error
