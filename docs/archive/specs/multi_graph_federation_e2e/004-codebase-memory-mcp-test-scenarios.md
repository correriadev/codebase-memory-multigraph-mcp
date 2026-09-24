# Test Scenarios — codebase-memory-mcp

**Domain:** multi_graph_federation_e2e  
**Project:** codebase-memory-mcp  
**Framework:** pytest / Python 3 unittest  
**Date:** 2026-09-11  

---

## 1. Unit Tests

> Testes unitários focados na lógica isolada dos componentes auxiliares do harness de teste E2E (parsers de protocolo, envelopes, manipuladores de specimens e cálculo de caminhos herméticos), sem I/O de rede ou subprocessos vivos.

### 1.1 Aggregates e Entidades do Harness

#### `TestSandboxEnvironment`
- [x] Should initialize isolated sandbox directory structure successfully when instantiated
- [x] Should configure CBM_CACHE_DIR and CBM_PROJECT_DIR environment variables pointing inside temporary directory
- [x] Should cleanly release and remove temporary directory on teardown

#### `MCPProcessSession`
- [x] Should format command line arguments correctly for native executable target
- [x] Should validate process descriptor fields upon initialization
- [x] Should track process liveness state transitions from ACTIVE to TERMINATED

#### `TwoTierRefactorSpecimen`
- [x] Should compute initial AST signature hash and byte offsets for target function specimen
- [x] Should insert comment lines prepending function and recalculate byte start without changing AST signature hash
- [x] Should modify function parameters and record changed AST signature hash indicating breaking mutation

#### `ReaperCrashSpecimen`
- [x] Should initialize with unique horizon ID and target client PID
- [x] Should detect presence or absence of target database files in given cache directory

### 1.2 Value Objects do Protocolo

#### `JsonRpcRequest`
- [x] Should format valid JSON-RPC 2.0 payload with method, id, and params
- [x] Should reject creation when method string is empty
- [x] Should serialize to newline-delimited UTF-8 bytes for stdio transport

#### `JsonRpcResponse`
- [x] Should parse valid JSON-RPC 2.0 success response with matching request id
- [x] Should parse valid JSON-RPC 2.0 error response with error code and message
- [x] Should consider response invalid if neither result nor error is present

#### `PaginationExpectation`
- [x] Should validate limit >= 1 and offset >= 0
- [x] Should maintain strictly ordered list of expected CBM-URIs for page comparison

#### `AnchorVerificationOutcome`
- [x] Should record successful admission when is_admitted is True
- [x] Should record ANCHOR_DRIFT error code when admission is rejected due to AST drift

### 1.3 Domain Services do Harness

#### `BaseGraphSeeder`
- [x] Should generate valid SQL statements for seeding mock symbol nodes and canonical calls
- [x] Should fail when requested schema file path does not exist
- [x] Should reset internal sequence counters between seed batches

---

## 2. Integration Tests

> Testes de integração entre componentes do harness com I/O real em disco e pipes anônimos do sistema operacional.

### 2.1 Stdio Subprocess Communication Channel
- [x] Should launch native binary subprocess and establish open stdin and stdout pipes
- [x] Should write JSON-RPC message to stdin and read complete JSON response from stdout without pipe buffer deadlock
- [x] Should capture stderr diagnostics separately without corrupting stdout JSON-RPC frame parsing
- [x] Should terminate child subprocess cleanly and close OS pipe file descriptors on demand

### 2.2 Base Graph Seeding and SQLite Persistence
- [x] Should create SQLite base graph database file and apply full schema migrations
- [x] Should insert nodes, edges, and file indices and confirm data integrity via direct SQLite queries
- [x] Should support concurrent read connections in SQLite WAL mode without database locked errors

### 2.3 Physical Specimen Source Manipulation on Disk
- [x] Should write valid C and Python source code files into project fixture directory
- [x] Should physically alter source file on disk and verify updated byte size and modified timestamp
- [x] Should restore original source file state from backup buffer upon completion

---

## 3. Functional Tests (E2E Scenarios)

> Fluxos completos de ponta a ponta executando o executável nativo real compilado via stdio JSON-RPC.

### 3.1 Happy Path Flows

#### Scenario 1: Subprocess Stdio Handshake and Protocol Compliance
- [ ] **Should complete MCP protocol initialization and list federated tools when server process starts**
  - **Given:** Um ambiente de sandbox hermético com o binário nativo `cbm_mcp` disponível
  - **When:** O harness dispara o subprocesso e envia uma requisição JSON-RPC `initialize` seguida de `tools/list`
  - **Then:** O servidor retorna `protocolVersion: "2024-11-05"`, `capabilities.tools` presentes, e a lista inclui `search_graph`, `query_graph`, `trace_path` e `promote_horizon` com descrições e schemas JSON válidos.

#### Scenario 2: Multi-Agent Isolation & Private Cognitive Horizons
- [ ] **Should isolate overlay symbols in private horizons between two concurrent agent sessions**
  - **Given:** Uma Base Graph semeada contendo o símbolo `pkg/orders.OrderHandler` e dois agentes virtuais (Agente A e Agente B)
  - **When:** Agente A cria o Horizonte `H_A` e propõe o símbolo especulativo `pkg/orders.NewFeatureService`; Agente B cria o Horizonte `H_B` e propõe `pkg/orders.AlternativeService`
  - **Then:** Chamadas `search_graph` do Agente A com `active_horizons: ["H_A"]` retornam `OrderHandler` e `NewFeatureService` (mas nunca `AlternativeService`); chamadas do Agente B com `active_horizons: ["H_B"]` retornam `OrderHandler` e `AlternativeService` (mas nunca `NewFeatureService`).

#### Scenario 3: Real-World Refactoring & Two-Tier Admission Gate
- [ ] **Should admit horizon promotion on benign byte shift and reject on breaking AST drift**
  - **Given:** Um arquivo de código real `src/calc.c` no repositório de teste contendo a função `int add(int a, int b) { return a + b; }`, indexado na Base, e um horizonte propondo uma extensão ancorada em `add`
  - **When (Subfluxo A - Benign Shift):** Um desenvolvedor insere 15 linhas de comentários e licença no topo de `src/calc.c` (deslocando o byte offset) e chama `promote_horizon`
  - **Then (Subfluxo A):** O Two-Tier Anchor Checker detecta o deslocamento de offset, recorre à AST do Tree-sitter, confirma a identidade da assinatura da função e admite a promoção com status `admitted: true`, consolidando os nós na Base.
  - **When (Subfluxo B - Breaking Drift):** O desenvolvedor altera a assinatura para `double add(double a, double b, double c)` e chama `promote_horizon`
  - **Then (Subfluxo B):** O Admission Gate detecta a quebra estrutural na AST, rejeita atômica e integralmente a promoção com erro `ANCHOR_DRIFT`, e preserva o Base Graph inalterado.

#### Scenario 4: Horizon Reaper Zombie Eviction & Crash Resilience
- [ ] **Should detect abrupt client termination and delete orphaned horizon databases without lock leakage**
  - **Given:** Um subprocesso de cliente MCP que cria o horizonte `H_ZOMBIE` registrando seu PID no SQLite
  - **When:** O harness encerra o cliente abruptamente com `SIGKILL` (simulando crash de IDE/sistema) e dispara a varredura do `HorizonReaper` com TTL configurado
  - **Then:** O Reaper verifica via chamadas de SO que o PID não existe mais, desaloca handles residuais no connection pool e remove fisicamente `H_ZOMBIE.db`, `H_ZOMBIE.db-wal` e `H_ZOMBIE.db-shm` sem erros de compartilhamento no Windows (`WinError 32`), enquanto horizontes de clientes vivos permanecem intocados.

#### Scenario 5: K-Way Merge Streaming, Pagination, & Node Shadowing
- [ ] **Should stream and paginate federated results deterministically with node shadowing**
  - **Given:** Uma Base com 20 nós (`sym_00` a `sym_19`), Horizonte 1 modificando nós `sym_05` e `sym_06`, e Horizonte 2 adicionando nós `sym_20` a `sym_24`
  - **When:** O cliente executa consultas federadas via MCP JSON-RPC paginando em blocos de 5 itens (`limit=5, offset=0`, `limit=5, offset=5`, `limit=5, offset=10`...) com `active_horizons: ["H1", "H2"]`
  - **Then:** O motor utiliza o Streaming K-Way Merge ($O(K)$ heap): cada página retorna registros perfeitamente ordenados por chave, as versões de `sym_05` e `sym_06` do Horizonte 1 mascaram (shadowing) as versões da Base, nenhum símbolo é omitido ou duplicado entre páginas, e o total consolidado contém exatamente 25 registros únicos.

#### Scenario 6: Concurrent Multi-Process Shared WAL Access
- [ ] **Should allow multiple independent MCP server processes to query shared Base Graph concurrently**
  - **Given:** Dois subprocessos executando `codebase-memory-mcp` simultaneamente apontando para o mesmo `CBM_PROJECT_DIR` com Base Graph em SQLite WAL
  - **When:** Ambos os processos executam consultas intensivas de leitura (`search_graph`, `query_graph`) simultaneamente enquanto um deles realiza uma escrita local em seu horizonte
  - **Then:** Nenhum dos processos trava por deadlock (`SQLITE_BUSY` ou `SQLITE_LOCKED`), ambos respondem dentro do timeout estrito e mantêm consistência estrita dos dados.

### 3.2 Alternative and Error Flows
- [ ] Should return JSON-RPC parse error (`code: -32700`) when client sends malformed or truncated JSON over stdin
- [ ] Should return JSON-RPC invalid params (`code: -32602`) when active_horizons contains non-existent or unreadable horizon ID
- [ ] Should reject promote_horizon with empty or corrupted anchor payload with informative validation error
- [ ] Should maintain server process stability without memory corruption when invalid Cypher query is submitted

### 3.3 Security Scenarios
- [ ] Should prevent path traversal attacks attempting to access directories outside sandbox via crafted horizon IDs (e.g. `../../sensitive`)
- [ ] Should enforce file descriptor ceiling (max 16 open SQLite handles) under heavy multi-horizon query load via LRU pool eviction
- [ ] Should redact absolute internal sandbox filesystem paths from user-facing error messages where possible
