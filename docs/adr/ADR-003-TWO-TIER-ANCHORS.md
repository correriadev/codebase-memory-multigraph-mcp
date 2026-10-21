---
doc_type: adr
domain: anchors
stack: [C, SQLite, MCP]
node_id: "adr:two-tier-anchors"
tags: [anchors, architecture]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---

# Âncoras e admissão no grafo

## CONTEXTO

A admissão compara âncoras com o arquivo atual antes de consolidar nós especulativos. Isso trata drift de posição e identidade estrutural; não valida que a mudança atende ao objetivo do usuário.

## IMPLEMENTAÇÃO ATUAL

O checker usa o arquivo completo e a declaração encontrada por Tree-sitter; hash zero é rejeitado. Hash estrutural permite reconhecer determinadas mudanças de posição/trivia. O suporte do checker é menor que o catálogo geral de linguagens do indexador.

Promoção exige sessão Union por padrão, base disponível, pelo menos uma âncora e ausência de bloqueios aplicáveis. A transação de admissão atualiza base e metadata do horizonte anexado antes de um único commit SQLite, sob precondições de journal verificadas.

## EVIDÊNCIA

`src/admission/anchor_checker.c::cbm_verify_two_tier_anchor`, `src/admission/admission_gate.c::cbm_promote_horizon` e `src/mcp/promote_handler.c::handle_promote_horizon`. Não existe prova de zero overhead ou de prevenção absoluta de drift fora desse caminho.

## REFERENCES

- [Arquitetura](ARCHITECTURE.md).
- [Comportamento e limites](../feature/cross_horizon_admission.md).
