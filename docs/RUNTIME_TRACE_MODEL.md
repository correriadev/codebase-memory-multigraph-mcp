# Traces: superfície atual

## O QUE EXISTE

`ingest_traces` é uma das 17 tools herdadas e recebe traces para anotação/uso pelo mecanismo existente. Seu schema atual está no [catálogo capturado](guide/mcp-tools.json). O fork também pode emitir um trace de sessão ao fechar uma sessão Union, com ações, recusas e exclusões. São superfícies distintas.

## LIMITES

O antigo texto desta página projetava um sidecar versionado, publicações compostas e overlay `RUNTIME_CALL`. Era um contrato de arquitetura proposto, identificado no próprio texto como não implementado. Foi retirado como documentação de funcionalidade atual.

Não deduza que spans comprovam chamadas estáticas, que o trace da sessão registra todo o diálogo ou que um produtor de telemetria já está integrado. `CALLS` estático, evidência recebida em traces e eventos Union precisam manter sua origem explícita.

## REFERENCES

- [Tools](guide/TOOLS_REFERENCE.md), [Union](feature/union_workflow.md), [arquitetura](adr/ARCHITECTURE.md).
