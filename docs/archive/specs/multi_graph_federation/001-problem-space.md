# 001 — Strategic Design: Problem Space
## Domínio: `multi_graph_federation`

---

## 1. Event Storming

Sessão de Big Picture Event Storming cobrindo o ciclo de vida de múltiplos grafos, referências simbólicas tardias, overlays de hipóteses cognitivas e admissão de evidências.

| # | Domain Event (past tense) | Command (trigger) | Aggregate | External Systems | Read Models |
|---|---|---|---|---|---|
| 1 | `SourceRepositoryIndexed` | `IndexRepository` | `BaseGraph` | Git / Filesystem | `GraphMetadataView` |
| 2 | `HorizonCreated` | `CreateHorizon` | `Horizon` | Agent Session | `ActiveHorizonsView` |
| 3 | `SymbolicNodeProposed` | `ProposeNode` | `Horizon` | AI Coding Agent | `HorizonHierarchyView` |
| 4 | `SymbolicLinkEstablished` | `LinkSymbolicRef` | `Horizon` | AI Coding Agent | `FederatedDependencyView` |
| 5 | `DanglingReferenceDetected` | `ResolveLateBinding` | `SymbolicResolver` | None | `UnanchoredSymbolsView` |
| 6 | `FederatedQueryExecuted` | `QueryGraphFederated` | `FederatedQueryEngine` | MCP Client | `FederatedGraphResult` |
| 7 | `RuntimeTraceIngested` | `IngestTrace` | `EvidenceGraph` | OpenTelemetry / CI | `ObservedCallGraphView` |
| 8 | `EvidenceAttestationRecorded` | `RecordAttestation` | `EvidenceGraph` | Compiler / Test Runner | `AttestationSummaryView` |
| 9 | `VerbatimAnchorVerified` | `VerifyAnchors` | `AdmissionGate` | Filesystem / Tree-sitter | `AnchorVerificationReport` |
| 10 | `HorizonPromoted` | `PromoteHorizon` | `AdmissionGate` | Operator / Agent | `ProjectVersionHistory` |
| 11 | `EpistemicRecallTriggered` | `ContestSymbol` | `AdmissionGate` | Test Suite / Agent | `ContestedImpactRadiusView` |
| 12 | `OrphanHorizonReaped` | `ReapOrphanHorizons` | `HorizonManager` | OS Process Table | `ReapedHorizonsAuditLog` |
| 13 | `HorizonDiscarded` | `DiscardHorizon` | `Horizon` | Agent Session | `ActiveHorizonsView` |

---

## 2. Subdomain Classification

Classificação das áreas de negócio do ecossistema federado:

| Subdomain | Type | Justification |
|---|---|---|
| **Federated Query Engine & Overlays** | Core | Diferencial competitivo primário: mesclagem via streaming (K-Way Merge) de grafos estáticos e efêmeros via CBM-URIs sem mutação física e com bounds de memória. |
| **Epistemic Admission Gate** | Core | Núcleo de autoridade e verificação determinística em duas fases (Two-Tier Anchors) contra o disco, prevenindo alucinações e *authority laundering*. |
| **Horizon Management & Lifecycle** | Supporting | Gerencia ciclo de vida, pool de conexões com teto de FDs, arquivos SQLite isolados em WAL mode e reaper de órfãos. |
| **Runtime Evidence Ingestion** | Supporting | Ingestão e agregação de provas empíricas (traces, logs de compilação e asserções de testes). |
| **AST Parsing & Static Indexing** | Generic | Mecanismo commodity de geração sintática via Tree-sitter e Hybrid LSP já estabilizado na Camada 0. |
| **MCP Protocol Adapter** | Generic | Camada de transporte JSON-RPC padronizada para comunicação com IDEs e agentes externos. |

---

## 3. Ubiquitous Language Glossary

| Term | Definition | Notes |
|---|---|---|
| **Base Graph** | Grafo físico estático e imutável que reflete com exatidão a sintaxe e estrutura do código-fonte em disco. | Corresponde à Camada 0 ($\alpha$, Source-Authoritative). Nunca modificado diretamente por agentes de IA. |
| **Horizon** | Grafo cognitivo efêmero e transiente, isolado em storage próprio (`horizons/<id>.db`), contendo hipóteses e anotações de um agente. | Camada 1. Conexões gerenciadas via pool com teto estrito de descritores de arquivo (FDs). |
| **CBM-URI** | Identificador simbólico universal no formato `cbm://<repo>/<path>#<symbol>` que referencia uma entidade de código independente do grafo físico onde ela reside. | Chave canônica que viabiliza *late-binding* e desacoplamento de storage. |
| **Symbolic Reference** | Relação lógica entre dois nós estabelecida via CBM-URIs sem a exigência de uma aresta física relacional no banco de dados. | Permite referenciar nós entre grafos heterogêneos sem violar integridade referencial. |
| **Dangling Node** | Nó especulativo criado em um Horizon que referencia um símbolo que ainda não existe no código em disco nem no Base Graph. | Possui `is_dangling = true`. Ancorado automaticamente quando o código for indexado. Ciclos prevenidos via `VisitedSet`. |
| **Virtual Edge** | Aresta computada em tempo de execução durante uma consulta federada, conectando nós do Base Graph e de Horizontes ativos. | Não persiste no disco; projeção volátil calculada pelo motor de Overlay. |
| **Overlay Query** | Consulta que projeta um ou mais Horizontes ativos sobre o Base Graph via streaming iterador K-Way Merge sem materializar o conjunto completo em memória. | Invocada via parâmetro `active_horizons[]` nas ferramentas MCP padrão. |
| **Two-Tier Anchor** | Mecanismo resiliente de verificação de integridade física: testa byte offsets rápidos primeiro; se deslocados, resolve via AST symbol signature hash. | Evita falsos positivos de *drift* causados por edições fora do corpo do símbolo. |
| **Horizon Reaper** | Processo em background gerenciado pelo daemon que expurga bancos SQLite órfãos deixados por processos de clientes que morreram sem descarte. | Baseado em verificação de PID vivo e TTL de inatividade. |
| **Admission Gate** | Componente determinístico que valida invariantes e âncoras verbatim antes de autorizar a transição de um Horizon para a verdade duradoura. | Executa a operação `PromoteHorizon`. Não escreve código; apenas admite ou rejeita. |
| **Epistemic Recall** | Invalidação transitiva em cascata ($deps^{-1}$) disparada quando uma premissa ou contrato é contestado por falha observável. | Rebaixa o status epistêmico de todos os nós dependentes para `contested`. |
| **Evidence Attestation** | Registro imutável de resultado empírico (sucesso/falha de compilação ou teste) associado a um CBM-URI no Evidence Graph. | Prova observável de terceiros que sustenta o Invariante da Evidência. |
