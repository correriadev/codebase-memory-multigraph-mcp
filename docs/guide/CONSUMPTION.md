# Uso do fork no ambiente de um agente

## COMEÇAR PELO GRAFO

O executável MCP oferece as tools. Uma skill é um roteiro que o agente lê; não acrescenta capacidade ao servidor. Um hook é um adaptador de evento do cliente. É possível usar o fork diretamente por MCP sem as skills experimentais ou hooks.

1. Compile o fork ou use uma build comprovadamente originada dele; o instalador remoto do upstream não garante as extensões deste checkout.
2. Configure o cliente para iniciar **esse executável** em modo MCP. Use a configuração MCP já suportada pelo cliente; depois reinicie a conexão.
3. Confira `tools/list`: esta revisão verificou 45 nomes. Ter só os 17 herdados indica outra superfície/build ou filtragem do cliente; conferir caminho e versão antes de atribuir isso ao fork.
4. Use `list_projects`, `index_repository`, `index_status` e consultas estruturais. A [referência](TOOLS_REFERENCE.md) diferencia as famílias.

O cliente pode impor allowlists e não expor todas as tools anunciadas. Nomes e schemas do servidor são verificáveis; disponibilidade em cada cliente é uma integração separada.

## RECURSOS OPCIONAIS

| Necessidade concreta | Recurso |
|---|---|
| Encontrar implementação e dependências | Grafo herdado; dispensa governança Union. |
| Consultar hipótese junto à base | Horizonte e `active_horizons`, com [limites do overlay](../feature/multi_graph_federation.md). |
| Consultar conhecimento separado/versionado | [Catálogo de temas](../feature/tradition.md). |
| Registrar claims e encerramento | [Union](../feature/union_workflow.md). |
| Obter contexto de uma mudança e registrar avaliação | [Harness](../feature/harness_v2.md). |
| Interceptar mutações reconhecidas pelo cliente | [Hooks opcionais](../feature/mutation_gate.md). |

## CONTRATOS E LIMITES

Use os identificadores devolvidos pelo servidor, não nomes imaginados de projeto/sessão. Confira erro MCP e erro no conteúdo. Há diferenças entre schemas públicos e handlers, sobretudo em promoção, autoridade normativa e revisão de regras. Essas lacunas estão registradas nos documentos de cada família; uma skill não as corrige.

Não use `CBM_ALLOW_LEGACY_PROMOTION=1` como requisito padrão. Não distribua credenciais de operador dentro de skills. Instalação não demonstra que o agente seguirá um protocolo em toda tarefa, nem que os mecanismos reduzirão erros no seu trabalho; isso exige comparação de tarefas reais.

## REFERENCES

- [Fluxos de validação](WORKFLOWS.md), [configuração](../CONFIGURATION.md), [build](../INFRASTRUCTURE.md).
