---
doc_type: feature
domain: anchors
stack: [C, SQLite, MCP]
node_id: "feature:ast-anchor-isolation"
tags: [anchors, tree-sitter]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:ast-anchor-isolation","domain":"anchors","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/admission/anchor_checker.h"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/admission/anchor_checker.c"],"test_files":["tests/test_anchor_checker.c","tests/test_ast_anchor_isolation.py"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:ast-anchor-verification","type":"capability","label":"AST Anchor Verification","definition":"Two-tier anchor verification ensuring syntactic and structural declaration integrity before base graph admission.","aliases":["two-tier-verification"]},{"id":"rule:reject-zero-hash","type":"rule","label":"Reject Zero AST Hash","definition":"Anchors presenting ast_signature_hash equal to zero are invalid and must be rejected immediately.","aliases":["zero-hash-guard"]},{"id":"rule:full-file-ast-confinement","type":"rule","label":"Full File AST Confinement","definition":"Declaration verification must parse the complete file AST to reject symbols defined inside comments or string literals.","aliases":["trivia-rejection"]},{"id":"contract:anchor-checker-api","type":"contract","label":"Anchor Checker API","definition":"cbm_verify_two_tier_anchor verifies byte and AST identity, returning relocation offsets without mutating input anchors.","aliases":["cbm_verify_two_tier_anchor"]}],"claims":[{"id":"claim:fast-path-validation","subject":"capability:ast-anchor-verification","relation":"constrained_by","object":"rule:full-file-ast-confinement","statement":"cbm_verify_two_tier_anchor validates symbol declarations in the full file AST, rejecting literals and comments even if byte offsets match.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/anchor_checker.c","locator":"cbm_verify_two_tier_anchor: full file parse and declaration search","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:zero-hash-rejection","subject":"capability:ast-anchor-verification","relation":"constrained_by","object":"rule:reject-zero-hash","statement":"cbm_verify_two_tier_anchor rejects anchors with ast_signature_hash == 0 as malformed input.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/anchor_checker.c","locator":"cbm_verify_two_tier_anchor: hash == 0 guard","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:api-contract-boundary","subject":"capability:ast-anchor-verification","relation":"exposes","object":"contract:anchor-checker-api","statement":"cbm_verify_two_tier_anchor populates out_relocation when benign shifts occur, preserving const anchor immutability.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/anchor_checker.h","locator":"cbm_verify_two_tier_anchor signature and TwoTierAnchorRelocation","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Verificação de âncoras AST

## OVERVIEW

O checker verifica a identidade de uma declaração no arquivo atual e pode retornar novas coordenadas quando há deslocamento compatível. O resultado valida essa âncora; não comprova correção funcional.

## CONTRATO

`cbm_verify_two_tier_anchor(root, anchor, ok, out_relocation)` recebe caminho, símbolo, offsets, texto esperado e hash estrutural. Hash zero é entrada inválida. O checker parseia o arquivo completo e procura uma declaração, evitando aceitar apenas uma ocorrência do nome em comentário ou literal.

Se o hash estrutural ainda coincide, a declaração pode ser reconhecida em outra posição. A saída de relocação é separada da âncora de entrada const. Mudança estrutural ou ausência do símbolo recusa a verificação. O dispatcher deste checker cobre C, TypeScript e Python; o suporte amplo do indexador não implica suporte amplo de âncoras.

## USO E LIMITES

A tool de promoção usa esse checker. Um hash arbitrário ou um hash do texto bruto não substitui o hash estrutural produzido pelo algoritmo. O handler pode calcular fallback de texto, mas isso não torna o resultado equivalente a uma assinatura estrutural em todos os casos. Não há tool pública dedicada para gerar uma âncora completa pronta para toda linguagem.

Definições de testes: `tests/test_anchor_checker.c`; o arquivo Python de isolamento é uma camada adicional, não prova de execução desses casos C.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
