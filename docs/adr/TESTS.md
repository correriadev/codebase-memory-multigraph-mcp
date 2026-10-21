---
doc_type: adr
domain: testing
stack: [C, SQLite, MCP]
node_id: "adr:tests"
tags: [testing, native, validation]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---

# Testes e evidências

## OVERVIEW

Há um runner C com suítes selecionáveis, testes Python, contratos shell e verificações de integração Windows. **Um arquivo de teste ou uma contagem histórica não demonstra aprovação da build atual.** Alguns testes Python reproduzem algoritmos em Python; não substituem o exercício do executável C.

## COMMANDS

Execute a partir da raiz do fork. Os comandos `make` exigem GNU Make e a toolchain do projeto; veja [build](../INFRASTRUCTURE.md).

| Escopo | Comando |
|---|---|
| Fundação | `make -f Makefile.cbm test-foundation` |
| Runner nativo completo | `make -f Makefile.cbm test` |
| Union E2E nativo | `make -f Makefile.cbm test-union-workflow` |
| Seleção explícita | `make -f Makefile.cbm test-focused TEST_SUITES="union_session union_theme_registry harness_v2"` |
| Âncoras e recall | `build/c/test-runner anchor_checker recall_engine` |
| Corridas | `make -f Makefile.cbm test-tsan` |
| E2E stdio | `python tests/e2e/run_e2e.py` |
| Windows, binário existente | `pwsh -File scripts/test-windows.ps1 -GuardsOnly -Binary build/c/codebase-memory-mcp.exe` |

Em Windows o runner pode ter extensão `.exe`. `Makefile.cbm` habilita ASan/UBSan por padrão; a toolchain Windows pode exigir `SANITIZE=`. Esse resultado deve ser identificado como execução sem sanitizers. Não confundir `test-windows.ps1` com todas as suítes C ou todos os cenários do fork.

## MINIMUM COVERAGE

Não foi comprovado um gate percentual para as antigas metas de 80%, 85% ou 90%; elas foram retiradas como requisitos desta documentação. Registre build, comando, seleção, ambiente, pass/fail/skip e sanitizers. Para medir cobertura, use o mecanismo de coverage do build e registre o artefato, sem deduzi-la da quantidade de testes.

## PATTERNS & BEST PRACTICES

Valide cada camada separadamente: algoritmo C, handler MCP e efeito observável no store. Para promoção, cubra sessão ausente, contestação, âncora inválida, drift, conflito concorrente e falha de commit. Para temas, cubra persistência, versão imutável e geração desatualizada. Para harness, cubra bundles incompletos, snapshots obsoletos e rejeição de autoridade autodeclarada.

As suítes `anchor_checker`, `recall_engine`, `union_session`, `union_gateway`, `union_sweep`, `union_theme_registry`, `union_binding`, `union_founding`, `union_workflow_e2e` e `harness_v2` estão registradas em `tests/test_main.c`. Sua presença não afirma que passaram.

## TOOLING

- `Makefile.cbm` e `tests/test_main.c`: build e seleção nativos.
- `tests/e2e/`: subprocesso MCP e fixtures.
- `tests/windows/` e `scripts/test-windows.ps1`: superfície Windows.
- `tests/test_*admission*.py` inclui modelos de referência; conferir se cada teste chama C, ctypes, subprocesso ou apenas Python antes de interpretar o resultado.

## TROUBLESHOOTING

Uma recusa `SESSION_REQUIRED` pode ocorrer antes da validação de anchors em `promote_horizon`; um teste esperando `INVALID_PARAMS` precisa preparar as pré-condições corretas. Os códigos descrevem a ordem atual, não um bypass a ser ativado para obter verde.

Nesta revisão documental, `tools/list` foi executado e comparado com o registro fonte. Não se executou novamente a suíte integral nem uma medição de coverage. Relatos anteriores de testes de hooks têm escopo restrito e não validam temas, promoção ou harness.

## REFERENCES

- [Arquitetura](ARCHITECTURE.md), [fluxos de validação](../guide/WORKFLOWS.md), [medição](../MEASURING_SAVINGS.md).
