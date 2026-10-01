# Test Scenarios: Ephemeral Horizon Compiler

**Domain:** `ephemeral_horizon_compiler`  
**Project:** `codebase-memory-mcp`  
**Status:** PROPOSED  

---

## 1. Acceptance Scenarios

### Scenario 1: Registro e Persistência de `based_on_seq` na Criação de Horizonte (PBI-01, PBI-02)
* **Goal:** Garantir que `create_horizon` registre o `based_on_seq` e o `client_pid` passados pelo orquestrador.
* **Given** o MCP Server em execução com o Grafo Base no `mutation_gen = "u4102g15"`.
* **When** o cliente invoca `create_horizon` com payload:
  ```json
  {
    "horizon_id": "h_test_scope",
    "client_pid": 12345,
    "based_on_seq": "u4102g15"
  }
  ```
* **Then** o arquivo `h_test_scope.db` é criado.
* **And** a consulta `SELECT client_pid, based_on_seq, status FROM horizon_metadata WHERE horizon_id = 'h_test_scope'` retorna:
  `client_pid = 12345`, `based_on_seq = 'u4102g15'`, `status = 'ACTIVE'`.

---

### Scenario 2: Extração Declarativa do Bloco Tático (PBI-03, PBI-04)
* **Goal:** Validar o parsing do bloco ````tactical-spec ... ```` e a gravação atômica em `symbolic_nodes` e `virtual_edges`.
* **Given** um documento `003-tactical-design.md` contendo um bloco ````tactical-spec```` com 2 nós e 1 aresta.
* **When** a ferramenta `sync_horizon_spec` é executada para o arquivo sobre `h_test_scope`.
* **Then** a tabela `symbolic_nodes` contém exatamente os 2 nós com `is_dangling = 1` e `epistemic_status = 'PROPOSED'`.
* **And** a tabela `virtual_edges` contém a aresta ligando os dois nós com o tipo especificado.
* **And** a reexecução com uma aresta alterada substitui os registros sem duplicidade (idempotência).

---

### Scenario 3: Ingestão de Seções e Busca FTS de Especificações (PBI-05, PBI-06)
* **Goal:** Comprovar a indexação transiente de cabeçalhos e texto dos documentos `001–004`.
* **Given** o arquivo `001-problem-space.md` contendo tópicos de Glossário e Event Storming.
* **When** `sync_horizon_spec` indexa o arquivo.
* **Then** nós `symbolic_nodes` com `label = 'Section'` são criados para cada cabeçalho.
* **And** uma consulta FTS `SELECT title FROM spec_fts WHERE spec_fts MATCH 'Glossary'` retorna os resultados correspondentes.

---

### Scenario 4: Rejeição de Adjacência Inválida contra a Base (PBI-07, PBI-08)
* **Goal:** Bloquear escopos que referenciam classes inexistentes no Grafo Base no `based_on_seq`.
* **Given** uma spec tática declarando uma aresta `DEPENDS_ON` para `cbm://repo/src/core/fake.ts#NonExistentClass`.
* **When** a ferramenta `validate_scope_horizon` é executada.
* **Then** o resultado retorna `success: false` com código de erro `UNRESOLVED_BASE_DEPENDENCY`.
* **And** aponta a URI canônica inválida e a linha correspondente da especificação.

---

### Scenario 5: Alerta de Nó Proposto Isolado (PBI-07)
* **Goal:** Detectar entidades concebidas no vácuo sem nenhuma conexão de entrada ou saída.
* **Given** uma spec contendo um nó `OrphanService` sem nenhuma aresta virtual conectada a ele.
* **When** `validate_scope_horizon` é executada com a flag `strict_connectivity: true`.
* **Then** a validação emite um aviso ou falha com `ISOLATED_DANGLING_NODE`.
