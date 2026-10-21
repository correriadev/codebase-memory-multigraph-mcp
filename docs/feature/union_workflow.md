---
doc_type: feature
domain: union
stack: [C, SQLite, MCP]
node_id: "feature:union-workflow"
tags: [union, sessions, claims]
edges:
  - relation: references
    target: "adr:architecture"
    read: must
updated: 2026-10-21
---
```graph
{"node_id":"feature:union-workflow","domain":"union","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/mcp/union_handler.c"],"registration_files":["src/mcp/mcp.c"],"reference_files":[],"code_files":["src/union/union_session.c","src/union/union_sweep.c","src/union/union_gateway.c"],"test_files":["tests/test_union_session.c","tests/test_union_sweep.c","tests/test_union_gateway.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:union-session","type":"capability","label":"Sessão Union","definition":"Registra estado e encerramento de sessão.","aliases":[]},{"id":"rule:sweep-before-close","type":"rule","label":"Sweep antes do fechamento","definition":"Claims não resolvidos podem impedir o fechamento.","aliases":[]}],"claims":[{"id":"claim:implemented-boundary","subject":"capability:union-session","relation":"constrained_by","object":"rule:sweep-before-close","statement":"union_session_close chama cbm_sweep_close_session quando existem claims e retorna recusa se o sweep não termina.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/mcp/union_handler.c","locator":"handle_union_session_close: sweep_ctx e sweep_rc","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:unknown-contract-restricted","subject":"capability:union-session","relation":null,"object":null,"statement":"cbm_session_open cria uma sessão restrita quando não encontra contrato e registra CONTRACT_UNKNOWN, sem recusar a abertura.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/union/union_session.c","locator":"cbm_session_open: restricted e retorno CBM_SESSION_OK","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# Sessões, ações e claims Union

## OVERVIEW

Union registra uma sessão de trabalho, ações declaradas, claims e encerramento. Uma sessão cria um horizonte próprio; os registros são um protocolo que o cliente precisa usar, não um observador automático de tudo que o agente faz.

## TOOLS EXCLUSIVAS

| Tool | Contrato e efeito |
|---|---|
| `union_session_open` | Requer `identity`. Sem contrato reconhecido abre em modo restrito e sinaliza `CONTRACT_UNKNOWN`; não falha simplesmente por isso. Retorna `horizon_id`. |
| `union_session_get` | Requer `horizon_id`; consulta a sessão registrada. |
| `union_record_action` | Requer ID e `action_name`; registra/verifica a classe de efeito. `COMPENSABLE` usa `idempotency_key`; `IRREVERSIBLE` exige autorização aplicável. Não executa a ação. |
| `classify_activity` | Requer `intent`; usa uma classificação heurística da intenção. O rótulo não prova a natureza real da tarefa. |
| `validate_provenance` | Recebe referência temática versionada ou `declared_invention` com justificativa. Verifica a declaração segundo o protocolo; não comprova a verdade de uma frase. |
| `contest_verify` | Requer `target_ref` e evidências não vazias; registra contestação e severidade. Pode apontar `target_horizon`. Não dispara um agente revisor nem investiga o conteúdo das evidências. |
| `union_claim_capture` | Requer sessão, `claim_id`, `type` e `predicate`; captura claim tipado. O predicado é uma frase declarativa terminada em ponto. |
| `union_claim_resolve` | Requer sessão, claim e destino: `PROMOTED`, `CONVERTED_OPEN_QUESTION` ou `DISCARDED`. Owner/motivo e validação dependem do tipo/destino. Destino PROMOTED no sweep não substitui a tool de admissão no grafo. |
| `union_session_close` | Requer ID; razões `NORMAL`, `ABNORMAL` ou `EMPTY`. Claims pendentes podem impedir fechamento por sweep incompleto; retorna contagens e pode incluir trace. |

## FLUXO MÍNIMO

```json
{"name":"union_session_open","arguments":{"identity":"explorador"}}
```

Use o ID retornado em `union_session_get` e depois em `union_session_close`. Esse fluxo simples permite observar abertura restrita e encerramento; não presume contrato instalado ou permissão de ações irreversíveis. Para testar claims, capture e resolva cada um antes de fechar, seguindo os erros retornados.

Para vincular mutações ao contexto do host, `host`, `context_id`, `grounding_kind`, `intent_key` e `intent_scope` são um grupo obrigatório em conjunto. O escopo contém de 1 a 4 pares de caminho absoluto/operação; citação exige `reference`, invenção exige `rationale`. Não é necessário para uma sessão simples.

## LIMITES E DEPENDÊNCIAS

Sessões, sweep e ledgers de ações não constituem um histórico conversacional durável geral. O encerramento pode escrever trace em log, mas isso não cria uma memória semântica completa. Regras de gateway só cobrem chamadas que passam por elas. Recusas podem bloquear promoção posterior. [Hooks](mutation_gate.md) são uma integração opcional adicional.

`validator_identity` informado em claims não deve ser confundido com a verificação de credenciais de operador das tools normativas de [temas](tradition.md). Esses caminhos têm validações diferentes; não há uma única garantia de autoridade transversal.

## REFERENCES

- [Arquitetura](../adr/ARCHITECTURE.md).
- [Testes](../adr/TESTS.md).
- [Catálogo de tools](../guide/TOOLS_REFERENCE.md).
