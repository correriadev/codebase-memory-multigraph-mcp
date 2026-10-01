# Strategic Design — Problem Space: Ephemeral Horizon Compiler

**Domain:** `ephemeral_horizon_compiler`  
**Project:** `codebase-memory-mcp`  
**Parent Product:** CBM-MGH Upstream Scope Refinement & Ephemeral Horizon Seeding Engine  
**Status:** PROPOSED  

---

## 1. Event Storming

Simulated Big Picture Event Storming over the ephemeral horizon compilation lifecycle:

| # | Domain Event (past tense) | Command (trigger) | Aggregate | External Systems | Read Models |
|---|---|---|---|---|---|
| E01 | `HorizonScopeInitiated` | `InitializeHorizonScope` | `HorizonMetadata` | Host OS (`client_pid`) | `ActiveHorizonsView` |
| E02 | `BaseGraphSeqSnapshotted` | `CaptureBaseSequence` | `HorizonMetadata` | CBM Store (`mutation_gen`) | `HorizonMetadataRecord` |
| E03 | `SpecificationDocumentSaved` | `SaveSpecificationFile` | `SpecDocument` | Local Filesystem | `SpecFilesystemCatalog` |
| E04 | `SpecSectionsTransientlyIndexed` | `IndexSpecSections` | `SpecSectionCorpus` | Horizon SQLite (`specs_fts`) | `SpecSearchFTSIndex` |
| E05 | `TacticalBlockExtracted` | `ParseTacticalSpecBlock` | `TacticalSpecParser` | Markdown AST Engine | `ParsedDeclarationAST` |
| E06 | `SymbolicNodesMaterialized` | `CompileSymbolicNodes` | `SymbolicNodeCollection` | Horizon SQLite (`symbolic_nodes`) | `FederatedOverlayGraph` |
| E07 | `VirtualEdgesWired` | `CompileVirtualEdges` | `VirtualEdgeCollection` | Horizon SQLite (`virtual_edges`) | `FederatedAdjacencyGraph` |
| E08 | `BaseAdjacencyValidated` | `VerifyBaseAdjacency` | `ScopeValidationGate` | CBM Base Store | `BlastRadiusDiagnostics` |
| E09 | `IsolatedDanglingNodeDetected` | `AuditDanglingNodes` | `ScopeValidationGate` | None | `OrphanSymbolAlert` |
| E10 | `ScopeCompilationCompleted` | `CommitScopeHorizon` | `HorizonMetadata` | CBM Connection Pool | `HorizonReadinessSummary` |

---

## 2. Subdomain Classification

| Subdomain | Type | Justification |
|---|---|---|
| **Horizon Seeding & Compilation Core** | **Core** | Principal diferencial competitivo do CBM-MGH: compilar deterministicamente intenção de escopo em grafo efêmero federado acoplado a `based_on_seq`. |
| **Tactical Spec Extraction** | **Supporting** | Parser determinístico de blocos ````tactical-spec```` contidos em arquivos Markdown; viabiliza o seeding sem sobrecarga manual no LLM. |
| **Transient Spec FTS Indexing** | **Supporting** | Ingestão das seções `001–004` no SQLite efêmero para consultas via `search_graph` e FTS durante a fase de planejamento. |
| **Federated Scope Validation** | **Core** | Verificação de integridade semântica (arestas virtuais apontando para alvos válidos na Base, ausência de nós órfãos) antes de liberar a execução. |
| **Horizon Storage & Connection Pool** | **Generic** | Gerenciamento de arquivos SQLite, transações WAL e pool LRU de descritores de arquivo. |

---

## 3. Ubiquitous Language Glossary

| Term | Definition | Notes |
|---|---|---|
| **Cognitive Horizon** | Banco de dados SQLite efêmero (`h_${domain}.db`) que abriga a projeção especulativa de escopo sem poluir o Grafo Base persistente. | Nunca persistido como verdade final; descartado após promoção ou abandono. |
| **based_on_seq** | Número sequencial monotônico (`mutation_gen`) do Grafo Base contra o qual o horizonte foi planejado e concebido. | Chave para detectar defasagem epistemológica (`STALE_BASE`). |
| **Symbolic Node** | Registro na tabela `symbolic_nodes` representando um conceito ou símbolo proposto (`is_dangling = 1`, `PROPOSED`). | Não possui código real comitado na Base. |
| **Virtual Edge** | Aresta na tabela `virtual_edges` conectando nós simbólicos entre si ou ligando um nó simbólico a um nó real da Base. | Representa `DEPENDS_ON`, `CALLS`, `EMITS`, `VERIFIES`. |
| **Tactical Block** | Bloco delimitado (````tactical-spec ... ````) em formato YAML/JSON dentro do documento `003` que expressa topologia pura. | Fonte de verdade estruturada para a compilação do horizonte. |
| **Dual-Plane Compilation** | Processo determinístico de reconciliação entre o documento Markdown em disco e o banco SQLite do horizonte. | Atômico e idempotente: editar o Markdown atualiza o banco sem corrupção. |
| **Ephemeral Spec FTS** | Tabela virtual FTS5 no horizonte efêmero indexando os termos, critérios de aceite e glossário dos documentos `001–004`. | Não altera o FTS da Base persistente. |
| **Base Adjacency** | Condição de validação que atesta se as dependências externas declaradas pelo escopo existem de fato no Grafo Base no `based_on_seq`. | Falhas geram `UNRESOLVED_BASE_DEPENDENCY`. |
