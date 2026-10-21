# Documentação CBM-MGH

Índice de navegação da documentação atual do fork. Use os links para escolher o assunto necessário.

## Documentation Index

| Document | Description | Reading |
|---|---|---|
| [**.digest.md**](.digest.md) | Orientação rápida e limites verificados. | **Mandatory** |
| [**.graph.json**](.graph.json) | Mapa compacto de relações documentais. | **Mandatory** |
| [**adr/ARCHITECTURE.md**](adr/ARCHITECTURE.md) | Arquitetura e fronteiras atuais. | **Mandatory** |
| [**adr/TESTS.md**](adr/TESTS.md) | Comandos, camadas de teste e limites da evidência. | **Mandatory** |
| [**guide/CONSUMPTION.md**](guide/CONSUMPTION.md) | Entrada prática para usar o fork. | Optional |
| [**guide/TOOLS_REFERENCE.md**](guide/TOOLS_REFERENCE.md) | Catálogo das 45 tools e diferenças de contrato. | Optional |
| [**guide/mcp-tools.json**](guide/mcp-tools.json) | Snapshot integral dos schemas anunciados. | Optional |
| [**guide/WORKFLOWS.md**](guide/WORKFLOWS.md) | Experimentos por capacidade e critério de sucesso. | Optional |
| [**CONFIGURATION.md**](CONFIGURATION.md) | Configuração do runtime e do cliente. | Optional |
| [**INFRASTRUCTURE.md**](INFRASTRUCTURE.md) | Build e organização do runtime. | Optional |
| [**INDEX_RESOURCE_LIMITS.md**](INDEX_RESOURCE_LIMITS.md) | Limites opcionais de descoberta. | Optional |
| [**cbmignore.md**](cbmignore.md) | Exclusões do índice e precedência. | Optional |
| [**RUNTIME_TRACE_MODEL.md**](RUNTIME_TRACE_MODEL.md) | Superfície atual de traces e limites. | Optional |
| [**SMART_CI.md**](SMART_CI.md) | Escopo da seleção experimental de testes. | Optional |
| [**BENCHMARK.md**](BENCHMARK.md) | Qualificação das medições disponíveis. | Optional |
| [**MEASURING_SAVINGS.md**](MEASURING_SAVINGS.md) | Método para comparar utilidade e custo. | Optional |
| [**SECURITY-DISCLOSURE.md**](SECURITY-DISCLOSURE.md) | Referência ao processo de segurança herdado. | Optional |
| [**images/README.md**](images/README.md) | Proveniência das capturas herdadas da UI. | Optional |
| [**adr/ADR-001-COGNITIVE-RESPIRATION.md**](adr/ADR-001-COGNITIVE-RESPIRATION.md) | Hooks de mutação opcionais. | Optional |
| [**adr/ADR-002-THREE-TERRITORIES.md**](adr/ADR-002-THREE-TERRITORIES.md) | Separação entre base, hipótese e tema. | Optional |
| [**adr/ADR-003-TWO-TIER-ANCHORS.md**](adr/ADR-003-TWO-TIER-ANCHORS.md) | Âncoras e admissão no grafo. | Optional |
| [**adr/ADR-004-HARNESS-PERFEITO-V2.md**](adr/ADR-004-HARNESS-PERFEITO-V2.md) | Bundles e governança local do harness. | Optional |
| [**feature/code_discovery.md**](feature/code_discovery.md) | Consultar o grafo de código. | Optional |
| [**feature/index_governance.md**](feature/index_governance.md) | Ciclo de vida e cobertura do índice. | Optional |
| [**feature/multi_graph_federation.md**](feature/multi_graph_federation.md) | Horizontes e overlays especulativos. | Optional |
| [**feature/ast_anchor_isolation.md**](feature/ast_anchor_isolation.md) | Verificação de âncoras AST. | Optional |
| [**feature/atomic_admission_concurrency.md**](feature/atomic_admission_concurrency.md) | Transação de promoção. | Optional |
| [**feature/cross_horizon_admission.md**](feature/cross_horizon_admission.md) | Admissão e conflitos entre horizontes. | Optional |
| [**feature/transitive_recall_closure.md**](feature/transitive_recall_closure.md) | Motor interno de recall. | Optional |
| [**feature/union_workflow.md**](feature/union_workflow.md) | Sessões, ações e claims Union. | Optional |
| [**feature/tradition.md**](feature/tradition.md) | Temas versionados, bindings e founding. | Optional |
| [**feature/harness_v2.md**](feature/harness_v2.md) | Bundles, fast path e regras locais. | Optional |
| [**feature/mutation_gate.md**](feature/mutation_gate.md) | Hooks e verificação opcional de mutação. | Optional |

## Recommended Reading Order

1. Se recebeu um caminho exato, leia esse documento diretamente.
2. Para orientação rápida de agente: [.digest.md](.digest.md), depois [.graph.json](.graph.json).
3. Para uso: [consumo](guide/CONSUMPTION.md), [tools](guide/TOOLS_REFERENCE.md) e [fluxos](guide/WORKFLOWS.md).
4. Consulte arquitetura, testes e a feature necessária; os ADRs adicionais registram decisões e limites específicos.
