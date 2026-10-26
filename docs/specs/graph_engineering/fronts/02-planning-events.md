# Frente 02 — Refinamento tático e cenários: Planejamento e Eventos

**Domínio:** Graph Engineering  
**Projeto:** `codebase-memory-multigraph-mcp`  
**Contexto:** Planejamento e Eventos (pacote proposto `packages/cbm-orchestrator/`, TypeScript)  
**Status:** refinamento `model`; nenhum contrato abaixo é API implementada  
**Data:** 2026-10-23

Este suplemento detalha F02 (PBI-06–10) e F03 (PBI-11–14) para consolidação posterior pelo CTO em 003/004. Usa `001-problem-space.md`, `002-context-map.md`, decisões em `DECISIONS.md`, contratos C01–C08 em `005-shared-contracts.md`, e limites de capacidade em `006-capability-matrix.md`. Não altera os contratos compartilhados nem afirma que mecanismos propostos existem. A orientação de grafo fornecida em `ORIENTATION.md` é Tier Verify para o projeto exato; não foi feita nova alegação estrutural fora dessa evidência documental.

O pacote TypeScript é uma proposta e consome o core C via MCP. O core C e seu SQLite permanecem inalterados; journal/outbox são limites lógicos TypeScript regidos por C07 e não exigem um banco físico próprio nem duplicação de ledger/outbox. Nomes de capacidades MCP citados abaixo são apenas os observados na matriz; capacidades parciais, não comprovadas ou novas continuam condicionadas a validação. Nenhum endpoint de anchors, recall closure ou dispatch é presumido. Claim documental Union não é lock de escrita.

## Fronteira e PBIs

| Dentro desta frente | Resultado especificado |
|---|---|
| F02 — PBI-06–10 | Snapshot do subgrafo afetado com fonte/revisões/cobertura; plano DAG de tarefas; claims exclusivos de trabalho com lease e fencing; serialização para contratos compartilhados; integração de resultados parciais sem promover conjunto não verificado. |
| F03 — PBI-11–14 | Sinais de seam/anomalia apenas com fonte comprovada; assinaturas configuráveis e versionadas; inbox/outbox, deduplicação lógica, limites de propagação/fan-out e despacho debitado no orçamento global C03; replay explícito de dead-letter. |
| Dependências transversais | C01 identifica projeto, candidato e revisões; C02 mantém estados/fases/motivos sem redefinição; C03 fornece o único orçamento GLOBAL aplicável a todos os despachos; C05 define identidade e permissões; C06 é o contrato canônico de plano, claim e envelope; C07 governa transações locais e efeitos externos; PBI-15/Q04 condicionam mediação confiável de escrita e dispatch. |
| Fora da fronteira | Outer Loop e recuperação (F01/F05), autorização/promoção e risco (F04/F06), evolução (F07), telemetria/medição completa (F08), mudança do core C, execução de spike, implementação ou testes executáveis. A frente não concede promoção nem substitui a evidência independente de C04. |

Tasks na seção própria são condicionais e não autorizam implementação. Q04 e ADR-005 permanecem `Deferred`/`PROPOSED`; Q05 permanece `Deferred`, sem números ou defaults inventados. Capacidades parciais ou não comprovadas em 006 não passam a fato por estarem modeladas aqui.

## Respostas de refinamento aplicáveis

| ID | Categoria | Pergunta | Recomendação registrada | Resposta final / fonte |
|---|---|---|---|---|
| Q04 | Integração | Qual contrato inicia, cancela, isola, mede e recupera agentes, inclusive para dispatch de eventos? | Definir e validar contrato agnóstico por spike; host confiável e mediação são propostas, não RBAC existente. | Unresolved / Deferred. ADR-005 permanece PROPOSED; PBB, DECISIONS e ORIENTATION. |
| Q05 | Orçamento | Quais defaults e limites de orçamento regem tarefas concorrentes, gatilhos e retries? | Exigir política explícita válida; definir valores somente após medições. Todos os despachos usam o ledger global C03. | Unresolved / Deferred; sem limites numéricos ou defaults humanos. PBB e DECISIONS. |

Q06–Q08 não são requisitos desta frente. Nenhuma recomendação adiada se torna comportamento esperado.

## Fronteiras e regras da frente

As regras locais especializam C01–C08 sem redefini-los. `state` e `phase` são campos separados: planejamento usa `phase=PLANNING`; decisões que exigem espera usam a transição `RUNNING → PAUSED` com `phase=AWAITING_DECISION`; retorno autorizado é `PAUSED → RUNNING`. A frente não cria estados, fases nem motivos terminais de C02.

| ID | Regra | Base |
|---|---|---|
| PE-R01 | Todo snapshot de topologia registra `projectId` resolvido, candidato/execução quando aplicável, geração do grafo, revisão de regras e cobertura, instante, fonte e referências consultáveis. Basename não é identidade. A tupla e a versão seguem C01. | C01/C06, PBI-06 |
| PE-R02 | Cobertura `PARTIAL`, stale, pending, skipped, excluded ou unknown acompanha o snapshot como lacuna explícita. Só dados frescos e cobertos podem sustentar uma decomposição afirmativa de independência. Sem prova, ampliar o escopo conservadoramente (até arquivo inteiro quando aplicável), serializar ou escalar; nunca converter lacuna em ausência de dependência. | C01/C06, matriz 006, PBI-06/08 |
| PE-R03 | O adapter de topologia usa apenas capacidades MCP observadas e mapeadas, como `get_architecture`, `search_graph`, `trace_path` e `check_index_coverage`, conforme aplicabilidade e contrato validado. Respostas incluem fonte e cobertura. Extração/validação de AST anchors não tem endpoint observado; `get_ast_anchors` não pode ser chamado como se existisse. A função interna de recall também não é endpoint MCP. | Matriz 006, ORIENTATION, PBI-06/07 |
| PE-R04 | `PlanSnapshot` é imutável e vinculado à revisão C01 e ao snapshot de cobertura. Cada tarefa tem escopo, âncoras/referências, pré-condições e dependências explícitas. O compilador detecta componentes fortemente conexos (SCCs), colapsa cada um em unidade serial bloqueada de replanejamento e preserva as arestas internas como diagnóstico; não libera membros separadamente nem inventa uma ordem. A projeção do plano pode manter unidades bloqueadas junto a componentes acíclicos independentes; estes não são descartados. Cada unidade só é liberada após resolução explícita, e a projeção executável permanece DAG. Limite de concorrência respeita política explícita e orçamento global C03. | C03/C06, PBI-08 |
| PE-R05 | Comunidade é indício de decomposição, não prova de independência. Contrato compartilhado, dependência cruzada, âncora compartilhada ou cobertura insuficiente exigem seam avaliado e serialização/agrupamento conservador antes de edição paralela. A ausência de uma relação no grafo não prova que ela não existe. | C06, PBI-08/09 |
| PE-R06 | Mudança de geração, cobertura, regras, escopo, contrato ou baseline invalida alocações incompatíveis e integração/verificação anterior. Replanejamento produz novo `planVersion`; tarefas e claims só permanecem se explicitamente revalidados contra a nova tupla. | C01/C06, PBI-06/08/10 |
| PE-R07 | `WorkClaim` é exclusividade nova da camada TS, não claim documental Union. Aquisição compara atomicamente o recurso efetivo, não apenas igualdade de chave: aliases são canonicalizados antes da comparação; arquivo inteiro conflita com qualquer âncora/range nele, âncoras/ranges sobrepostos conflitam e aliases do mesmo escopo não concedem claims duplicados. Só âncoras validadas e comprovadamente não sobrepostas podem coexistir no mesmo arquivo; âncora não comprovada usa escopo de arquivo. O recurso registra escopo/path canônico, âncora ou range e baseline, dono autenticado, modo, lease e fencing token monotônico. Sem store/fencing comum a todos os escritores, não prometer exclusão distribuída. | C05/C06, PBI-07 |
| PE-R08 | Lease expirado marca claim como elegível para revalidação/revogação; não prova que o processo antigo terminou. Toda escrita passa por mediação de host/write-boundary que resolve de novo path/alias/escopo e valida o token corrente imediatamente antes do efeito. Claim antigo, conflito por sobreposição ou fencing token inferior é recusado mesmo se processo antigo continua vivo. | C05/C06, Q04, PBI-07 |
| PE-R09 | Renovação não pode restaurar token revogado. Abandono libera claim só após fencing/revogação eficaz. Transferência revoga o token anterior e emite um maior atomicamente antes de conceder capacidade ao novo dono. Conflito deixa o recurso sem novo editor até estado seguro ser estabelecido. | C06, PBI-07 |
| PE-R10 | Renome, remoção, troca de baseline ou drift da âncora dispara revalidação da identidade/referência e da cobertura. Âncora ausente/ambígua/stale invalida claim e plano dependente; fallback para arquivo inteiro apenas reduz o paralelismo, não prova âncora válida nem independência. | C01/C06, matriz 006, PBI-06/07 |
| PE-R11 | Avaliação de seam é pré-condição para dois trabalhos que tocam contrato comum. Evidência deve identificar contrato, versões e origem; resultado ausente, inconclusivo, stale ou divergente mantém serialização e não libera edição paralela. Não inventar tool `evaluate_seam_map`: a matriz mapeia capacidade apenas parcial em bundle/evaluation existentes. | C04/C06, matriz 006, PBI-09 |
| PE-R12 | Integração registra cada resultado/tarefa separadamente. Falha parcial preserva os resultados independentes que passaram e seus diagnósticos; não apaga trabalho não relacionado. O candidato integrado só avança para verificação do conjunto após todas as tarefas obrigatórias resolvidas e versão revalidada. Nunca promove o conjunto sem evidência independente verde atual e gate C04. | C01/C04/C06, PBI-10 |
| PE-R13 | Um sinal de violação de seam ou anomalia de trace só é evento acionável quando adapter apresenta fonte comprovada, objeto/versão, evidência consultável e semântica de detecção validada. Callback de reindexação do watcher não é violação de contrato; ingestão de traces não prova, por si, que houve anomalia. | C04/C06, matriz 006, PBI-11/12 |
| PE-R14 | Assinatura de evento tem `signatureId` e revisão imutável, schema/version, tipo e origem permitidos, filtro, perfil alvo, condição de evidência, limites/falha e identidade autorizada de quem a alterou. Alterar assinatura cria nova revisão; entrega já enfileirada aponta para a revisão avaliada e não herda silenciosamente uma configuração posterior. | C05/C06, PBI-13 |
| PE-R15 | Envelope usa C06: `eventId`, `type`, `schemaVersion`, `projectId`, `runId`/`candidateVersion` quando aplicáveis, `occurredAt`, `recordedAt`, `source`, `causationId`, `correlationId`, `originAgentId`, `hopCount`, referência de evidência, chave lógica de dedupe e revisão da assinatura. Não incluir conversa, segredo ou payload maior que o necessário. | C01/C06, PBI-11–14 |
| PE-R16 | Entrega é pelo menos uma vez, com outbox transacional local e inbox deduplicada. Deduplicação considera `eventId` e chave lógica tipo+fonte+objeto+versão; UUID diferente não permite duplicar o mesmo gatilho lógico. Reordenação é tratada por revisão/versão e idempotência do consumidor, sem alegar ordem global ou exactly-once remoto. | C06/C07, PBI-11–14 |
| PE-R17 | Todo dispatch de agente, incluindo gatilho, retry e replay, reserva atomicamente no mesmo `BudgetLedger` global C03 antes de despacho. Agentes, assinaturas, triggers e retries compartilham o teto do objetivo; subagente ou consumidor não recebe teto próprio. Reservas em voo e custo pendente permanecem contabilizados até reconciliação; uso ausente/desconhecido suspende novos despachos sem cobertura conservadora. Política Q05 não resolvida exige configuração válida explícita; números e limiares não são inventados. | C03, Q05, PBI-14 |
| PE-R18 | Propagação limita hops, recursão, fan-out, concorrência e fila por política explícita; origem/correlation/causation evitam loops do próprio dispatch. Atingir limite suprime ou coloca em dead-letter com motivo e auditoria, sem emitir novos despachos fora do orçamento. | C03/C06/C07, PBI-14 |
| PE-R19 | Dead-letter preserva evento, versão da assinatura, motivo e tentativas. Replay é comando explícito e autorizado, revalida fonte/assinatura/versão atual e idempotência, cria nova tentativa correlacionada e reserva orçamento C03; não há replay automático nem repetição cega após efeito incerto. | C05/C06/C07, PBI-14 |
| PE-R20 | Falha ao persistir plano, claim, inbox/outbox, intenção de dispatch ou decisão de replay impede o efeito externo correspondente. Após crash na janela entre despacho remoto e resultado local, reconcilia-se por fonte/consumidor se suportado; sem confirmação, estado permanece incerto e não é duplicado automaticamente. | C06/C07, PBI-07/10–14 |
| PE-R21 | O resolver de recurso aplica regras do filesystem para canonicalizar separadores/case e seguir symlinks; aquisição e boundary de escrita detectam também rename/drift. Ambiguidade de identidade ou cobertura aumenta o escopo de forma conservadora, serializa ou bloqueia. Fencing e recusa só são garantidos se todos os writers e aliases efetivos passam pelo mesmo mediador/store; isso continua por provar em Q04. | C05/C06, PBI-07 |

### Limites de capacidade e confiança

| Necessidade | Evidência atual | Condição para aceite |
|---|---|---|
| Ler subgrafo/cobertura | Tools observadas `get_architecture`, `search_graph`, `trace_path`, `check_index_coverage`, best effort. | Adapter preserva revisão, gaps e fonte; parcial/stale aciona fallback conservador. |
| Extrair/validar anchors | `get_ast_anchors` não consta como nome MCP; anchors são aceitos em superfície de promoção e fontes/AST internas existem. | Validar capacidade acessível sem escrita direta no store C; até lá, PBI-06/07 não declaram escopo de anchor comprovado. |
| Avaliar seams | Bundle/evaluation de harness é parcial; não equivale a avaliação semântica completa nem execução de testes. | Adapter valida retorno, evidência, revisão e condição de serialização; sem prova, serializa. |
| Claim exclusivo | Claims Union observados são documentais, não locks de AST. | WorkClaim TS, store compartilhado, fencing e mediação em todo caminho de escrita, incluindo processo antigo. |
| Origem de eventos | Watcher reindexa; ingestão de traces existe; event dispatcher de negócio é novo. | Fonte/semântica específica de seam e anomalia deve ser comprovada pelo adapter e evidência consultável. |
| Execução de agente | Contrato Q04 / ADR-005 PROPOSED. | Host confiável, identidade, cancelamento, isolamento, medição, dispatch e recuperação têm de ser validados; desenho não prova execução. |

## Estrutura, tipos e interfaces lógicas

Elementos e nomes abaixo são propostas internas TS, não classes existentes, APIs públicas nem alteração do core. A arquitetura é um pacote de aplicação com domínios pequenos e adapters ACL; não se exige framework ou migração arquitetural do repositório.

### Elementos principais

| Elemento | Camada / tipo | Invariantes / regras técnicas | Forma ilustrativa (máx. 4 linhas) |
|---|---|---|---|
| `AffectedSubgraphSnapshot` | Planejamento / read model imutável | Projeto e candidato identificados por C01; grafo, regras, cobertura, fontes, gaps e anchors reportados com proveniência. | ver bloco A |
| `PlanSnapshot` | Planejamento / raiz lógica | Plano versionado e imutável; tarefas pertencem à versão; SCCs ficam como grupos serializados e bloqueados até resolução; projeção executável é DAG; lacunas ficam explícitas. | ver bloco B |
| `WorkClaim` | Planejamento / raiz lógica | Exclusividade no store TS mediado; recurso canônico, lease, token monotônico e estado são validados atomicamente. | ver bloco C |
| `EventSubscription` | Eventos / registro versionado | Revisões imutáveis; origem/schema/evidência/filtro/perfil e política explícitos; mudança cria revisão. | ver bloco D |
| `EventDelivery` | Eventos / inbox-outbox | Estado de entrega durável; chave lógica e `eventId`; retries/replay preservam versão e orçamento. | ver bloco E |

```text
A: AffectedSubgraphSnapshot
  projectId, candidateVersion, graphGeneration, coverageRevision
  coverage: COMPLETE | PARTIAL | STALE | UNKNOWN; sources, gaps, anchors
  // ausência de cobertura permanece visível
```

```text
B: PlanSnapshot
  planId, planVersion, candidateVersion, tasks: Task[]
  dependencies: DAG<TaskId>; blockedSccs: SerialGroup[]
  // grupos cíclicos ficam bloqueados até replanejamento explícito
```

```text
C: WorkClaim
  claimId, taskId, owner: AuthenticatedAgentIdentity
  resource: CanonicalPathScopeBaseline; leaseUntil, fencingToken
  // escrita exige token corrente no boundary mediador
```

```text
D: EventSubscription
  signatureId, revision, schemaVersion, sourceFilter, evidenceRule
  targetProfile, limits, failurePolicy, updatedBy: AuthenticatedIdentity
  // alteração gera revisão imutável
```

```text
E: EventDelivery
  eventId, logicalDedupeKey, subscriptionRevision, hopCount
  status: PENDING | DELIVERED | SUPPRESSED | DEAD_LETTER | UNKNOWN
  // pelo menos uma vez; consumidor precisa ser idempotente
```

### Tipos e validação

| Tipo / contrato | Contexto | Regras de validação |
|---|---|---|
| `CoverageState` | Snapshot / C01, C06 | `COMPLETE`, `PARTIAL`, `STALE`, `PENDING`, `SKIPPED`, `EXCLUDED`, `UNKNOWN`; lacuna nunca é coerção para `COMPLETE`. |
| `TopologyRevision` | Snapshot / C01 | Tupla do projeto, `graphGeneration`, `rulesRevision`, `coverageRevision`; revalidar antes de reservar/editar/integrar. |
| `CanonicalResourceRef` | Claim / C06 | Path efetivo canonicalizado segundo o filesystem + escopo de arquivo/âncora/range + baseline; detecção de sobreposição inclui aliases, arquivo inteiro e ranges; âncora sem validação usa escopo conservador e não atesta independência. |
| `FencingToken` | Claim / C06 | Inteiro monotônico do store coordenador; token inferior, expirado ou revogado falha antes de qualquer escrita. |
| `TaskDependency` | Plano / C06 | Origem/destino existentes; SCC fica agrupado e bloqueado até resolução; projeção executável precisa DAG antes de liberar tarefa. |
| `SeamEvidenceRef` | Planejamento / C04/C06 | Contrato, versões, fonte, resultado `COMPATIBLE|CONFLICT|INCONCLUSIVE|UNAVAILABLE` e referência consultável; somente `COMPATIBLE` atual sustenta paralelismo. |
| `EventEnvelope` | Eventos / C01/C06 | Campos canônicos presentes quando aplicáveis; `schemaVersion` conhecido, fonte autenticável, timestamps preservados, sem segredo/conversa. |
| `LogicalDedupeKey` | Inbox / C06 | Derivada de tipo+fonte+objeto+versão; validação não depende somente de UUID `eventId`. |
| `SignatureRevisionRef` | Dispatch / C06 | Aponta para revisão imutável usada ao enfileirar/avaliar; versão incompatível é recusada, não atualizada silenciosamente. |
| `DispatchBudgetReservation` | Orquestração / C03 | Reserva no ledger GLOBAL antes do dispatch; único ciclo/objetivo contabiliza confirmados, pendentes e estimativas de forma compatível. |

### Ports e adapters

| Port/adaptador proposto | Responsabilidade lógica | Limite / dependência |
|---|---|---|
| `TopologyReadPort` | Ler arquitetura, nós/arestas, dependências e cobertura; devolver proveniência/revisões. | ACL para MCP. Só usa tools observadas e superfície validada; não presume anchor endpoint nem recall API. |
| `AnchorValidationPort` | Resolver/revalidar referências de anchor/path/baseline. | Capacidade parcial; se não acessível, registrar lacuna, ampliar escopo/serializar ou bloquear aceite. |
| `SeamEvidencePort` | Obter/validar evidência de contratos comuns. | Bundle/evaluation parcial; resultado sem referência atual não libera paralelismo. |
| `PlanStorePort` | Persistir snapshots/tarefas e ler versão corrente por CAS. | Boundary lógico TypeScript/C07; não altera banco C nem exige arquivo SQLite separado. |
| `WorkClaimStorePort` | Adquirir/renovar/revogar/transferir claims e avançar token; comparar escopos efetivos por sobreposição. | Coordenação só vale se todos os writers usarem o mesmo store e fencing. |
| `FencedMutationPort` | Mediar escrita pela identidade, resolver aliases/escopo efetivo e validar token corrente do claim. | Depende de host/write-boundary Q04; sem cobertura de todos os caminhos, exclusividade não está entregue. |
| `EventSourcePort` | Validar origem, semântica, revisão e evidência do sinal recebido. | Watcher/`ingest_traces` não provam seam/anomalia por si. |
| `SignatureRegistryPort` | Ler/publicar revisão autorizada de assinatura. | Operação exige autoridade C05; assinatura não confere permissão de execução ao alvo. |
| `EventJournalPort` | Commit local de inbox/outbox, dedupe, dead-letter e replay intent. | Journal lógico TypeScript/C07; atomicidade local somente, não com MCP/host, sem exigir SQLite físico próprio. |
| `AgentDispatchPort` | Solicitar execução do perfil selecionado e correlacionar resultado. | Visão de despacho do contrato único de host/runtime Q04 (não um runtime separado); toda chamada reserva orçamento global C03 antes de despacho. |
| `GlobalBudgetPort` | Reservar, reconciliar e debitar gatilho/retry/replay no mesmo ledger do ciclo. | C03 é autoridade; sem política/capacidade de medição válida, suspender novos despachos. |

## Use cases e coordenação

| Operação lógica | Responsabilidade | Coordena |
|---|---|---|
| `IdentifyAffectedSubgraph` | Obter snapshot da mudança com fonte, revisões e estado de coverage; gerar lacunas/fallback requerido. | `TopologyReadPort`, `AnchorValidationPort`, `PlanStorePort` |
| `CompileWorkPlan` | Gerar tarefas e dependências; identificar SCC, bloquear unidade cíclica para replanejamento e validar DAG da projeção executável, âncoras, contratos comuns e concorrência compatível com C03. | `AffectedSubgraphSnapshot`, `SeamEvidencePort`, `PlanStorePort` |
| `RevalidatePlanSnapshot` | Comparar candidato, graph/rules/coverage, anchors e contracts; produzir nova `planVersion` ou invalidar claims. | `TopologyReadPort`, `AnchorValidationPort`, `PlanStorePort`, `WorkClaimStorePort` |
| `AcquireWorkClaim` | Adquirir recurso atomicamente, recusar conflito e emitir fencing token. | `WorkClaimStorePort`, identidade C05, `FencedMutationPort` |
| `RenewOrTransferWorkClaim` | Renovar ou revogar/transferir claim com token crescente e sem editor antigo válido. | `WorkClaimStorePort`, `FencedMutationPort` |
| `EvaluateSharedSeam` | Validar evidência atual para contrato tocado por múltiplas tarefas; manter serialização se faltar prova. | `SeamEvidencePort`, `PlanStorePort` |
| `IntegrateTaskResults` | Preservar resultados e falhas individualmente, integrar tarefas obrigatórias resolvidas e solicitar verificação do conjunto. | Plan snapshot, result store/journal, C04 verifier boundary |
| `RegisterEventSubscription` | Validar identidade, schema, fonte, perfil, limites e criar revisão imutável. | `SignatureRegistryPort`, autoridade C05 |
| `ValidateGraphSignal` | Converter sinal bruto em evento acionável apenas com evidência e fonte comprovadas. | `EventSourcePort`, `TopologyReadPort`, `EventJournalPort` |
| `DispatchBoundEvent` | Match de assinatura revisionada; dedupe, orçamento global, limites e chamada autorizada. | `SignatureRegistryPort`, inbox/outbox, C03, `AgentDispatchPort` |
| `ReplayDeadLetter` | Exigir comando autenticado, revalidar versão/fonte, preservar idempotência e reservar orçamento. | `EventJournalPort`, C05, C03, `AgentDispatchPort` |

## Eventos, mensagens e consumidores

Payloads são referências mínimas a dados consultáveis, não cópias de contexto ou código. Cada evento segue envelope C06 e identidade/versão aplicáveis C01.

| Evento (alias lógico TS) | Gatilho | Payload mínimo além do envelope | Consumidores / efeito |
|---|---|---|---|
| `AffectedSubgraphIdentified` | Snapshot de alcance concluído, inclusive com gaps. | `snapshotRef`, `coverageState`, `gapRefs`, `topologyRevision`. | Compilador de plano; Operador para inspecionar incerteza. |
| `WorkPlanCompiled` | Snapshot persistido com a projeção executável em DAG; SCCs não resolvidas aparecem como unidades bloqueadas de replanejamento. | `planId`, `planVersion`, `taskIds`, `blockedSccRefs`, `dependencyDigest`, `coverageRef`. | UI/consulta de plano e scheduler; não despacha sozinho. |
| `WorkClaimAcquired` | Aquisição atômica confirmada. | `claimId`, `taskId`, `resourceRef`, identidade do dono, lease, fencing token. | Scheduler/mediador de escrita e auditoria. |
| `WorkClaimConflictDetected` | Claim concorrente, token incompatível ou sobreposição/alias de recurso. | refs/escopos em conflito, revisão e resultado recusado. | Scheduler e Operador; nenhuma escrita paralela. |
| `WorkClaimTransferred` | Revogação e novo token confirmados atomicamente. | `claimId`, dono antigo/novo, recurso, token anterior/novo, lease. | Scheduler, mediador de escrita, auditoria. |
| `PlanSnapshotInvalidated` | Mudança de versão/revisão, renome/drift ou nova lacuna. | `planId`, versão anterior, motivo, refs divergentes, tarefas afetadas. | Replanejamento; claims incompatíveis são revogados/fenced. |
| `SharedSeamEvidenceRecorded` | Avaliação de seam consultável e atual concluída. | `contractRef`, resultado, versões, `evidenceRef`, revisão. | Scheduler, integração e Operador. Ausência/inconclusão não é evento de liberação. |
| `ParallelResultsIntegrated` | Todas as tarefas obrigatórias resolvidas e composição criada. | `planId`, resultados por tarefa, candidato/revisão, status de integração, lacunas. | Gate de verificação independente; não promove. |
| `VerifiedSeamViolationObserved` | Adapter comprova violação com evidência e origem. | `contractRef`, `evidenceRef`, commit/candidato, `sourceProofRef`. | Assinaturas compatíveis e perfil configurado, usualmente adversarial se autorizado. |
| `VerifiedTraceAnomalyObserved` | Analisador de trace entrega anomalia reproduzível/consultável. | `traceRef`, tipo de anomalia, `evidenceRef`, regra/origem e revisão. | Assinaturas compatíveis; decisão/relato conforme configuração. |
| `EventSignatureRevisionPublished` | Operador/identidade autorizada atualiza assinatura. | `signatureId`, nova revisão, schema, fonte, alvo, filtros/limites, ator. | Registry, despachos futuros e auditoria. |
| `GraphEventDispatchRequested` | Sinal comprovado casa com assinatura revisionada. | `eventId`, chave lógica, revisão, `hopCount`, reserva requerida. | Inbox/scheduler; ainda não prova dispatch externo. |
| `GraphEventDispatchSuppressed` | Duplicata, recursão, limite ou falta de orçamento. | `eventId`, chave, motivo, revisão, contagem/limite; sem payload sensível. | Auditoria/telemetria e Operador conforme política. |
| `GraphEventDispatched` | Outbox entregue e host aceita solicitação após reserva C03. | `eventId`, tentativa, perfil/alvo autenticado, reserva, `correlationId`. | Runtime Q04 e reconciliação; entrega pode repetir. |
| `GraphEventDeadLettered` | Diagnóstico interno de entrega bloqueada ou falha classificada; escalona se a incerteza for material. | Envelope/ref, revisão, motivo, tentativas, status externo conhecido/incerto. | Operador autorizado; sem replay implícito. |
| `GraphEventReplayRequested` | Operador autorizado comanda replay após validar versão/fonte. | Evento original, nova tentativa, assinatura usada, chave idempotente e reserva. | `AgentDispatchPort`; audita nova decisão. |

Os nomes acima são aliases técnicos para persistência/integração, não uma segunda lista canônica de domínio. Os nomes do Event Storming em `001-problem-space.md` permanecem a linguagem pública desta frente:

| Alias técnico TS | Evento canônico de 001 ou relação explícita |
|---|---|
| `AffectedSubgraphIdentified` | #3 — **Subgrafo afetado identificado**. |
| `WorkPlanCompiled` | #4 — **Plano de tarefas compilado**. |
| `WorkClaimAcquired`, `WorkClaimConflictDetected`, `WorkClaimTransferred` | #5 — **Claim exclusivo de trabalho concedido**; conflito e transferência são detalhes do ciclo de vida, não novos eventos de domínio. |
| `PlanSnapshotInvalidated` | #20 — **Avaliação anterior invalidada por mudança de versão**, quando a tupla muda; replanejamento sem nova versão é detalhe interno que antecede novo #4. |
| `SharedSeamEvidenceRecorded` | Registro de evidência/guarda de planejamento, sem novo evento de domínio em 001; pode sustentar a tarefa pronta/liberada (#10), conforme resultado. |
| `ParallelResultsIntegrated` | #18 — **Resultado de tarefa integrada**. |
| `VerifiedSeamViolationObserved`, `VerifiedTraceAnomalyObserved` | #6 — **Evento de grafo recebido**; `Verified` exige a prova adicional PE-R13, não contida no watcher/ingestão simples. |
| `EventSignatureRevisionPublished` | Evento local de configuração/auditoria; a avaliação que casa sinal e assinatura corresponde ao #7 — **Assinatura de evento correspondida**. |
| `GraphEventDispatchRequested` | #7 — **Assinatura de evento correspondida**, como transição interna para despacho. |
| `GraphEventDispatchSuppressed` | #8 — **Disparo duplicado ou recursivo suprimido** para duplicata, loop, hop/fan-out; #26 — **Novas ações bloqueadas por orçamento esgotado** quando falta reserva C03. |
| `GraphEventDeadLettered` | Estado/diagnóstico interno; corresponde a #8 quando registra supressão por duplicata/recursão/limite. Se refletir incerteza material que exige decisão humana, relaciona-se a #15 — **Incerteza material escalada ao Operador**; não cria evento de domínio novo. |
| `GraphEventDispatched` | #9 — **Verificação de evento despachada**. |
| `GraphEventReplayRequested` | Comando interno explícito; resultado é #9 se despachado, #8 se suprimido por duplicata/recursão/limite, #26 por orçamento esgotado ou #15 quando incerteza material exige decisão. |

O evento #10 de 001 (**Tarefa liberada após dependências satisfeitas**) continua sendo a emissão canônica do scheduler quando uma tarefa se torna pronta; não foi duplicado com alias novo nesta tabela.

## Persistência, transações e recuperação

Persistência proposta como journal lógico do pacote TS/C07. O esquema lógico cobre `plan_snapshot/task/dependency`, `work_claim/fencing_counter`, `event_signature/revision`, `event_inbox/outbox/delivery_attempt`, dedupe, dead-letter e referências de evidência. Pode compartilhar infraestrutura física TS sem cruzar limites de escrita; não escreve no SQLite C e não supõe transação distribuída.

| Operação | Limite transacional e falha |
|---|---|
| Publicar plano | Snapshot, tarefas, dependências, SCCs bloqueadas, digest e evento de outbox local são gravados no mesmo commit. Revisão atual e DAG da projeção executável são validados; falha deixa plano anterior intacto. |
| Adquirir/transferir claim | Compare-and-set de sobreposição de escopo efetivo, lease e contador fencing é atômico no store coordenador. Token anterior fica inválido antes de qualquer capacidade nova. Se mediador/store não cobrir todos os writers e aliases, não alegar exclusividade. |
| Aplicar escrita | Re-resolver path/alias/escopo e revalidar token corrente no boundary imediatamente antes do efeito. Processo antigo vivo com token vencido/revogado é recusado. Timeout/resultado desconhecido exige reconciliação; lease sozinho não garante cancelamento. |
| Registrar sinal | Evento comprovado e inbox/dedupe são persistidos antes de criar dispatch. Reordenação compara versão, revisão e chave lógica; evento atrasado não sobrescreve estado mais novo. |
| Enfileirar despacho | Reservar atomicamente no ledger global e persistir intenção/outbox no journal lógico C07 antes do efeito. Não se exige banco físico separado. Se reserva e intenção não compartilham transação, usar saga idempotente; nenhuma chamada sai sem ambos duráveis. Reserva e custo em voo continuam contabilizados até reconciliação; não afirmar commit conjunto não demonstrado. |
| Entregar sinal | Outbox pode reenviar após crash; consumidor usa inbox/idempotência. Estado entre aceitação remota e confirmação local pode ser `UNKNOWN`; consultar/reconciliar antes de repetir efeito não deduplicável. Exactly-once remoto não é prometido. |
| Rejeitar/limitar sinal | Deduplicata, recursão, hops/fan-out excedido ou orçamento indisponível gera registro durável suprimido/dead-letter; nenhum novo dispatch ultrapassa limite. |
| Replay | Requer comando explicitamente autorizado e nova validação de fonte, versão de assinatura, idempotência e C03. Persistir replay intent antes da chamada; falha após efeito remoto reconcilia, sem retry cego. |

Reorder não exige ordem global entre assinaturas independentes. Quando ordem for necessária para o mesmo objeto/versão, consumidor exige sequência/revisão monotônica e descarta ou reconcilia evento obsoleto. A outbox/inbox fornece entrega pelo menos uma vez, dedupe e recuperação local; não transforma MCP, runtime, watcher ou tool em transação confiável.

## Refinamento por PBI

| PBI | Aceite refinado desta frente | Dependências e limites |
|---|---|---|
| **PBI-06 — Identificar subgrafo afetado** | Para um objetivo, produz snapshot com projeto exato, comunidade/dependências/anchors que a superfície consultável comprova, versões/fonte e cobertura. Parcial/stale/unknown e gaps ficam explícitos; fallback reduz paralelismo/expande escopo ou bloqueia. Renome/drift torna referência pendente até revalidação. | C01/C06 e adapter MCP validado; serve de entrada a PBI-08/10, sem depender deles. Anchor extraction é parcial e não tem endpoint MCP demonstrado; não declarar cobertura completa por conveniência. |
| **PBI-07 — Claim exclusivo de trabalho** | Aquisição concorrente de recursos iguais ou sobrepostos tem um único vencedor; arquivo inteiro conflita com símbolo/range contido, ranges sobrepostos e aliases filesystem (case conforme filesystem, symlink e rename) são canonicalizados antes de avaliar conflito. Escopos distintos só coexistem com âncoras/ranges validados e comprovadamente não sobrepostos; referência ausente/ambígua assume arquivo inteiro. Conflito não autoriza edição. Lease, revogação, transferência e fencing rejeitam token antigo mesmo com processo vivo; boundary revalida alias/escopo antes da escrita. | C05/C06, PBI-06/15, Q04/ADR-005 e mediação de todos os writers. Claim Union não satisfaz PBI; sem fence comum, manter explicitamente bloqueado. |
| **PBI-08 — Compilar plano dependente** | Plano determinístico para a mesma tupla/política; o plano executável é DAG. Ciclos de entrada são identificados por SCC e agrupados em unidade serial bloqueada para replanejamento, preservando as dependências e permitindo componentes acíclicos independentes seguirem sem paralelismo inventado. Concorrência só quando cobertura sustenta independência, contratos não conflitam ou seam aprovado. Limite considera concorrência/orçamento global C03. | C01/C03/C06, PBI-06. Não usar contagem fixa de comunidades nem inferir independência de edge ausente. |
| **PBI-09 — Avaliar seam para contrato compartilhado** | Tarefas que tocam contrato comum não editam em paralelo sem evidência `COMPATIBLE` atual e vinculada às mesmas revisões; `CONFLICT` mantém tarefas agrupadas/serializadas para resolução e resultado ausente, stale ou inconclusivo não libera paralelismo. | C04/C06, PBI-06/08 e capacidade parcial de Harness da matriz 006. Sem evidência, não simular aceite de seam. |
| **PBI-10 — Integrar resultados paralelos** | Resultados/falhas são registrados por tarefa; reprovação parcial preserva trabalho independente aprovado. Integração do conjunto espera tarefas obrigatórias resolvidas, revalida versão e solicita evidência independente do conjunto; nenhuma promoção decorre da integração. | C01/C04/C06, PBI-08/09; verificador/autoridade da F04. Promoção segue exclusivamente C04. |
| **PBI-11 — Disparar por violação de seam** | Adversarial é acionado somente por evento de violação com fonte e evidência comprováveis, versão compatível e assinatura ativa; reindexação do watcher isolada não é violação. Falha da fonte gera não-dispatch/diagnóstico. | C04/C05/C06, PBI-09/13/14, Q04; o runtime e a prova de evento não estão assumidos. |
| **PBI-12 — Disparar por anomalia em trace** | Uma ingestão de trace pode alimentar análise; só anomalia produzida por regra/analisador cuja evidência e origem sejam consultáveis aciona assinatura. Evento correlaciona trace, regra e versão. | C01/C06, PBI-13/14; `ingest_traces` não é em si detector nem prova de anomalia. |
| **PBI-13 — Assinaturas configuráveis** | Operador autenticado define tipo/fonte/schema, filtro, evidência mínima, perfil alvo, limites e comportamento de falha; cada edição publica revisão imutável. Envelope enfileirado conserva revisão usada. Configuração inválida/sem autoridade não entra em vigor. | C05/C06 e schemas de sinais PBI-11/12. Perfil alvo não concede autoridade ou autentica identidade do agente; execução da assinatura depende separadamente de PBI-15/Q04. |
| **PBI-14 — Proteger contra tempestade** | Deduplica eventId e identidade lógica, limita loops/hops/fan-out/concorrência/fila por política explícita; todos dispatches/retries/replays reservam orçamento GLOBAL C03. Limite produz supressão/dead-letter durável; replay exige comando explícito, autorizado e revalidado. | C03/C05/C06/C07, PBI-11–13, PBI-02/03 e política válida Q05. Sem orçamento mensurável, novos despachos param; nenhum limite numérico é inferido. |

## Ordered Development Tasks

Tasks condicionais para decomposição futura; estimativas são sessões de trabalho aproximadas, não prazo prometido. Escopo indica artefatos lógicos propostos, não arquivos existentes. Implementação só após resolver dependências de capacidade/autoridade pertinentes e aprovar compromisso de entrega. São decomposição de F02/F03 e não criam pré-requisito para o slice inicial PBI-01–05/PBI-16 além dos bloqueios já declarados no PBB/DECISIONS.

```json
[
  {
    "id": "PE-01",
    "title": "Definir snapshot de topologia e cobertura",
    "description": "Preserva proveniência, revisões, anchors consultáveis e lacunas antes de compilar trabalho.",
    "pbis": ["PBI-06"],
    "estimate_hours": 3,
    "scope": ["AffectedSubgraphSnapshot", "TopologyReadPort", "CoverageState"],
    "acceptance": ["Snapshot sempre identifica projeto, fonte, revisões e gaps", "PARTIAL/STALE/UNKNOWN nunca são normalizados para COMPLETE"],
    "depends_on": null,
    "conditional_on": ["Adapter MCP e cobertura acessível mapeados; anchors permanecem condicionais à capacidade comprovada"]
  },
  {
    "id": "PE-02",
    "title": "Compilar plano DAG versionado",
    "description": "Produz tarefas e dependências a partir do snapshot sem inferir paralelismo por contagem de comunidades.",
    "pbis": ["PBI-06", "PBI-08"],
    "estimate_hours": 4,
    "scope": ["PlanSnapshot", "TaskDependency", "CompileWorkPlan"],
    "acceptance": ["SCC cíclica vira unidade serial bloqueada de replanejamento; componentes acíclicos não são descartados", "Independência não é inferida de edge ausente ou cobertura incompleta"],
    "depends_on": "PE-01",
    "conditional_on": ["Política de concorrência explícita e ledger global C03 disponível"]
  },
  {
    "id": "PE-03",
    "title": "Implementar claim com lease, sobreposição e fencing",
    "description": "Coordena recursos efetivos no store TS e impede escrita por conflito de escopo, alias ou token revogado.",
    "pbis": ["PBI-07"],
    "estimate_hours": 4,
    "scope": ["WorkClaim", "WorkClaimStorePort", "CanonicalResourceRef", "FencingToken"],
    "acceptance": ["Aquisição concorrente de escopos sobrepostos concede no máximo um token corrente", "Boundary revalida alias/escopo e transferência revoga token antigo antes de novo grant"],
    "depends_on": "PE-01",
    "conditional_on": ["Q04/ADR-005 comprova store compartilhado e mediação de todo caminho de escrita"]
  },
  {
    "id": "PE-04",
    "title": "Revalidar anchors após drift",
    "description": "Invalida escopo e claims quando path, baseline, âncora ou cobertura muda.",
    "pbis": ["PBI-06", "PBI-07"],
    "estimate_hours": 3,
    "scope": ["AnchorValidationPort", "RevalidatePlanSnapshot", "PlanSnapshotInvalidated"],
    "acceptance": ["Renome/drift não mantém âncora válida por suposição", "Sem capacidade de revalidação, plano reduz paralelismo ou bloqueia"],
    "depends_on": "PE-03",
    "conditional_on": ["Matriz 006 atualizada com superfície de validação acessível"]
  },
  {
    "id": "PE-05",
    "title": "Bloquear seam sem evidência atual",
    "description": "Exige evidência consultável para liberar trabalhos que tocam contrato comum.",
    "pbis": ["PBI-09"],
    "estimate_hours": 3,
    "scope": ["SeamEvidenceRef", "SeamEvidencePort", "EvaluateSharedSeam"],
    "acceptance": ["Somente evidência atual compatível libera a política de paralelismo", "Ausente/inconclusiva/stale mantém serialização"],
    "depends_on": "PE-02",
    "conditional_on": ["Capacidade parcial do Harness validada no adapter; sem equivalência semântica presumida"]
  },
  {
    "id": "PE-06",
    "title": "Integrar resultados de tarefas com verificação do conjunto",
    "description": "Preserva sucesso parcial e impede que composição incompleta seja tratada como candidata verificada.",
    "pbis": ["PBI-10"],
    "estimate_hours": 4,
    "scope": ["IntegrateTaskResults", "ParallelResultsIntegrated", "CandidateVersionRef"],
    "acceptance": ["Falha de tarefa não apaga resultado independente aprovado", "Integração não libera promoção sem veredicto C04 atual"],
    "depends_on": "PE-02",
    "conditional_on": ["Contrato de evidência C04 e port de verificação da frente F04"]
  },
  {
    "id": "PE-07",
    "title": "Validar origem de sinais de grafo",
    "description": "Normaliza somente sinais com semântica e evidência comprovadas em eventos acionáveis.",
    "pbis": ["PBI-11", "PBI-12"],
    "estimate_hours": 3,
    "scope": ["EventSourcePort", "ValidateGraphSignal", "EventEnvelope"],
    "acceptance": ["Watcher reindex isolado não gera violação de seam", "Trace ingerido sem anomalia provada não gera dispatch"],
    "depends_on": null,
    "conditional_on": ["Fonte e detector específicos validados; runtime de agente Q04 não assumido"]
  },
  {
    "id": "PE-08",
    "title": "Versionar assinaturas de evento",
    "description": "Mantém filtros, evidência e alvo imutáveis por revisão e autoridade do operador.",
    "pbis": ["PBI-13"],
    "estimate_hours": 3,
    "scope": ["EventSubscription", "SignatureRegistryPort", "SignatureRevisionRef"],
    "acceptance": ["Mudança cria revisão nova autorizada", "Entrega enfileirada conserva a revisão originalmente avaliada"],
    "depends_on": null,
    "conditional_on": ["Identidade/autorização C05 validada para quem publica; runtime de dispatch é pré-requisito apenas para ativar entrega"]
  },
  {
    "id": "PE-09",
    "title": "Persistir inbox e outbox deduplicadas",
    "description": "Torna a entrega repetível após crash sem duplicar o mesmo gatilho lógico.",
    "pbis": ["PBI-11", "PBI-12", "PBI-14"],
    "estimate_hours": 4,
    "scope": ["EventJournalPort", "EventDelivery", "LogicalDedupeKey"],
    "acceptance": ["Inbox compara eventId e chave lógica tipo+fonte+objeto+versão", "Outbox é durável no journal lógico C07, sem afirmar atomicidade remota nem exigir SQLite físico separado"],
    "depends_on": null,
    "conditional_on": ["Journal TS C07 e estratégia CAS/idemência disponíveis"]
  },
  {
    "id": "PE-10",
    "title": "Aplicar orçamento e limites globais ao dispatch",
    "description": "Debita no C03 antes de cada gatilho, retry ou replay e limita recursão, hops e fan-out.",
    "pbis": ["PBI-14"],
    "estimate_hours": 4,
    "scope": ["DispatchBoundEvent", "GlobalBudgetPort", "EventDeliveryLimits"],
    "acceptance": ["Nenhum dispatch ocorre sem reserva no ledger GLOBAL", "Loop/hops/fan-out excedido é suprimido ou dead-letterado com registro"],
    "depends_on": "PE-09",
    "conditional_on": ["Política explícita Q05 válida e ledger compartilhado C03; sem defaults inventados"]
  },
  {
    "id": "PE-11",
    "title": "Registrar dead-letter e exigir replay explícito",
    "description": "Preserva diagnóstico e permite nova tentativa autorizada, revalidada e debitada.",
    "pbis": ["PBI-14"],
    "estimate_hours": 3,
    "scope": ["GraphEventDeadLettered", "ReplayDeadLetter", "GraphEventReplayRequested"],
    "acceptance": ["Dead-letter nunca reenvia automaticamente", "Replay valida fonte/revisão/idempotência e reserva C03 antes do efeito"],
    "depends_on": "PE-10",
    "conditional_on": ["Permissão de Operador C05 e reconciliação pós-timeout de C07"]
  },
  {
    "id": "PE-12",
    "title": "Despachar evento ao perfil configurado",
    "description": "Entrega evento comprovado a runtime autorizado preservando correlação e status incerto.",
    "pbis": ["PBI-11", "PBI-12", "PBI-13", "PBI-14"],
    "estimate_hours": 4,
    "scope": ["AgentDispatchPort", "GraphEventDispatched", "EventDispatchReconciliation"],
    "acceptance": ["Perfil corresponde à assinatura e identidade autenticada", "Crash após aceitação remota reconcilia antes de retry não deduplicável"],
    "depends_on": "PE-10",
    "conditional_on": ["Spike Q04 comprova identidade, isolamento, cancelamento/medição relevantes e rota de dispatch"]
  }
]
```

`depends_on` indica o predecessor principal apenas. Pré-requisitos adicionais não criam uma cadeia artificial entre F02 e F03 e ficam explícitos aqui:

| Task | Pré-requisito adicional | Por quê |
|---|---|---|
| PE-05 | PE-04 antes de liberar paralelismo baseado em anchors; se a capacidade não existir, permanece serializado. | Seam precisa usar referências/coverage revalidadas. |
| PE-06 | PE-05 e contrato/verificador C04 disponíveis. | Integra resultados de plano após guardas de seam e solicita evidência do conjunto. |
| PE-10 | PE-07 e PE-08 para ativar despacho; configuração explícita e ledger global C03/Q05. | Limites podem ser modelados antes, mas evento só segue após fonte/assinatura e orçamento válidos. |
| PE-11 | PE-09 e permissão C05. | Dead-letter/replay usa journal e comando autenticado. |
| PE-12 | PE-07, PE-08 e PE-09, mais prova do runtime Q04. | Dispatch só ocorre com fonte, revisão e journal válidos; não depende do replay PE-11. |

## Cenários de teste especificados

Especificações, não testes executáveis. Cada cenário deriva dos contratos C01/C03–C07, das regras PE-R01–R20 e do aceite refinado dos PBIs. Os cenários Unit isolam regras puras; Integration valida o store/adapter correspondente; Functional atravessa a fronteira do pacote e só é executável quando fonte/runtime existirem. Cada PBI tem pelo menos um caminho nominal e um cenário de falha.

### Unit

<a id="pe-s01"></a>

#### PE-S01 — Should manter gaps de cobertura explícitos ao identificar o subgrafo

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-06; PE-R01/02/03.
- **Given:** resposta do snapshot contém revisão de grafo e coverage parcial/stale, com arquivos ou anchors não cobertos.
- **When:** `IdentifyAffectedSubgraph` normaliza a resposta.
- **Then:** snapshot preserva status, fonte, gaps e revisões; plano consumidor não recebe essa lacuna como prova de independência.

<a id="pe-s02"></a>

#### PE-S02 — Should bloquear escopo afirmativo quando a cobertura não pode ser consultada

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-06; PE-R02/03/10.
- **Given:** adapter retorna estado desconhecido ou não tem capacidade comprovada de resolver a âncora alterada.
- **When:** o compilador tenta declarar o subgrafo completo.
- **Then:** a declaração é recusada e o resultado exige fallback conservador, serialização ou bloqueio explícito.

<a id="pe-s03"></a>

#### PE-S03 — Should compilar tarefas independentes somente em DAG acíclico

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-08; PE-R04/05.
- **Given:** snapshot completo para os recursos relevantes e tarefas com dependências sem ciclo e sem contrato compartilhado não resolvido.
- **When:** `CompileWorkPlan` cria a versão do plano.
- **Then:** tarefas prontas são ordenadas por dependência e concorrência respeita política explícita C03.

<a id="pe-s04"></a>

#### PE-S04 — Should agrupar SCC cíclica e preservar tarefas independentes

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-08; PE-R04.
- **Given:** tarefas A e B dependem uma da outra, enquanto C é acíclica e independente.
- **When:** `CompileWorkPlan` detecta os componentes fortemente conexos.
- **Then:** A/B viram unidade serial bloqueada para replanejamento, suas arestas são preservadas e C não é descartada; nenhum membro do grupo cíclico é liberado até resolução explícita.

<a id="pe-s05"></a>

#### PE-S05 — Should emitir fencing token monotônico em claim transferido

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-07; PE-R07/09.
- **Given:** claim válido será transferido após revogar o dono anterior.
- **When:** regra de transferência produz novo claim.
- **Then:** novo token é maior, token anterior consta revogado e apenas novo dono pode solicitar escrita mediada.

<a id="pe-s06"></a>

#### PE-S06 — Should recusar token antigo após expiração enquanto processo antigo permanece ativo

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-07; PE-R08/09/10.
- **Given:** lease expirou, processo antigo continua ativo e o recurso recebeu token atual diferente.
- **When:** processo antigo tenta escrever com seu token anterior.
- **Then:** mediação recusa a escrita; expiração, por si, não é interpretada como confirmação de encerramento do processo.

<a id="pe-s07"></a>

#### PE-S07 — Should reter resultados independentes aprovados em falha parcial

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-10; PE-R12.
- **Given:** duas tarefas independentes concluíram com resultado aprovado e uma terceira falhou.
- **When:** `IntegrateTaskResults` registra falha parcial.
- **Then:** resultados independentes aprovados e seus diagnósticos permanecem; a tarefa falha continua explícita e o conjunto não é marcado verificado/promovido.

<a id="pe-s08"></a>

#### PE-S08 — Should não ativar dispatch com sinal sem fonte comprovada

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-11/12; PE-R13/15.
- **Given:** sinal bruto de watcher ou ingestão sem evidência validada de seam/anomalia.
- **When:** `ValidateGraphSignal` aplica uma assinatura compatível.
- **Then:** nenhuma mensagem de dispatch é criada; a falta de prova é registrada como motivo.

<a id="pe-s09"></a>

#### PE-S09 — Should manter revisão imutável da assinatura usada pelo evento

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-13; PE-R14/15.
- **Given:** evento enfileirado sob revisão N e assinatura atual alterada para N+1.
- **When:** o consumidor lê a entrega original.
- **Then:** entrega referencia N; revalidação explícita decide se pode seguir, sem trocar silenciosamente para N+1.

<a id="pe-s10"></a>

#### PE-S10 — Should detectar duplicata lógica mesmo com eventId diferente

- **Nível:** Unit.
- **PBI / regra fonte:** PBI-14; PE-R15/16.
- **Given:** dois envelopes têm UUIDs distintos, mas mesmo tipo, fonte, objeto e versão.
- **When:** a inbox calcula a chave de dedupe.
- **Then:** a segunda entrega é tratada como repetição lógica e não dispara novamente o mesmo consumidor.

### Integration

<a id="pe-s11"></a>

#### PE-S11 — Should invalidar plano quando renome altera caminho ou anchor

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-06/07/08; PE-R06/10.
- **Given:** plano persistido aponta para caminho e referência de anchor que mudou após renome/drift.
- **When:** `RevalidatePlanSnapshot` consulta fonte e coverage atuais.
- **Then:** plano incompatível e claims dependentes ficam invalidados/fenced; novo escopo não herda a referência antiga sem validação.

<a id="pe-s12"></a>

#### PE-S12 — Should conceder somente um claim em aquisições concorrentes

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-07; PE-R07/08/09/20.
- **Given:** duas solicitações concorrentes no store compartilhado reivindicam, uma, o arquivo inteiro e, outra, uma âncora/range contido nele.
- **When:** ambas tentam adquirir o claim.
- **Then:** compare-and-set concede apenas um token corrente; outra solicitação recebe conflito e nenhuma escrita ocorre sem o mediador.

<a id="pe-s13"></a>

#### PE-S13 — Should preservar plano anterior quando persistência do plano falha

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-08; PE-R04/06/20.
- **Given:** snapshot anterior foi committed; plano novo registra SCC bloqueada e componente acíclico, mas a gravação/outbox falha.
- **When:** compilação tenta gravar tarefas, dependências e outbox.
- **Then:** novo plano não fica parcialmente visível e o snapshot anterior mantém sua revisão até replanejamento válido.

<a id="pe-s14"></a>

#### PE-S14 — Should liberar paralelismo somente com seam compatível atual

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-09; PE-R05/11.
- **Given:** duas tarefas se referenciam ao mesmo contrato e adapter retorna evidência consultável `COMPATIBLE` ligada às revisões atuais.
- **When:** scheduler avalia sua elegibilidade paralela.
- **Then:** ambas podem ser liberadas conforme a política explícita de concorrência; se retorno for `CONFLICT`, ausente, inconclusivo ou stale, mantém serialização/agrupamento.

<a id="pe-s15"></a>

#### PE-S15 — Should preservar sucesso independente e exigir verificação do conjunto integrado

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-10; PE-R06/12/20.
- **Given:** saída de tarefas está persistida e uma falha parcial não altera os recursos independentes aprovados.
- **When:** store integra os resultados obrigatórios resolvidos e registra pendências.
- **Then:** saídas independentes continuam consultáveis; conjunto só vai para verificador C04 após tarefas requeridas resolvidas e versão revalidada; falha de commit não deixa integração parcial visível.

<a id="pe-s16"></a>

#### PE-S16 — Should recusar evento quando watcher somente reindexa ou trace só foi ingerido

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-11/12; PE-R13/20.
- **Given:** adapter recebe callback de reindexação ou trace ingerido sem evidência de violação/anomalia.
- **When:** `ValidateGraphSignal` tenta persistir inbox/outbox.
- **Then:** nenhuma outbox de dispatch é criada e a fonte insuficiente fica registrada; a mera chamada de watcher/ingestão não satisfaz a semântica do evento.

<a id="pe-s17"></a>

#### PE-S17 — Should recuperar entrega reordenada ou interrompida sem repetir efeito lógico

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-11/12/14; PE-R15/16/20.
- **Given:** duas entregas do mesmo objeto chegam fora de ordem, ou processo cai após aceitação do dispatch e antes da confirmação local.
- **When:** inbox/outbox processa e reconcilia a entrega após reinício.
- **Then:** versão antiga não sobrescreve estado mais novo; a chave lógica evita duplicata; status remoto incerto é consultado ou preservado como `UNKNOWN`, sem retry cego não deduplicável.

<a id="pe-s18"></a>

#### PE-S18 — Should registrar assinatura nova sem reescrever entrega pendente

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-13; PE-R14/20.
- **Given:** revisão N tem entrega pendente e operador autorizado publica N+1.
- **When:** registry grava nova assinatura junto à sua outbox local.
- **Then:** N+1 é atômica e imutável; pendência segue referenciada a N ou é explicitamente invalidada por política, nunca reinterpretada em silêncio.

<a id="pe-s19"></a>

#### PE-S19 — Should manter custo pendente no orçamento global antes de novo dispatch

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-14; PE-R17/20.
- **Given:** um dispatch já aceito remotamente caiu antes da medição/resultado e sua reserva continua em voo; o uso é desconhecido sem cobertura conservadora.
- **When:** outro gatilho ou retry do mesmo objetivo tenta reservar no ledger C03.
- **Then:** custo/reserva pendente não é liberado nem duplicado, novo dispatch fica suspenso e a intenção bloqueada é registrada; assinatura ou subagente não recebe teto próprio.

<a id="pe-s20"></a>

#### PE-S20 — Should bloquear tempestade recursiva e preservar dead-letter para replay manual

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-14; PE-R18/19/20.
- **Given:** evento originado por agente retorna ao mesmo gatilho acima do limite explícito de hops/fan-out.
- **When:** scheduler avalia propagação e operador depois comanda replay autenticado.
- **Then:** tempestade é suprimida/dead-letterada com diagnóstico; replay cria nova tentativa correlacionada após revalidar fonte, assinatura e idempotência e reservar o mesmo C03; não ocorre replay automático.

<a id="pe-s30"></a>

#### PE-S30 — Should detectar aliases do filesystem e conflito de ranges sobrepostos

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-07; PE-R07/08/21.
- **Given:** dois pedidos concorrentes referenciam o mesmo arquivo por caminho real e symlink, com ranges sobrepostos; a semântica de case corresponde ao filesystem ativo.
- **When:** store canonicaliza os recursos na aquisição e o boundary revalida o path antes da escrita.
- **Then:** no máximo um claim é concedido; rename/symlink ambíguo invalida a referência antiga e nenhum writer com alias/token antigo alcança o recurso.

<a id="pe-s31"></a>

#### PE-S31 — Should reservar um único orçamento global sob dispatches concorrentes

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-14; PE-R17/20.
- **Given:** dois gatilhos concorrentes de agentes/assinaturas diferentes no mesmo objetivo têm saldo C03 validado para apenas uma reserva.
- **When:** ambos tentam reservar atomicamente antes de criar a intenção durável.
- **Then:** ledger concede no máximo uma reserva; o outro dispatch não sai, não recebe saldo duplicado e sua supressão/bloqueio fica auditável.

### Functional

<a id="pe-s21"></a>

#### PE-S21 — Should compilar plano conservador para alteração com coverage parcial

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-06/08; PE-R01/02/03/05.
- **Given:** Operador pede mudança cuja consulta retorna cobertura parcial/stale e dependências não cobertas.
- **When:** pacote consulta snapshot e tenta compilar trabalho.
- **Then:** plano expõe lacunas e usa escopo/serialização conservadora ou bloqueia; nenhum agente recebe tarefa como se independência estivesse provada.

<a id="pe-s22"></a>

#### PE-S22 — Should recusar escrita de agente antigo após transferência do claim

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-07; PE-R07/08/09/10.
- **Given:** claim expirou e foi transferido para outro agente com token maior, enquanto processo antigo ainda executa.
- **When:** antigo agente tenta editar o arquivo/anchor pela fronteira mediada.
- **Then:** escrita com token antigo é recusada e só o port com token atual pode avançar; se uma rota não mediada alcançar o recurso, o critério de exclusividade falha.

<a id="pe-s23"></a>

#### PE-S23 — Should manter entrega conjunta sem promoção quando uma tarefa reprova

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-09/10; PE-R11/12.
- **Given:** plano inclui tarefas paralelas independentes e tarefas serializadas por seam, com uma falha parcial.
- **When:** resultados são integrados ao candidato corrente.
- **Then:** trabalho independente aprovado permanece; tarefas/contratos não resolvidos ficam identificados e o candidato não é tratado como verificado nem promovido sem verificador independente C04.

<a id="pe-s24"></a>

#### PE-S24 — Should despachar somente violação de seam comprovada à assinatura configurada

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-11/13; PE-R13–16.
- **Given:** commit tem evidência consultável de violação de contrato e assinatura ativa para fonte/schema/revisão e perfil permitido.
- **When:** evento validado é entregue ao dispatcher.
- **Then:** uma solicitação correlacionada é enfileirada com revisão e reserva; reindexação sem a evidência equivalente não aciona Adversarial.

<a id="pe-s25"></a>

#### PE-S25 — Should não despachar anomalia de trace sem detector e evidência validados

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-12; PE-R13/15.
- **Given:** traces foram ingeridos, mas a regra de detecção não produziu referência verificável para anomalia.
- **When:** fluxo de evento avalia assinatura de trace.
- **Then:** não há dispatch e o resultado distingue ingestão de evidência de anomalia; nenhum alerta de negócio é inferido da ingestão isolada.

<a id="pe-s26"></a>

#### PE-S26 — Should deduplicar tempestade e exigir replay com orçamento

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-14; PE-R14–19.
- **Given:** evento elegível é reentregue por outbox e gera cadeia recursiva além do limite, com uma dead-letter resultante.
- **When:** Operador autorizado solicita replay após corrigir configuração/fonte.
- **Then:** eventId e chave lógica impedem repetição automática; replay é nova tentativa auditada, validada contra revisão apropriada e debitada do orçamento GLOBAL C03 antes do despacho.

<a id="pe-s27"></a>

#### PE-S27 — Should despachar anomalia de trace depois de evidência validada

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-12/13; PE-R13–17.
- **Given:** analisador configurado encontra anomalia em trace ingerido e produz referência consultável, origem autenticável, versão e revisão de regra compatíveis com uma assinatura ativa.
- **When:** `ValidateGraphSignal` valida a anomalia e o scheduler reserva orçamento C03 para o perfil autorizado.
- **Then:** outbox registra um dispatch correlacionado ao trace, à assinatura e à evidência; a ingestão isolada, sem esse resultado do analisador, não o produziria.

<a id="pe-s28"></a>

#### PE-S28 — Should recusar publicação de assinatura sem autoridade ou contrato válido

- **Nível:** Integration.
- **PBI / regra fonte:** PBI-13; PE-R14/15/20.
- **Given:** assinatura proposta por identidade sem autoridade de Operador, com schema desconhecido ou origem sem adapter comprovado.
- **When:** `RegisterEventSubscription` tenta persistir a revisão.
- **Then:** nenhuma revisão ativa nem outbox de publicação é gravada; a revisão anterior permanece íntegra e o motivo da recusa fica auditável.

<a id="pe-s29"></a>

#### PE-S29 — Should solicitar verificação para conjunto integrado quando todas tarefas obrigatórias passam

- **Nível:** Functional.
- **PBI / regra fonte:** PBI-10; PE-R06/12.
- **Given:** todas as tarefas obrigatórias do plano atual concluíram, revisão/claims continuam válidos e nenhuma lacuna obrigatória permanece.
- **When:** Operador integra os resultados do plano.
- **Then:** candidato composto é vinculado à versão atual e enviado à verificação independente C04; integração não chama promoção nem declara o conjunto aprovado antes do veredicto.

## Gaps mantidos para consolidação

- Anchor extraction/revalidation ainda não tem endpoint MCP demonstrado; PBI-06/07 dependem de capacidade acessível validada. Não substituir isso por `get_ast_anchors` fictício nem por recall interno.
- Exclusividade de claim exige mediação/fencing cobrindo toda escrita e processo antigo, store comum e prova de Q04. `union_claim_capture/resolve`, sessão Union e `restricted` não satisfazem lock, RBAC ou exclusão de AST.
- Identificação confiável de contrato compartilhado e seam é capacidade parcial. Enquanto a prova não for suficiente e atual, a regra é serializar/agrupar ou bloquear paralelismo.
- Watcher comprova reindexação, não semântica de violação; ingestão de trace não comprova anomalia. F03 requer fonte e detector validados por sinal.
- Outbox/inbox dá entrega pelo menos uma vez e recuperação local; não garante ordem global, exactly-once remoto ou transação atômica com MCP/runtime. Resultado remoto em janela de crash pode ficar `UNKNOWN`.
- Dispatch e replay dependem de identidade/autoridade e host Q04. Todos compartilham C03; valores de orçamento Q05 permanecem Deferred e não há defaults numéricos.
- A integração PBI-10 apenas compõe resultados para verificação do conjunto; evidência, decisão e promoção seguem C04 e a frente de Autoridade e Decisão.
- Não foram implementados código, schema de produção, tasks, teste, spike ou alteração de ADR/PBB/briefing. Nenhum comportamento especificado nesta frente foi comprovado em runtime.
