---
doc_type: adr
domain: harness-v2
stack: [C, SQLite, MCP]
node_id: "adr:harness-perfeito-v2"
tags: [harness-v2, architecture]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---

# Bundles e governança local do harness

## CONTEXTO

O nome de arquivo preserva referências anteriores. “Harness perfeito” era o nome de uma proposta, não uma qualificação do produto atual.

## IMPLEMENTAÇÃO ATUAL

Quatro tools extraem um bundle delimitado por símbolos, avaliam fast path, registram manifests e propõem/revisam regras locais. O bundle combina candidatos e contexto estrutural, efeitos, seams, coverage e revisões. O ledger de governança usa SQLite.

O fast path requer seams resolvidas, efeitos sem incerteza, coverage adequada, ausência de bloqueios e tipo de mudança compatível. Não exige necessariamente um seam map vazio. O handler de manifest reavalia elegibilidade quando recebe bundle e compara revisões atuais; alguns campos de evidência continuam declarados pelo chamador.

O schema público não anuncia todos os campos lidos pelos handlers, inclusive credenciais de revisão e bundle do manifest. A documentação de uso registra essa lacuna; não apresenta uma aprovação integral automática como disponível.

## EVIDÊNCIA

`src/mcp/harness_handlers.c`, `src/harness/harness_v2.c::cbm_harness_evaluate_fast_path` e o registro `TOOLS[]` de `src/mcp/mcp.c`. Fonte inspecionado confirma mecanismos específicos, não a conclusão original de perfeição ou suficiência universal.

## REFERENCES

- [Arquitetura](ARCHITECTURE.md).
- [Comportamento e limites](../feature/harness_v2.md).
