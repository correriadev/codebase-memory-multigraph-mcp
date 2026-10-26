# Refinamento de escopo — CBM System Intelligence Engine

**Data:** 2026-10-23. **Escopo:** PBB v3 integral — 32 PBIs, F01–F08. **Equipe:** CTO coordenador e subagentes `gpt-6-luna`, effort `xhigh`, em cinco frentes com revisão por autor diferente. **Natureza:** especificação documental; sem implementação, spike, benchmark ou testes de produto executados.

## Rota de leitura

| Para entender | Artefato |
|---|---|
| Intenção original e backlog | [BRIEFING](BRIEFING.md), [PBB v3](PBB.md), preservados |
| Domínio, eventos, subdomínios e glossário | [001 — Espaço do problema](001-problem-space.md) |
| Contextos, propriedade de dados e integrações | [002 — Mapa de contextos](002-context-map.md) |
| Estrutura TS, contratos, ports e tasks por projeto | [003 — Desenho tático](003-codebase-memory-multigraph-mcp-tactical-design.md) |
| Índice Unit, Integration e Functional e cenários transversais | [004 — Cenários de teste](004-codebase-memory-multigraph-mcp-test-scenarios.md) |
| Identidade, estados, orçamento, evidências e consistência | [005 — Contratos comuns C01–C08](005-shared-contracts.md) |
| Capacidades observadas e gaps, sem aliases fictícios | [006 — Matriz de capacidades](006-capability-matrix.md) |
| Pré-requisitos e ordem de entrega S0–S6 | [007 — Sequenciamento](007-delivery-sequence.md) |
| Cada PBI ligado a regra, contrato, cenário, task e dependência | [008 — Rastreabilidade](008-traceability.md) |
| Revisões cruzadas, correções e validações documentais | [009 — Revisão e verificação](009-review-verification.md) |
| Progresso da implementação real S0/S1, evidências, falhas e retomada | [010 — Progresso do orquestrador](../../adr/SPEC-010-ORCHESTRATOR-IMPLEMENTATION-PROGRESS.md) |
| Decisões humanas, recomendações e questões adiadas | [DECISIONS](DECISIONS.md) |
| Orientação de fontes, grafo/cobertura e fronteira de evidência | [ORIENTATION](ORIENTATION.md) |

## Cinco frentes

| Frente | PBIs principais | Suplemento do desenho tático |
|---|---|---|
| Execução e recuperação | 01–05, 19–22 | [01](fronts/01-execution-recovery.md) |
| Planejamento e eventos | 06–14 | [02](fronts/02-planning-events.md) |
| Autoridade e decisão | 15–18, 23–26 | [03](fronts/03-authority-decision.md) |
| Evolução supervisionada | 27–30 | [04](fronts/04-supervised-evolution.md) |
| Observabilidade e valor | 31–32 | [05](fronts/05-observability-value.md) |

Os suplementos integram o desenho de um único projeto. 001 é autoridade de linguagem, 002 de integração e 005 de contratos comuns; conflitos e aliases precisam ser reconciliados, não selecionados por conveniência. As tasks e cenários consolidados preservam a origem em cada frente.

O texto Given/When/Then de cada cenário tem uma fonte única nas frentes; 004 contém links e define somente os transversais. O gate central S0_APPROVED em 003 distingue registrar o spike de aprová-lo. ER-17 entrega o primeiro adapter real do host validado; ativação e rollback de evolução exigem guarda atômica de versão no destino. O conjunto atualizado tem 64 tasks e 126 cenários, todos ainda especificações futuras.

## Condições preservadas

O pacote TypeScript é novo e consome MCP; o core C permanece inalterado. O caminho `packages/cbm-orchestrator/` é proposta de organização, não arquivo implementado. Promover horizonte consolida conhecimento; não aplica patch de código.

Q04–Q08 continuam Deferred. O [ADR-005](../../adr/ADR-005-AGENT-EXECUTION-CONTRACT.md) continua PROPOSED e só pode ser aceito após o spike futuro. Os números de orçamento, cadência, retenção e limiares comparativos não foram inventados. A aprovação deste plano de refinamento não é aprovação desses parâmetros ou da arquitetura ainda não comprovada.

O slice inicial continua PBI-01–05, PBI-16 e telemetria mínima de PBI-31. Antes de comprometer sua implementação, é necessário demonstrar contrato de execução e barreira que impeça o Builder de promover por chamada direta ou canal alternativo. Sem F05, crash exige diagnóstico e novo ciclo desde o objetivo, sem retomada automática nem repetição cega de efeitos incertos.

Nenhuma rota de promoção dispensa evidência independente verde para a versão atual; fast path e aprovação humana preservam essa regra. `restricted` não equivale a RBAC completo, claim Union não equivale a lock de edição e flags de manifest não provam execução de testes. Deep link de nó/aresta e ativação real de skills deferred ainda exigem prova de capacidade; um fallback inferior não encerra esses PBIs.

## Limite da entrega

Validações desta entrega são documentais: cobertura de PBIs, referências, estrutura JSON, IDs/dependências, cenários Given/When/Then e reconciliação de contratos. Prontidão de runtime, eficácia de isolamento e valor econômico dependem dos experimentos e testes futuros especificados.

Esta pasta está sob a regra existente `docs/specs/*` do `.gitignore`; os artefatos são locais e não foram adicionados ao index Git, nem foi criado commit. Entradas e alterações preexistentes fora da pasta não são parte desta entrega.
