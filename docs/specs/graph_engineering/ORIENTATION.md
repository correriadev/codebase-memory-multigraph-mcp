# Orientação compartilhada — Graph Engineering

Data: 2026-10-23. Projeto: `codebase-memory-multigraph-mcp`. Escopo: PBB v3, 32 PBIs, F01–F08. Execução autorizada pelo operador: refinamento documental completo por cinco frentes; subagentes `gpt-6-luna`, effort `xhigh`; sem implementação nem execução do spike.

## Autoridade e limites

- Entradas: `BRIEFING.md`, `PBB.md`; decisões humanas do PBB têm precedência sobre recomendações. O briefing é motivação, não prova de capacidades nem de suas referências externas; não se fez pesquisa externa nesta etapa.
- Camada nova em TypeScript, pacote neste repositório, core C mantido. Novo pacote proposto: `packages/cbm-orchestrator/`; caminho é recomendação de projeto, não implementação existente.
- ADR-005 (`docs/adr/ADR-005-AGENT-EXECUTION-CONTRACT.md`) é PROPOSED: iniciar/cancelar/isolar/medir/recuperar dependem dos cinco critérios do spike. Não aceitar o ADR nem executar o protótipo durante este refinamento.
- Slice inicial: PBI-01–05 + PBI-16 + telemetria mínima de PBI-31 + contrato de execução e mapeamento de capacidades. Sem F05, crash implica diagnóstico e novo ciclo desde objetivo; não retomada automática nem repetição cega de efeitos externos incertos.
- Não alterar entradas, ADRs, código, `.codebase-memory/` ou mudanças preexistentes. Não ler/escrever `docs/harness-history/`. O acesso a estes documentos de escopo e aos novos artefatos é autorizado pelo plano aprovado, sobrepondo o isolamento genérico da skill.
- Glossário: `001-problem-space.md` é autoridade após criação. Integrações: `002-context-map.md`. Contratos comuns: `005-shared-contracts.md`. Questões: `DECISIONS.md`. Todos são propostas de refinamento, salvo regra vinculante explicitamente atribuída ao PBB.
- Plano aprovado autoriza execução das etapas documentais sem nova aprovação intermediária. Questões adiadas permanecem pendências; recomendações são `model`, nunca `human`. Nenhum comportamento de produto está provado por escrever sua especificação.

## Orientação documental carregada pelo CTO

`docs/.digest.md` e `docs/.graph.json` lidos uma vez. Backend nativo C/SQLite/Tree-sitter; UI opcional Node/Vite; novo orquestrador é serviço/processo TS, sem impor migração DDD ao core.

Nós selecionados: `adr:architecture`, `adr:tests`, `adr:agent-execution-contract` (ADR-005 existe, mas ainda não consta do índice documental); `feature:union-workflow`, `feature:harness-v2`, `feature:atomic-admission-concurrency`, `feature:multi-graph-federation`, `feature:mutation-gate`, `feature:tradition`, `feature:transitive-recall-closure`, `feature:code-discovery`, `feature:index-governance`, `feature:ast-anchor-isolation`, `feature:cross-horizon-admission`.

As features selecionadas exigem `docs/adr/ARCHITECTURE.md` (lido) e não têm rotas opcionais. Arestas de um salto: cada feature referencia/implementa arquitetura e é `tested_by` testes; arquitetura referencia testes. Micrografos relevantes:

| Feature | Entrypoints / registro | Código | Testes de referência, não executados |
|---|---|---|---|
| Union | `src/mcp/union_handler.c`; `src/mcp/mcp.c` | `src/union/union_session.c`, `union_sweep.c`, `union_gateway.c`, `union_claim.h` | `tests/test_union_session.c`, `test_union_sweep.c`, `test_union_gateway.c` |
| Harness | `src/mcp/harness_handlers.c`; `src/mcp/mcp.c` | `src/harness/harness_v2.c` | `tests/test_harness_v2.c` |
| Promoção | `src/mcp/promote_handler.c`; `src/mcp/mcp.c` | `src/admission/admission_gate.c`, `.h` | `tests/test_atomic_admission_concurrency.py`, `tests/test_anchor_checker.c` |
| Horizontes | `src/mcp/horizon_handler.c`, `horizon_sync_handler.c`; `src/mcp/mcp.c` | `src/mcp/handlers.c`, `src/core/horizon_pool.c` | `tests/test_horizon_spec_parser.c`, `test_horizon_reaper.c` |
| Hooks | `src/main.c`, `src/cli/hook_augment.c` | `src/union/mutation_gate.c`, `mutation_journal.c` | `tests/test_cli.c`, `tests/windows/test_hook_augment.py` |
| Temas | `src/mcp/union_handler.c`; `src/mcp/mcp.c` | `src/union/union_theme_registry.c`, `union_binding.c`, `union_founding.c` | `tests/test_union_theme_registry.c`, `test_union_binding.c`, `test_union_founding.c` |
| Recall | `src/admission/recall_engine.h`; sem tool pública equivalente demonstrada | `src/admission/recall_engine.c` | `tests/test_recall_engine.c`, `test_transitive_recall_closure.py` |

Documentos adicionais válidos: `docs/guide/TOOLS_REFERENCE.md`, `docs/guide/mcp-tools.json`, `docs/guide/CONSUMPTION.md`, `docs/guide/WORKFLOWS.md`, `docs/adr/TESTS.md`, `skills/README.md`. Ler apenas os trechos relevantes da frente; não recarregar índices globais.

## Evidência de grafo fornecida aos agentes

Tier Verify, limitado aos contratos relevantes, não auditoria exaustiva do repositório. Projeto MCP exato: `C-Users-User-Documents-codebase-memory-multigraph-mcp`. Índice ready, geração/indexed_at `2026-10-23T18:37:27Z`, coverage v9, metadata matches nos caminhos exatos consultados. Watch Git ativo. Mudanças futuras exigem revalidar freshness.

Consulta exata de funções (7 resultados, sem próxima página): `^(handle_union_session_open|handle_union_session_close|cbm_enforce_union_session|cbm_promote_horizon|cbm_harness_evaluate_fast_path|handle_record_admission_manifest)$`. Prefixo QN = projeto + caminho pontuado sem extensão. Achados:

- `src.mcp.union_handler.handle_union_session_open` linhas 454–660; close 710–829.
- `src.admission.admission_gate.cbm_enforce_union_session` 1538–1552; snippet exato confirma checagem de string não vazia e override legado, não RBAC. `cbm_promote_horizon` 439–1536.
- `src.harness.harness_v2.cbm_harness_evaluate_fast_path` 1357–1622.
- `src.mcp.harness_handlers.handle_record_admission_manifest` 345–510.
- Um homônimo `cbm_enforce_union_session` em teste Python é modelo de teste, não o C de produção.

Pesquisa ampla inicial trouxe 60/197 resultados e foi abandonada em favor da consulta exata; não usar essa página para alegar completude. Schema foi inspecionado parcialmente (50/70), suficiente para Function/CALLS; nenhuma alegação sobre todos os tipos. Não houve trace de cadeia de chamadas; não alegar que foi feito.

Coverage exato: sem problema registrado em `union_session.c`, `admission_gate.c`, `harness_handlers.c`, `harness_v2.c`, `union_claim.c`; não prova completude. Partial em `mcp.c` (204,214,4906,5052,7602,7678,8503,9183,9783,11024,13755,14182,18465,20356,20775,20788), `union_handler.c` (88,1754), `promote_handler.c` (117). Fonte relevante foi lido diretamente para sessão, restrição, schema de tools e promoção, incluindo ranges de Union e promoção. Agente que basear nova alegação em outro range parcial deve ler o range e contexto primeiro.

Coverage por escopo foi paginado novamente até completo para `src/mcp` (9 arquivos com gaps) e `src/watcher` (1: watcher.c:1041); não usar ausência no grafo como prova negativa. `src/union` tem gaps em binding.c:88,101 e theme_registry.c:108. `docs/specs/graph_engineering` é ignorado por design: consultar arquivos diretamente. Diretórios passados como paths retornam not_tracked, não ausência de implementação.

## Achados vinculantes para o refinamento

1. `restricted` = contrato de skill não encontrado; restringe determinadas classes irreversíveis em `union_record_action`, não perfis builder/adversarial/gatekeeper nem uma barreira geral às tools. Fonte: `union_session.c:129–169`, `union_handler.c:875`.
2. Claim Union é proposição documental (`union_claim.h`), não exclusão mútua sobre AST. Claims exclusivos de trabalho exigem contrato novo na camada TS.
3. `promote_horizon` é tool pública que verifica sessão/contestações/âncoras; `handle_promote_horizon` não comprova identidade de perfil nem veredicto independente atual. `CBM_ALLOW_LEGACY_PROMOTION=1` é bypass legado, incompatível com a garantia desejada. Isolamento mediado por host/proxy é proposta a provar, não fato do CBM.
4. Promoção consolida grafo, não aplica patch de código. O journal TS não deve escrever no banco C. Fechamento Union destrói conteúdo especulativo; exportar evidência antes e não confundir sessão e overlay fractal.
5. Tools reais incluem `extract_harness_bundle`, `evaluate_fast_path`, `record_admission_manifest`, `promote_horizon`, `check_horizon_conflicts`, `union_session_open/get/close`. Nomes no briefing não constituem endpoints disponíveis. Fast path não substitui evidência independente. `tdd_oracles_passed` no manifest é declaração, não execução de teste.
6. Hooks são opt-in, `CBM_ENFORCE_MUTATION_GATE=1`; não garantem interceptação de todo efeito externo. Limite de intent_scope: 1–4 pares exatos path+operação; host anunciado: codex/antigravity.
7. Skills de evolução estão deferred e analysis-only (`skills/README.md`); não há admissão real de skill demonstrada. Temas e regras locais do harness são mecanismos distintos.
8. Watcher é mecanismo de reindexação; contrato de eventos de negócio/anomalias precisa ser projetado. Deep link por nó/aresta da graph-ui não está comprovado: há parâmetros de workspace/project; validar navegação exata antes de prometer link funcional.

## Coordenação e entregas

Frentes: 1 execução/recuperação F01+F05; 2 planejamento/eventos F02+F03; 3 autoridade/decisão F04+F06; 4 evolução F07; 5 observabilidade/valor F08. Até três agentes simultâneos; arquivos de frente exclusivos; todos estão no mesmo worktree e devem preservar alterações alheias. CTO é dono de contratos compartilhados e consolida 003/004 por projeto. Revisões cruzadas por autor diferente; cenários são especificados, não executados.
