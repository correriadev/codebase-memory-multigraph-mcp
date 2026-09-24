# 002 — Strategic Design: Context Map
## Domínio: `multi_graph_federation_e2e` | Projeto: `codebase-memory-mcp`

---

## 1. Bounded Context Identification

| Bounded Context | Responsabilidade | Fronteira (Excluído) | Team Ownership | Key Entities |
|---|---|---|---|---|
| **`TestHarnessCore`** | Gerenciar o ciclo de vida dos subprocessos do executável nativo, sandboxes temporários isolados em disco e variáveis de ambiente. | Não interpreta semântica do grafo de código ou AST. | Core Tooling / QA | `TestSandboxEnvironment`, `SubprocessDriver`, `StdioPipeChannel` |
| **`MCPProtocolSession`** | Codificar, enviar e decodificar mensagens JSON-RPC 2.0 via `stdin`/`stdout`, validando envelopes, multiplexação e códigos de erro de protocolo. | Não implementa regras de negócio do grafo ou persistência SQLite direta. | Protocol / Integration | `JsonRpcEnvelope`, `ClientHandshake`, `ToolCallRequest`, `ToolCallResponse` |
| **`FederatedQueryAssertion`** | Validar asserções funcionais de consultas federadas (`search_graph`, `query_graph`, `trace_path`), incluindo ordenação de K-Way Merge, paginação e deduplicação. | Não manipula subprocessos de SO nem cria arquivos de horizonte diretamente. | Query Engine / E2E | `FederatedQueryFixture`, `PaginationCursorAssertion`, `MergedRecordVerifier` |
| **`AdmissionDriftSimulation`** | Orquestrar alterações físicas de código-fonte em disco e verificar a resposta do Admission Gate (aprovação em benign shifts vs rejeição `ANCHOR_DRIFT`). | Não executa o daemon de reaper nem monitora consumo de memória RAM. | Semantic / E2E | `SourceMutationFixture`, `AstShiftSpecimen`, `AdmissionGateResultAssertion` |
| **`CrashRecoveryReaper`** | Simular encerramentos abruptos de clientes (`SIGKILL`), testar a retenção/evicção de bancos SQLite WAL e inspecionar liberação de file locks no SO. | Não formula queries Cypher nem valida sintaxe de AST. | Reliability / Platform | `ZombieProcessSpecimen`, `OrphanDatabaseFixture`, `ReaperAuditAssertion` |

---

## 2. Context Map

Relacionamentos e padrões de integração entre os Bounded Contexts do ecossistema E2E:

### [TestHarnessCore] → [MCPProtocolSession]
- **Pattern:** Open Host Service / Published Language
- **Direction:** Upstream (`TestHarnessCore`) → Downstream (`MCPProtocolSession`)
- **Justification:** `TestHarnessCore` fornece os pipes `stdin`/`stdout` do subprocesso ativo através de um canal IPC padronizado sobre o qual o protocolo JSON-RPC opera.

### [MCPProtocolSession] → [FederatedQueryAssertion]
- **Pattern:** Customer-Supplier
- **Direction:** Upstream (`MCPProtocolSession`) → Downstream (`FederatedQueryAssertion`)
- **Justification:** O motor de asserções federadas consome os payloads de resposta MCP estruturados pelo cliente de protocolo, com requisitos estritos de validação sobre `content[0].text`.

### [MCPProtocolSession] → [AdmissionDriftSimulation]
- **Pattern:** Customer-Supplier
- **Direction:** Upstream (`MCPProtocolSession`) → Downstream (`AdmissionDriftSimulation`)
- **Justification:** A simulação de admissão dispara chamadas `promote_horizon` via MCP JSON-RPC e valida se as respostas contêm `status: admitted` ou erros tipados `ANCHOR_DRIFT`.

### [TestHarnessCore] → [AdmissionDriftSimulation]
- **Pattern:** Shared Kernel
- **Direction:** Bidirecional
- **Justification:** Ambos compartilham as definições e o caminho absoluto do sandbox temporário (`CBM_PROJECT_DIR` e repositório git fixture com arquivos C/Python).

### [TestHarnessCore] → [CrashRecoveryReaper]
- **Pattern:** Partnership
- **Direction:** Bidirecional
- **Justification:** A simulação de crash necessita de controle de processo de baixo nível (`os.kill`, inspeção de PID) fornecido pelo harness, enquanto o reaper precisa do caminho de `CBM_CACHE_DIR/horizons/`.

---

## 3. Core Domain Highlight

### Context: `AdmissionDriftSimulation`
- **Razão:** Representa a garantia empírica de que a memória federada não corrompe o grafo canônico diante de refatorações de código reais realizadas por múltiplos agentes ou desenvolvedores.
- **Investimento DDD:** Modelagem rigorosa de mutações de AST (benign shifts vs breaking changes), ancoragem de dois níveis e asserções de integridade transacional pós-promoção.

### Context: `FederatedQueryAssertion`
- **Razão:** Garante a transparência da sobreposição (overlay) para agentes e LLMs, atestando que consultas paginadas unificam Base e Horizontes sem duplicação ou omissão em pipeline $O(K)$.
- **Investimento DDD:** Verificadores estritos de cursores de paginação (`LIMIT`/`OFFSET`), masking de nós sobrepostos e validação de CBM-URIs canônicos.

### Context: `CrashRecoveryReaper`
- **Razão:** Garante que a aplicação sobreviva em ambientes de longa duração sem vazar file descriptors nem acumular bancos SQLite zumbis no Windows/POSIX.
- **Investimento DDD:** Harness de ciclo de vida de PIDs, controle de timeouts e verificação determinística de integridade do pool de conexões.

---

## 4. Architectural Decisions

### ADR-E2E-01: Execução Black-box Subprocess sobre Stdio JSON-RPC
- **Decisão:** Os testes de integração E2E devem compilar e instanciar o executável nativo real (`bin/codebase-memory-mcp.exe` ou alvo `bin/cbm_mcp.exe`) via subprocesso Python, comunicando-se exclusivamente via `stdin` e `stdout`.
- **Contexto:** Testes unitários internos em C validam componentes isolados, mas não cobrem buffering de pipes, tratamento de sinais do SO, parsing JSON real e concorrência externa.
- **Consequências:** (+) Validação 100% fiel ao ambiente de produção do MCP; (+) detecção real de vazamentos de locks de arquivo; (-) tempo de execução ligeiramente superior a chamadas em memória (controlado via sandboxes efêmeros).

### ADR-E2E-02: Sandboxes Herméticos por Caso de Teste com Override de Ambiente
- **Decisão:** Cada cenário de teste deve instanciar um `tempfile.TemporaryDirectory` injetando variáveis de ambiente `CBM_CACHE_DIR` e `CBM_PROJECT_DIR` dedicadas.
- **Contexto:** Prevenir contaminação entre testes, conflitos de concorrência com instâncias de desenvolvimento locais em `~/.cache` e resíduos de arquivos `-wal` pós-falha.
- **Consequências:** (+) Total reprodutibilidade e independência de testes; (+) limpeza garantida pelo context manager do Python; (-) leve overhead de criação de diretórios e cópia de fixtures.

### ADR-E2E-03: Simulação Física de Refatoração com Edições Reais em Disco
- **Decisão:** Testes de âncoras e Admission Gate devem alterar arquivos de código reais em disco (C/Python) para testar deslocamentos de bytes e mudanças na AST do Tree-sitter.
- **Contexto:** Mocks sintéticos de AST ocultam inconsistências no cálculo de byte offsets, encoding UTF-8 e parsing do Tree-sitter em disco.
- **Consequências:** (+) Teste fiel de casos reais (inserção de comentários de cabeçalho, alteração de parâmetros); (+) prova inequívoca de ausência de falsos positivos de rejeição; (-) exige que fixtures contenham arquivos de sintaxe válida.

### ADR-E2E-04: Simulação de Falha Abrupta de Clientes com SIGKILL e Inspeção de Locks
- **Decisão:** Para testar o `HorizonReaper`, subprocessos clientes são propositalmente terminados com sinal de terminação forçada (`SIGKILL` ou equivalente do SO), disparando o ciclo do reaper com TTL configurável.
- **Contexto:** Garantir que o reaper não cause violação de compartilhamento de arquivos (*sharing violation* no Windows) e remova com segurança arquivos `.db`, `-wal` e `-shm`.
- **Consequências:** (+) Cobertura de cenários catastróficos reais em produção; (-) exige cuidado no isolamento do processo filho para não afetar o runner de teste.
