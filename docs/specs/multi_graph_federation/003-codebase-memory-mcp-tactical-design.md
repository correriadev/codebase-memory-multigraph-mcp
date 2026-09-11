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

## 6. Ordered Implementation Tasks

Tarefas estritamente ordenadas por dependência para execução via TDD:

1. **Task 1 — CBM-URI Parser & Interning Library:** Implementar parser determinístico para `cbm://<repo>/<path>#<symbol>` com cálculo de hash FNV-1a e zero-allocation slicing.
2. **Task 2 — Horizon SQLite Schema & Connection Pool (LRU):** Criar DDL de migração e pool com teto estrito de conexões (máx. 16 FDs) para `horizons/<id>.db` com índices em `cbm_uri`.
3. **Task 3 — Horizon Reaper Daemon Worker:** Implementar verificação de liveness de PID no Windows (`OpenProcess`) e POSIX (`kill 0`) com expurgo seguro de arquivos órfãos após TTL.
4. **Task 4 — Symbolic Node & Dangling Reference CRUD:** Implementar inserção e consulta de nós com suporte à flag `is_dangling` e `status: proposed` prevenindo ciclos via `VisitedSet`.
5. **Task 5 — Streaming K-Way Merge Iterator:** Implementar a estrutura de mesclagem ordenada via Min-Heap em C com limites de RAM $O(K)$ para paginação `LIMIT`/`SKIP`.
6. **Task 6 — MCP Parameter Extension (`active_horizons[]`):** Adicionar parsing do parâmetro opcional nas tools `query_graph`, `search_graph` e `trace_path` em `src/mcp/mcp.c`.
7. **Task 7 — Two-Tier Anchor Checker:** Implementar fast-path por byte offset com fallback para busca de símbolo e hash de assinatura na AST do Tree-sitter.
8. **Task 8 — Admission Gate (`promote_horizon` Tool):** Criar a tool MCP de promoção que valida âncoras Two-Tier contra o disco e orquestra a transição atômica.
9. **Task 9 — Acyclic BFS Reverse Recall Engine:** Implementar cálculo de fechamento de dependências reversas ($deps^{-1}$) com poda de ciclos para propagação de status `CONTESTED`.
