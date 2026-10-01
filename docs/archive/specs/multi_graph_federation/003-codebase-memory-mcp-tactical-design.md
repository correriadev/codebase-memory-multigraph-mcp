# 003 — Tactical Design: Solution Space
## Projeto: `codebase-memory-mcp` | Domínio: `multi_graph_federation`

---

## 0. Refinement Questions and Answers

| ID | Category | Question | Recommendation | Final Answer | Answered By |
|---|---|---|---|---|---|
| Q01 | architecture | Como os grafos efêmeros de horizontes cognitivos (Camada 1) devem ser armazenados e isolados do Grafo Base? | Arquivos SQLite dedicados por horizonte (`horizons/<id>.db`) com WAL mode, garantindo isolamento total de escrita e zero contenção de lock com o Grafo Base. | Arquivos SQLite dedicados por horizonte (`horizons/<id>.db`) com WAL mode, garantindo isolamento total de escrita e zero contenção de lock com o Grafo Base. | human |
| Q02 | integration | Como o motor deve expor consultas federadas com overlays na interface MCP? | Parâmetro opcional `active_horizons[]` nas ferramentas MCP existentes (`query_graph`, `search_graph`, `trace_path`), preservando a sintaxe Cypher padrão e compatibilidade com clientes legados. | Parâmetro opcional `active_horizons[]` nas ferramentas MCP existentes (`query_graph`, `search_graph`, `trace_path`), preservando a sintaxe Cypher padrão e compatibilidade com clientes legados. | human |
| Q03 | boundary | Qual é a responsabilidade do codebase-memory durante a promoção de um horizonte (`promote_horizon`)? | Gate de Admissão: o motor valida âncoras verbatim e invariantes contra o disco; o agente/humano aplica o código, e o `promote_horizon` valida e consolida a transição na Base. | Gate de Admissão: o motor valida âncoras verbatim e invariantes contra o disco; o agente/humano aplica o código, e o `promote_horizon` valida e consolida a transição na Base. | human |
| Q04 | data | Como símbolos futuros/inexistentes no código (*dangling nodes*) devem ser identificados no Grafo de Horizontes? | CBM-URI canônico determinístico com flags `status: proposed` e `is_dangling: true`, permitindo ancoragem tardia (*late-binding*) automática quando o símbolo for indexado na Base. | CBM-URI canônico determinístico com flags `status: proposed` e `is_dangling: true`, permitindo ancoragem tardia (*late-binding*) automática quando o símbolo for indexado na Base. | human |
| Q05 | performance | Como o motor previne a exaustão de file descriptors (especialmente o teto de 512 do CRT no Windows) com dezenas de horizontes SQLite WAL abertos? | `HorizonConnectionPool` com política LRU e teto estrito de conexões ativas (default 16 FDs ativos), fechando conexões ociosas e configurando `_setmaxstdio(2048)` no Windows. | `HorizonConnectionPool` com LRU e teto de 16 conexões ativas; fecha handles ociosos e eleva `_setmaxstdio(2048)`. | model (audited) |
| Q06 | performance | Como a paginação (`LIMIT`/`SKIP`) deve operar em consultas com overlay sem materializar o resultado completo em memória? | Streaming K-Way Merge com Min-Heap: lê cursores ordenados de cada banco em pipeline $O(K)$, aplicando shadowing e emitindo registros sob demanda até atingir o `LIMIT`. | Streaming K-Way Merge com Min-Heap ($O(K)$ em RAM), paginando sobre cursores ordenados da Base e Horizontes. | model (audited) |
| Q07 | maintainability | Qual mecanismo impede o acúmulo de bancos SQLite órfãos deixados por processos de clientes MCP que sofreram crash? | `HorizonReaperService` no daemon: varredura periódica que valida se o `client_pid` do criador ainda existe no SO e remove bancos órfãos cujo TTL excedeu 1 hora. | Reaper em background no daemon validando liveness de `client_pid` e TTL de 1h para remoção de `.db`, `-wal` e `-shm`. | model (audited) |
| Q08 | reliability | Como o Admission Gate evita falsos positivos de anchor drift provocados por deslocamento de offsets de bytes (ex: comentários no início do arquivo)? | `TwoTierAnchor`: testa byte offset rápido primeiro; se divergente, localiza o símbolo pela AST e compara o hash de corpo e assinatura (`ast_signature_hash`). | Two-Tier Anchor: fast-path por byte offset com fallback para hash de assinatura do símbolo na AST do Tree-sitter. | model (audited) |
| Q09 | reliability | Como o motor evita recursões e loops infinitos diante de ciclos de nós especulativos (*dangling nodes*) apontando entre si? | `VisitedSet` com hash FNV-1a de 64 bits em todas as travessias de grafo (BFS/DFS) e teto estrito de profundidade (`max_depth = 5`). | VisitedSet por hash de CBM-URI de 64 bits em BFS/DFS com profundidade máxima de 5 níveis. | model (audited) |

---

## 1. Main Structure (Aggregates & Invariants)

| Element | Layer / Type | Invariants / Tech Rules | 4-line Snippet |
|---|---|---|---|
| `HorizonAggregate` | Domain / Aggregate Root | Um Horizon pertence a exatamente uma sessão; não pode mutar o Base Graph; status deve ser `ACTIVE`, `PROMOTED` ou `DISCARDED`. | ```struct HorizonAggregate { HorizonId id; uint32_t client_pid; HorizonStatus status; uint64_t last_heartbeat; };``` |
| `HorizonConnectionPool` | Infrastructure / Pool | Mantém no máximo `CBM_MAX_HORIZON_FDS` (default 16) conexões SQLite abertas; despeja via LRU fechando handles ociosos. | ```struct HorizonConnectionPool { sqlite3 *open_handles[16]; HorizonId active_ids[16]; uint64_t lru_ticks[16]; size_t count; };``` |
| `SymbolicNodeAggregate` | Domain / Entity | Um nó simbólico deve possuir CBM-URI válido; `is_dangling=true` exige `status=PROPOSED`. | ```struct SymbolicNode { CbmUri uri; NodeLabel label; EpistemicStatus status; bool is_dangling; };``` |
| `AdmissionGateAggregate` | Domain / Aggregate Root | Rejeita promoção se qualquer âncora falhar na AST (`ANCHOR_DRIFT`); recall propaga status `CONTESTED` para todo $deps^{-1}$. | ```struct AdmissionGate { ProjectId project_id; uint64_t base_generation; uint32_t active_recalls_count; };``` |
| `EvidenceAggregate` | Domain / Aggregate Root | Atestações empíricas são append-only; uma atestação nunca pode ser sobrescrita ou deletada. | ```struct EvidenceStore { ProjectId project_id; AttestationId latest_id; uint64_t total_attestations; };``` |

---

## 2. Value Objects / Types / Interfaces

| Name | Context / Layer | Validation & Typing Rules | 4-line Snippet |
|---|---|---|---|
| `CbmUri` | Shared Kernel / VO | Formato canônico `cbm://<repo>/<path>#<symbol>`. Imutável, livre de alocação dinâmica quando internado. | ```struct CbmUri { const char *repo; const char *path; const char *symbol; uint64_t hash; };``` |
| `TwoTierAnchor` | Admission / VO | Offset de bytes de primeira fase + hash SHA256 da assinatura do nó na AST para resiliência a deslocamentos. | ```struct TwoTierAnchor { uint32_t byte_start; uint32_t byte_len; uint64_t ast_signature_hash; const char *expected_text; };``` |
| `VirtualEdge` | Federated Query / VO | Conecta dois nós (Base ou Horizon) em tempo de query; possui tipo (`VIRTUAL_CALLS`, `REPLACES`). | ```struct VirtualEdge { CbmUri source; CbmUri target; EdgeType type; HorizonId origin_horizon; };``` |
| `AttestationProof` | Evidence / VO | Representa resultado de execução com código de saída, payload de log truncado e timestamp imutável. | ```struct AttestationProof { int32_t exit_code; uint64_t executed_at; const char *producer; const char *log_snippet; };``` |

---

## 3. Domain Services / Use Cases

| Service / Use Case | Layer | Responsibility | 4-line Snippet |
|---|---|---|---|
| `CreateHorizonUseCase` | Application | Inicializa banco SQLite isolado com schema de horizonte sob `$CBM_CACHE_DIR/horizons/<id>.db`. | ```int cbm_create_horizon(uint32_t pid, HorizonId *out_id) { char db_path[PATH_MAX]; snprintf(db_path, sizeof(db_path), "%s/horizons/%s.db", cbm_cache_dir(), out_id); return cbm_sqlite_init_horizon(db_path, pid); }``` |
| `KWayMergeIterator` | Domain Service | Consome cursores ordenados da Base e Horizontes ativos via Min-Heap sem materializar todo o dataset em RAM. | ```int cbm_kway_merge_step(KWayMergeContext *ctx, Record *out_record, bool *has_more) { return cbm_min_heap_pop_shadowed(ctx->heap, out_record, has_more); }``` |
| `VerifyTwoTierAnchorService` | Domain Service | Testa offset direto; se falhar, busca símbolo na AST do Tree-sitter e compara `ast_signature_hash`. | ```int cbm_verify_two_tier_anchor(const char *root, const TwoTierAnchor *a, bool *ok) { if (cbm_fast_offset_match(root, a)) return (*ok = true), 0; return cbm_ast_signature_match(root, a, ok); }``` |
| `HorizonReaperService` | Domain Service | Varre diretório de horizontes a cada 15min; remove bancos com PID morto e TTL > 1h. | ```void cbm_reap_orphan_horizons(void) { cbm_scan_horizons_dir(cbm_is_pid_dead_and_expired, cbm_unlink_horizon_files); }``` |
| `EpistemicRecallService` | Domain Service | Executa BFS reverso com `VisitedSet` (FNV-1a 64-bit) podando ciclos e emitindo `SymbolContestedEvent`. | ```int cbm_trigger_recall(const CbmUri *contested_root, RecallReport *out_report) { VisitedSet v = cbm_visited_init(); return cbm_bfs_reverse_deps_acyclic(contested_root, &v, 5, out_report); }``` |

---

## 4. Domain Events

| Event Name | Emitter | Payload | Consumers |
|---|---|---|---|
| `HorizonCreatedEvent` | `HorizonAggregate` | `horizon_id, client_pid, timestamp` | Federated Query Engine, Horizon Pool |
| `SymbolicNodeProposedEvent` | `HorizonAggregate` | `horizon_id, cbm_uri, label, is_dangling` | Overlay Cache, Audit Logger |
| `AnchorDriftDetectedEvent` | `AdmissionGate` | `cbm_uri, expected_ast_hash, actual_ast_hash` | Agent Notification, Recall Engine |
| `HorizonPromotedEvent` | `AdmissionGate` | `horizon_id, project_id, promoted_nodes_count` | Base Indexer, Daemon Reindex Worker |
| `OrphanHorizonReapedEvent` | `HorizonReaperService` | `horizon_id, dead_pid, freed_bytes` | Daemon Logger, Audit Trail |
| `SymbolContestedEvent` | `EpistemicRecallService` | `contested_uri, reason, affected_uris[]` | Active Horizons, Agent Workspace |

---

## 5. Repositories / Ports / Persistence

| Repository / Port | Operations | Storage Technology | Consistency Mode |
|---|---|---|---|
| `BaseGraphRepository` | `find_by_uri, trace_calls, run_cypher` | SQLite (WAL Mode, read-only mmap) | Strong Consistency (Pinned Generation) |
| `HorizonRepository` | `create, insert_node, insert_edge, discard` | SQLite dedicado por horizonte gerenciado via `HorizonConnectionPool` | Eventual per session / Isolated |
| `EvidenceRepository` | `record_attestation, get_attestations_for_uri` | SQLite append-only (`evidence.db`) | Immediate Consistency |
| `FilesystemVerificationPort` | `read_slice, parse_ast_signature` | Native OS File I/O + Tree-sitter in-memory parse | Read-through on Demand |

---

## 6. Ordered Development Tasks

```json
[
  {
    "id": "01",
    "title": "Implement CBM-URI Parser & Interning Library",
    "description": "Deterministic parser for cbm://<repo>/<path>#<symbol> with FNV-1a hash calculation and zero-allocation slicing.",
    "scope": [
      "src/core/cbm_uri.h",
      "src/core/cbm_uri.c",
      "tests/test_cbm_uri.c"
    ],
    "acceptance": [
      "Parses valid cbm URIs into components",
      "Rejects malformed URIs without symbol fragment",
      "Computes deterministic FNV-1a 64-bit hash"
    ],
    "depends_on": null
  },
  {
    "id": "02",
    "title": "Implement Horizon SQLite Schema & Connection Pool (LRU)",
    "description": "Migration DDL and connection pool with strict ceiling of 16 active connections for horizons/<id>.db with indexes on cbm_uri.",
    "scope": [
      "src/core/horizon_pool.h",
      "src/core/horizon_pool.c",
      "src/db/horizon_schema.sql"
    ],
    "acceptance": [
      "Initializes isolated SQLite horizon DB",
      "Enforces LRU eviction at 16 active DB handles",
      "Transparently reopens evicted horizon on query"
    ],
    "depends_on": "01"
  },
  {
    "id": "03",
    "title": "Implement Horizon Reaper Daemon Worker",
    "description": "Client PID liveness verification on Windows (OpenProcess) and POSIX (kill 0) with safe unlinking of orphan horizon files after TTL.",
    "scope": [
      "src/daemon/horizon_reaper.h",
      "src/daemon/horizon_reaper.c",
      "tests/test_horizon_reaper.c"
    ],
    "acceptance": [
      "Reaps horizon files when client PID is dead and TTL > 1h",
      "Retains active horizons with living PID"
    ],
    "depends_on": "02"
  },
  {
    "id": "04",
    "title": "Implement Symbolic Node & Dangling Reference CRUD",
    "description": "Node insertion and querying supporting is_dangling flag and status: proposed, preventing cycles via VisitedSet.",
    "scope": [
      "src/core/symbolic_node.h",
      "src/core/symbolic_node.c",
      "src/core/visited_set.h"
    ],
    "acceptance": [
      "Stores and queries proposed symbolic nodes",
      "Handles dangling references without base node present",
      "Prunes circular traversals using VisitedSet"
    ],
    "depends_on": "02"
  },
  {
    "id": "05",
    "title": "Implement Streaming K-Way Merge Iterator",
    "description": "Sorted merge iterator using Min-Heap in C with O(K) RAM bounds for LIMIT and SKIP pagination.",
    "scope": [
      "src/query/kway_merge.h",
      "src/query/kway_merge.c",
      "tests/test_kway_merge.c"
    ],
    "acceptance": [
      "Streams merged ordered rows from base and horizons",
      "Maintains O(K) memory footprint",
      "Respects LIMIT and SKIP offsets correctly"
    ],
    "depends_on": "04"
  },
  {
    "id": "06",
    "title": "Extend MCP Tools with active_horizons Parameter",
    "description": "Add optional active_horizons[] parameter parsing in query_graph, search_graph, and trace_path tools.",
    "scope": [
      "src/mcp/mcp.c",
      "src/mcp/handlers.c",
      "tests/test_mcp_federation.c"
    ],
    "acceptance": [
      "Parses active_horizons parameter in MCP calls",
      "Integrates with K-Way merge engine without breaking legacy calls"
    ],
    "depends_on": "05"
  },
  {
    "id": "07",
    "title": "Implement Two-Tier Anchor Checker",
    "description": "Fast-path byte offset verification with fallback to Tree-sitter AST symbol signature hash comparison.",
    "scope": [
      "src/admission/anchor_checker.h",
      "src/admission/anchor_checker.c",
      "tests/test_anchor_checker.c"
    ],
    "acceptance": [
      "Fast path matches unchanged byte offsets",
      "Tier 2 AST hash matches when comments shift offsets",
      "Detects actual drift when symbol body diverges"
    ],
    "depends_on": "01"
  },
  {
    "id": "08",
    "title": "Implement Admission Gate promote_horizon Tool",
    "description": "MCP promotion tool validating Two-Tier anchors against disk and executing atomic transition to base.",
    "scope": [
      "src/admission/admission_gate.h",
      "src/admission/admission_gate.c",
      "src/mcp/promote_handler.c"
    ],
    "acceptance": [
      "Validates anchors before admitting horizon",
      "Rejects promotion on anchor drift",
      "Transitions horizon to PROMOTED"
    ],
    "depends_on": "07"
  },
  {
    "id": "09",
    "title": "Implement Acyclic BFS Reverse Recall Engine",
    "description": "Reverse dependency closure (deps^-1) calculation with cycle pruning to propagate CONTESTED status.",
    "scope": [
      "src/admission/recall_engine.h",
      "src/admission/recall_engine.c",
      "tests/test_recall_engine.c"
    ],
    "acceptance": [
      "Traverses reverse dependencies up to depth limit",
      "Avoids infinite loops on cyclic graphs",
      "Emits contestation reports accurately"
    ],
    "depends_on": "08"
  }
]
```
