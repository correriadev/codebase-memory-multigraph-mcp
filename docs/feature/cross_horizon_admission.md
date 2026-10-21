---
doc_type: feature
domain: admission
stack: [C, SQLite, MCP]
node_id: "feature:cross-horizon-admission"
tags: [admission, conflicts]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:cross-horizon-admission","domain":"admission","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/promote_handler.c","src/mcp/horizon_sync_handler.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/admission/admission_gate.c"],"test_files":["tests/test_union_contest.c","tests/test_cross_horizon_admission.py"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:horizon-admission","type":"capability","label":"Admissão de horizonte","definition":"Consolida dados especulativos no grafo base.","aliases":[]},{"id":"rule:session-required","type":"rule","label":"Sessão exigida por padrão","definition":"Exceção explícita via CBM_ALLOW_LEGACY_PROMOTION=1.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:horizon-admission","relation":"constrained_by","object":"rule:session-required","statement":"handle_promote_horizon recusa sessão ausente, salvo override exato 1, e verifica contestações antes de parsear âncoras.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/promote_handler.c","locator":"handle_promote_horizon: CONTEST_BLOCKED, SESSION_REQUIRED e CBM_ALLOW_LEGACY_PROMOTION","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Admissão e conflitos entre horizontes

## OVERVIEW

`promote_horizon` tenta incorporar elementos especulativos ao grafo base mediante pré-condições. Não implementa a mudança sugerida e não executa uma revisão independente.

## ORDEM DO HANDLER

1. Valida ID e consulta contestações bloqueantes.
2. Procura sessão Union com o mesmo `horizon_id`. Por padrão, sem sessão retorna `SESSION_REQUIRED`; com recusas registradas pode retornar `GATEWAY_BLOCKED`.
3. Faz parsing e limites das âncoras.
4. O gate exige base disponível e ao menos uma âncora, verifica conflitos e drift, e tenta a consolidação transacional.

`CBM_ALLOW_LEGACY_PROMOTION=1` permite a exceção explícita à sessão; não ignora os demais checks e não é um passo necessário de uso normal.

## DIFERENÇA ENTRE SCHEMA E EXECUÇÃO

O schema anuncia apenas `horizon_id` obrigatório e descreve `anchors` como opcional. O gate exige uma lista não vazia; o parser exige também `symbol_name` para cada âncora, embora o schema aninhado exija apenas `file_path`. Arquivo, símbolo e hash precisam corresponder à implementação atual. Não forneça hashes inventados para satisfazer o formato.

Uma validação de escopo ou conflito feita antes não substitui o check na admissão. A ordem das recusas importa: uma sessão ausente pode esconder uma âncora malformada até a primeira pré-condição ser resolvida.

## DEPENDÊNCIAS

Veja [âncoras](ast_anchor_isolation.md) para identidade/relocação e [transação](atomic_admission_concurrency.md) para persistência. Uma contestação bloqueante é registrada por `contest_verify`; isso não dispara automaticamente o [recall interno](transitive_recall_closure.md).

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
