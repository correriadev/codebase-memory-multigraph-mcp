# Tactical Design — codebase-memory-multigraph-mcp

**Domínio:** `harness_perfect_v2`  
**Projeto:** `codebase-memory-multigraph-mcp`  
**Stack:** C11, Tree-sitter, SQLite WAL, mimalloc/slab_alloc, yyjson  
**Data:** 2026-10-09

## Refinement Questions and Answers

O pedido fornece escopo e invariantes. Não foram identificadas perguntas de negócio impeditivas. As entradas abaixo são refinamentos técnicos propostos pelo modelo, ainda distinguíveis de regras humanas aprovadas; não representam uma entrevista realizada com o usuário. Todas se aplicam ao projeto `codebase-memory-multigraph-mcp`.

| ID | Categoria | Questão resolvida | Recomendação | Resposta de design | Fonte da resposta / evidência |
|---|---|---|---|---|---|
| Q01 | data | Onde preservar governança entre swaps? | Escolher a opção de banco separado do ADR. | `governance.db` dedicado e gerenciado, WAL/FULL. | model; ADR-004 §2.1, inspeção de pipeline em 002 |
| Q02 | concurrency | Como impedir swap após validar snapshot? | Compartilhar fence entre publicadores e registradores. | Fence por projeto entre processos + transação curta `BEGIN IMMEDIATE`. | model; ADR-004 §2.1, D-03 de 002 |
| Q03 | integrity | Como representar prazo vencido sem escrita em consulta? | Incorporar estado efetivo na revisão normativa. | `rules_revision` derivada da revisão persistida e dos estados efetivos no instante capturado. | model; ADR-004 cadência, leitura passiva da arquitetura |
| Q04 | authorization | Como distinguir humano de agente? | Usar canal confiável do host, não booleano JSON. | Porta de operador externa às quatro ferramentas; aprovação/revisão autenticadas. | model; ADR-004 e ADR-002 |
| Q05 | acceptance | Qual baseline mede 80%? | Mesmo escopo e mesmos metadados; dependências integrais na baseline. | Comparar bytes UTF-8 do JSON completo e publicar redução da projeção de código em paralelo. | model; ADR-004 §4.1 |

## Section 1 — Main Structure

### 1.1 Organização modular C11

Reutilizar as camadas MCP, admission, pipeline, core e store. Não introduzir classes, herança ou ORM. Os caminhos `src/harness/*` e `src/store/governance_store.*` abaixo são módulos **propostos**. O servidor MCP coordena casos de uso; políticas puras recebem structs imutáveis; adaptadores possuem recursos de SQLite, parser e JSON.

| Elemento | Contexto / tipo | Identidade e invariantes | Local proposto |
|---|---|---|---|
| Bundle de Evidência | Evidência Bifocal / raiz de agregado imutável | `bundle_hash`; referências fechadas ao snapshot; alvos e cápsulas disjuntos; todas as lacunas explícitas. | `src/harness/evidence_bundle.*` |
| Alvo de Mutação | Evidência Bifocal / entidade interna | Identidade qualificada + caminho + extensão; bytes completos da extensão declarada. | Bundle |
| Cápsula de Dependência | Evidência Bifocal / entidade interna | Identidade qualificada; assinatura AST, documentação declarada ou null; todos os efeitos obrigatórios. | `src/harness/dependency_capsule.*` |
| Mapa de Costuras | Evidência Bifocal / coleção de entidades | `seam_id` estável; endpoints existentes e proveniência válida; hipóteses não-sintáticas. | `src/harness/seam_map.*` |
| Regra de Tradição | Tradição Normativa / raiz de agregado | `rule_id`; ciclo humano; `NORMATIVE` ou `CONSULTED`; prazo positivo. | `src/harness/tradition_rule.*` |
| Manifesto de Admissão | Admissão Normativa / raiz de agregado append-only | `manifest_id`; hashes vinculados; zero bloqueios; snapshot atual na linearização do commit. | `src/harness/admission_manifest.*` |
| Avaliação de Fast-Path | Admissão Normativa / resultado imutável | `eligible` é conjunção das cinco condições e validade da evidência/mudança; motivos sem duplicação. | `src/harness/fast_path.*` |
| Snapshot de Evidência | Topologia e Cobertura / VO composto | Cinco componentes canônicos do ADR-004; leitura coerente e geração publicada. | `src/harness/snapshot.*` |

Exemplos ilustrativos de forma, não implementação:

```c
typedef struct cbm_harness_bundle cbm_harness_bundle_t;
typedef struct cbm_tradition_rule cbm_tradition_rule_t;
typedef struct cbm_admission_manifest cbm_admission_manifest_t;
/* Raízes opacas; alterações apenas pelos construtores/casos de uso. */
```

```c
typedef enum { CBM_EFFECT_PRESENT, CBM_EFFECT_ABSENT, CBM_EFFECT_UNKNOWN } cbm_effect_state_t;
typedef enum { CBM_RULE_UNVERIFIED_CANDIDATE, CBM_RULE_APPROVED, CBM_RULE_REVIEW_PENDING } cbm_rule_status_t;
typedef enum { CBM_ENFORCEMENT_NORMATIVE, CBM_ENFORCEMENT_CONSULTED } cbm_enforcement_t;
/* Ordinais internos não são valores de protocolo. */
```

### 1.2 Contratos de memória e recursos

| Recurso | Dono | Regra de validade/liberação |
|---|---|---|
| JSON de entrada `yyjson_doc` | Adapter MCP da requisição | Strings emprestadas até `yyjson_doc_free`; entidades persistentes copiam os bytes. |
| Bundle e buffers variáveis | Arena/allocador da requisição com mimalloc | Vetores e strings pertencem ao bundle; liberação única pelo destrutor; nenhum ponteiro escapa após destruir a arena. |
| Nós temporários de tamanho fixo | `slab_alloc` da requisição | Devolver ao mesmo slab; não usar slab para bytes variáveis nem guardar ponteiros em cache persistente. |
| AST Tree-sitter | Extrator local | Tree/parser destruídos em todos os caminhos; copiar assinatura, coordenadas e documentação antes de destruir a árvore. |
| `yyjson_mut_doc` de saída | Serializer | Usar allocator explicitamente compatível com mimalloc; `yyjson_mut_doc_free` inclusive em OOM/cancelamento. |
| Buffer JSON serializado | Adapter MCP | Alocar com allocator de saída declarado, liberar com o mesmo par; jamais assumir que `free` e `mi_free` são intercambiáveis. |
| Statement SQLite | Repository / lease de conexão | Preparar em conexão gerenciada; reset/clear bindings ao reutilizar; finalize antes de liberar/destruir a conexão. |
| Handle SQLite e fence | Pool/gerenciador por projeto | Lease pinado enquanto houver transação; nenhuma evicção de handle ativo; exclusão e ordem de locks explícitas. |

`size_t` representa comprimentos; offsets de arquivo são inteiros não negativos verificados antes de conversão. Checar overflow em soma/multiplicação, alocação e conversão de limites JSON. Fonte é vetor de bytes com comprimento explícito, nunca `strlen` como medida de arquivo. Para fonte UTF-8 válida, JSON mantém a string exata após decoding; para bytes não UTF-8, usar `source_encoding=base64` e hash dos bytes originais. Não normalizar CRLF, whitespace ou comentários dos alvos.

Cancelamento/OOM faz unwind de AST, statements, documentos, buffers, leases e fences. Não devolver bundle truncado como sucesso. O servidor não encerra requisição com ponteiros dependentes do store que `cbm_mcp_handle_tool` libera.

## Section 2 — Value Objects / Types / Interfaces

### 2.1 Value Objects e validação

| VO | Representação C11 proposta | Regras |
|---|---|---|
| Identidade de Projeto/Símbolo | String copiada com tamanho + referência qualificada | Não vazio; projeto resolvido pelo servidor; caminhos relativos canônicos dentro do root; comparação sem ambiguidade. |
| Digest SHA-256 | 32 bytes; wire hexadecimal minúsculo de 64 caracteres | Validar comprimento/alphabet; não usar FNV de visited set como hash de integridade. |
| Extensão de Fonte | Caminho + `uint64_t` byte_start/byte_len + source_hash | Intervalo dentro do arquivo; captura exata e verificável; arquivo-alvo usa extensão completa. |
| Profundidade k-hop | `uint32_t` | Inteiro entre 0 e 8 nesta versão; padrão 1; limite é política proposta e publicada no schema MCP. |
| Impressão de Efeitos | Vetor fixo de quatro efeitos estruturados | Campos obrigatórios: `writes_db`, `acquires_lock`, `transaction_scope`, `triggers_events`; estado inválido ou campo faltante é erro. |
| Efeito Estruturado | Enum + reason + evidence_ref opcional + details opcional | Reason não vazio; `present` exige evidência; `absent` exige análise fechada documentada; `unknown` explica limitação. |
| Costura Tipada | ID + tipo + endpoints + chave + proveniência + validation_status | Tipo inicial `async_message_bridge` ou `webhook_dispatch`; status v2 exclusivamente `unverified_hypothesis`. |
| Cadência de Revisão | `int64_t interval_seconds`, `review_due_at` | Intervalo > 0; unix seconds UTC; checar overflow; padrão 2592000 segundos. |
| Descritor de Mudança | Targets + patches/bytes propostos + `change_hash` | Scope compatível com bundle; classificação crítica derivada pelo servidor; payload sem mudança não ganha Fast-Path. |
| Resultado de Domínio | Código enum + mensagem + detalhes tipados | Separar recusa de negócio, falha transitória e erro de argumentos; nunca booleano permissivo em falha. |

```c
typedef struct { unsigned char bytes[32]; } cbm_sha256_t;
typedef struct { cbm_effect_state_t state; const char *reason, *evidence_ref, *details; } cbm_effect_t;
typedef struct { int64_t interval_seconds, review_due_at; } cbm_review_cadence_t;
/* As strings acima pertencem à arena da instância; VOs são imutáveis. */
```

Parsing de cadência é estrito: a ferramenta recebe `review_interval_seconds` como inteiro JSON positivo. A porta de operador pode aceitar representação decimal, mas rejeita sinal negativo, espaços, fração, sufixos como `30d`, zero e overflow. Não implementar um interpretador livre de duração. `reviewed_at=0` é sentinela explícita de candidata ainda não revisada; aprovação usa relógio do servidor e define `review_due_at=reviewed_at+interval`.

### 2.2 Contrato completo do Bundle de Evidência v2

| Campo JSON | Tipo / cardinalidade | Semântica |
|---|---|---|
| `schema_version` | String constante `2` | Compatibilidade explícita; outras versões são recusadas. |
| `project_id`, `graph_generation` | Strings não vazias | Identidade resolvida e geração imutável publicada. |
| `coverage_revision`, `rules_revision`, `extractor_version` | Strings canônicas não vazias | Componentes exatos do snapshot. |
| `snapshot_id` | SHA-256 hexadecimal | Hash da tupla canônica abaixo. |
| `scope` | Objeto | Targets únicos ordenados, `k_hop_depth`, domínios únicos ordenados e política de relações aplicada. |
| `target_mutation_nodes` | Array não vazio | `node_id`, `qualified_name`, `file_path`, extensão, `source`, `source_encoding`, `source_hash` e efeitos conservadores dos alvos. |
| `dependency_capsules` | Array, possivelmente vazio | Identidade, extensão de proveniência, `ast_signature`, `signature_hash`, `declared_doc`, `effect_fingerprint` e refs resolvidas/não resolvidas. |
| `seam_map` | Objeto com `seams` array obrigatório | Hipóteses tipadas; nunca null nem ausência para indicar vazio. |
| `coverage_digest` | Objeto | `status`, paths/ranges/reasons das lacunas, `scope_complete` e indicação de paginação/truncamento. |
| `tradition_rules` | Array | Regras aplicáveis aprovadas ou pendentes, força, estado efetivo, revisão e proveniência; candidatas excluídas da norma aplicável. |
| `unresolved_dependencies` | Array | Referência, origem e motivo; vazio explícito quando não há lacunas conhecidas. |
| `bundle_hash` | SHA-256 hexadecimal | Hash do bundle canônico sem o próprio campo `bundle_hash`. |

Todos os membros do contrato são definidos, inclusive arrays vazios e nulls permitidos. Assinatura AST inclui declaração e tipos públicos observáveis, não corpo integral; documentação é somente a declarada. Falha no parsing de uma dependência resulta em cápsula com assinatura indisponível explicitamente marcada, lacuna no coverage e efeitos `unknown`. Falha de captura integral de alvo recusa extração (`TARGET_SOURCE_UNAVAILABLE` ou `SOURCE_DRIFT`). Não gerar docstrings por inferência.

Travessia de dependências usa conjunto de visitados, identidade estável, relações selecionadas registradas no scope e profundidade delimitada. k=0 não remove a análise de efeitos dos próprios alvos. Iterar páginas até completar a fronteira; se limites de recurso interromperem a leitura, marcar `scope_complete=false`, coverage `partial` e impedir Fast-Path. Ordenar alvos e cápsulas por identidade qualificada e caminho; se um alvo reaparecer na fronteira, mantê-lo apenas integral.

### 2.3 Canonicalização e identidade de snapshot

Aplicar canonicalização v2 definida pelo serializer: UTF-8, objetos com chaves em ordem lexicográfica, arrays de conjuntos ordenados por identidade, sem whitespace, inteiros decimais, null explícito e escaping JSON determinístico. Preservar conteúdo/ordem de arrays semanticamente sequenciais. Evitar números flutuantes no bundle; score do manifesto é finito e usa uma representação decimal determinística na identidade idempotente. Não usar a ordem de inserção yyjson como substituto desse contrato.

`snapshot_id = SHA256(canonical(project_id, graph_generation, coverage_revision, rules_revision, extractor_version))`, exatamente como no ADR-004. A tupla é um objeto com as cinco chaves, não concatenação sem delimitadores. `graph_generation` é identificador não reutilizável; `coverage_revision` avança em toda alteração de cobertura/conteúdo publicado. `rules_revision` é o SHA-256 canônico de revisão persistida do catálogo local mais pares ordenados `(rule_id, status_efetivo)` das regras aprovadas/pendentes. O estado efetivo usa um único `now` do servidor capturado na leitura; muda ao cruzar `review_due_at`, mesmo sem escrita. Propostas incrementam revisão persistida conservadoramente.

Relógio UTC do servidor é confiável e injetável nos testes. Recuo detectado do relógio torna a cadência incerta e recusa admissão até recuperar referência confiável, em vez de rejuvenecer a norma. `bundle_hash` inclui scope e toda a evidência; timestamp diagnóstico volátil fica fora do bundle selado para não impedir determinismo. Um hash não é assinatura de autoridade nem prova da autenticidade do veredito JEV.

### 2.4 Derivação tri-state e integridade do Mapa de Costuras

| Situação de análise | Estado | Motivo exemplar |
|---|---|---|
| Chamada de escrita SQL reconhecida | `present` em `writes_db` | `sql_exec_call` com referência à chamada |
| Lock reconhecido | `present` em `acquires_lock` | `sync_primitive_call` |
| BEGIN/COMMIT ou requisito transacional conhecido | `present` em `transaction_scope` | `transaction_boundary`, details `required` quando aplicável |
| Emissão de evento reconhecida | `present` em `triggers_events` | `event_publish_call` |
| Corpo e cadeia fechados, suportados e totalmente examinados, sem efeito da categoria | `absent` | `closed_scope_no_effect`; ausência AST local isolada não basta |
| Despacho dinâmico, função externa desconhecida, macro não expandida, parsing/lacuna/truncamento | `unknown` | `dynamic_delegate_dispatch`, `unsupported_macro`, `coverage_gap` etc. |

Ao combinar evidências de uma categoria: `present` domina; se não houver `present`, `unknown` domina `absent`; `absent` exige que todas as contribuições sejam fechadas. Ciclos são tratados por ponto fixo conservador; não terminar um ciclo atribuindo pureza por default. Efeito desconhecido em alvo também bloqueia Fast-Path.

Costura mínima: `seam_id`, `seam_type`, `source_ref`, `destination_ref`, `correlation_key`, `projection_rule_id`, `projection_version`, `provenance[]`, `validation_status`. Proveniência identifica projeto, caminho, bytes/linhas, hash do conteúdo e sinal observado. A chave Kafka inclui cluster/namespace/tópico; destino Celery exige evidência explícita de bridge/roteamento compatível, não mera igualdade de nomes. Ambos os endpoints devem pertencer ao snapshot/fronteira declarada; se a projeção encontra fronteira incompleta, registrar lacuna/dependência não resolvida. Duplicatas idênticas são deduplicadas por chave estável; conflitos de endpoints/proveniência são recusados (`SEAM_INTEGRITY_ERROR`). Tópicos desconectados não produzem costura. Não inserir essas hipóteses como `CALLS` sintáticos no Base Graph.

## Section 3 — Domain Services / Use Cases / Actions

| Serviço/caso de uso | Responsabilidade e sequência | Invariantes |
|---|---|---|
| Extrair Bundle de Evidência | Fixar geração; resolver alvos; percorrer fronteira; apurar cobertura; derivar cápsulas/efeitos/seams; consultar normas; canonicalizar. | Leitura coerente, sem lock SQLite de escrita; fonte integral e lacunas explícitas. |
| Avaliar Fast-Path | Validar versão, hashes e vínculo à evidência do servidor; derivar classificação da mudança; avaliar todas as condições. | Nunca confiar em coverage/efeitos/regras autodeclarados pelo cliente. |
| Registrar Manifesto de Admissão | Validar veredito e hashes fora do writer lock; sob fence + `BEGIN IMMEDIATE`, revalidar snapshot e inserir. | Sem bloqueios; idempotência; sucesso somente após commit FULL. |
| Propor Regra de Tradição | Validar domínio/texto/cadência; construir candidata; persistir transacionalmente. | Status fixo; `reviewed_at=0`; nenhuma autopromoção. |
| Aprovar/Revisar Regra de Tradição | Receber capacidade do operador confiável; validar transição; atualizar revisão/prazo e auditoria. | Exclusivamente humano; não é quinta ferramenta MCP. |
| Publicar Snapshot Topológico | Coordenar swap e identidade publicada, com recuperação fail-closed. | Nenhuma publicação contorna a fence; governança não faz parte do arquivo substituído. |

```c
int cbm_harness_extract(cbm_harness_ctx_t *ctx, const cbm_extract_request_t *req, cbm_harness_bundle_t **out);
int cbm_harness_evaluate(cbm_harness_ctx_t *ctx, const cbm_evaluate_request_t *req, cbm_fast_path_result_t **out);
int cbm_harness_record(cbm_harness_ctx_t *ctx, const cbm_record_request_t *req, cbm_manifest_receipt_t **out);
int cbm_harness_propose(cbm_harness_ctx_t *ctx, const cbm_propose_request_t *req, cbm_tradition_rule_t **out);
```

Tipos opacos declarados no header do módulo; código de retorno zero para sucesso e enum de erro para falha, `*out=NULL` em erro. As saídas pertencem ao chamador e têm destrutores explícitos; argumentos emprestados não são retidos.

### 3.1 Máquina de estados da Tradição Normativa

| Estado atual | Ação | Estado resultante | Autoridade / efeito |
|---|---|---|---|
| Inexistente | Propor | `UNVERIFIED_CANDIDATE` | Agente pode propor; não participa como norma. |
| `UNVERIFIED_CANDIDATE` | Aprovar | `APPROVED` | Operador humano autenticado; definir reviewed_at, prazo, reviewer e nova revisão. |
| `APPROVED` | Consultar com `now < review_due_at` | `APPROVED` | Leitura; sem escrita nem enfraquecimento. |
| `APPROVED` | Consultar com `now >= review_due_at` | `REVIEW_PENDING` efetivo | Projeção temporal; mantém enforcement. Materialização opcional fora da consulta. |
| `REVIEW_PENDING` | Revisar e aprovar | `APPROVED` | Operador humano; nova evidência, prazo e revisão. |
| Qualquer | Promover via proposta MCP ou capacidade de agente | Sem mudança | `HUMAN_APPROVAL_REQUIRED`; tentativa não altera norma. |

Não inventar estado `EXPIRED` nem exclusão automática. Edição de texto já aprovado exige revisão humana e audit trail; não permitir overwrite normativo por `tradition_propose`. `snapshot_hash` identifica a evidência/cânone usado na revisão da regra; não significa que a norma deixa de valer quando o grafo muda.

### 3.2 Matriz de Fast-Path e erros

Avaliar a evidência autorizada pelo servidor, por recomposição determinística a partir de scope/snapshot ou cache imutável com hash validado. Cache é otimização: ausência não flexibiliza validação. Veredito/score não participa dessas cinco condições.

| Condição impeditiva | `reason_code` | `blocking_details` mínimo |
|---|---|---|
| Coverage diferente de `clean`, scope incompleto ou lacuna | `COVERAGE_NOT_CLEAN` | paths, ranges, status e motivo |
| Algum efeito `unknown` | `UNKNOWN_EFFECT` | node_ref, categoria, reason, evidence_ref |
| Dependência não resolvida | `UNRESOLVED_DEPENDENCY` | origem, referência, motivo |
| Mudança em macros, autenticação, transações ou schemas | `CRITICAL_CHANGE` | path, intervalo, categoria e evidência do diff |
| Alguma costura | `SEAM_PRESENT` | seam_id, tipo e endpoints |

Não parar no primeiro impedimento: retornar todos, únicos e em ordem lexicográfica, com detalhes estáveis. Resultado elegível tem arrays vazios. `INVALID_ARGUMENT`, `UNSUPPORTED_VERSION`, `EVIDENCE_HASH_MISMATCH`, `SEAM_INTEGRITY_ERROR`, `SOURCE_DRIFT`, `STALE_SNAPSHOT`, `PUBLICATION_IN_PROGRESS`, `CHANGE_SCOPE_MISMATCH`, `CHANGE_EVIDENCE_REQUIRED` e `CLOCK_UNCERTAIN` são recusas/falhas de validação que nunca retornam elegibilidade verdadeira. Descritor de mudança ausente ou não interpretável falha fechado; classificação de área crítica não é booleano fornecido pelo chamador.

### 3.3 Contratos e assinaturas das quatro ferramentas MCP

As quatro ferramentas são registradas em `tools/list` com inputSchema estrito (`additionalProperties=false` nos objetos de entrada), tipos/limites e documentação de erros. Campos de status e autoridade humana não são entradas da proposta. Parâmetros e saída UTF-8 usam o envelope MCP existente; falhas de ferramenta têm `isError=true` e corpo de erro estruturado, sem simular sucesso via texto livre.

```c
char *mcp_cbm_extract_harness_bundle(cbm_mcp_server_t *srv, const char *args_json);
char *mcp_cbm_evaluate_fast_path(cbm_mcp_server_t *srv, const char *args_json);
char *mcp_cbm_record_admission_manifest(cbm_mcp_server_t *srv, const char *args_json);
char *mcp_cbm_tradition_propose(cbm_mcp_server_t *srv, const char *args_json);
```

As assinaturas são propostas de adapters compatíveis com a fronteira existente `cbm_mcp_handle_tool`; a integração deve preservar ownership do resultado MCP já adotado pelo servidor.

| Ferramenta | Entrada | Saída / erros materialmente relevantes |
|---|---|---|
| `mcp_cbm_extract_harness_bundle` | `project` obrigatório; `mutation_targets` não vazio de `{qualified_name,file_path}` ou arquivo com identidade explícita; `k_hop_depth` default 1 (0..8); `domains` array obrigatório. | Bundle v2 completo, `bundle_hash`, `snapshot_id`; alvo ambíguo/inexistente, fonte divergente, OOM e publicação em recuperação recusam sucesso. |
| `mcp_cbm_evaluate_fast_path` | `project`, bundle v2 ou referência selada recuperável, e `change` com scope e bytes/patches canônicos propostos. | `{eligible,reason_codes,blocking_details,snapshot_id,bundle_hash,change_hash}`; recompõe evidência, verifica scope/hash e classifica criticidade. |
| `mcp_cbm_record_admission_manifest` | `project`, `manifest_id`, `bundle_hash`, `snapshot_id`, `change_hash` opcional, referência/bundle verificável, `jev_overall_score`, `blocking_violations`, `warnings`, `fast_path_granted`; descritor de mudança obrigatório se `fast_path_granted=true`. | `{manifest_id,recorded:true,idempotent,snapshot_id,admitted_at}` após commit; `BLOCKING_VIOLATIONS`, `STALE_SNAPSHOT`, `MANIFEST_CONFLICT`, `FAST_PATH_NOT_ELIGIBLE` ou erro de persistência sem linha nova. |
| `mcp_cbm_tradition_propose` | `project`, `rule_id`, `domain`, `norm_text`, `enforcement_level`, `snapshot_hash`, `review_interval_seconds` opcional default 2592000. | Regra persistida com `UNVERIFIED_CANDIDATE`; ID existente conflitante → `RULE_CONFLICT`; status/promoção/autodeclaração humana na entrada → argumento inválido. |

`project` não é um caminho arbitrário de banco: é resolvido no registro do servidor. `domains=[]` significa seleção por domínios efetivos dos alvos mais regras globais aplicáveis, não ignorar todas as normas. Um filtro não pode suprimir normas obrigatórias aplicáveis. `rule_id` e `manifest_id` são identificadores opacos únicos não vazios; conteúdo de proposta igual para mesmo ID retorna candidata idempotente, sem atualizar uma regra posteriormente aprovada.

Score JEV é número finito; o ADR não fixa escala nem threshold, portanto não impor um limiar arbitrário. Arrays de violações/avisos têm itens `{code,message,evidence_refs}` validados. `change_hash=null` admite apenas atestação vinculada ao bundle, sem concessão Fast-Path nem autorização de uma mudança concreta. `fast_path_granted=true` só é persistido se reavaliação local confirmar elegibilidade e hash de mudança. O canal JEV usa a identidade confiável do host/integrador conforme os gates existentes; um hash ou campo no JSON não autentica o JEV.

Limites propostos publicados no schema/configuração: 64 alvos, 64 domínios, 16 KiB de texto normativo e 1000 itens por array de veredito. Os limites de bytes do bundle/fronteira são configuráveis, explícitos na resposta e nunca truncam alvos. Strings vazias onde identidade é obrigatória, inteiros fora de faixa, NaN/Infinity, chaves duplicadas e JSON malformado são recusados antes de efeito durável. Consultas seguem o caminho passivo existente; proposal/record passam pelos controles de mutação de governança do host, sem conceder por isso acesso à escrita do repositório.

## Section 4 — Events / Messages / Async Flows

| Evento | Disparo | Payload mínimo | Consumidores / publicação |
|---|---|---|---|
| Bundle de Evidência Selado | Extração concluída | project_id, snapshot_id, bundle_hash | Advisor/JEV; resposta síncrona |
| Fast-Path Considerado Elegível | Todas as condições satisfeitas | bundle_hash, change_hash, snapshot_id | Orquestrador; resposta síncrona |
| Fast-Path Bloqueado | Algum impedimento | bundle_hash, reason_codes, blocking_details | Advisor/JEV; resposta síncrona |
| Regra de Tradição Proposta | Commit de candidata | rule_id, status, revisão persistida | Operador; resposta após commit |
| Regra de Tradição Aprovada / Revisada | Commit humano | rule_id, reviewer_id, revisão, reviewed_at | Consulta normativa e audit trail |
| Revisão de Regra Tornada Pendente | Consulta cruza prazo | rule_id, review_due_at, status efetivo | Operador/JEV; projeção, sem broker/escrita automática |
| Admissão Recusada por Snapshot Obsoleto | Revalidação divergente | expected_snapshot_id, current_snapshot_id, code | JEV; rollback, sem manifesto |
| Manifesto de Admissão Registrado | Commit FULL | manifest_id, snapshot_id, bundle_hash, admitted_at | Auditor/gate; ledger como fonte de verdade |
| Geração Topológica Publicada | Swap e metadados estabilizados | project_id, graph_generation, coverage_revision | Extrator/admissão |

Outros eventos de 001, como captura de alvos e julgamento JEV, permanecem etapas internas ou responsabilidade externa. Não adicionar transporte Kafka/Celery para entregar estes eventos. Falha após commit e antes da resposta é resolvida por retry idempotente; não emitir sucesso antes do commit.

## Section 5 — Persistence / Repository / Data Access Interfaces

### 5.1 Banco durável e DDL

Banco por projeto, em localização gerenciada fora do caminho substituído do grafo: `governance.db`. Pool dedicado gerencia conexão, lease e fechamento; estender abstrações gerenciadas existentes com perfil de governança, sem abrir handles avulsos nos handlers. Não reutilizar o perfil `NORMAL` de horizonte para este ledger. Validar na inicialização e em cada aquisição/reabertura `journal_mode=WAL`, `synchronous=FULL`, schema version e integridade da configuração; falha → `GOVERNANCE_UNAVAILABLE`.

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = FULL;
PRAGMA foreign_keys = ON;
PRAGMA busy_timeout = 2000;
```

Timeout de 2000 ms é política proposta configurável. `SQLITE_BUSY`/`SQLITE_LOCKED` torna-se `GOVERNANCE_BUSY`, sem fallback para NORMAL. FULL dá garantias do SQLite/FS/VFS suportado; testes de crash verificam commit/recuperação, não alegam provar comportamento de hardware defeituoso.

DDL de `tradition_rules` abaixo conserva as colunas do ADR e acrescenta validações compatíveis. Os snippets SQL têm até quatro linhas; esquemas completos são compactados, não corpos de implementação.

```sql
CREATE TABLE tradition_rules (rule_id TEXT PRIMARY KEY NOT NULL, domain TEXT NOT NULL, norm_text TEXT NOT NULL, enforcement_level TEXT NOT NULL CHECK(enforcement_level IN ('NORMATIVE','CONSULTED')),
status TEXT NOT NULL CHECK(status IN ('UNVERIFIED_CANDIDATE','APPROVED','REVIEW_PENDING')), snapshot_hash TEXT NOT NULL, review_interval_seconds INTEGER NOT NULL DEFAULT 2592000 CHECK(review_interval_seconds > 0),
review_due_at INTEGER NOT NULL, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, reviewed_at INTEGER NOT NULL,
CHECK(length(rule_id)>0 AND length(domain)>0 AND length(norm_text)>0), CHECK(reviewed_at>=0 AND review_due_at>=0));
```

```sql
CREATE INDEX idx_tradition_domain ON tradition_rules(domain);
CREATE INDEX idx_tradition_status ON tradition_rules(status);
CREATE INDEX idx_tradition_due ON tradition_rules(review_due_at);
/* Cadência vencida é também calculada nas consultas, não depende deste índice. */
```

```sql
CREATE TABLE harness_admission_manifests (manifest_id TEXT PRIMARY KEY NOT NULL, bundle_hash TEXT NOT NULL, change_hash TEXT, snapshot_id TEXT NOT NULL, jev_overall_score REAL NOT NULL,
blocking_violations TEXT NOT NULL CHECK(json_valid(blocking_violations) AND json_type(blocking_violations)='array' AND json_array_length(blocking_violations)=0),
warnings TEXT NOT NULL CHECK(json_valid(warnings) AND json_type(warnings)='array'), fast_path_granted INTEGER NOT NULL CHECK(fast_path_granted IN (0,1)), admitted_at INTEGER NOT NULL,
CHECK(length(manifest_id)>0 AND length(bundle_hash)=64 AND length(snapshot_id)=64), CHECK(change_hash IS NULL OR length(change_hash)=64));
```

```sql
CREATE INDEX idx_manifest_snapshot ON harness_admission_manifests(snapshot_id);
CREATE TRIGGER manifest_no_update BEFORE UPDATE ON harness_admission_manifests BEGIN SELECT RAISE(ABORT,'IMMUTABLE_MANIFEST'); END;
CREATE TRIGGER manifest_no_delete BEFORE DELETE ON harness_admission_manifests BEGIN SELECT RAISE(ABORT,'IMMUTABLE_MANIFEST'); END;
/* Ledger append-only; retries não usam INSERT OR REPLACE. */
```

```sql
CREATE TRIGGER manifest_no_replace BEFORE INSERT ON harness_admission_manifests WHEN EXISTS(SELECT 1 FROM harness_admission_manifests WHERE manifest_id=NEW.manifest_id)
BEGIN SELECT RAISE(ABORT,'IMMUTABLE_MANIFEST'); END;
```

O trigger de inserção impede substituição por conflito mesmo sem triggers recursivos de DELETE habilitados. Retry idempotente consulta a linha existente e devolve recibo, sem tentar inserir novamente.

JSON SQL functions são requisito a verificar no SQLite embarcado. Não retirar CHECKs em silêncio se indisponíveis. Hexadecimal, finitude do score e limites são verificados na camada de domínio; constraints SQL reforçam integridade, não autenticação humana.

Metadados e audit trail são extensões propostas necessárias à coordenação e rastreabilidade:

```sql
CREATE TABLE governance_state (project_id TEXT PRIMARY KEY NOT NULL, graph_generation TEXT NOT NULL, coverage_revision TEXT NOT NULL, rules_revision INTEGER NOT NULL,
extractor_version TEXT NOT NULL, publication_status TEXT NOT NULL CHECK(publication_status IN ('STABLE','PUBLISHING','RECOVERY_REQUIRED')),
pending_generation TEXT, schema_version INTEGER NOT NULL, last_trusted_time INTEGER NOT NULL);
CREATE TABLE tradition_reviews (review_id TEXT PRIMARY KEY NOT NULL, rule_id TEXT NOT NULL REFERENCES tradition_rules(rule_id), reviewer_id TEXT NOT NULL, from_status TEXT NOT NULL, to_status TEXT NOT NULL, snapshot_hash TEXT NOT NULL, reviewed_at INTEGER NOT NULL);
```

`governance_state` nunca reside apenas no arquivo reconstruível. A revisão persistida cresce a cada mutação de regra; sua leitura compõe `rules_revision` efetiva. Audit trail de revisão não permite UPDATE/DELETE na aplicação e recebe triggers equivalentes aos do ledger durante a migração. Migrações são versionadas e transacionais no banco durável; não executar DROP/CREATE do ledger em reindexação. Hashes/cópias de backup incluem WAL por backup API consistente, nunca cópia isolada do arquivo aberto.

### 5.2 Repositórios e prepared statements

| Porta | Operações | Retorno/integridade |
|---|---|---|
| SnapshotRepository | pin/read_current/validate/publication_state | Tupla publicada consistente; `RECOVERY_REQUIRED` recusa leitura/admissão. |
| TraditionRepository | propose/load_applicable/review_by_operator/read_revision | Candidata, normas efetivas e revisão; mutação incrementa revisão na mesma transação. |
| ManifestRepository | find_by_id/insert_if_current | Recibo imutável; mesma identidade/conteúdo → retry idempotente; conflito → recusa. |
| GraphEvidenceRepository | resolve_targets/read_source/traverse/coverage | Geração pinada; pages completas ou lacunas explícitas; nenhuma escrita em governança. |

```c
int cbm_governance_load_rules(cbm_governance_lease_t *lease, const cbm_rule_scope_t *scope, int64_t now, cbm_rule_set_t **out);
int cbm_governance_insert_manifest(cbm_governance_lease_t *lease, const cbm_admission_manifest_t *manifest);
int cbm_governance_propose_rule(cbm_governance_lease_t *lease, const cbm_tradition_rule_t *rule);
int cbm_governance_review_rule(cbm_governance_lease_t *lease, const cbm_operator_capability_t *human, const cbm_rule_review_t *review);
```

Preparar SQL constante com `sqlite3_prepare_v2`; parâmetros via `sqlite3_bind_*`, textos copiados com `SQLITE_TRANSIENT` quando necessário. Nunca interpolar domínio, texto, ID ou JSON no SQL. Toda escrita verifica step/commit e trata falha liberando transação; busy retry limitado e observável, sem loop infinito.

```sql
SELECT * FROM tradition_rules WHERE domain=?1 AND status IN ('APPROVED','REVIEW_PENDING') ORDER BY rule_id;
SELECT * FROM harness_admission_manifests WHERE manifest_id=?1;
INSERT INTO harness_admission_manifests VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9);
UPDATE governance_state SET rules_revision=rules_revision+1 WHERE project_id=?1;
```

A seleção roda por todos os domínios aplicáveis e pelo domínio global registrado, depois deduplica/ordena. A leitura aplica o estado efetivo por prazo; filtrar só `status='APPROVED'` é incorreto. A inserção real lista explicitamente colunas para resistir à evolução de schema; a forma compacta acima apenas ilustra binds.

### 5.3 Fence, publicação recuperável e TOCTOU

Fence por projeto é lock gerenciado com exclusão entre threads **e processos**. Ordem global para operações coordenadas: adquirir fence → adquirir lease → `BEGIN IMMEDIATE`; nunca adquirir fence já segurando writer lock SQLite. Leituras de extração usam pin/shared fence da geração e transações de leitura; fazem a travessia sem writer lock. Apenas a publicação final usa fence exclusiva, não o rebuild inteiro.

Protocolo de publicação proposto, integrado a `cbm_pipeline_publish_staged`/finalização:

1. Preparar e validar nova geração fora da fence exclusiva, com ID único e marcador interno verificável.
2. Sob fence exclusiva, gravar `publication_status=PUBLISHING` e `pending_generation` em transação FULL curta no banco durável.
3. Substituir o arquivo do grafo com o mecanismo de publicação existente; não remover sidecars de `governance.db`.
4. Verificar geração publicada e, em nova transação FULL, atualizar graph_generation/coverage_revision e estado `STABLE`; invalidar caches antigos. Só então liberar fence.
5. Em falha/crash entre etapas, manter `PUBLISHING`/`RECOVERY_REQUIRED`; nenhuma admissão é permitida. Recuperador sob fence reconcilia marcador do arquivo publicado com a geração anterior/preparada e estabiliza uma delas. Não adivinhar estado pela existência do arquivo.

Não afirmar atomicidade distribuída de dois arquivos SQLite WAL. O journal de publicação e a recusa durante recuperação eliminam a janela de falsa admissão. Toda mudança de cobertura/geração, inclusive incremento e promoção de horizonte que altere Base Graph, participa da mesma coordenação. Evitar ABA com IDs nunca reutilizados.

Protocolo do manifesto:

1. Fora do writer lock, validar args, autoridade do integrador, veredito sem bloqueios, evidência/hash e descritor de mudança. Reconstituir bundle autorizado se necessário.
2. Adquirir fence exclusiva, lease durável e `BEGIN IMMEDIATE`; confirmar `publication_status=STABLE` e componentes atuais, inclusive cadência no `now` confiável.
3. Consultar ID existente: conteúdo semanticamente idêntico retorna recibo original, inclusive seu timestamp, sem nova admissão; conteúdo diferente recusa `MANIFEST_CONFLICT`. Um retry de recibo existente não autoriza nova execução contra snapshot antigo.
4. Para inserção nova, recalcular `snapshot_id` e comparar por igualdade com o solicitado e o bundle; discrepância → `ROLLBACK`, `STALE_SNAPSHOT`. Revalidar integridade e a elegibilidade se houver concessão Fast-Path; checar cadência imediatamente antes do commit e abortar se o estado efetivo mudou.
5. Inserir manifesto append-only, `admitted_at` do servidor e executar `COMMIT`; retornar sucesso apenas após commit. Falha em step/commit → rollback e nenhum sucesso. Liberar recursos na ordem inversa.

```sql
BEGIN IMMEDIATE;
SELECT graph_generation,coverage_revision,rules_revision,extractor_version,publication_status FROM governance_state WHERE project_id=?1;
/* Comparar snapshot, validar regra temporal e inserir; qualquer divergência exige ROLLBACK. */
COMMIT;
```

O snippet representa o protocolo, não permite commit incondicional. SQLite fornece serialização das escritas de governança; a fence protege a identidade do grafo publicado durante a comparação/inserção. Atualizações de regras usam a mesma ordem e transação, com incremento de revisão e audit trail atômicos.

Arquivos fonte modificados por ferramentas externas não são bloqueados por um lock SQLite. Captura verifica hashes antes/depois; detecção de conteúdo divergente invalida cobertura/revisão ou recusa `SOURCE_DRIFT`. A garantia de snapshot é do estado publicado e da evidência capturada; execução de alterações externas ao protocolo exige os gates de bytes/AST do ADR-003 no momento da mutação. Não alegar que o ledger sozinho impede toda escrita externa concorrente.

### 5.4 Métrica de redução bifocal

Fixture favorável fixa: um alvo e dez dependências com corpos extensos e contratos pequenos, sem alterar documentação para favorecer a medição. Construir baseline JSON v2 de referência com dependências integrais, preservando os mesmos IDs, escopo, metadados, cobertura e normas. Apenas a representação de dependências difere. Medir bytes UTF-8 após a mesma canonicalização, incluindo escaping e metadados; `reduction = 1 - bifocal_bytes / full_source_bytes`. Aceite `reduction >= 0.80`; publicar os dois tamanhos e o tamanho da projeção de código em diagnóstico do teste. Fonte do alvo decodificada deve ser idêntica à baseline. Não prometer 80% para qualquer projeto nem usar contagem de tokens variável como oráculo.

## Section 6 — Ordered Development Tasks

```json
[
  {"id":"01","title":"Definir VOs de identidade e cadência","description":"Escrever testes vermelhos e implementar validação estrita de hashes, limites e intervalos em C11.","scope":["src/harness/types.h","src/harness/review_cadence.c","tests/test_harness_values.c"],"acceptance":["Rejeitar overflow, zero e frações de cadência","Comparar SHA-256 por bytes e validar wire hexadecimal"],"depends_on":null},
  {"id":"02","title":"Modelar regras e manifesto imutável","description":"Implementar raízes e transições puras após testes de autoridade, cadência e bloqueios.","scope":["src/harness/tradition_rule.c","src/harness/admission_manifest.c","tests/test_harness_domain.c"],"acceptance":["Candidata não promove por agente","Prazo vencido mantém enforcement","Manifesto com bloqueios é recusado"],"depends_on":"01"},
  {"id":"03","title":"Implementar canonicalização e snapshot","description":"Implementar identidade determinística e ownership yyjson após testes de ordem, estados efetivos e OOM.","scope":["src/harness/snapshot.c","src/harness/evidence_json.c","tests/test_harness_canonical.c"],"acceptance":["Mesmo conteúdo produz mesmos bytes e hashes","Cada componente altera snapshot","Cadência vencida altera revisão efetiva"],"depends_on":"02"},
  {"id":"04","title":"Implementar análise tri-state conservadora","description":"Derivar efeitos com motivos e proveniência após fixtures fechadas, dinâmicas e cíclicas.","scope":["src/harness/effect_fingerprint.c","src/harness/dependency_capsule.c","tests/test_harness_effects.c"],"acceptance":["Despacho desconhecido não vira absent","Present e unknown propagam conservadoramente"],"depends_on":"03"},
  {"id":"05","title":"Implementar projeção tipada de costuras","description":"Projetar seams determinísticos com endpoints e proveniência após testes positivos, desconectados e adulterados.","scope":["src/harness/seam_map.c","tests/test_harness_seams.c","tests/fixtures/harness_v2/seams"],"acceptance":["Bridge explícito produz hipótese tipada","Tópicos desconectados não produzem seam","Referência inválida gera erro"],"depends_on":"04"},
  {"id":"06","title":"Implementar política de Fast-Path","description":"Implementar conjunção fail-closed após a matriz de condições e combinações.","scope":["src/harness/fast_path.c","tests/test_harness_fast_path.c"],"acceptance":["Cinco impedimentos isolados bloqueiam","Caso limpo aprova com arrays vazios","Mudança ausente ou scope inválido não aprova"],"depends_on":"05"},
  {"id":"07","title":"Criar store durável de governança","description":"Criar migração e leases WAL FULL após testes reais de configuração, constraints e reinicialização.","scope":["src/store/governance_store.c","src/store/governance_store.h","tests/test_governance_store.c"],"acceptance":["FULL/WAL verificados em reabertura","Constraints e prepared statements preservam integridade","Erro de configuração recusa serviço"],"depends_on":"06"},
  {"id":"08","title":"Persistir propostas e revisões humanas","description":"Implementar repositório de regras, revisão e auditoria atômica após testes de concorrência e retry.","scope":["src/store/governance_rules.c","src/harness/operator_port.h","tests/test_governance_rules.c"],"acceptance":["Proposta permanece candidata","Revisão humana incrementa revisão e audit trail","Rollback não deixa revisão parcial"],"depends_on":"07"},
  {"id":"09","title":"Coordenar fence e publicação recuperável","description":"Introduzir coordenação interprocesso e journal de publicação após testes de swap, cancelamento e crash.","scope":["src/harness/publication_fence.c","src/pipeline/pipeline.c","tests/test_harness_publication.c"],"acceptance":["Swap não ocorre durante commit de admissão","Crash entre swap e metadados bloqueia até recuperação","Governança não é substituída"],"depends_on":"08"},
  {"id":"10","title":"Integrar leitura fixada do bundle","description":"Conectar travessia paginada, cobertura e fonte integral aos extratores puros após testes de drift e benchmark.","scope":["src/harness/evidence_bundle.c","src/harness/graph_evidence.c","tests/test_harness_bundle.c"],"acceptance":["Alvos preservados byte a byte","Fixture completa reduz bytes em pelo menos 80%","Truncamento não aparece como clean"],"depends_on":"09"},
  {"id":"11","title":"Registrar admissão transacional idempotente","description":"Implementar ledger com revalidação protegida após testes de stale snapshot, commit falho e ID repetido.","scope":["src/store/governance_manifests.c","src/harness/record_admission.c","tests/test_harness_admission.c"],"acceptance":["Stale snapshot causa rollback sem linha","Mesmo ID/conteúdo retorna recibo original","Bloqueios e elegibilidade falsa impedem inserção"],"depends_on":"10"},
  {"id":"12","title":"Publicar adapters MCP de leitura","description":"Registrar extração e avaliação com schemas estritos após testes stdio de payload e evidência adulterada.","scope":["src/mcp/harness_handlers.c","src/mcp/mcp.c","tests/test_harness_mcp_read.c"],"acceptance":["Tools list publica contratos v2","Cliente não falsifica coverage ou efeitos","Leituras não criam sessão nem escrevem governança"],"depends_on":"11"},
  {"id":"13","title":"Publicar adapters MCP de governança","description":"Registrar proposta e manifesto com erros tipados após testes de autoridade e retry por perda de resposta.","scope":["src/mcp/harness_handlers.c","src/mcp/mcp.c","tests/test_harness_mcp_write.c"],"acceptance":["Quatro ferramentas novas são publicadas","Nenhuma ferramenta promove regra","Sucesso só é emitido após commit"],"depends_on":"12"},
  {"id":"14","title":"Validar durabilidade e fluxo completo","description":"Integrar suites ao runner existente e executar concorrência, rebuild e verificações de memória em ambiente suportado.","scope":["tests/test_harness_durability.c","tests/e2e/test_harness_v2.py","tests/test_main.c","Makefile.cbm"],"acceptance":["Rebuild e reindex preservam ledger e regras","Processos concorrentes não perdem commits","ASan/UBSan e TSan não apontam falhas na mudança"],"depends_on":"13"}
]
```

Cada tarefa é uma sessão estimada de 2–4 horas com ciclo Red → Green → Refactor; dividir novamente se a integração concreta exceder esse limite. Os caminhos são propostas, não tarefas já implementadas. O array de tarefas é dado de planejamento, não snippet de implementação; todos os exemplos C/SQL acima respeitam quatro linhas. A cobertura e os comandos de validação seguem [TESTS.md](../../adr/TESTS.md); os cenários vinculados estão em [004](004-codebase-memory-multigraph-mcp-test-scenarios.md).

## Referências e rastreabilidade

[001 — Linguagem e invariantes](001-problem-space.md), [002 — Contextos e inspeção Tier 2](002-context-map.md), [ADR-004](../../adr/ADR-004-HARNESS-PERFEITO-V2.md), [ADR.md](../../../ADR.md). Os pontos existentes de dispatch, pool e swap foram verificados conforme o registro em 002. Perfis de conexão, fence, schema auxiliar, limites e portas C aqui descritos são decisões propostas para a implementação da evolução.
