---
doc_type: feature
domain: horizons
stack: [C, SQLite, MCP]
node_id: "feature:multi-graph-federation"
tags: [horizons, federation]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:multi-graph-federation","domain":"horizons","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/horizon_handler.c","src/mcp/horizon_sync_handler.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/mcp/handlers.c","src/core/horizon_pool.c"],"test_files":["tests/test_horizon_spec_parser.c","tests/test_horizon_reaper.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:horizon-overlay","type":"capability","label":"Overlay de horizonte","definition":"Acrescenta nós especulativos às respostas.","aliases":[]},{"id":"rule:overlay-query-boundary","type":"rule","label":"Limite da consulta do overlay","definition":"A seleção de nós não equivale à execução da consulta base.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:horizon-overlay","relation":"constrained_by","object":"rule:overlay-query-boundary","statement":"Os wrappers de search e query leem symbolic_nodes não contestados do horizonte por SELECT próprio, sem executar ali o texto da consulta base.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/handlers.c","locator":"cbm_mcp_handle_federated_search_graph e cbm_mcp_handle_federated_query_graph: SELECT symbolic_nodes","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Horizontes e overlays especulativos

## OVERVIEW

Um horizonte armazena nós simbólicos e arestas virtuais separados da base. Pode representar uma hipótese e ser consultado explicitamente como overlay; criá-lo não implementa arquivos nem valida sua hipótese.

## TOOLS EXCLUSIVAS

| Tool | Entrada e comportamento |
|---|---|
| `list_horizons` | `project` obrigatório; filtra status e pagina. Default `ACTIVE`, limite 50, máximo 500. |
| `create_horizon` | Cria ou popula horizonte; `horizon_id`, `project`, `nodes` e `edges` no schema. Cada nó requer `cbm_uri` e `label`; cada aresta requer origem, destino e tipo. |
| `sync_horizon_spec` | Requer `horizon_id` e `file_path`; lê Markdown do arquivo ou do `content` fornecido e extrai sua representação simbólica. Não é um parser universal de especificações. |
| `validate_scope_horizon` | Requer `horizon_id`; verifica conectividade (`strict_connectivity` default true) e opcionalmente conflitos. |
| `check_horizon_conflicts` | Requer `horizon_id`; consulta conflitos antecipáveis. Não reserva exclusividade sobre todo o repositório. |
| `promote_horizon` | Tenta consolidar no grafo base; ver [admissão](cross_horizon_admission.md) antes de usar. |

## EXEMPLO

Chamada MCP conceitual, substituindo `PROJETO` pelo nome retornado por `list_projects`:

```json
{"name":"create_horizon","arguments":{"project":"PROJETO","horizon_id":"h_experimento","nodes":[{"cbm_uri":"cbm://PROJETO/src/example.c#example","label":"Function","epistemic_status":"PROPOSED"}]}}
```

Retenha o `horizon_id` retornado. Uma consulta com `active_horizons` usa esse identificador; uma consulta sem ele continua na base. Uma sessão Union cria seu próprio horizonte: use o ID dessa sessão para fluxos que exigem sessão, em vez de presumir que qualquer horizonte tem uma.

## LIMITES IMPLEMENTADOS

Os wrappers em `src/mcp/handlers.c` consultam a base e acrescentam overlays. Em search/query, os SELECTs dos nós do overlay excluem `CONTESTED` e ordenam por URI, mas não reaplicam necessariamente o texto ou o Cypher da consulta base. Portanto, a saída não equivale a executar a mesma consulta sobre um único grafo materializado. Também não assumir que o overlay de trace constitui uma cadeia de chamadas validada.

Horizontes têm lifecycle e reaper; não são um arquivo durável de memória pessoal/corporativa. O contrato público de criação não anuncia alguns campos lidos pelo handler, como `client_pid` e `based_on_seq`. Use os schemas anunciados para a superfície suportada pelo cliente e confira erros do handler.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
