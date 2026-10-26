---
doc_type: adr
domain: architecture
stack: [TypeScript, C, SQLite, MCP]
node_id: "adr:agent-execution-contract"
tags: [architecture, orchestration, pbb, graph-engineering]
edges:
  - relation: references
    target: "adr:three-territories"
    read: must
updated: 2026-10-23
---

# Contrato de execução de agentes para a camada de orquestração (Q04 do PBB)

**Status: PROPOSED (Implementação e validação ao vivo em andamento)** — A aceitação formal permanece pendente até a demonstração ponta-a-ponta do ciclo real S0/S1 com modelos reais Codex e CBM conectado (Spec 010). A viabilidade conceitual e os testes em subprocessos locais com SQLite WAL foram verificados em `packages/cbm-orchestrator/`.

## CONTEXTO

O PBB ([docs/specs/graph_engineering/PBB.md](../specs/graph_engineering/PBB.md), v3) marca a questão Q04 como pré-requisito do slice inicial: definir o contrato mínimo de execução de agentes — iniciar, cancelar, isolar, medir, recuperar — antes de quebrar as tasks. A auditoria de um terceiro revisor confirmou que o PBB exige recuperação sem provar que a arquitetura escolhida consegue implementá-la, e enumerou quatro perguntas abertas: qual runtime executa Builder e Adversarial, como o orquestrador controla o ciclo de vida dos processos, como sessões são reconstruídas após reinicialização, e qual mecanismo garante persistência durável e coordena a retomada. Este ADR fixa o contrato e define o spike que o valida.

## DECISÃO

Agentes são **processos OS separados**, gerenciados pela camada TS (cbm-orchestrator), cada um **cliente MCP do servidor CBM** sob um perfil (builder / adversarial). Host-agnóstico: codex CLI, gemini CLI, opencode ou agente próprio — escolha de configuração, não de código.

Contrato mínimo (as cinco operações de Q04):

| Operação | Mecanismo previsto |
| --- | --- |
| **iniciar** | Spawn de processo com perfil + `union_session_open` (`identity`, `client_pid`, `host`, `context_id`); builder mutante registra `intent_key`/`intent_scope` com pares exatos path+operação. |
| **cancelar** | Término do processo + `union_session_close` (ABNORMAL) e/ou descarte do território especulativo; interrupção registrada no journal. |
| **isolar** | Um processo por agente + sessão `restricted` limitando a superfície de ferramentas; o admission gate já recusa operações de escrita sem sessão válida com intent scope exato. |
| **medir** | Contabilidade por ciclo no journal do orquestrador (tokens/custo, veredictos) — subconjunto do PBI-31 (telemetria mínima do slice). |
| **recuperar** | Journal de execução próprio em SQLite WAL; reconstrução = reler journal + `union_session_get`/estado de horizonte; semântica de crash conforme o PBB (sem F05: reinício/reexecução; com F05: retomada idempotente per PBI-20). |

**Persistência durável:** SQLite WAL em dois lados — os territórios CBM (ADR-002, ADR-004) e o journal do orquestrador (novo, de propriedade da camada TS; não escreve no banco do core).

**Coordenação de retomada:** o journal é a fonte de verdade da execução; o CBM responde pelo estado de conhecimento. Nenhum lado presume o outro: divergência (ex.: journal aponta horizonte já descartado) resulta em **reinício do ciclo, nunca promoção** — consistente com a invariante de promoção do PBB.

## SPIKE DE VIABILIDADE (critério de aceite deste ADR)

Protótipo enxuto em TS (o "Outer Loop de ~80 linhas" proposto no briefing): ciclo Builder → Adversarial → Gate sobre uma mudança trivial. O ADR só passa a ACCEPTED se:

1. O ciclo ponta-a-ponta completa com decisão final registrada.
2. Kill do orquestrador no meio do ciclo → reinício apresenta estado consistente, sem corrupção; território inconsistente é descartável.
3. Cancelar um agente encerra o processo e registra a interrupção.
4. Custo por ciclo é medido e persistido no journal.
5. Nenhuma operação fora do `intent_scope` registrado passa pelo admission gate (recusa efetivamente observada).

Falha em qualquer item devolve este ADR a PROPOSED com revisão, antes da quebra de tasks do slice.

## EVIDÊNCIA

- `src/union/union_session.h` / `src/union/union_session.c` — `CbmSessionHorizon` (campos `restricted`, `client_pid`, `host`); sessões persistidas como horizontes.
- `src/mcp/union_handler.c` — `handle_union_session_open` / `handle_union_session_get` / `handle_union_session_close` (linhas 454–829).
- `src/admission/admission_gate.c` — `cbm_enforce_union_session` (linhas 1538–1552): o gate recusa operação sem sessão válida.
- `docs/guide/WORKFLOWS.md` — protocolo de sessão com `identity`/`restricted`.
- Testes: `tests/test_union_session.c`, `tests/test_union_workflow_e2e.c` (session open exige e registra intent scope exato), `tests/test_atomic_admission_concurrency.py`.
- ADR-002 (três territórios) e ADR-004 (SQLite WAL, Harness v2).

## LIMITES

- O journal do orquestrador é **novo**: nada dele existe ainda; o spike é a prova da cadeia completa.
- Sessões Union persistem estado de conhecimento e protocolo, **não** o contexto conversacional interno do host do agente; reconstrução do raciocínio do agente está fora de contrato — o journal registra entradas, saídas e efeitos.
- Medição depende do host reportar uso (codex CLI reporta tokens; outros hosts variam).
- A interface atual aceita hosts `codex` e `antigravity`; estender a outros hosts é extensão futura, não presumida por este ADR.

## REFERENCES

- PBB: [docs/specs/graph_engineering/PBB.md](../specs/graph_engineering/PBB.md) — Q04, Fronteira do slice inicial, PBI-19..22, PBI-31.
- [ADR-002](ADR-002-THREE-TERRITORIES.md), [ADR-004](ADR-004-HARNESS-PERFEITO-V2.md).
- [docs/guide/CONSUMPTION.md](../guide/CONSUMPTION.md), [docs/guide/WORKFLOWS.md](../guide/WORKFLOWS.md).
