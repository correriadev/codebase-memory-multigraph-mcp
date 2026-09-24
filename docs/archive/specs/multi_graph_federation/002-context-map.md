# 002 — Strategic Design: Context Map
## Domínio: `multi_graph_federation`

---

## 1. Bounded Context Identification

Identificação dos Bounded Contexts que compõem a arquitetura federada:

| Bounded Context | Responsibility | Boundary (excluded) | Team Ownership | Key Entities |
|---|---|---|---|---|
| **Base Indexing Context** | Analisar código-fonte via Tree-sitter e Hybrid LSP, mantendo a representação imutável da AST e do grafo físico em SQLite. | Não gerencia hipóteses, não faz validação epistêmica de autoridade, não persiste estados transientes de agentes. | Core Engine Team (C/Platform) | `BaseGraph`, `SourceFile`, `AstNode`, `CallsEdge`, `ContainsEdge` |
| **Horizon Context** | Gerenciar o ciclo de vida de grafos efêmeros (`horizons/<id>.db`), pool LRU de descritores de arquivo, e reaper de instâncias órfãs. | Não valida conformidade contra o disco, não compila nem executa testes, não altera a Base Graph. | Agent Framework Team | `Horizon`, `HorizonConnectionPool`, `SymbolicNode`, `DanglingNode` |
| **Federated Query Context** | Processar consultas Cypher e buscas estruturais aplicando streaming K-Way Merge com limites $O(K)$ de memória e ordenação em pipeline. | Não persiste arestas virtuais em disco, não faz escrita em bancos, não modifica sintaxe Cypher original. | Core Engine Team (Query/Cypher) | `FederatedQueryPlan`, `KWayMergeIterator`, `VirtualEdge`, `MergedResult` |
| **Epistemic Admission Context** | Validar âncoras em duas fases (Two-Tier Anchors), invariantes de prova e orquestrar promoções e recalls em cascata ($deps^{-1}$). | Não escreve arquivos no repositório de código, não gera ASTs, não executa ferramentas de terceiros. | Architecture & Governance Team | `AdmissionGate`, `TwoTierAnchor`, `PromotionProposal`, `Contestation` |
| **Evidence Ingestion Context** | Ingerir, armazenar e consultar atestações de execução de testes, saídas de compilador e traces HTTP OpenTelemetry. | Não gera testes, não orquestra pipelines de CI, não infere arestas sintáticas estáticas. | Observability & QA Team | `EvidenceGraph`, `EvidenceAttestation`, `ObservedTrace`, `AttestationProof` |

---

## 2. Context Map

Mapeamento de relações e padrões de integração DDD:

```
[Base Indexing Context] ──(Published Language / Open Host Service)──► [Federated Query Context]
Pattern   : Open Host Service / Published Language
Direction : Base Indexing (Upstream) → Federated Query (Downstream)
Justification: O Base Graph expõe schema estável de leitura SQLite (WAL mode) e CBM-URIs como linguagem canônica compartilhada.

[Horizon Context] ──(Shared Kernel: CBM-URI)──► [Federated Query Context]
Pattern   : Shared Kernel
Direction : Bidirectional
Justification: Ambos os contextos compartilham estritamente o Value Object imutável CBM-URI para identificação de nós simbólicos e resolução de overlays.

[Horizon Context] ──(Customer-Supplier)──► [Epistemic Admission Context]
Pattern   : Customer-Supplier
Direction : Horizon (Upstream Supplier) → Epistemic Admission (Downstream Customer)
Justification: O Horizon submete uma PromotionProposal com seu grafo de hipóteses; o Admission Gate dita os requisitos rigorosos de aceitação.

[Epistemic Admission Context] ──(Anti-Corruption Layer)──► [Base Indexing Context]
Pattern   : Anti-Corruption Layer (ACL)
Direction : Epistemic Admission (Upstream) → Base Indexing (Downstream)
Justification: A admissão de uma promoção valida âncoras contra o disco e apenas autoriza a reindexação controlada do Base Graph, isolando o núcleo estático de contaminações.

[Evidence Ingestion Context] ──(Conformist)──► [Epistemic Admission Context]
Pattern   : Conformist
Direction : Evidence Ingestion (Upstream) → Epistemic Admission (Downstream)
Justification: O Admission Gate consome atestações empíricas como fatos brutos observados, sem renegociar o formato dos traces.
```

---

## 3. Core Domain Highlight

```
Context   : Federated Query Context & Epistemic Admission Context
Reason    : Constituem a barreira primária contra a alucinação e "authority laundering" de frotas autônomas de IA, entregando a fusão em tempo real de hipóteses sobre o código sem degradar a latência (<1ms).
Investment: Foco em estruturas de dados zero-allocation em C, paralelização de overlays via threads nativas, algoritmos eficientes de busca reversa ($deps^{-1}$), resolução resiliente de âncoras Two-Tier e streaming K-Way Merge com bounds de memória.
```

---

## 4. Architectural Decisions

```
Decision    : ADR-01 — Isolamento Físico de Horizontes em Arquivos SQLite Separados
Context     : Múltiplos agentes e subagentes executando em paralelo precisam criar ramos de hipóteses e refatorações sem disputar locks de escrita no SQLite.
Consequences: Positivo: Elimina completamente a contenção de escrita (zero lock contention com a Base), permite descarte trivial com deleção de arquivo. Negativo: Exige que o motor federado gerencie múltiplos file descriptors.

Decision    : ADR-02 — Resolução de Arestas Virtuais em Tempo de Execução (Zero-Mutation Overlay)
Context     : Persistir arestas entre hipóteses e o Base Graph no banco de dados principal exigiria mutações caras, quebra de integridade referencial e reversões lentas.
Consequences: Positivo: Base Graph permanece 100% estático e imutável durante o raciocínio. Negativo: Custo de resolução em memória na camada de query (mitigado por hash tables de CBM-URIs em C).

Decision    : ADR-03 — Admission Gate como Verificador Desacoplado da Escrita Física
Context     : Se o codebase-memory assumisse a escrita física de arquivos em disco, violaria o princípio de responsabilidade única (SRP) e aumentaria o risco de corrupção de código.
Consequences: Positivo: O motor foca em ser a "corte de admissão da verdade" que confere âncoras e invariantes, enquanto agentes e humanos utilizam ferramentas especializadas de edição. Negativo: Requer que o agente aplique o patch antes de solicitar a consolidação da promoção.

Decision    : ADR-04 — Identidade Canônica via CBM-URI para Suporte a Nós Dangling
Context     : Agentes precisam propor interfaces, funções e arquivos antes que eles existam no repositório.
Consequences: Positivo: Late-binding automático sem necessidade de refatorar IDs quando o código for criado. Negativo: O motor de busca federado deve lidar com referências que temporariamente não têm correspondência física no disco.

Decision    : ADR-05 — Two-Tier Anchor Resolution com Fallback para Assinatura de Nó na AST
Context     : Mudanças triviais fora do corpo do símbolo (ex: comentários no topo do arquivo) deslocam offsets brutos de bytes e causam falsos positivos de anchor drift.
Consequences: Positivo: Tolerância robusta a deslocamentos físicos de linhas; só acusa drift quando o corpo/assinatura do símbolo diverge da AST. Negativo: Requer consulta à AST do arquivo indexado caso a verificação rápida de offset falhe.

Decision    : ADR-06 — Horizon Connection Pool (LRU) e Daemon Reaper para Prevenção de Exaustão de FDs
Context     : Múltiplos arquivos SQLite em WAL mode abrem 3 FDs por conexão, podendo esgotar o limite do CRT do Windows (512 FDs) ou deixar arquivos órfãos em caso de crash do cliente MCP.
Consequences: Positivo: Impõe teto estrito de conexões ativas (ex: máx. 16), reciclando FDs via LRU; daemon limpa arquivos de PIDs mortos após TTL de 1 hora. Negativo: Pequeno overhead de reabertura de SQLite para horizontes frios.
```
