---
doc_type: adr
domain: architecture
stack: [C, SQLite, MCP]
node_id: "adr:architecture"
tags: [architecture, runtime, fork]
edges:
  - relation: references
    target: "adr:tests"
updated: 2026-10-21
---

# Arquitetura atual do CBM-MGH

## OVERVIEW

O fork é um servidor MCP nativo para consultar um grafo local de código, documentação e infraestrutura. Acrescenta horizontes especulativos, sessões Union, catálogo de temas e ferramentas de contexto/governança. A superfície verificada em 21/10/2026 tem **45 tools: 17 herdadas e 28 exclusivas**, comparando `src/mcp/mcp.c` com o ref local `upstream/main` (`3457b21b0a3d75960c07ed3ec666a9e684985ef9`) e com `tools/list` do executável instalado.

Esta revisão descreve o checkout em `8dd9f4ec7057e642d9c66d0c8d3619f2700e62a5` com alterações locais, incluindo hooks de mutação desativados por padrão. O índice usado para localizar símbolos é anterior a essas alterações; o comportamento relevante foi conferido diretamente no fonte. Implementação observada não equivale a validação integral do produto.

## FOLDER STRUCTURE

```text
src/
  mcp/          # schemas, dispatch e handlers
  pipeline/     # descoberta e indexação
  store/        # grafo persistente e governança SQLite
  core/         # URIs, nós simbólicos e pool de horizontes
  admission/    # âncoras, conflitos, promoção e recall interno
  union/        # sessões, claims, temas, efeitos e journal
  harness/      # bundles e avaliação do fast path
  cli/          # comandos, integração com clientes e hooks
  daemon/       # coordenação e ciclo de vida
tests/          # runner C, testes Python e contratos shell
graph-ui/       # interface web opcional herdada
```


## LAYERS

1. O cliente envia `tools/call` ao servidor MCP. Schemas anunciados e despacho estão em `src/mcp/mcp.c`.
2. Handlers resolvem projeto, parâmetros, autoridade e stores. As dependências são passadas em structs e ponteiros C; não há framework de injeção de dependências.
3. Indexação produz o grafo persistente. Horizontes usam bancos separados com nós e arestas especulativos.
4. Sessões e regras aplicam verificações nos caminhos que as invocam. O agente continua responsável por usar o protocolo; instalar o MCP não obriga chamadas a essas tools.

## MODULES

| Módulo | Responsabilidade e referência |
|---|---|
| Consultas | [Descoberta](../feature/code_discovery.md) e [ciclo do índice](../feature/index_governance.md). |
| Horizontes | [Overlay e tools](../feature/multi_graph_federation.md). |
| Admissão | [Pré-condições](../feature/cross_horizon_admission.md), [âncoras](../feature/ast_anchor_isolation.md) e [transação](../feature/atomic_admission_concurrency.md). |
| Union | [Sessões, ações, claims e contestações](../feature/union_workflow.md). |
| Temas | [Catálogo versionado, bindings e founding](../feature/tradition.md). |
| Harness | [Bundles, regras locais e manifests](../feature/harness_v2.md). |
| Hooks | [Interceptação opcional](../feature/mutation_gate.md). |
| Recall | [Motor C e limite de integração](../feature/transitive_recall_closure.md). |

## PATTERNS

- **Evidência consultável:** um resultado do grafo precisa ser interpretado junto à geração e à cobertura; lacunas exigem leitura do fonte.
- **Separação de estado:** base indexada, horizonte especulativo e tema publicado têm papéis distintos. Não são sinônimos de verdade, hipótese validada ou norma corporativa aprovada.
- **Admissão explícita:** promover consolida dados no grafo; não escreve uma implementação no repositório e não executa testes por si só.
- **Autoridade do host:** operações normativas verificam credenciais no servidor. Declarar `validated_by: "operator"` não concede autoridade.
- **Erros por operação:** verificar `isError`, códigos e conteúdo do resultado. Não existe envelope uniforme para todas as famílias.

## INTEGRATIONS

O runtime usa C, SQLite, Tree-sitter e bibliotecas vendorizadas; a UI opcional usa a cadeia Node/Vite. Há instalação para clientes herdada do upstream, mas skills são instruções ao agente e hooks são integrações separadas. Nenhuma das duas é requisito para consultar as tools.

**Limites atuais:** overlays não aplicam necessariamente a consulta do grafo base; há diferenças entre schemas e validações dos handlers; recall não tem chamador de produção localizado; classificações de atividade/efeitos não provam semântica; aprovação de manifests recebe parte das evidências declarada pelo chamador. Não há demonstração de eliminação de alucinações, superioridade de agentes ou prontidão corporativa.

## REFERENCES

- [Índice](../README.md), [contratos das tools](../guide/TOOLS_REFERENCE.md), [consumo](../guide/CONSUMPTION.md).
- [Build e runtime](../INFRASTRUCTURE.md), [testes](TESTS.md).
