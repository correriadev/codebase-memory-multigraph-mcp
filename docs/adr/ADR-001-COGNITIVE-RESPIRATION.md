---
doc_type: adr
domain: mutation-gate
stack: [C, SQLite, MCP]
node_id: "adr:cognitive-respiration"
tags: [mutation-gate, architecture]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---

# Hooks de mutação opcionais

## CONTEXTO

A implementação separa abertura/contexto de sessão e verificação de mutação. O nome histórico “respiração cognitiva” designava essa separação; não é uma garantia de raciocínio correto.

## IMPLEMENTAÇÃO ATUAL

Os modos de hook de mutação retornam sem bloquear por padrão. A verificação estrita exige `CBM_ENFORCE_MUTATION_GATE=1`, além de registro do hook no cliente. A engine interna continua validando contexto, grounding e escopo quando chamada. Hooks informativos e MCP são integrações distintas.

Essa política evita tornar a consulta do grafo dependente de um fluxo obrigatório de sessão. Não oferece uma barreira universal a alterações feitas por shell, editores ou ferramentas fora do hook.

## EVIDÊNCIA

`src/main.c` e `src/cli/hook_augment.c::cbm_hook_mutation_enforcement_enabled`. Esta revisão registra a implementação local; não atribui status de aprovação a propostas históricas.

## REFERENCES

- [Arquitetura](ARCHITECTURE.md).
- [Comportamento e limites](../feature/mutation_gate.md).
