---
doc_type: feature
domain: admission
stack: [C, SQLite, MCP]
node_id: "feature:atomic-admission-concurrency"
tags: [admission, sqlite]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:atomic-admission-concurrency","domain":"admission","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/promote_handler.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/admission/admission_gate.c","src/admission/admission_gate.h"],"test_files":["tests/test_atomic_admission_concurrency.py"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:atomic-promotion","type":"capability","label":"Atomic Horizon Promotion","definition":"Consolida nós e arestas verificados usando transação SQLite entre base e horizonte anexado.","aliases":["2pc-promotion"]},{"id":"rule:two-phase-reconciled-order","type":"rule","label":"Atualização conjunta de base e horizonte","definition":"Base e metadata do horizonte anexado são atualizadas na mesma transação, antes do COMMIT SQLite.","aliases":["2pc-order"]},{"id":"rule:fail-fast-step-validation","type":"rule","label":"Fail-Fast Step Validation","definition":"Falhas verificadas de prepare/step durante consolidação abortam e revertem a transação.","aliases":["consolidation-check"]},{"id":"contract:admission-error-codes","type":"contract","label":"Admission Error Taxonomy","definition":"Standardized admission error codes: CONSOLIDATION_FAILED (-7), COMMIT_FAILED (-8), SESSION_REQUIRED (-9), and STATE_TRANSITION_FAILED (-11).","aliases":["f002-errors"]}],"claims":[{"id":"claim:fail-fast-consolidation","subject":"capability:atomic-promotion","relation":"constrained_by","object":"rule:fail-fast-step-validation","statement":"Falhas verificadas de prepare/step na consolidação abortam a transação com ROLLBACK e erro específico de consolidação ou commit.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/admission_gate.c","locator":"cbm_promote_horizon: sqlite3_step validation loops","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:two-phase-order","subject":"capability:atomic-promotion","relation":"constrained_by","object":"rule:two-phase-reconciled-order","statement":"O caminho com base persistente atualiza h_db.horizon_metadata para PROMOTED antes do COMMIT conjunto; a consolidação usa rollback journal DELETE e synchronous>=FULL verificados.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/admission_gate.c","locator":"cbm_promote_horizon: journal checks, UPDATE h_db.horizon_metadata e COMMIT","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:error-contract-boundary","subject":"capability:atomic-promotion","relation":"exposes","object":"contract:admission-error-codes","statement":"cbm_promote_horizon maps failure modes to distinct negative return codes and corresponding JSON-RPC refusal strings.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/admission_gate.h","locator":"CBM_ADMISSION_ERR_* constants","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Transação de promoção

## OVERVIEW

Depois de validar conflitos e âncoras, `cbm_promote_horizon` consolida nós e arestas no banco base. Essa operação modifica o grafo, não arquivos-fonte.

## TRANSAÇÃO ATUAL

O caminho com base persistente anexa o banco do horizonte, verifica rollback journal `DELETE` e `synchronous >= FULL` nos bancos envolvidos e abre `BEGIN IMMEDIATE`. Revalida a geração, insere os elementos admitidos e atualiza `h_db.horizon_metadata` para `PROMOTED` **antes do commit conjunto**. Falhas verificadas geram rollback e erro.

Descrições antigas de “primeiro commit da base, depois mudança isolada de status” não representam esse caminho atual. A presença dessas verificações no código não substitui um teste de falha/crash na plataforma usada.

## CONTRATO E DEPENDÊNCIAS

O gate depende de base disponível, pool de horizontes, âncoras e estado compatível. Recusa parâmetros inválidos, drift, conflito, falha de consolidação, commit ou transição. A lista de códigos está em `src/admission/admission_gate.h`; a tradução MCP está no handler de promoção. Reindexações posteriores podem reconstruir a base: promoção não deve ser tratada como publicação de uma especificação permanente.

Os testes Python de concorrência incluem um modelo de referência e não equivalem a um ensaio de crash do C/SQLite. Consulte as [pré-condições públicas](cross_horizon_admission.md).

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
