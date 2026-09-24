# Tactical Design: Ephemeral Horizon Compiler

**Domain:** `ephemeral_horizon_compiler`  
**Project:** `codebase-memory-mcp`  
**Status:** PROPOSED  

---

## 0. Refinement Questions and Answers

| ID | Category | Question | Recommendation | Final Answer | Answered By |
|---|---|---|---|---|---|
| Q01 | architecture | Como reagir a classes da Base inexistentes no bloco declarativo? | Rejeitar com `UNRESOLVED_BASE_DEPENDENCY`. | Falhar imediatamente para evitar contaminação downstream. | human |
| Q02 | data | Onde residem as seções dos arquivos de especificação (`001–004`)? | Em `symbolic_nodes` (`label = 'Section'`) + tabela FTS local. | Reutilizar `symbolic_nodes` e criar tabela virtual `specs_fts` no horizonte. | human |
| Q03 | architecture | Onde reside fisicamente a lógica do parser? | Integrado ao CBM Core em C11 (`src/core/` / `src/mcp/`). | No binário C11 nativo para garantir performance sub-milissegundo e zero deps. | human |

---

## 1. CBM-MGH Overlay & Canonical CBM-URI Binding

```
CANONICAL CBM-URI FORMAT: cbm://<repo>/<path>#<symbol>
SPECULATIVE HORIZON: h_ephemeral_horizon_compiler
EPISTEMIC STATUS: PROPOSED
DANGLING STATUS: is_dangling = 1
SIGNATURE TOKEN BUDGET: < 1500 tokens
```

### Declarative Tactical Specification Block

```tactical-spec
domain: ephemeral_horizon_compiler
bounded_context: EphemeralHorizonCompiler
nodes:
  - symbol: HorizonMetadataExtension
    type: AggregateRoot
    target_path: src/db/horizon_schema.sql
    description: "Extensão da tabela horizon_metadata com based_on_seq e client_pid estável"
    methods:
      - "add_based_on_seq_column(): void"
  - symbol: HorizonSpecParser
    type: DomainService
    target_path: src/core/horizon_spec_parser.c
    description: "Parser C11 de blocos ```tactical-spec e seções CommonMark"
    methods:
      - "cbm_parse_tactical_spec(const char *markdown, TacticalSpecResult *out): int"
      - "cbm_index_spec_sections(sqlite3 *hdb, const char *file_path, const char *markdown): int"
  - symbol: HorizonSyncHandler
    type: DomainService
    target_path: src/mcp/horizon_sync_handler.c
    description: "Handler MCP para sincronização de especificações e compilação dual-plane"
    methods:
      - "handle_sync_horizon_spec(cbm_mcp_server_t *srv, const char *args_json): char*"
  - symbol: ScopeValidationGate
    type: DomainService
    target_path: src/admission/scope_validator.c
    description: "Verificador de adjacência pré-execução e detecção de nós órfãos"
    methods:
      - "cbm_validate_scope_adjacency(cbm_store_t *base_store, sqlite3 *hdb, ValidationReport *out): int"
edges:
  - source: HorizonSyncHandler
    target: HorizonSpecParser
    type: CALLS
  - source: HorizonSyncHandler
    target: HorizonMetadataExtension
    type: USES
  - source: HorizonSyncHandler
    target: ScopeValidationGate
    type: CALLS
  - source: HorizonMetadataExtension
    target: "cbm://C-Users-corre-Documents-codebase-memory-mcp/src/core/horizon_pool.c#cbm_create_horizon"
    type: EXTENDS
  - source: ScopeValidationGate
    target: "cbm://C-Users-corre-Documents-codebase-memory-mcp/src/store/store.c#cbm_store_generation"
    type: CALLS
```

---

## 2. Database Schema Evolution (`src/db/horizon_schema.sql`)

### Alteração da tabela `horizon_metadata`
Adição da coluna `based_on_seq TEXT NOT NULL DEFAULT '0'`:

```sql
CREATE TABLE IF NOT EXISTS horizon_metadata (
    horizon_id TEXT PRIMARY KEY,
    client_pid INTEGER NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('ACTIVE', 'PROMOTED', 'DISCARDED')),
    created_at INTEGER NOT NULL,
    last_heartbeat INTEGER NOT NULL,
    based_on_seq TEXT NOT NULL DEFAULT '0'
);
```

### Nova Tabela de Busca FTS de Especificações no Horizonte
Para busca FTS ultra-rápida de critérios de aceite e glossário:

```sql
CREATE VIRTUAL TABLE IF NOT EXISTS spec_fts USING fts5(
    file_path UNINDEXED,
    heading_slug,
    title,
    content,
    tokenize = 'porter unicode61'
);
```

---

## 3. Ordered Development Tasks

1. **Task 1 (PBI-01, PBI-02):** Schema & Creation Handler
   * Modificar `src/db/horizon_schema.sql` para incluir `based_on_seq`.
   * Atualizar `cbm_create_horizon` em `src/core/horizon_pool.c` e `src/core/horizon_pool.h` para aceitar `based_on_seq` e persistir no INSERT.
   * Atualizar `handle_create_horizon` em `src/mcp/horizon_handler.c` para extrair `client_pid` (com fallback ao OS PID) e `based_on_seq` (com fallback a `cbm_store_generation`).
2. **Task 2 (PBI-03, PBI-04):** Declarative Tactical Spec Parser
   * Criar `src/core/horizon_spec_parser.h` e `src/core/horizon_spec_parser.c`.
   * Implementar extração do bloco ````tactical-spec ... ```` via YAML/JSON scanning.
   * Inserir deterministicamente `symbolic_nodes` (`is_dangling = 1`, `PROPOSED`) e `virtual_edges` em transação atômica.
3. **Task 3 (PBI-05, PBI-06):** Transient Spec Ingestion & FTS
   * Implementar `cbm_index_spec_sections` para fatiar arquivos `001–004` por cabeçalhos CommonMark.
   * Criar registros `symbolic_nodes` com `label = 'Section'` e preencher `spec_fts`.
   * Expor tool MCP `sync_horizon_spec` em `src/mcp/horizon_sync_handler.c` e registrá-la em `src/mcp/mcp.c`.
4. **Task 4 (PBI-07, PBI-08):** Federated Scope Validation
   * Implementar `cbm_validate_scope_adjacency` em `src/admission/scope_validator.c`.
   * Verificar se nós propostos possuem conexões e se arestas para a Base apontam para URIs existentes.
   * Expor tool MCP `validate_scope_horizon`.
