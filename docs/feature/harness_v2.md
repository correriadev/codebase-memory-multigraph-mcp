---
doc_type: feature
domain: harness
stack: [C, SQLite, MCP]
node_id: "feature:harness-v2"
tags: [harness, governance, bundles]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:harness-v2","domain":"harness","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/harness_handlers.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/harness/harness_v2.c"],"test_files":["tests/test_harness_v2.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:harness-fast-path","type":"capability","label":"Avaliar fast path","definition":"Avalia um bundle e suas revisões.","aliases":[]},{"id":"rule:fast-path-eligibility","type":"rule","label":"Elegibilidade conjunta","definition":"Seams, efeitos, coverage, revisões e tipo de mudança precisam ser compatíveis.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:harness-fast-path","relation":"constrained_by","object":"rule:fast-path-eligibility","statement":"cbm_harness_evaluate_fast_path combina seams_ok, effects_ok, cov_ok, trad_ok e kind_ok; seams resolvidas podem existir.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/harness/harness_v2.c","locator":"cbm_harness_evaluate_fast_path: eligible","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:manifest-server-verification","subject":"capability:harness-fast-path","relation":null,"object":null,"statement":"O handler de manifest mantém server_fp_eligible falso sem bundle e reavalia quando ele é fornecido, antes de registrar.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/harness_handlers.c","locator":"handle_record_admission_manifest: server_fp_eligible e cbm_gov_record_manifest","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Bundles, fast path e regras locais

## OVERVIEW

Quatro tools produzem contexto delimitado e registros de admissão. Não executam sozinhas o desenvolvimento, os testes nem a revisão técnica. Regras locais residem no store de governança SQLite do projeto, separado do catálogo de temas.

## TOOLS EXCLUSIVAS

| Tool | Contrato e efeito |
|---|---|
| `extract_harness_bundle` | Requer `poles`, array não vazio de nomes/qualified names de símbolos. `project`, `task_id`, `change_kind` e `k_hop` (1–3, default 1) delimitam o contexto. Retorna bundle, hashes e revisões. |
| `evaluate_fast_path` | Recebe `bundle_json` ou `poles` para extração. Retorna elegibilidade e motivos, não permissão para ignorar testes ou aprovação do usuário. |
| `record_admission_manifest` | Requer `manifest_id`, `task_id`, `snapshot_id`, `bundle_hash`. Persiste decisão/evidências declaradas e valida revisões atuais. Pode registrar uma recusa; ver campos `recorded` e erro. |
| `tradition_propose` | Requer `rule_id` no schema; `action` default `propose`. Proposta precisa também de escopo, tipo e payload válidos. Cria `UNVERIFIED_CANDIDATE`; revisão exige credenciais e demais campos do handler. |

## FLUXO

Localize um símbolo real antes de extrair:

```json
{"name":"extract_harness_bundle","arguments":{"project":"PROJETO","task_id":"teste-contexto","change_kind":"pure_refactor","poles":["SIMBOLO_REAL"],"k_hop":1}}
```

Passe o bundle devolvido serializado como string em `bundle_json` de `evaluate_fast_path`. Preserve snapshot e hashes originais; se o grafo, as regras ou a cobertura mudarem, extraia novamente. Extrair/avaliar pode inicializar armazenamento de governança; registrar manifest ou regra é uma mutação persistente explícita.

## BUNDLE E ELEGIBILIDADE

O bundle tem candidatos com contexto integral/skeleton, mapa de seams, efeitos, coverage digest, regras e revisões. Os sete campos de efeito são `writes_db`, `network_io`, `fs_mutation`, `spawns_process`, `global_state_write`, `throws_or_panics` e `dynamic_dispatch`, com estados `present`, `absent` ou `unknown`.

O avaliador exige candidatos não vazios, efeitos sem unknown/razões de incerteza, seams resolvidas sem uncertainty flags, coverage completa, revisões compatíveis e tipo de mudança elegível. Um seam map não vazio pode ser aceito se resolvido. Essas classificações são análises do contexto disponível, não prova completa de ausência de efeitos.

## MANIFESTS E AUTORIDADE

O handler calcula revisões atuais, usa o relógio do servidor e verifica fast path novamente quando recebe bundle. Sem bundle, a elegibilidade verificada pelo servidor permanece falsa. `tdd_oracles_passed` é uma declaração e tem default true no handler: o registro não demonstra que testes foram executados. `advisor_invoked` e evidências similares também não invocam um revisor.

O schema de manifest não anuncia `bundle`/`bundle_json`, revisões adicionais ou hash do parecer lidos pelo handler. O schema de revisão de regra anuncia `action: review`, mas omite `new_status`, identidade/token de operador e outros campos necessários. Esses caminhos não oferecem hoje um fluxo completo para todo cliente estrito. `operator_id`/`operator_token` são verificados no host; o agente não promove regras apenas declarando ser operador.

O ledger e os códigos de erro estão no store de governança; não confundir status de manifest com promoção de horizonte nem com publicação de tema.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
