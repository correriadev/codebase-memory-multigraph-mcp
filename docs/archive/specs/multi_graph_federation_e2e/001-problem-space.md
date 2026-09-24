# 001 — Strategic Design: Problem Space
## Domínio: `multi_graph_federation_e2e` | Projeto: `codebase-memory-mcp`

---

## 1. Event Storming

Sessão de Event Storming temporal cobrindo o ciclo de vida completo dos testes End-to-End (E2E) da federação de múltiplos grafos, desde o bootstrapping de processos até a validação de invariantes, resiliência a falhas e encerramento.

| # | Domain Event (passado) | Command (gatilho) | Aggregate | External Systems | Read Models |
|---|---|---|---|---|---|
| E01 | `E2EEnvironmentBootstrapped` | `BootstrapE2EEnvironment` | `TestHarnessEnvironment` | OS File System, OS Environment | `SandboxConfigProjection` |
| E02 | `BaseGraphFixtureSeeded` | `SeedBaseGraphFixture` | `BaseGraphFixture` | SQLite Engine, Tree-sitter AST | `BaseGraphCatalogProjection` |
| E03 | `MCPServerSubprocessSpawned` | `SpawnMCPServerProcess` | `MCPProcessSession` | OS Process Manager, Stdio Pipes | `ProcessDescriptorProjection` |
| E04 | `MCPClientHandshakeCompleted` | `PerformClientHandshake` | `MCPClientSession` | Stdio JSON-RPC Protocol | `ServerCapabilitiesProjection` |
| E05 | `CognitiveHorizonCreated` | `CreateCognitiveHorizon` | `HorizonLifecycleSession` | OS Process Manager | `ActiveHorizonsListProjection` |
| E06 | `DanglingSymbolProposed` | `ProposeDanglingSymbol` | `SymbolicOverlayAggregate` | SQLite WAL Engine | `OverlaySymbolCatalogProjection` |
| E07 | `FederatedQueryDispatched` | `DispatchFederatedQuery` | `FederatedQuerySession` | Stdio JSON-RPC Protocol | `FederatedQueryResponseProjection` |
| E08 | `StreamingKWayPageMerged` | `FetchPaginatedStream` | `StreamingMergeCursor` | SQLite Read Cursor | `PaginatedRecordSetProjection` |
| E09 | `SourceCodeRefactoredOnDisk` | `MutateSourceFileOnDisk` | `SourceRefactoringFixture` | OS File System | `DiskSourceDeltaProjection` |
| E10 | `TwoTierAnchorVerified` | `VerifyAnchorTiers` | `AdmissionGateFixture` | Tree-sitter AST, File System | `AnchorVerificationReportProjection` |
| E11 | `HorizonAdmissionPromoted` | `PromoteHorizonAdmission` | `AdmissionGateFixture` | SQLite Engine | `BaseGraphRevisionProjection` |
| E12 | `AnchorDriftRejected` | `RejectDriftedAdmission` | `AdmissionGateFixture` | Stdio JSON-RPC Protocol | `AdmissionErrorResponseProjection` |
| E13 | `ClientProcessForceKilled` | `SimulateProcessCrash` | `MCPProcessSession` | OS Process Manager (`SIGKILL`) | `ProcessLivenessStatusProjection` |
| E14 | `OrphanedHorizonsReaped` | `TriggerReaperScan` | `HorizonReaperHarness` | OS File System, Process Manager | `ReaperEvictionAuditProjection` |
| E15 | `TestAssertionsConcluded` | `EvaluateTestInvariants` | `E2EAssertionEngine` | Test Runner (pytest/unittest) | `TestExecutionSummaryReport` |
| E16 | `E2ESandboxCleanedUp` | `TeardownE2ESandbox` | `TestHarnessEnvironment` | OS File System | `CleanupAuditProjection` |

---

## 2. Subdomain Classification

Classificação das áreas de negócio e suporte identificadas no fluxo de teste de ponta a ponta:

| Subdomain | Tipo | Justificativa |
|---|---|---|
| **Federated Protocol & Stdio Orchestration** | Core | Diferenciador crítico: garante que a interface externa real JSON-RPC MCP e seus overlays funcionem sem vazamento em nível de SO. |
| **Admission & Refactor Drift Verification** | Core | Núcleo de integridade semântica: valida fisicamente no disco e na AST se mutações de código preservam âncoras ou acionam rejeição. |
| **Crash Resilience & Reaper Lifecycle** | Core | Confiabilidade de produção: assegura que deadlocks, processos mortos e acúmulo de bancos SQLite órfãos sejam higienizados deterministicamente. |
| **Streaming K-Way Merge & Pagination** | Supporting | Suporte de alto volume: valida ordenação correta, shadowing de nós e consistência de cursores paginados entre Base e Horizontes. |
| **Isolated Sandbox & Fixture Seeding** | Supporting | Permite execução hermética e repetível sem contaminação entre suítes ou estados residuais de filesystem. |
| **Test Runner & CI Reporting Integration** | Generic | Automação padrão de execução de testes (`pytest`, targets de makefile e serializadores de sumário de saída). |

---

## 3. Ubiquitous Language Glossary

Terminologia padronizada a ser adotada estritamente em todas as especificações e asserções do domínio:

| Termo | Definição | Notas (Sinônimos / Anti-patterns) |
|---|---|---|
| **Subprocess Stdio Harness** | Componente de teste que gerencia o ciclo de vida do executável nativo compilado, lendo e escrevendo mensagens JSON-RPC 2.0 via `stdin`/`stdout`. | Evitar mocks internos quando o escopo é E2E. Sinônimo: *Black-box MCP Driver*. |
| **Hermetic Sandbox** | Diretório temporário isolado por caso de teste (`tempfile.TemporaryDirectory`), contendo seu próprio `CBM_CACHE_DIR`, repositório git fixture e Base Graph. | Proibido compartilhar diretórios padrão (`~/.cache`) entre testes concorrentes. |
| **Base Graph Fixture** | Banco SQLite pré-semeado com nós de grafo estáveis, arestas canônicas e código-fonte sincronizado, representando o estado master antes de qualquer mutação. | Não confundir com arquivos de horizonte efêmeros. |
| **Private Cognitive Horizon** | Arquivo SQLite WAL efêmero associado a uma sessão de trabalho de um agente específico, contendo propostas, mutações locais e nós especulativos. | Isolado de outros horizontes; invisível para agentes sem delegação explícita. |
| **Virtual Client Session** | Representação no harness de teste de uma sessão de agente cliente independente emitindo comandos JSON-RPC com identificador de horizonte e PID próprios. | Pode compartilhar a mesma instância de servidor stdio ou rodar em processos distintos. |
| **Two-Tier Anchor Verification** | Validação em dois estágios no Admission Gate: primeiro verifica byte offset no arquivo; se houver deslocamento, recorre ao hash de assinatura do nó na AST. | Previne falsos positivos de rejeição decorrentes de refatorações ou adições de linhas inofensivas. |
| **Benign Shift** | Inserção de linhas (ex: comentários, licença de cabeçalho, imports) que desloca a posição em bytes de um símbolo sem alterar sua semântica ou corpo na AST. | Deve ser aceito com sucesso pelo Two-Tier Anchor Checker. |
| **Breaking Drift (`ANCHOR_DRIFT`)** | Alteração estrutural no código-fonte ancorado (mudança de assinatura, alteração de parâmetros ou deleção) que invalida a pré-condição da proposta. | Deve causar rejeição atômica da promoção com código `ANCHOR_DRIFT`. |
| **Zombie Client Simulation** | Encerramento abrupto de um subprocesso cliente via sinal de terminação do SO (`SIGKILL` / `kill(9)`), deixando seus bancos SQLite órfãos em disco. | Usado para exercitar o reaper em condições idênticas a falhas de IDE ou cancelamentos abruptos. |
| **Horizon Reaper Cycle** | Varredura periódica de limpeza executada pelo daemon ou disparada pelo harness, que detecta PIDs extintos no SO e remove arquivos `.db`, `-wal` e `-shm`. | Não deve desalocar ou remover horizontes pertencentes a processos ainda vivos. |
| **Streaming K-Way Merge** | Algoritmo de junção baseado em Min-Heap que consome múltiplos cursores ordenados de SQLite em pipeline $O(K)$, emitindo registros ordenados e mascarando chaves sobrepostas. | Garante que páginas de resposta MCP (`limit`/`offset`) venham unificadas e sem duplicação. |
| **Late-Binding Resolution** | Resolução automática de um nó especulativo pendente (*dangling node*) quando o código-fonte correspondente é posteriormente implementado e indexado na Base. | Garante transição atômica de `PROPOSED` para `VERIFIED` na Base. |
