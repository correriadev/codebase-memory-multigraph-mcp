---
doc_type: spec
domain: graph-engineering
stack: [TypeScript, Node.js, C, SQLite, MCP, Codex]
node_id: "spec:orchestrator-implementation-progress"
tags: [orchestration, implementation, progress, validation]
edges:
  - relation: references
    target: "adr:architecture"
  - relation: references
    target: "adr:tests"
updated: 2026-10-26
---

# Spec 010 — Progresso e retomada da integração real do orquestrador

## STATUS E AUTORIZAÇÃO

**EM IMPLEMENTAÇÃO. S0/S1 não estão concluídos.** Checkpoint de 26/10/2026 sobre `4be59df7`, com alterações não commitadas e arquivos novos não rastreados.

O usuário solicitou concluir a integração e autorizou explicitamente o ajuste mínimo no core C para promoção segura, conforme a decisão exigida em 007. Isso não aprova S0, parâmetros de orçamento ou todos os PBIs.

**Alvo:** executar S0/S1 por MCP com agentes reais, sessões, evidência independente, orçamento/journal e promoção versionada. S2–S6 ficam fora desta retomada. Última solicitação: registrar progresso e fornecer prompt de continuação.

## IMPLEMENTAÇÃO PRESENTE

Arquivos TS abaixo pertencem a `packages/cbm-orchestrator/src/`. Presença no fonte não comprova conclusão.

| Frente | Alterações presentes | Validação e limite |
|---|---|---|
| Transporte MCP | `mcp-transport.ts`: JSON-RPC stdio, negociação, paginação, recusas, timeout/cancelamento e encerramento. | Subprocessos e leitura do MCP instalado passaram. |
| Canal por agente | `agent-channel.ts`, `agent-gateway.ts`: pipe/socket autenticado por invocação, sem canal privilegiado. | Fixture testa descoberta e promoção direta negada; falta provar Codex real. |
| Adapter Codex | `codex-runner.ts`: JSON/schema, inspeção do CLI, read-only, configuração/rules ignoradas, ferramentas desabilitadas, sessões/logs. | Exercitado com fixture; falta ciclo com modelos reais. |
| Evidência | Verificador separado, candidata selada e comando de teste do host com saída persistida e EvidenceRecords. | Encadeamento testado com fixture; falta matriz negativa real. |
| Adapter CBM | `cbm-backend.ts`: schemas, projeto/workspace exatos, snapshot Harness, sessões, staging e anchors. | Fixture aprovada; nenhuma promoção do fluxo no binário instalado. |
| Ciclo/journal | Runner obrigatório; mocks em `test/helpers/`; UNKNOWN preservado; cancelamento, eventos, CAS SQLite e breaker. | TS passa; contratos ainda precisam das correções abaixo. |
| Entrada | `application.ts`, `cli.ts`: `serve`, `probe`, `run`, `spike`, `approve-s0`; tools `orchestrator_probe`, `orchestrator_run_cycle`, `orchestrator_get_run`, `orchestrator_cancel_cycle`. | Falta integração do servidor TS completo pelo cliente MCP. |
| S0 | `s0-spike.ts`, `live-spike.ts`, `journal-crash-probe.ts`: relatório, aprovação CLI e crash real SQLite. | Crash probe passou; S0 completo não foi aprovado. |
| Core C | `src/admission/admission_gate.{c,h}`, `src/mcp/{promote_handler,mcp}.c`, `src/store/store.{c,h}`: geração sob transação, recibo idempotente e guard de snapshot/governança. | Teste de geração passou; snapshot foi editado durante o build e exige reconstrução estável. |

## EVIDÊNCIA EXECUTADA

| Verificação | Resultado observado | Alcance |
|---|---|---|
| `npm --prefix packages/cbm-orchestrator test` | **30 passaram, 0 falharam**, no estado TS registrado. | Fixtures, processos, mediação MCP e crash SQLite; sem modelos reais. |
| TypeScript com `--noEmit` | **Código de saída 0** no estado TS registrado. | Usa o compilador já instalado em `graph-ui` e tipos Node disponíveis no runtime local. |
| `npm --prefix packages/cbm-orchestrator run test:live-mcp` | **1 passou**; versão `mgh-8dd9f4ec-codex-20261021-relaxed`, **45 tools**, `guardedPromotion:false`. | Handshake/schemas/leitura no instalado; não prova promoção ou patch C. |
| C: `anchor_checker union_workflow_e2e`, sem sanitizers | **50 passaram, 3 falharam**. `test_promote_horizon_guarded_generation_and_idempotency_c` passou. | Log: `build/orchestrator-native-tests.log`. A origem das três falhas ainda não foi isolada. |
| Build do executável | **Concluído, saída 0.** | `build/orchestrator-native-build.log`; alvo `build/codex-mgh-20261021/codebase-memory-mcp.exe`. Falta smoke do binário novo e confirmação de correspondência ao fonte final. |

REQUIRED: reconstruir C após estabilizar o fonte; o build anterior coincidiu com edições. Não houve suíte integral ou medição de coverage.

### FALHAS NATIVAS ABERTAS

1. `test_promotion_requires_anchors_and_persistent_base`, `tests/test_union_workflow_e2e.c:138`: `ASSERT(empty_rejected)`; o handler respondeu `SESSION_REQUIRED` antes da validação esperada.
2. `test_promote_rejects_invalid_anchor_fields_before_lookup`, mesmo arquivo, linha 1063: esperado `INVALID_PARAMS`, observado `SESSION_REQUIRED`.
3. `test_promote_horizon_in_memory_base_db_rejected_c`, `tests/test_anchor_checker.c:351`: esperado `CBM_ADMISSION_ERR_COMMIT_FAILED (-8)`, observado `-1`.

REQUIRED: reproduzir e comparar ao baseline antes de corrigir. PROHIBITED: enfraquecer autoridade ou ativar override legado para obter verde.

## TRABALHO RESTANTE, EM ORDEM

### P0 — ESTABILIZAR PROMOÇÃO REAL

1. Estabilizar o build C e as três falhas. Cobrir snapshot, drift de regras/cobertura, rollback, replay e colisão de chave. Verificar store de consulta dentro da transação e ordem dos locks grafo/governança.
2. Validar no **executável recém-construído**, por MCP real, os novos parâmetros: `expected_graph_generation`, `candidate_version_id`, `idempotency_key`, `expected_snapshot_id`, `snapshot_poles`, `project`, `repo_path` e anchors. REQUIRED: drift recusa sem consolidação; replay não duplica efeito.
3. Exercitar `cliente MCP → servidor TS → Codex real → CBM real`: candidata, teste independente, promoção e estado confirmado. O instalado não contém o guard novo.

### P1 — FECHAR CONTRATOS E RECUPERAÇÃO

4. Corrigir `application.ts`: `connection()` pode lançar antes da limpeza. Garantir encerramento, tratamento seguro do lock/horizonte e consulta de execução interrompida; preservar UNKNOWN.
5. Vincular S0 à **identidade/versão efetiva dos executáveis**: o digest atual do JSON não detecta troca de binário no mesmo caminho.
6. Retirar fallbacks sintéticos de revisões e `policyRevision` fixo em `orchestrator.ts`. Registrar ALLOW após a validação completa; hoje o evento a precede.
7. Endurecer `cbm-backend.ts`: staging escreve nos arquivos reais; falha pode deixar alterações parciais, e rejeição não restaura código. Provar worktree/candidata, escrita atômica e drift sem apagar alterações de terceiros.
8. Completar testes de uso ausente/malformado, verificador indisponível/inconclusivo, kill e timeout com subprocessos filhos, breaker por classe/janela, cancelamento via MCP, drift pós-revisão e falha entre efeito remoto e registro local. Manter consumo e efeito incertos explícitos, sem replay cego.
9. Quantificar exposição financeira em voo. Reservas/reconciliação não provam teto rígido no CLI; exigir política mensurável e relatar risco residual.

### P2 — FECHAR S0 E ENTREGAR USO EXPLORATÓRIO

10. Substituir a prova de autoridade local simulada de `runLiveSpike()` por evidência na fronteira real, incluindo chamada direta, canal alternativo, verificador ausente e mudança de versão. Hoje parte do spike ainda verifica um proxy em memória, mesmo com `source: live`.
11. Reconciliar o ADR-005: ele afirma ACCEPTED com base no spike anterior simulado. REQUIRED: registrar essa evidência como insuficiente e manter a aceitação real pendente até cumprir todos os critérios e obter a decisão apropriada. A autorização de alteração do core não aceita o ADR automaticamente.
12. Entregar configuração de exemplo validada e guia do pacote: modelo/host compatíveis, tarifas versionadas, orçamento explícito, workspace, projeto/poles/targets, comando de verificação, probe, spike e aprovação S0. Registrar o servidor TS no cliente MCP e demonstrar as quatro tools operacionais. Não alterar configurações pessoais nem instalar o binário silenciosamente.

## COMANDOS PARA RETOMADA

CORRECT: execute os checks TS da raiz do repositório.

```powershell
npm --prefix packages/cbm-orchestrator test
npm --prefix packages/cbm-orchestrator run test:live-mcp
node graph-ui/node_modules/typescript/bin/tsc --noEmit --allowImportingTsExtensions --types node --typeRoots C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@types -p packages/cbm-orchestrator/tsconfig.json
```

CORRECT: use GNU Make/MSYS2 com a toolchain configurada; os comandos abaixo são os alvos executados, sem a camada de invocação PowerShell.

```bash
make -j4 -f Makefile.cbm test-focused CC=clang CXX=clang++ SANITIZE= BUILD_DIR=build/codex-mgh-tests-20261021 TEST_SUITES="anchor_checker union_workflow_e2e"
make -j4 -f Makefile.cbm cbm CC=clang CXX=clang++ BUILD_DIR=build/codex-mgh-20261021
```

REQUIRED: conferir builds ativos antes de recompilar. Logs em `build/` são locais.

## CRITÉRIO DE CONCLUSÃO

S0/S1 exigem checks aprovados, integração real, candidata/evidência/promoção vinculadas, isolamento comprovado, journal recuperável e instrução manual reproduzível. PROHIBITED: declarar conclusão por fixtures verdes, interfaces ou `tools/list`.

Preservar alterações do usuário, inclusive `.codebase-memory/*`, e arquivos novos. Não criar commit sem solicitação. Esta spec não autoriza S2–S6 nem substitui os contratos originais.

## REFERENCES

- [003 — Desenho tático](../specs/graph_engineering/003-codebase-memory-multigraph-mcp-tactical-design.md): tasks SP, ER e AD.
- [005 — Contratos compartilhados](../specs/graph_engineering/005-shared-contracts.md): C01–C08 e fronteiras de confiança.
- [007 — Sequenciamento](../specs/graph_engineering/007-delivery-sequence.md): saída S0/S1 e decisão de mudanças no core.
- [ADR-005](ADR-005-AGENT-EXECUTION-CONTRACT.md): contrato e afirmação de aceitação a reconciliar.
- [ARCHITECTURE](ARCHITECTURE.md) e [TESTS](TESTS.md): contexto e interpretação dos checks nativos.
