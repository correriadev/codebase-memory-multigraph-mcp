# Contratos compartilhados — proposta de refinamento

Data: 2026-10-23. Status: desenho `model`, não API implementada. Regras INV-01–08: [DECISIONS.md](DECISIONS.md). Vocabulário final: [001](001-problem-space.md). Integrações: [002](002-context-map.md). Um nome nesta página é contrato lógico TS; nomes de tools reais ficam na [matriz de capacidades](006-capability-matrix.md).

## C01 — Identidade e versão candidata

| Campo | Semântica / validação |
|---|---|
| `projectId` | Projeto exato resolvido no CBM; nunca inferir identidade só pelo basename |
| `runId` | Execução de um objetivo; novo ciclo após crash pré-F05 recebe novo ID e `parentRunId` |
| `taskId`, `attemptId` | Tarefa do plano e tentativa única; numeração não é identidade global |
| `agentId`, `profile` | Identidade emitida pelo host confiável; perfil builder/adversarial/gatekeeper/meta, não autodeclarado pelo LLM |
| `sessionId`, `horizonId` | Sessão Union e horizonte candidato têm vínculos explícitos; não presumir que todo overlay é sessão Union |
| `candidateVersion` | Identificador imutável que vincula projeto, horizonte, conteúdo/patch de código e revisões de conhecimento usadas |
| `artifactDigest` | Hash do conteúdo candidato completo e do seu baseline; não apenas hash do diff textual ou geração do grafo |
| `graphGeneration`, `rulesRevision`, `coverageRevision` | Revisões usadas na avaliação; alteração relevante invalida avaliação e exige revalidação |
| `policyRevision`, `contractVersion` | Configuração de risco/autoridade e versão dos schemas do orquestrador |

Qualquer alteração de conteúdo cria nova versão candidata. Vínculo de evidência exige igualdade exata da tupla registrada, além de checagem de atualidade antes da promoção. Digest é identificador de integridade, não prova de qualidade. O formato canônico de serialização/hash será fechado no spike; comparação de versões nunca depende de ordem arbitrária de JSON.

## C02 — Execução, estados e encerramento

Estados de execução propostos: `CREATED`, `RUNNING`, `PAUSED`, `COMPLETED`. `phase` detalha `PLANNING`, `BUILDING`, `VERIFYING`, `AWAITING_DECISION`, `PROMOTING`, `RECOVERING`, `CLEANING_UP`. Motivo terminal separado: `ACCEPTED`, `REJECTED`, `VERIFIER_UNAVAILABLE`, `BUDGET_EXHAUSTED`, `CANCELLED`, `FAILED`, `RECOVERY_REQUIRED`. `COMPLETED` não significa promoção nem aceitação.

Entrada em qualquer slice de produção exige o gate `S0_APPROVED` definido em 003. Concluir SP-05 significa registrar o resultado do spike, inclusive uma reprovação; não satisfaz o gate. Os cinco critérios precisam de PASS explícito, invariantes verificadas e configuração correspondente à evidência, além do ADR-005 ACCEPTED por decisão apropriada. FAIL, inconclusão, indisponibilidade, campo ausente ou configuração divergente bloqueiam a entrada. O estado atual continua pendente: este refinamento não executou o spike nem aceitou o ADR.

| Transição | Pré-condição | Registro durável |
|---|---|---|
| CREATED → RUNNING | Configuração validada, capacidade mínima/autoridade disponíveis, reserva inicial | Objetivo, política, orçamento e baseline |
| RUNNING → PAUSED | Circuit breaker, alto risco, incerteza material ou dependência indisponível | Motivo e pacote de decisão; impedir novos efeitos |
| PAUSED → RUNNING | Decisão aplicável à versão atual e orçamento ainda válido | Identidade decisora, comando e transição compare-and-set |
| RUNNING/PAUSED → COMPLETED | Motivo final determinado, efeitos em voo reconciliados ou explicitamente incertos | Relatório final e consumo; outcome de promoção separado |

Em qualquer interrupção: parar novos despachos, cancelar processos sob controle, reconciliar chamadas em voo e registrar incerteza residual. Cancelamento não prova ausência de efeito remoto. Relatório tem `terminationReason`, versões, veredictos, iterações, custo conhecido/estimado/pendente, efeitos incertos e referências de evidência. Falha ao persistir decisão impede promoção e exige reconciliação.

Pré-F05: nenhum checkpoint autoriza retomada automática; diagnosticar e iniciar nova execução desde objetivo com vínculo anterior. Isso não permite repetir automaticamente efeitos incertos: continua INV-04. Pós-F05: replay de comandos apenas pela política C07 e evidência de estado externo.

## C03 — Orçamento compartilhado

`BudgetPolicy`: teto de tokens, custo com moeda/unidade, tentativas e tempo; timeout por chamada; limite de concorrência; limiar/janela do breaker; política de cobrança em voo. Valores numéricos Q05 pendentes. Proposta segura para implementação: exigir política explícita validada em vez de inventar defaults.

`BudgetLedger`: consumo confirmado + reservas em voo + estimativas conservadoras identificadas. Reservar atomicamente ANTES de despachar; todos os agentes, gatilhos e retries debitam a execução/objetivo correspondente. Não conceder a cada subagente o teto inteiro. Reconciliar resultado e liberar somente saldo não consumido; registro duplicado não debita duas vezes. Dados de uso ausentes deixam consumo desconhecido e suspendem novos despachos sem cobertura conservadora. Taxas devem ter origem e versão; moeda não se soma a tokens.

Ao alcançar teto: bloquear novas ações, solicitar cancelamento das pendentes e contabilizar custo residual. Não prometer teto financeiro rígido se o host não fornece limite de gasto/cancelamento efetivo; registrar exposição máxima estimada e bloquear a autonomia se nem isso puder ser limitado. Tempo usa relógio monotônico durante processo e timestamps duráveis para reconstrução; não estender orçamento por reinício.

## C04 — Evidência e decisão de promoção

`EvidenceRecord`: `evidenceId`, tupla C01, origem autenticada, papel produtor, tipo (teste, revisão, cobertura, efeito, seam), comando/ambiente quando executável, início/fim, resultado `PASS|FAIL|INCONCLUSIVE|UNAVAILABLE`, digest do artefato e referência consultável. Produção e validação são separadas. Declarações `tdd_oracles_passed`/`advisor_invoked` não substituem saída do executor.

`IndependentVerdict`: tupla C01, identidade de verificador diferente do Builder, status `GREEN|RED|INCONCLUSIVE|UNAVAILABLE`, razões e evidências. `GREEN` exige conclusão de verificações aplicáveis; timeout nunca vira verde.

`PromotionDecision`: identidade confiável, versão, evidências, revisão de política, resultado `ALLOW|DENY|ESCALATE`, motivo, instante e chave idempotente. Gatekeeper é componente determinístico. Permissão humana não dispensa revisão independente verde, âncoras válidas nem gate C. Fast path requer baixo risco comprovado mais todas essas condições.

Fluxo: congelar versão → verificar independente → avaliar risco/decisão humana se exigida → revalidar tupla sob exclusão de mutações → registrar intenção de promoção → invocar tool real pelo canal autorizado → confirmar estado CBM → registrar resultado. Se versão/revisão muda no intervalo, recusar e refazer avaliação. Se o transporte não permite comprovar exclusão/atualidade, não promover; o spike deve validar essa fronteira. Timeout após chamada é efeito incerto: consultar estado, não promover novamente às cegas.

## C05 — Autoridade e fronteira de confiança

| Perfil | Permissão lógica | Proibições mínimas |
|---|---|---|
| Builder | Ler contexto autorizado; editar somente escopo de trabalho alocado; produzir candidato | Promover, conceder autoridade, emitir veredicto independente para seu candidato, ativar evolução |
| Adversarial | Ler candidato congelado/evidências; produzir contestação/veredicto | Alterar candidato, promover, remover bloqueio próprio sem evidência |
| Gatekeeper | Validar decisão/evidências e invocar promoção existente | Gerar código ou confiar em autoatribuição de papel |
| Meta-Agent | Ler auditoria autorizada; propor evolução | Ativar, aprovar ou substituir sua própria proposta |

Proposta D02: proxy/host confiável com identidade vinculada ao processo e autorização no despacho de cada chamada, incluindo chamadas diretas por nome. Remover tool da lista é só descoberta. Processos separados exigem controle real de transporte, credenciais, endpoints, arquivos e subprocessos; Builder não recebe o canal direto do CBM nem material de operador. Sem impedir caminho alternativo no ambiente controlado, PBI-16 não está entregue. Não presumir isolamento contra operador malicioso fora dessa fronteira.

Core C permanece sem alteração nesta proposta. Se mediação externa não garantir a propriedade, registrar gap e solicitar decisão de escopo; não declarar que `restricted`, intent scope ou um prompt resolveram a autorização. Overrides legados não podem estar habilitados no ambiente de autonomia; provar isso na validação, sem alterar ambiente atual nesta etapa.

## C06 — Plano, claims de trabalho e eventos

`PlanSnapshot`: versão C01 + tarefas, dependências, escopo por arquivo/âncora, limites de concorrência e lacunas de cobertura. Comunidades sugerem decomposição, não independência. Ciclos ou contratos compartilhados exigem serialização/agrupamento explícitos. Replanejamento invalida alocações incompatíveis e a avaliação integrada anterior.

`WorkClaim` é recurso TS, distinto de claim documental Union: `claimId`, dono, recurso canônico (arquivo+âncora+baseline), modo de acesso, prazo/lease e token de fencing monotônico. Aquisição atômica; conflito impede edição; renovação/abandono/transferência revogam capacidade antiga antes de conceder nova. Expirar lease não prova que processo parou: cercar escrita pelo token atual. Renome, remoção e âncora com drift exigem revalidação; arquivo inteiro é fallback conservador, não prova de independência. Não prometer lock distribuído entre coordenadores sem store e fencing compartilhados.

Conflito é por sobreposição do recurso efetivo, não somente igualdade de uma chave: claim de arquivo inteiro conflita com âncoras nele, ranges sobrepostos conflitam e aliases do mesmo arquivo (normalização de separadores/case conforme filesystem, symlink ou renome) não concedem duas exclusividades. A identidade canônica é validada na aquisição e no boundary de escrita; ambiguidade exige escopo conservador/serialização ou bloqueio. Esses controles detalham a exclusividade já exigida por PBI-07; sua implementação e a mediação de todos os writers continuam por provar.

Envelope de evento TS: `eventId`, `type`, `schemaVersion`, `projectId`, `runId` quando aplicável, `candidateVersion` quando aplicável, `occurredAt`, `recordedAt`, `source`, `causationId`, `correlationId`, `originAgentId`, `hopCount`, referência de evidência, chave de deduplicação e revisão de assinatura. Não enviar contexto conversacional ou segredo no payload.

Entrega pelo menos uma vez com inbox deduplicada e outbox transacional local; não alegar exactly-once remoto. Dedupe lógico considera tipo+fonte+objeto+versão, não só UUID. Assinaturas têm revisão, perfil alvo, limites, filtro e política de falhas. Recursão/hops, fan-out, fila e orçamento são limitados. Dead letter preserva diagnóstico; replay é explícito, validado e debitado. Fonte watcher/anomalia exige adapter com prova de evento; simples reindexação não equivale a violação de seam.

## C07 — Journal, checkpoint e auditoria

Journal em SQLite TS separado, sem escrita no banco C. Estado e outbox local são gravados na mesma transação; CBM e tools externas não participam dessa atomicidade. Atualização usa versão otimista/compare-and-set e chave de comando idempotente. Um efeito externo segue intenção durável → despacho → resultado durável. Janela entre efeito e resultado é `UNKNOWN`, nunca preenchida por inferência.

Checkpoint F05 contém entradas/saídas necessárias, referências de mensagens/tools, árvore de tentativas, versões, claims e mapa de efeitos `PENDING|CONFIRMED|FAILED|UNKNOWN`, sem prometer restaurar o raciocínio interno do host. Conteúdo sensível é minimizado/redigido; anexos autorizados têm controle de acesso e retenção separada de metadados. Redação não deve destruir informação necessária para detectar duplicata; usar referência/digest seguro.

Recuperação: validar journal → consultar estado CBM → confirmar efeitos consultáveis/deduplicáveis → reconstruir somente trabalho seguro → escalar incerteza material. Divergência de território/versão impede promoção. Descarte: congelar → exportar/persistir auditoria → confirmar descarte do conhecimento via capacidade suportada → invalidar checkpoints retomáveis → registrar tombstone; cada passo repetível. Falha parcial permanece `CLEANING_UP` ou bloqueada com diagnóstico. `union_session_close` não é API geral de descarte de qualquer horizonte. Retenção Q07 pendente; registros ainda requeridos pela retrospectiva ficam protegidos.

## C08 — Decisão humana, evolução e medição

Pacote humano: objetivo/mudança, versão, evidências, razão de risco, impacto e referência de navegação. Link exato por nó/aresta só é marcado funcional após validação na UI existente. Link de projeto ou coordenada textual não satisfaz sozinho PBI-25.

Comandos humanos autenticados: aprovar, rejeitar, solicitar correção. Compare-and-set por versão/estado; correção cria versão nova e invalida aprovação anterior. Aprovação registra intenção, nunca uma promoção automática sem C04.

Proposta de evolução: ID/versão, registros de falha fonte, hipótese, alvo (tema, regra local ou contrato de skill), artefato/digest, resultado do attestor quando aplicável, baseline e evidência de não-regressão, aprovação, ativação e versão anterior reversível. Estados propostos `PROPOSED→VALIDATED→APPROVED→ACTIVE→REVERTED`; qualidade reprovada ou capacidade de ativação ausente impede progressão. Habilitar skills deferred é marco de capacidade, separado de provar qualidade de uma proposta. Rollback restaura versão ativa anterior, não desfaz efeitos históricos.

Ativação e reversão carregam targetId, expectedTargetVersion, desiredTargetVersion e operationId. A comparação da versão no destino e sua troca devem ser atômicas. Na ativação, a versão esperada é o baseline aprovado; na reversão, é a versão instalada por aquela ativação, não simplesmente a versão lida no momento do pedido. Drift recusa com motivo TARGET_VERSION_CONFLICT, sem sobrescrever a versão corrente. Sem CAS no destino, só aceitar serialização confiável por alvo que cubra todos os escritores durante comparação e escrita; uma trava apenas no journal TS ou leitura seguida de escrita não basta. Sem uma dessas garantias, bloquear a operação. Confirmação posterior não substitui essa guarda nem desfaz uma atualização concorrente já perdida.

Telemetria mínima nasce do journal/outbox e é correlacionada por C01; relatório deve funcionar sem serviço externo de métricas. Dados de uso têm qualidade `MEASURED|ESTIMATED|UNKNOWN` e origem. Comparação usa protocolo pré-registrado, tarefas/aceite equivalentes e todos os resultados, incluindo falhas/retries/custos de revisão. Sem limiares humanos Q08, produzir medição descritiva, nunca alegar superioridade ou sucesso de negócio.

Cobertura de telemetria e comparação declara capacidades, métricas e população efetivamente incluídas. Medir S1 não depende de F07 ainda futura e só sustenta conclusões sobre S1; habilitar uma capacidade exige incorporar seu produtor e revalidar a cobertura correspondente. Um alvo restrito não conclui PBI-31 integral nem remove os produtores previstos na visão F01–F08. A entrega completa de PBI-31 exige todos os produtores requeridos pelo produto final, incluindo evolução; fontes habilitadas não podem ser omitidas para declarar completude global.

## Responsabilidade por mudanças

Somente CTO altera este contrato na consolidação. Frentes podem detalhar internamente, mas não redefinir IDs, estados, motivos, tupla de versão ou permissões sem propor alteração explícita. Especializações precisam mapear-se a C01–C08. Contratos não resolvem Q04–Q08; tornam suas dependências testáveis.
