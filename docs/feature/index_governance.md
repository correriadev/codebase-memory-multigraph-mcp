---
doc_type: feature
domain: indexing
stack: [C, SQLite, MCP]
node_id: "feature:index-governance"
tags: [indexing, coverage]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:index-governance","domain":"indexing","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/mcp.c"],"registration_files":[],"reference_files":[],"code_files":[],"test_files":[],"knowledge":{"schema_version":1,"entities":[{"id":"capability:index-inspection","type":"capability","label":"Inspeção do índice","definition":"Expõe estado e cobertura do índice.","aliases":[]},{"id":"rule:coverage-best-effort","type":"rule","label":"Cobertura best effort","definition":"O diagnóstico não prova completude.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:index-inspection","relation":"constrained_by","object":"rule:coverage-best-effort","statement":"check_index_coverage retorna sinal best_effort e explicita que ausência de issues não prova completude.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/mcp.c","locator":"handle_check_index_coverage: signal e caveat","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Ciclo de vida e cobertura do índice

## OVERVIEW

`index_repository` cria ou atualiza o grafo local. `index_status` e `check_index_coverage` ajudam a distinguir índice pronto, operação em curso e lacunas de análise. Estar pronto não equivale a ter todas as relações possíveis.

## USO

Passe um `repo_path` absoluto para `index_repository`. Confira o resultado, descubra o nome em `list_projects` e use esse identificador nas consultas. Reindexar pode alterar a geração e tornar inválido o pin de um tema previamente publicado.

`delete_project` remove um índice; não é uma limpeza necessária a cada teste. `detect_changes` analisa o diff e símbolos relacionados; não roda os testes por si só. A sincronização em background depende do modo de execução e do registro do projeto, não de uma promessa de atualização imediata em qualquer chamada CLI.

## CONFIGURAÇÃO

- [Configuração](../CONFIGURATION.md): cache e parâmetros de execução.
- [Exclusões](../cbmignore.md): alcance efetivo do índice.
- [Limites de recursos](../INDEX_RESOURCE_LIMITS.md): limites de descoberta.

## LIMITES

A cobertura é um diagnóstico best effort. Leia o arquivo atual quando houver geração antiga, metadata alterada ou faixas não parseadas. Não use um índice anterior como evidência de uma edição recente.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
