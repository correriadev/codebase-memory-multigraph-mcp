# Strategic Design — Context Map: Ephemeral Horizon Compiler

**Domain:** `ephemeral_horizon_compiler`  
**Project:** `codebase-memory-mcp`  
**Status:** PROPOSED  

---

## 1. Bounded Contexts

```mermaid
graph TD
    classDef core fill:#4f46e5,stroke:#312e81,stroke-width:2px,color:#fff;
    classDef supporting fill:#0284c7,stroke:#0369a1,stroke-width:2px,color:#fff;
    classDef generic fill:#475569,stroke:#334155,stroke-width:2px,color:#fff;

    BC_ORCH["Scope Refinement Orchestrator (Upstream Planner)"]:::supporting
    BC_COMPILER["Ephemeral Horizon Compiler (Core Engine)"]:::core
    BC_POOL["Horizon Connection & Storage Pool"]:::generic
    BC_BASE_STORE["CBM Base Graph Store (Solidified Reality)"]:::generic
    BC_MCP["MCP JSON-RPC Gateway"]:::generic

    BC_ORCH -->|Customer-Supplier [Upstream: Planner, Downstream: Compiler]| BC_COMPILER
    BC_COMPILER -->|Conformist [Consumes Base Schema & Store Generation]| BC_BASE_STORE
    BC_COMPILER -->|Shared Kernel [SQLite Schemas & Handles]| BC_POOL
    BC_MCP -->|Open Host Service / Published Language [Tool Handlers]| BC_COMPILER
```

### Context Definitions

1. **`Scope Refinement Orchestrator` (Upstream)**
   * **Papel:** Produz as especificações DDD (`001` a `004`), conduz entrevistas e dispara as chamadas de inicialização e sincronização.
   * **Tipo:** Supporting.
2. **`Ephemeral Horizon Compiler` (Core)**
   * **Papel:** Analisa blocos declarativos (````tactical-spec````), extrai seções Markdown, valida adjacência com a Base e grava transações atômicas no SQLite efêmero.
   * **Tipo:** Core.
3. **`Horizon Connection & Storage Pool`**
   * **Papel:** Fornece handles SQLite limitados via LRU (`CBM_MAX_HORIZON_FDS = 16`), gerencia diretórios em cache e garante modo WAL.
   * **Tipo:** Generic.
4. **`CBM Base Graph Store`**
   * **Papel:** Detém o Grafo Base persistente, gerencia a sequência de mutações (`mutation_gen` / `cbm_store_generation`) e responde consultas Cypher/K-Way.
   * **Tipo:** Generic (Foundation).

---

## 2. Context Map Relationships & Integration Contracts

| Upstream Context | Downstream Context | Relationship Pattern | Interface / Contract |
|---|---|---|---|
| `Scope Refinement Orchestrator` | `Ephemeral Horizon Compiler` | **Customer / Supplier** | Invocação MCP JSON-RPC (`create_horizon`, `cbm_sync_horizon_spec`, `cbm_validate_scope_horizon`). |
| `CBM Base Graph Store` | `Ephemeral Horizon Compiler` | **Conformist** | Consulta direta à geração da Base (`cbm_store_generation`) e checagem de existência de URIs canônicas. |
| `Ephemeral Horizon Compiler` | `Horizon Storage Pool` | **Shared Kernel** | DDL SQLite compartilhado (`src/db/horizon_schema.sql`), LRU handles via `cbm_horizon_pool_get`. |
| `Ephemeral Horizon Compiler` | `MCP JSON-RPC Gateway` | **Open Host Service (OHS)** | JSON-RPC 2.0 payloads para agentes LLM e ferramentas CLI. |

---

## 3. Cross-Context Invariants

1. **`INV-HORIZON-01` (Isolamento Absoluto de Estado):** O compilador do horizonte efêmero NUNCA escreve diretamente no banco do Grafo Base. Todas as mutações de escopo são restritas ao banco local `h_${domain}.db`.
2. **`INV-HORIZON-02` (Ancoragem de Sequência Imutável):** Ao instanciar `h_${domain}.db`, o compilador captura compulsoriamente a geração corrente do Grafo Base (`based_on_seq`). Um horizonte criado sem `based_on_seq` é considerado malformado.
3. **`INV-HORIZON-03` (Idempotência da Compilação Tática):** A reexecução da compilação de `003-tactical-design.md` substitui os nós e arestas daquela especificação de forma limpa via transação atômica (`BEGIN IMMEDIATE ... COMMIT`), sem deixar registros órfãos ou duplicados.
4. **`INV-HORIZON-04` (Validação de Adjacência Fail-Fast):** Arestas virtuais que apontam para alvos da Base que não existem no `based_on_seq` registrado disparam erro de compilação `UNRESOLVED_BASE_DEPENDENCY`, impedindo que o escopo seja marcado como pronto para execução.
