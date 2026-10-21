---
doc_type: feature
domain: tradition
stack: [C, SQLite, MCP]
node_id: "feature:tradition"
tags: [tradition, themes, authority]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:tradition","domain":"tradition","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/union_handler.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/union/union_theme_registry.c","src/union/union_binding.c","src/union/union_founding.c"],"test_files":["tests/test_union_theme_registry.c","tests/test_union_theme_drift.c","tests/test_union_binding.c","tests/test_union_founding.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:theme-governance","type":"capability","label":"Temas e autoridade","definition":"Registra temas e estabelece bindings.","aliases":[]},{"id":"rule:authenticated-normative-binding","type":"rule","label":"Binding normativo autenticado","definition":"Autodeclaração de validador não concede autoridade.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:theme-governance","relation":"constrained_by","object":"rule:authenticated-normative-binding","statement":"binding_claim em modo NORMATIVE exige verify_operator_authority e ignora validated_by fornecido como prova.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/union_handler.c","locator":"handle_binding_claim: operator_verified e BINDING_SELF_VALIDATED","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:catalog-persistence","subject":"capability:theme-governance","relation":null,"object":null,"statement":"O catálogo serializa versão, target_uri e target_generation e substitui o arquivo de armazenamento por rename.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/union/union_theme_registry.c","locator":"theme_registry_save","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Temas versionados, bindings e founding

## OVERVIEW

O catálogo registra temas com versão imutável e pode apontar para um grafo de projeto indexado. Permite descobrir e consultar conhecimento separado da base do trabalho. **Catálogo de temas e regras locais do harness são mecanismos diferentes**; estas estão em [harness](harness_v2.md).

## TOOLS EXCLUSIVAS

| Tool | Contrato e resultado |
|---|---|
| `theme_register` | Requer `theme_id`, `namespace`, `curator` e `version`. Com `target_uri: "cbm-project://PROJETO"`, publica referência ao grafo inteiro e fixa geração. Nome, descrição, aliases, tags, provenance e status são opcionais no schema. |
| `theme_lookup` | Requer ID; versão omitida procura a entrada mais recente. Consulta catálogo, não conteúdo de código. |
| `theme_search` | Requer `query`; descoberta textual de temas, com namespace/status e paginação opcionais. |
| `theme_list` | Lista entradas, com os mesmos filtros de catálogo. Limite default 20, máximo 100. |
| `theme_graph_search` | Requer ID e query; busca no grafo apontado pela versão escolhida. Limite default 50, máximo 500. |
| `theme_graph_query` | Requer ID e query; consulta read-only no grafo temático. `max_rows` default 200; paginação por offset. |
| `binding_claim` | Requer claim, tema, versão e `validated_by` no schema; modo `NORMATIVE` default ou `CONSULTED`. Normativo exige autoridade autenticada do host. |
| `founding_propose` | Requer ID sugerido, namespace, justificativa e sessão de origem; registra proposta de tema. Não publica uma norma ativa. |
| `founding_decide` | Requer ID sugerido e booleano de aceitação; decisão exige credencial válida. Aceitação retorna `DRAFT`; não equivale a publicação ACTIVE. |

## EXEMPLO DE CATÁLOGO

Depois de indexar um repositório de conhecimento e obter seu nome real:

```json
{"name":"theme_register","arguments":{"theme_id":"@local/exemplo","namespace":"local","curator":"usuario","version":"1.0.0","target_uri":"cbm-project://PROJETO_CONHECIMENTO","name":"Exemplo local"}}
```

Consulte primeiro `theme_lookup`; depois `theme_graph_search` com o mesmo ID e versão. Esse teste registra dados persistentes no catálogo: use um ID de experimento e confira erros. Não existe tool pública de exclusão de tema nesta superfície.

## PERSISTÊNCIA E LIMITES

O catálogo usa `theme_registry.json` e bindings usam `theme_bindings.json` no cache gerenciado; não são apenas memória de uma chamada. A mesma versão não deve ser reinterpretada como outro conteúdo: o registro verifica imutabilidade. Se a geração do projeto alvo mudar, a leitura temática pode recusar como desatualizada; não assume que o pin acompanha reindexações.

`target_uri` suporta um projeto inteiro registrado. Não há montagem arbitrária de subpastas como tema, servidor remoto de conhecimento nem pacote corporativo distribuído demonstrado aqui. Um rótulo `ACTIVE` não comprova qualidade do conteúdo.

## AUTORIDADE E LACUNA DO SCHEMA

`binding_claim` ignora `validated_by` como prova e verifica `operator_id`/`operator_token`. `founding_decide` também exige token (`operator_token` ou alias lido pelo handler) e rejeita autonomia declarada. O servidor verifica a identidade/token configurados pelo host, inclusive `CBM_OPERATOR_IDENTITY` e `CBM_OPERATOR_TOKEN`; não há UI de aprovação humana criada por essas tools.

Esses campos de credencial não estão anunciados nos respectivos schemas. Clientes estritos podem impedir a chamada completa. Não há exemplo universal de aprovação normativa com apenas os argumentos públicos; preencher `validated_by` ou `decided_by` com “operator” não resolve essa lacuna.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
