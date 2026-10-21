---
doc_type: feature
domain: hooks
stack: [C, SQLite, MCP]
node_id: "feature:mutation-gate"
tags: [hooks, mutation-gate]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:mutation-gate","domain":"hooks","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/main.c","src/cli/hook_augment.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/union/mutation_gate.c","src/union/mutation_journal.c"],"test_files":["tests/test_cli.c","tests/windows/test_hook_augment.py"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:host-mutation-hook","type":"capability","label":"Hook de mutação","definition":"Intercepta mutações reconhecidas quando ativado.","aliases":[]},{"id":"rule:strict-opt-in","type":"rule","label":"Ativação estrita explícita","definition":"Somente valor 1 habilita enforcement.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:host-mutation-hook","relation":"constrained_by","object":"rule:strict-opt-in","statement":"cbm_hook_mutation_enforcement_enabled retorna true somente para CBM_ENFORCE_MUTATION_GATE=1; o handler sai cedo com sucesso nos demais casos.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/cli/hook_augment.c","locator":"cbm_hook_mutation_enforcement_enabled e guard do hook","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Hooks e verificação opcional de mutação

## OVERVIEW

Hooks integram eventos do cliente ao CBM. Hooks de contexto fornecem orientação/grounding; hooks de mutação podem consultar o gate antes de alterações reconhecidas. São recursos distintos do servidor MCP e não são requisito para usar o grafo.

## POLÍTICA ATUAL

A interceptação estrita de mutação está **desligada por padrão**. `cbm_hook_mutation_enforcement_enabled()` só habilita quando `CBM_ENFORCE_MUTATION_GATE` é exatamente `1`. Sem isso, os modos de mutação retornam sucesso sem ler payload, calcular hashes ou chamar o daemon. A checagem também ocorre na entrada do executável.

Quando habilitada e registrada no cliente, a engine verifica contexto do host, sessão, grounding e pares exatos de caminho/operação do escopo. Há journal e recusas estruturadas. A engine interna não foi afrouxada: mudou a ativação do interceptador no host.

## USO

Para testar o MCP, mantenha hooks fora da configuração do cliente e invoque as tools diretamente. Para experimentar interceptação estrita, primeiro prepare sessão/contexto/escopo e então configure o hook compatível com o cliente e a variável no ambiente do processo que o executa. Só definir a variável sem registrar hook não intercepta ferramentas.

`install` pode escrever integrações conforme cliente e opções; revisar sua configuração depois de usá-lo. Copiar uma skill não instala automaticamente um hook. O estado instalado em uma máquina não faz parte do contrato do repositório.

## LIMITES

O gate não é um sandbox do sistema operacional. Seu alcance depende de eventos, payloads e ferramentas reconhecidas pelo adaptador. Scripts arbitrários ou editores fora da integração não são cobertos universalmente. Não há garantia de impedir alucinação nem justificativa para tornar todo uso do agente dependente desse fluxo.

Testes relevantes ficam em `tests/test_cli.c` e na camada Windows; conferir build e seleção antes de interpretar resultados históricos.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
