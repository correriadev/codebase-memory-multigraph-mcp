# 003 — Tactical Design: Solution Space
## Projeto: `codebase-memory-mcp` | Domínio: `multi_graph_federation_e2e`

---

## 0. Refinement Questions and Answers

| ID | Category | Question | Recommendation | Final Answer | Answered By |
|---|---|---|---|---|---|
| Q01 | architecture | Qual stack e modelo de execução deve conduzir a suíte de testes End-to-End (E2E) da federação de múltiplos grafos? | Python test harness (pytest / unittest) gerenciando subprocessos do binário nativo compilado via stdio JSON-RPC real, com suporte a múltiplos clientes concorrentes e inspeção de PIDs. | Python test harness (pytest / unittest) gerenciando subprocessos do binário nativo compilado via stdio JSON-RPC real, com suporte a múltiplos clientes concorrentes e inspeção de PIDs. | human |
| Q02 | boundary | Como o test harness de E2E deve isolar o estado de disco (Base Graph, Horizontes SQLite WAL e diretórios de cache) entre execuções de teste? | Isolamento hermético por caso de teste usando tempfile.TemporaryDirectory() com CBM_CACHE_DIR e CBM_PROJECT_DIR apontando para pastas temporárias efêmeras excluídas no teardown. | Isolamento hermético por caso de teste usando tempfile.TemporaryDirectory() com CBM_CACHE_DIR e CBM_PROJECT_DIR apontando para pastas temporárias efêmeras excluídas no teardown. | human |
| Q03 | concurrency | Como o cenário de concorrência multi-agente deve ser exercitado pelo harness de E2E? | Suporte dual: múltiplos clientes MCP virtuais concorrentes falando com uma mesma instância de servidor via stdio, mais testes multi-processo com múltiplos binários acessando a mesma Base em SQLite WAL. | Suporte dual: múltiplos clientes MCP virtuais concorrentes falando com uma mesma instância de servidor via stdio, mais testes multi-processo com múltiplos binários acessando a mesma Base em SQLite WAL. | human |
| Q04 | acceptance | Como a simulação de refatoração de código do mundo real e a validação do Two-Tier Anchor Checker devem ser estruturadas nos testes E2E? | Manipulação física de arquivos-fonte reais em disco no fixture temporário (C e Python) simulando: (1) shift benigno de linhas/comentários, (2) mutação de assinatura que quebra contrato (ANCHOR_DRIFT), e (3) resolução de nó dangling após indexação. | Manipulação física de arquivos-fonte reais em disco no fixture temporário (C e Python) simulando: (1) shift benigno de linhas/comentários, (2) mutação de assinatura que quebra contrato (ANCHOR_DRIFT), e (3) resolução de nó dangling após indexação. | human |
| Q05 | failure | Como a recuperação de falhas (crash recovery) e a evicção pelo Horizon Reaper devem ser testadas em E2E? | Spawning de subprocesso de cliente real, criação de horizonte com PID registrado, terminação abrupta com SIGKILL/kill(9), disparo do ciclo do Reaper com TTL customizado curto, e asserção de unlinking dos arquivos .db/-wal/-shm sem vazamento de locks. | Spawning de subprocesso de cliente real, criação de horizonte com PID registrado, terminação abrupta com SIGKILL/kill(9), disparo do ciclo do Reaper com TTL customizado curto, e asserção de unlinking dos arquivos .db/-wal/-shm sem vazamento de locks. | human |
| Q06 | integration | Como o comportamento de K-Way Merge streaming, paginação (LIMIT/OFFSET) e shadowing de nós deve ser verificado em E2E? | Teste de paginação de múltiplos passos (LIMIT/OFFSET) via MCP JSON-RPC com Base e múltiplos Horizontes contendo chaves sobrepostas, validando ordenação determinística, shadowing de nós e ausência de duplicação entre páginas. | Teste de paginação de múltiplos passos (LIMIT/OFFSET) via MCP JSON-RPC com Base e múltiplos Horizontes contendo chaves sobrepostas, validando ordenação determinística, shadowing de nós e ausência de duplicação entre páginas. | human |
| Q07 | maintainability | Como a suíte de testes E2E deve ser exposta no fluxo de desenvolvimento e automação de build? | Adicionar target dedicado 'test-e2e' em Makefile.cbm e script executável direto (ex: pytest tests/e2e/), reportando tempos de execução, status por cenário e logs detalhados de stdio em caso de falha. | Adicionar target dedicado 'test-e2e' em Makefile.cbm e script executável direto (ex: pytest tests/e2e/), reportando tempos de execução, status por cenário e logs detalhados de stdio em caso de falha. | human |

---

## 1. Main Structure (Aggregates & Invariants)

| Element | Layer / Type | Invariants / Tech Rules | 4-line Snippet |
|---|---|---|---|
| `TestSandboxEnvironment` | Harness / Aggregate Root | Cada sandbox possui caminho de cache exclusivo; todo arquivo deve ser removido no teardown; processo pai gerencia lifecycle. | ```class TestSandboxEnvironment:   def __init__(self, prefix="cbm_e2e_"): self.tmp_dir = tempfile.TemporaryDirectory(prefix=prefix)   def env_vars(self) -> dict: return {"CBM_CACHE_DIR": os.path.join(self.tmp_dir.name, "cache"), "CBM_PROJECT_DIR": self.tmp_dir.name}   def cleanup(self): self.tmp_dir.cleanup()``` |
| `MCPProcessSession` | Driver / Aggregate Root | Um subprocesso deve ter PID válido; `stdin` e `stdout` não compartilham buffers; encerramento forçado não deixa zombies. | ```class MCPProcessSession:   def __init__(self, binary_path: str, env: dict): self.proc = subprocess.Popen([binary_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)   def send_rpc(self, req: dict, timeout=5.0) -> dict: ...   def kill_force(self): self.proc.kill(); self.proc.wait()``` |
| `TwoTierRefactorSpecimen` | Domain / Entity | Specimen deve conter código sintaticamente válido; alteração de assinatura deve invalidar âncora; shift benigno preserva AST hash. | ```class TwoTierRefactorSpecimen:   def __init__(self, file_path: str, symbol: str): self.file_path, self.symbol = file_path, symbol   def apply_benign_comment_shift(self, lines: int) -> int: ...   def apply_breaking_signature_mutation(self) -> str: ...``` |
| `ReaperCrashSpecimen` | Domain / Aggregate Root | Deve registrar PID no SQLite; após `kill_force()`, PID não existe no SO; reaper remove `.db`, `.db-wal`, `.db-shm`. | ```class ReaperCrashSpecimen:   def __init__(self, horizon_id: str, client_pid: int): self.id, self.pid = horizon_id, client_pid   def verify_unlinked(self, cache_dir: str) -> bool: return not os.path.exists(os.path.join(cache_dir, "horizons", f"{self.id}.db"))``` |

---

## 2. Value Objects / Types / Interfaces

| Name | Context / Layer | Validation & Typing Rules | 4-line Snippet |
|---|---|---|---|
| `JsonRpcRequest` | Protocol / VO | `jsonrpc` deve ser `"2.0"`; `id` deve ser inteiro único; `method` não vazio. | ```@dataclass(frozen=True) class JsonRpcRequest:   method: str; id: int; params: dict   def to_bytes(self) -> bytes: return (json.dumps({"jsonrpc":"2.0","id":self.id,"method":self.method,"params":self.params}) + "\n").encode()``` |
| `JsonRpcResponse` | Protocol / VO | Contém `id` coincidente com a requisição; possui `result` ou `error` (mutuamente exclusivos). | ```@dataclass(frozen=True) class JsonRpcResponse:   id: int; result: Optional[dict] = None; error: Optional[dict] = None   def is_success(self) -> bool: return self.error is None and self.result is not None``` |
| `PaginationExpectation` | Query / VO | `limit >= 1`; `offset >= 0`; lista de chaves esperadas estritamente ordenada. | ```@dataclass(frozen=True) class PaginationExpectation:   page_index: int; limit: int; offset: int   expected_uris: List[str]``` |
| `AnchorVerificationOutcome` | Admission / VO | `is_admitted` booleano; se falso, `error_code` deve ser `ANCHOR_DRIFT` ou `INVARIANT_VIOLATION`. | ```@dataclass(frozen=True) class AnchorVerificationOutcome:   is_admitted: bool; error_code: Optional[str]   admitted_symbols: List[str]``` |

---

## 3. Domain Services / Use Cases

| Operation / Service | Responsibility | Coordinates | 4-line Snippet |
|---|---|---|---|
| `SeedBaseGraphFixtureUseCase` | Inicializa banco Base SQLite com esquema canônico e nós pré-indexados para testes determinísticos. | `TestSandboxEnvironment`, `sqlite3` | ```def seed_base_graph_fixture(sandbox: TestSandboxEnvironment, schema_path: str, symbols: list) -> str:   db_path = os.path.join(sandbox.env_vars()["CBM_PROJECT_DIR"], ".cbm", "graph.db")   os.makedirs(os.path.dirname(db_path), exist_ok=True); cbm_apply_sql(db_path, schema_path, symbols)   return db_path``` |
| `VerifyMultiAgentIsolationUseCase` | Executa 2 clientes virtuais criando horizontes distintos e valida que propostas do Agente A não vazam para Agente B. | `MCPProcessSession`, `JsonRpcRequest` | ```def verify_multi_agent_isolation(sess_a: MCPProcessSession, sess_b: MCPProcessSession, h_a: str, h_b: str):   sess_a.send_rpc(make_propose_req(h_a, "symA")); res = sess_b.send_rpc(make_query_req(active=[h_b]))   assert "symA" not in str(res.result)``` |
| `ExecuteRefactorSimulationUseCase` | Aplica mutação em arquivo físico, dispara `promote_horizon` e valida resposta de admissão ou drift. | `TwoTierRefactorSpecimen`, `MCPProcessSession` | ```def execute_refactor_simulation(sess: MCPProcessSession, specimen: TwoTierRefactorSpecimen, expect_drift: bool):   specimen.apply_benign_comment_shift(10); res = sess.send_rpc(make_promote_req(specimen.horizon_id))   assert (res.is_success() if not expect_drift else "ANCHOR_DRIFT" in str(res.error))``` |
| `SimulateClientCrashAndReapUseCase` | Mata subprocesso cliente com SIGKILL e valida que o ciclo do reaper remove os arquivos SQLite órfãos. | `MCPProcessSession`, `ReaperCrashSpecimen` | ```def simulate_crash_and_reap(sess: MCPProcessSession, specimen: ReaperCrashSpecimen, cache_dir: str):   sess.kill_force(); run_reaper_sweep(cache_dir, ttl=0)   assert specimen.verify_unlinked(cache_dir)``` |

---

## 4. Events / Messages / Async Flows

| Event / Action Name | Trigger | Minimum Payload | Consumers |
|---|---|---|---|
| `SandboxCreated` | Início do teste E2E | `{ "cache_dir": str, "project_dir": str }` | `BaseGraphFixtureSeeder` |
| `ProcessTerminatedAbruptly` | Simulação de crash do cliente | `{ "pid": int, "signal": "SIGKILL" }` | `HorizonReaperHarness` |
| `HorizonPromoted` | Chamada `promote_horizon` com âncoras válidas | `{ "horizon_id": str, "nodes_admitted": int }` | `FederatedQueryVerifier` |
| `AnchorDriftDetected` | Chamada `promote_horizon` com código quebrado | `{ "horizon_id": str, "drift_type": "AST_MISMATCH" }` | `E2EAssertionEngine` |
| `ReaperCleanupFinished` | Varredura periódica ou sob demanda do reaper | `{ "unlinked_dbs": list, "retained_dbs": list }` | `E2EAssertionEngine` |

---

## 5. Persistence / Repository / Data Access Interfaces

| Resource / Adapter | Methods / Actions | Return Types / Expected State |
|---|---|---|
| `MCPStdioChannel` | `write_message(msg: bytes) -> None`<br>`read_message(timeout: float) -> bytes` | Buffer de bytes decodificável em JSON-RPC 2.0 |
| `DiskFixtureStore` | `write_source_file(path: str, code: str) -> None`<br>`read_source_file(path: str) -> str` | String de código-fonte persistida no filesystem |
| `SqliteFixtureInspector` | `fetch_scalar(db_path: str, sql: str) -> Any`<br>`count_records(db_path: str, table: str) -> int` | Valor escalar ou inteiro de contagem direta no SQLite |

```
// 4-line snippet example:
interface MCPStdioChannel:
  def write_message(self, msg: bytes) -> None: ...
  def read_message(self, timeout: float = 5.0) -> bytes: ...
  // communication channel over OS subprocess pipes
```

---

## 6. Ordered Development Tasks

```json
[
  {
    "id": "01",
    "title": "Implement MCP Stdio JSON-RPC Process Driver",
    "description": "Create robust subprocess management harness executing the native C binary over stdio with non-blocking JSON-RPC 2.0 exchange and clean termination.",
    "scope": ["tests/e2e/mcp_process_driver.py", "tests/e2e/jsonrpc_client.py"],
    "acceptance": [
      "Launches native C binary as child subprocess and completes MCP initialize handshake",
      "Sends and receives JSON-RPC 2.0 tool requests with timeout protection",
      "Kills child process cleanly without leaving zombie processes on Windows/POSIX"
    ],
    "depends_on": null
  },
  {
    "id": "02",
    "title": "Implement Hermetic Sandbox and Base Graph Seeder",
    "description": "Provide temporary directory sandbox isolation overriding CBM environment variables and seeding a deterministic Base Graph database.",
    "scope": ["tests/e2e/sandbox_environment.py", "tests/e2e/fixtures/base_seeder.py"],
    "acceptance": [
      "Generates ephemeral isolated directories for CBM_CACHE_DIR and CBM_PROJECT_DIR",
      "Seeds canonical SQLite graph with test symbols and edges before test execution",
      "Removes all temporary files and handles on teardown with retry resilience on Windows"
    ],
    "depends_on": "01"
  },
  {
    "id": "03",
    "title": "Implement Multi-Agent Horizon Isolation E2E Scenario",
    "description": "Test concurrent client sessions writing to independent cognitive horizons, validating zero state bleed between agents before promotion.",
    "scope": ["tests/e2e/test_multi_agent_isolation.py"],
    "acceptance": [
      "Spawns two distinct agent client sessions with separate horizon IDs",
      "Asserts Agent A proposed symbols are invisible to Agent B in search_graph and query_graph",
      "Verifies active_horizons filtering isolates results strictly per-session"
    ],
    "depends_on": "02"
  },
  {
    "id": "04",
    "title": "Implement Physical Refactoring and Two-Tier Anchor E2E Scenario",
    "description": "Simulate physical source code refactoring on disk to test benign offset shift tolerance versus breaking AST signature rejection.",
    "scope": ["tests/e2e/test_refactoring_admission.py", "tests/e2e/fixtures/specimens.py"],
    "acceptance": [
      "Verifies benign comment/header insertion is admitted via AST signature match",
      "Verifies breaking function signature alteration is rejected with ANCHOR_DRIFT",
      "Verifies successful promote_horizon consolidates admitted symbols into Base Graph"
    ],
    "depends_on": "03"
  },
  {
    "id": "05",
    "title": "Implement Client Crash and Horizon Reaper Eviction E2E Scenario",
    "description": "Test abrupt termination of client subprocesses, simulating orphaned horizon databases and validating reaper eviction without file lock errors.",
    "scope": ["tests/e2e/test_crash_recovery_reaper.py"],
    "acceptance": [
      "Simulates abrupt client crash via SIGKILL leaving horizon .db, -wal, -shm files",
      "Executes reaper cycle and validates unlinking of expired orphan databases",
      "Asserts active horizon databases of live client PIDs are strictly preserved"
    ],
    "depends_on": "04"
  },
  {
    "id": "06",
    "title": "Implement Streaming K-Way Merge Pagination E2E Scenario",
    "description": "Verify multi-horizon streaming query pagination, validating deterministic ordering, node shadowing, and zero duplicate records across pages.",
    "scope": ["tests/e2e/test_streaming_pagination.py"],
    "acceptance": [
      "Executes multi-step paginated queries with LIMIT and OFFSET across Base and Horizons",
      "Validates horizon nodes shadow Base Graph symbols when keys collide",
      "Asserts total concatenated pages produce exact expected sorted result set without duplicates"
    ],
    "depends_on": "05"
  },
  {
    "id": "07",
    "title": "Integrate E2E Suite into Makefile and CI Automation",
    "description": "Add test-e2e target to Makefile.cbm and provide runner script with structured reporting and failure diagnostics.",
    "scope": ["Makefile.cbm", "tests/e2e/run_e2e.py"],
    "acceptance": [
      "make -f Makefile.cbm test-e2e builds native binary and executes full Python E2E suite",
      "Reports clear execution times, per-scenario pass/fail status and stdio error logs on failure",
      "Returns non-zero exit code if any E2E scenario fails"
    ],
    "depends_on": "06"
  }
]
```
