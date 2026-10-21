---
doc_type: feature
domain: discovery
stack: [C, SQLite, MCP]
node_id: "feature:code-discovery"
tags: [discovery, queries]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:code-discovery","domain":"discovery","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/mcp.c"],"registration_files":[],"reference_files":[],"code_files":["src/mcp/handlers.c"],"test_files":[],"knowledge":{"schema_version":1,"entities":[{"id":"capability:code-discovery","type":"capability","label":"Consulta estrutural","definition":"Consulta o grafo indexado.","aliases":[]},{"id":"rule:overlay-explicit","type":"rule","label":"Overlay explícito","definition":"A consulta base é usada quando não há horizonte ativo.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:code-discovery","relation":"constrained_by","object":"rule:overlay-explicit","statement":"Sem active_horizons, o wrapper federado delega a consulta ao handler base.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/handlers.c","locator":"cbm_mcp_handle_federated_search_graph: horizon_count == 0","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Consultar o grafo de código

## OVERVIEW

As tools herdadas localizam símbolos, relações, fontes e documentação indexada. Uma consulta responde sobre a geração indexada; não é uma execução do programa nem prova de completude do repositório.

## USO

1. Obter o nome exato com `list_projects` e verificar `index_status`.
2. Localizar símbolos com `search_graph`; seguir chamadas com `trace_path`.
3. Ler a implementação com `get_code_snippet` ou `get_file_outline`.
4. Usar `check_index_coverage` nos arquivos relevantes e ler diretamente lacunas ou fontes alteradas.
5. Usar `query_graph`, `get_graph_schema` e `get_architecture` para perguntas de relações ou arquitetura.

`search_code` serve à busca textual. `compare_graphs`, `detect_changes` e `manage_adr` têm outros contratos; consultar a referência antes de chamá-las. Parâmetros, paginação e defaults completos estão no [snapshot dos schemas](../guide/mcp-tools.json).

## LIMITES

Um resultado vazio pode refletir filtros, linguagem, geração ou cobertura. Não concluir ausência sem conferir essas condições. `active_horizons` acrescenta dados especulativos em algumas consultas; os detalhes e as limitações estão em [horizontes](multi_graph_federation.md). O uso básico não exige sessão, skills experimentais ou hooks.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
