---
doc_type: feature
domain: recall
stack: [C, SQLite, MCP]
node_id: "feature:transitive-recall-closure"
tags: [recall, dependencies]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:transitive-recall-closure","domain":"recall","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/admission/recall_engine.h"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/admission/recall_engine.c"],"test_files":["tests/test_recall_engine.c","tests/test_transitive_recall_closure.py"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:transitive-recall","type":"capability","label":"Transitive Recall Closure","definition":"Traverses reverse causal dependency graphs to transitively contest derived symbols upon root invalidation.","aliases":["epistemic-recall"]},{"id":"rule:causal-edge-filtering","type":"rule","label":"Causal Edge Filtering","definition":"Reverse dependency traversal expands only across causal derivation edges (DEPENDS_ON, DERIVED_FROM, CALLS, IMPLEMENTS, EXTENDS), ignoring non-causal edges.","aliases":["causal-filter"]},{"id":"rule:transactional-recall-consistency","type":"rule","label":"Transactional Recall Consistency","definition":"BEGIN IMMEDIATE must execute before BFS traversal, ensuring traversal and status UPDATE operate within the exact same database snapshot.","aliases":["recall-tx"]},{"id":"contract:recall-engine-api","type":"contract","label":"Recall Engine API","definition":"cbm_trigger_recall marks root and dependents CONTESTED within an atomic transaction, returning RecallReport with total metrics.","aliases":["cbm_trigger_recall"]}],"claims":[{"id":"claim:causal-traversal-boundary","subject":"capability:transitive-recall","relation":"constrained_by","object":"rule:causal-edge-filtering","statement":"cbm_bfs_reverse_deps_acyclic filters exclusively by causal derivation edges, ignoring DOC_REF and TAGGED_WITH.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/recall_engine.c","locator":"cbm_bfs_reverse_deps_acyclic: SQL WHERE edge_type IN clause","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:transactional-consistency","subject":"capability:transitive-recall","relation":"constrained_by","object":"rule:transactional-recall-consistency","statement":"cbm_trigger_recall begins transaction before BFS traversal and commits only after all UPDATE statements succeed.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/recall_engine.c","locator":"cbm_trigger_recall: BEGIN IMMEDIATE before BFS call","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:api-contract-boundary","subject":"capability:transitive-recall","relation":"exposes","object":"contract:recall-engine-api","statement":"cbm_trigger_recall populates total_affected_count and is_committed in RecallReport, rolling back on any SQLite failure.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/recall_engine.h","locator":"RecallReport struct and CBM_RECALL_ERR_TX_FAILED","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:production-wiring-boundary","subject":"capability:transitive-recall","relation":null,"object":null,"statement":"A revisão do fonte localizou a definição e declaração de cbm_trigger_recall, sem chamadas em src; contest_verify registra contestação sem invocá-la.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/union_handler.c","locator":"handle_contest_verify: cbm_contest_submit e resposta, sem cbm_trigger_recall","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Motor interno de recall

## OVERVIEW

O motor C percorre dependências reversas e marca a raiz e dependentes como `CONTESTED` numa transação. É um componente interno; **não há uma tool MCP de recall nem chamada de produção a `cbm_trigger_recall` localizada nesta revisão**. A busca do grafo foi confirmada com busca textual em `src`.

## CONTRATO

`cbm_trigger_recall` inicia `BEGIN IMMEDIATE`, faz BFS sem limite de profundidade (`max_depth=0`), usa conjunto de visitados e atualiza os nós antes de `COMMIT`. Erros nos passos verificados provocam rollback. O relatório distingue contagem total de dependentes da lista limitada de URIs retornada; o limite de relatório não delimita sozinho a atualização transacional.

O filtro percorre `DEPENDS_ON`, `DERIVED_FROM`, `CALLS`, `IMPLEMENTS` e `EXTENDS`. Essas arestas são a definição operacional do filtro, não uma prova geral de causalidade semântica.

## LIMITE DE USO

`contest_verify` recebe referências de evidência e registra uma contestação, mas não chama esse motor. Não esperar invalidação transitiva automática ao contestar um claim. Para validar o componente isolado use a suíte C `recall_engine`; não confundir sua aprovação com integração no fluxo do agente.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
