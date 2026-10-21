# Referência das tools MCP

## SUPERFÍCIE VERIFICADA

**45 tools: 17 herdadas + 28 exclusivas do fork.** Comparação do registro `TOOLS[]` com o ref local `upstream/main` e conferência por `tools/list` em 21/10/2026. Esta comparação delimita exclusividade nesse ref, não em todas as versões possíveis do upstream. As 17 herdadas também podem ter extensões de parâmetros/comportamento.

O [snapshot completo dos schemas](mcp-tools.json) contém tipos, enums, defaults, limites, objetos aninhados e descrições das 45 tools anunciadas. É uma captura da interface real, não uma especificação idealizada. Os handlers impõem validações adicionais; diferenças relevantes estão nas páginas de cada família.

## TOOLS HERDADAS

| Função | Tools |
|---|---|
| Índice | `index_repository`, `list_projects`, `delete_project`, `index_status`, `check_index_coverage` |
| Exploração | `search_graph`, `query_graph`, `trace_path`, `search_code`, `get_code_snippet`, `get_file_outline`, `get_graph_schema`, `get_architecture` |
| Mudanças e evidências | `compare_graphs`, `detect_changes`, `manage_adr`, `ingest_traces` |

Comece por [descoberta](../feature/code_discovery.md) e [índice](../feature/index_governance.md). `search_graph`, `query_graph` e `trace_path` aceitam overlay explícito por `active_horizons`, com semântica específica do fork.

## 28 TOOLS EXCLUSIVAS

As tabelas resumem os argumentos **anunciados**; obrigatórios do handler podem ser mais restritos. `—` significa que o schema não marca argumentos obrigatórios, não que qualquer payload será aceito.


### Horizontes

[Comportamento, exemplos e limites](../feature/multi_graph_federation.md).

| Tool | Obrigatórios no schema | Opcionais no schema |
|---|---|---|
| `list_horizons` | `project` | `status`, `offset`, `limit` |
| `create_horizon` | — | `horizon_id`, `project`, `nodes`, `edges` |
| `promote_horizon` | `horizon_id` | `anchors` |
| `sync_horizon_spec` | `horizon_id`, `file_path` | `content`, `project` |
| `validate_scope_horizon` | `horizon_id` | `strict_connectivity`, `check_conflicts` |
| `check_horizon_conflicts` | `horizon_id` | — |

### Union

[Comportamento, exemplos e limites](../feature/union_workflow.md).

| Tool | Obrigatórios no schema | Opcionais no schema |
|---|---|---|
| `union_session_open` | `identity` | `contract_id`, `based_on_seq`, `client_pid`, `host`, `context_id`, `grounding_kind`, `intent_key`, `intent_scope`, `reference`, `rationale` |
| `union_session_get` | `horizon_id` | — |
| `union_session_close` | `horizon_id` | `reason` |
| `union_record_action` | `horizon_id`, `action_name` | `effect_class`, `auth_id`, `idempotency_key` |
| `classify_activity` | `intent` | — |
| `validate_provenance` | — | `theme_id`, `node_uri`, `pinned_version`, `declared_invention`, `rationale` |
| `contest_verify` | `target_ref`, `evidence` | `target_horizon`, `severity` |
| `union_claim_capture` | `horizon_id`, `claim_id`, `type`, `predicate` | `consequence`, `based_on_seq`, `exchange_seq` |
| `union_claim_resolve` | `horizon_id`, `claim_id`, `destination` | `owner_or_reason`, `validator_identity` |

### Temas

[Comportamento, exemplos e limites](../feature/tradition.md).

| Tool | Obrigatórios no schema | Opcionais no schema |
|---|---|---|
| `theme_lookup` | `theme_id` | `version` |
| `theme_search` | `query` | `namespace`, `status`, `limit`, `offset` |
| `theme_list` | — | `namespace`, `status`, `limit`, `offset` |
| `theme_graph_search` | `theme_id`, `query` | `version`, `limit`, `offset` |
| `theme_graph_query` | `theme_id`, `query` | `version`, `max_rows`, `offset` |
| `theme_register` | `theme_id`, `namespace`, `curator`, `version` | `name`, `target_uri`, `description`, `aliases`, `tags`, `founding_provenance`, `status` |
| `binding_claim` | `claim_id`, `theme_id`, `pinned_version`, `validated_by` | `mode`, `binding_scope` |
| `founding_propose` | `suggested_theme_id`, `namespace`, `rationale`, `origin_session` | `suggested_curator` |
| `founding_decide` | `suggested_theme_id`, `operator_accepted` | `decided_by`, `is_agent_autonomous` |

### Harness

[Comportamento, exemplos e limites](../feature/harness_v2.md).

| Tool | Obrigatórios no schema | Opcionais no schema |
|---|---|---|
| `extract_harness_bundle` | `poles` | `project`, `task_id`, `change_kind`, `k_hop` |
| `evaluate_fast_path` | — | `project`, `change_kind`, `bundle_json`, `poles` |
| `record_admission_manifest` | `manifest_id`, `task_id`, `snapshot_id`, `bundle_hash` | `project`, `fast_path_granted`, `fast_path_reasons`, `advisor_invoked`, `blocking_violations`, `warnings`, `tdd_oracles_passed` |
| `tradition_propose` | `rule_id` | `project`, `action`, `scope_glob`, `constraint_kind`, `payload_json`, `evidence_refs` |

## DIFERENÇAS QUE AFETAM O USO

- Promoção: schema descreve anchors opcionais; gate exige lista não vazia e símbolo, hash e conteúdo compatíveis.
- Sessão: campos de contexto/grounding/escopo são opcionais isoladamente no schema, mas formam grupo exigido em conjunto quando usados.
- Temas: credenciais lidas por binding/founding não são anunciadas. Autodeclarar operador não autentica.
- Harness: revisão de regra e manifest leem campos omitidos pelo schema, inclusive bundle para reavaliação do fast path.
- Erros: verificar tanto o envelope MCP como o conteúdo da tool; famílias têm formatos diferentes.

Não há tool pública de recall, de registro geral de contratos de skill, de exclusão de temas ou de aprovação humana interativa nesta lista. Isso descreve a superfície registrada, não a ausência de funções internas correspondentes.

## REFERENCES

- [Consumo](CONSUMPTION.md), [validação prática](WORKFLOWS.md), [arquitetura](../adr/ARCHITECTURE.md).
