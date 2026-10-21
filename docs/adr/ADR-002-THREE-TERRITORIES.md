---
doc_type: adr
domain: tradition
stack: [C, SQLite, MCP]
node_id: "adr:three-territories"
tags: [tradition, architecture]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---

# Separação entre base, hipótese e tema

## CONTEXTO

Os três territórios são uma nomenclatura para base indexada, horizonte especulativo e conhecimento temático. A distinção ajuda a não apresentar uma proposta como fato do repositório.

## IMPLEMENTAÇÃO ATUAL

A base contém o que o indexador extraiu. Horizontes têm bancos próprios e podem ser usados como overlays explícitos. O catálogo de temas aponta para grafos inteiros de projetos registrados, fixando versão e geração. Regras locais do harness usam outro store e outro ciclo de revisão.

Esses mecanismos não constituem três níveis universais de verdade nem asseguram aplicação automática de toda norma a toda alteração. O fluxo efetivo depende do handler chamado e do cliente.

## EVIDÊNCIA

`src/mcp/horizon_handler.c`, `src/mcp/union_handler.c`, `src/union/union_theme_registry.c` e `src/mcp/harness_handlers.c`. O catálogo é persistido; descrições antigas de um catálogo exclusivamente em memória ficaram obsoletas.

## REFERENCES

- [Arquitetura](ARCHITECTURE.md).
- [Comportamento e limites](../feature/tradition.md).
