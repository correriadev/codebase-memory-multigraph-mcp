# Desenho tático — Execução e Recuperação

**Domínio:** Graph Engineering  
**Projeto:** `codebase-memory-multigraph-mcp`  
**Contexto:** Execução (pacote proposto `packages/cbm-orchestrator/`, TypeScript)  
**Status:** refinamento `model`; nenhum contrato abaixo é API implementada  
**Data:** 2026-10-23

Este suplemento detalha F01 e F05 para consolidação posterior em 003/004. Ele toma `001-problem-space.md`, `002-context-map.md` e C01–C08 de `005-shared-contracts.md` como entradas; não altera esses contratos. O pacote TypeScript e seus caminhos são propostas. O core C/SQLite permanece inalterado e é consumido por MCP. Os nomes de tools e limites observados ficam em `006-capability-matrix.md` e `ORIENTATION.md`; a matriz é inspeção estática, não prova de execução.

## Fronteira e PBIs

| Dentro desta frente | Resultado especificado |
|---|---|
| F01 — PBI-01–05 | Ciclo mínimo com um Builder, verificação independente, iteração, orçamento, breaker, cancelamento e relatório final auditável. |
| F05 — PBI-19–22 | Checkpoint vinculado ao território, retomada idempotente quando F05 existir, diagnóstico da árvore de tentativas e descarte coordenado com preservação de auditoria. |
| Dependências transversais | PBI-16 é pré-condição mínima do slice: o Builder não pode promover. O subconjunto de telemetria de PBI-31 necessário a custo, veredictos e orçamento é registrado pelo journal/outbox. Não se refinam aqui PBI-15/17/18 nem F08 completo. |
| Fora da fronteira | Planejamento/topologia, avaliação de risco, decisão completa de promoção, eventos externos do grafo, evolução e comparação de valor. A frente registra apenas os vínculos e bloqueios que afetam execução/recuperação. |

As tasks abaixo são **condicionais** e não autorizam implementação. Antes de comprometer o slice, Q04 e o mapeamento de capacidades precisam ser resolvidos conforme PBB/DECISIONS. Q04–Q08 continuam Deferred; nenhum número, fornecedor, prazo de retenção ou capacidade é inferido.

## Refinement Questions and Answers

| ID | Categoria | Pergunta | Recomendação registrada | Resposta final / fonte |
|---|---|---|---|---|
| Q04 | Integração | Qual contrato inicia, cancela, isola, mede e recupera agentes? | Especificar e validar o contrato mínimo por um spike; mediação confiável é proposta e não RBAC existente. | Unresolved / Deferred. ADR-005 permanece PROPOSED; PBB, DECISIONS e ORIENTATION. |
| Q05 | Orçamento | Quais são os defaults e limites do loop? | Exigir política explícita e válida antes de autonomia; definir valores somente depois de medições. | Unresolved / Deferred; não há valores numéricos nem defaults humanos. PBB e DECISIONS. |
| Q07 | Retenção | Por quanto tempo manter auditoria após descarte? | Tornar a retenção configurável e proteger registros ainda não consumidos pela retrospectiva. | Unresolved / Deferred; sem prazo inferido. PBB e DECISIONS. |

### Spike Q04 — protocolo a especificar, não executar nesta etapa

ADR-005 segue **PROPOSED**. O experimento futuro deve registrar ambiente, host/versão, configuração, comandos, entradas, logs, journal/estado CBM antes e depois, resultado e limitações. Seus cinco critérios permanecem: ciclo ponta a ponta com decisão registrada; kill/reinício consistente; cancelamento efetivo e registrado; custo persistido; recusa observável fora do intent scope. Acrescentar tentativas diretas de promoção pelo Builder, mudança de versão entre avaliação e ação, e indisponibilidade do verificador. Nenhum critério é considerado aprovado aqui. Se a barreira não puder ser mediada fora do core, devolver o gap para decisão de escopo; não expandir C silenciosamente.

## Regras de execução e recuperação

As regras são propostas locais mapeadas a C01–C08 e INV-01–08. Não são nova API nem evidência de capacidade.

| ID | Regra | Base |
|---|---|---|
| ER-R01 | Um ciclo registra `projectId`, `runId`, objetivo, política, baseline e tupla de versão conforme C01. IDs vêm de identidade confiável. Mudança no conteúdo ou em revisão relevante cria `candidateVersion` nova e invalida veredicto/aprovação anteriores. | C01, PBI-01/04 |
| ER-R02 | Autonomia só começa com configuração explícita válida, capacidade mínima e autoridade disponíveis. Q04 e Q05 não definem hosts, números ou defaults; sem orçamento mensurável/limites válidos não há despacho. | C02/C03, INV-07, PBI-01/02 |
| ER-R03 | `state` usa exatamente `CREATED`, `RUNNING`, `PAUSED`, `COMPLETED`; `phase` é campo distinto e usa exatamente `PLANNING`, `BUILDING`, `VERIFYING`, `AWAITING_DECISION`, `PROMOTING`, `RECOVERING`, `CLEANING_UP`. `terminationReason` é separado e usa C02. Transições de estado permitidas: `CREATED→RUNNING`, `RUNNING→PAUSED`, `PAUSED→RUNNING` e `RUNNING/PAUSED→COMPLETED`, sujeitas às pré-condições C02. Fases não criam estados adicionais. | C02 |
| ER-R04 | Builder produz o candidato; só então o host captura e sela sua `candidateVersion` antes da verificação independente. Cada veredicto fica vinculado à tupla C01 dessa versão selada. Correção produz nova candidata e nova selagem antes de reverificar. `COMPLETED` não significa aceitação nem promoção; o motivo terminal e o outcome de promoção são campos distintos. | C01/C02/C04, PBI-01/04/05 |
| ER-R05 | Verificador verde, vermelho, inconclusivo ou indisponível tem resultado explícito. Indisponibilidade/inconclusão nunca vira verde nem autoriza promoção. Builder não recebe rota de promoção: host/proxy confiável vincula identidade ao processo e autoriza no despacho de cada chamada, inclusive tool chamada diretamente por nome e caminhos alternativos ao proxy. `restricted`, ocultar tool ou separar processos sem controle efetivo de transporte, credenciais, endpoints, arquivos e subprocessos não prova RBAC nem fronteira segura. A proposta TypeScript não altera core C; todos os caminhos precisam ser provados no spike. Se algum caminho alternativo continuar alcançável, registrar gap e bloquear autonomia que dependa da barreira. | C04/C05, INV-01/02, PBI-01/16 |
| ER-R06 | `BudgetPolicy` contém tetos/unidades de tokens, custo, tentativas e tempo, timeout, concorrência, breaker e cobrança em voo. `BudgetLedger` reserva atomicamente antes de cada despacho; agentes, retries e chamadas atribuíveis à execução/objetivo compartilham consumo e reserva. Tokens e moedas não se somam. | C03, PBI-02/03 |
| ER-R07 | Ao atingir teto, bloquear novas ações, solicitar cancelamento do trabalho controlável e contabilizar consumo pendente/residual. Reconciliar uso e liberar apenas saldo não consumido. Uso desconhecido suspende novos despachos sem limite conservador; não prometer teto financeiro rígido quando host/runtime não o impõe. | C03, INV-07, PBI-02 |
| ER-R08 | O breaker compara falhas da mesma classe ao limiar/janela configurados e, ao disparar, pausa e escala com motivo/diagnóstico duráveis. Não se define limiar, janela ou comportamento padrão; falhas de classes diferentes não são agregadas como se fossem iguais. | C02/C03, PBI-03 |
| ER-R09 | Veredicto vermelho alimenta a próxima iteração do Builder com evidência e versão identificadas; qualquer correção cria nova versão e exige nova verificação. Só a conclusão verde das verificações aplicáveis pode encerrar como aceita; orçamento, breaker e dependências continuam podendo interromper o loop. | C01/C04, PBI-04 |
| ER-R10 | O relatório final registra motivo terminal, versões, veredictos, iterações, evidências, custos conhecidos/estimados/pendentes e efeitos incertos. Dados ausentes mantêm qualidade `UNKNOWN`; falha ao persistir decisão/relatório impede declarar conclusão durável ou prosseguir para promoção. | C02/C03/C04/C08, PBI-05/31 |
| ER-R11 | O journal é SQLite próprio do pacote TS, fonte da execução, separado do grafo/território CBM e sem escrita no banco C. Estado e outbox local são atômicos entre si; versões usam compare-and-set/controle otimista e comando tem chave idempotente. Não existe transação distribuída com MCP, host ou tool externa. | C07, INV-03 |
| ER-R12 | Efeito externo obedece intenção durável → despacho → resultado durável. A lacuna entre efeito e resultado local é `UNKNOWN`. Após crash, consultar estado externo ou deduplicar se a capacidade existir; se não puder confirmar nem deduplicar, tratar como ocorrido, não repetir às cegas e escalar se material. | C07, INV-04, PBI-20 |
| ER-R13 | Antes de F05, crash não retoma checkpoint automaticamente: diagnosticar o ciclo, registrar a anomalia e iniciar novo `runId` desde o objetivo com `parentRunId`. Estado/território inconsistente só é isolado ou descartado por capacidade suportada; sem ela, bloquear retomada autônoma. A nova execução não pode repetir efeito incerto. A fase `RECOVERING` descreve reconciliação, não uma nova transição de estado. | C01/C02/C07, PBI-05/19/20 |
| ER-R14 | Checkpoint F05 liga território/horizonte, run/candidato, referências necessárias de mensagens e tools, árvore de tentativas, claims e mapa de efeitos `PENDING|CONFIRMED|FAILED|UNKNOWN`. Não promete restaurar raciocínio interno do host; minimiza conteúdo sensível sem apagar a referência necessária à deduplicação. | C07, PBI-19/21 |
| ER-R15 | Pós-F05, retomada valida journal e tupla, consulta estado CBM, reconcilia efeitos consultáveis/deduplicáveis e reconstrói somente trabalho seguro. Divergência de território/versão bloqueia promoção; `UNKNOWN` material pausa para decisão. Replay segue chave/comando idempotente e política C07. | C01/C02/C07, PBI-20 |
| ER-R16 | Cancelamento para novos despachos, solicita cancelamento de processos sob controle e reconcilia chamadas em voo. Cancelar não prova ausência de efeito remoto. O ciclo só conclui com efeitos reconciliados ou incerteza explicitamente registrada; motivo terminal `CANCELLED` não significa rollback externo. | C02/C03/C07, PBI-02/05/20 |
| ER-R17 | Atualização concorrente do mesmo ciclo/ledger exige compare-and-set atômico. Somente uma transição concorrente baseada na mesma revisão vence; reserva duplicada não debita duas vezes. Despacho valida a revisão e estado correntes; se disputa com cancelamento/pausa, nenhum novo efeito é autorizado por estado obsoleto. | C03/C07, PBI-02/03 |
| ER-R18 | Descarte congela despachos, exporta/persiste auditoria, confirma descarte do território por capacidade suportada, invalida checkpoints retomáveis e grava tombstone. Cada etapa deve ser repetível; falha parcial fica em `CLEANING_UP` ou bloqueada com diagnóstico. Descarte não desfaz efeitos externos nem apaga evidência necessária; retenção temporal segue Q07 Deferred. | C07, INV-06, PBI-22 |
| ER-R19 | Eventos locais derivam do journal/outbox com IDs e correlação C01/C06. Entrega é pelo menos uma vez com inbox deduplicada; não declarar exactly-once. Payload não inclui conversa ou segredo. | C01/C06/C08, PBI-05/19/22 |

## Estrutura, tipos, ports e use cases

Todos os elementos abaixo são contratos lógicos propostos para `packages/cbm-orchestrator/`; caminhos não indicam arquivos existentes.

### Elementos principais

| Elemento | Camada/tipo | Invariantes e responsabilidade |
|---|---|---|
| `ExecutionCycle` | Aggregate Root — Execução | Mantém identidade/versão, política, `state`, `phase`, motivo final e revisão otimista; valida somente transições C02. |
| `Iteration` e `Attempt` | Entidades — Execução | Cada tentativa tem `attemptId` único, vínculo ao veredicto/versão e relação explícita na árvore; ordinal isolado não é identidade. |
| `BudgetLedger` | Aggregate Root — Orçamento | Reserva antes do despacho, reconcilia uso confirmado/pendente/estimado e deduplica lançamento por chave; não inventa taxas. |
| `ExternalEffectRecord` | Entidade — Execução | Registra intenção, chave idempotente, chamada e resultado; janela sem confirmação persiste como `UNKNOWN`. |
| `ExecutionCheckpoint` | Entidade — Recuperação | Estado recuperável associado ao horizonte e às versões; não serializa como promessa o estado cognitivo do host. |
| `FinalReport` | Read model/audit record | Projeta motivo, evidências, iterações, consumo e incerteza do journal; não concede autoridade nem prova promoção. |

### Tipos e contratos

| Nome | Camada | Regra de validação |
|---|---|---|
| `RunId`, `TaskId`, `AttemptId`, `CandidateVersion` | Identidade (C01) | IDs correlacionados; conteúdo/revisão relevante alterado exige nova versão candidata. |
| `ExecutionState`, `ExecutionPhase`, `TerminationReason` | Domínio (C02) | Enums mantêm exatamente os valores de C02 e permanecem campos separados. |
| `BudgetPolicy`, `BudgetReservation`, `UsageRecord` | Domínio (C03) | Unidade/moeda e qualidade/origem explícitas; desconhecido não é convertido em zero. |
| `EffectStatus`, `IdempotencyKey` | Domínio (C07) | Status só `PENDING|CONFIRMED|FAILED|UNKNOWN`; chave correlaciona tentativas do mesmo comando. |
| `CheckpointSnapshot`, `AttemptTree`, `FinalReport` | Aplicação/auditoria (C02/C07) | Snapshot referencia versão/horizonte e evidência; relatório mantém custos e efeitos desconhecidos explicitamente. |

### Ports e adapters

| Port/adaptador | Operações lógicas | Limite |
|---|---|---|
| `AgentRuntimePort` | iniciar sessão por perfil; cancelar; recolher uso e efeitos; consultar/reconstruir sessão se suportado | Host e contrato Q04 não escolhidos. Só pode ser concretizado após spike; identidade não pode vir de autodeclaração do LLM. |
| `ValidatedHostAdapter` (ER-17) | Conectar o primeiro host validado às portas e à composição da aplicação; iniciar Builder/verificador, capturar saída/uso e cancelar processos próprios | Reaproveitar a integração SP-02 quando adequada; não criar arquitetura multi-host antes da necessidade. Dados ausentes/saída inválida e cancelamento não confirmado permanecem falhas ou incertezas explícitas, nunca sucesso inventado. |
| `IndependentVerifierPort` | verificar versão selada; devolver status, evidências e referência de execução | Veredicto precisa ser independente e ligado à tupla; chamada/saída declarada não equivale a teste executado. |
| `CycleJournalPort` | carregar ciclo; gravar CAS; anexar tentativas, efeitos, checkpoints, relatório e outbox em transação local | Implementação proposta em SQLite TS; nunca escreve no store C. |
| `BudgetLedgerPort` | reservar, contabilizar, reconciliar e consultar saldo/pêndencias | Reserva deve ser atômica, compartilhar orçamento aplicável e bloquear em uso desconhecido sem cobertura. |
| `KnowledgeStatePort` (ACL MCP) | consultar estado/revisão do território; solicitar descarte quando suportado | Capacidade de descarte geral não foi comprovada; `union_session_close` não é API geral de território. |
| `AuditPort` / `OutboxPort` | preservar referência auditável; publicar eventos locais com deduplicação | Não apaga auditoria não consumida; não promete entrega remota exactly-once. |
| `ClockPort` | fornecer monotônico durante processo e timestamps duráveis | Reinício não estende orçamento de tempo. |

Não há `PromotionPort` exposto ao Builder. O Gatekeeper proposto é política determinística TS antes do gate C já existente; ele não substitui nem redefine admissão do core. Qualquer rota controlada de promoção pertence a Autoridade e Decisão e continua condicionada a INV-01/02 e prova do spike; este suplemento não modela o Gatekeeper pleno nem autoriza alterar o core.

### Use cases

| Operação | Responsabilidade | Coordena |
|---|---|---|
| `StartExecution` | Validar capacidade/configuração, registrar objetivo/política/baseline, reservar e iniciar ciclo | CycleJournal, BudgetLedger, AgentRuntime |
| `RunIteration` | Chamar Builder para produzir candidato; congelar/selar a versão resultante; executar verificador independente sobre essa versão e decidir correção/parada | AgentRuntime, IndependentVerifier, CycleJournal, BudgetLedger |
| `ApplyCircuitBreaker` | Agrupar falhas pela classe/janela configuradas, pausar e produzir escalonamento | ExecutionCycle, BudgetLedger, Audit/Outbox |
| `CancelExecution` | Parar novos despachos, cancelar processos controláveis e reconciliar chamadas em voo | AgentRuntime, CycleJournal, BudgetLedger |
| `DiagnosePreF05Crash` | Diagnosticar ciclo interrompido e abrir novo ciclo do objetivo original com novo `runId`/`parentRunId`; não ler checkpoint nem repetir efeito incerto | CycleJournal, AgentRuntime, FinalReport, Audit |
| `ResumeExecutionPostF05` | Validar checkpoint e estado externo; retomar apenas trabalho seguro após reconciliar efeitos | CycleJournal, KnowledgeState, AgentRuntime, BudgetLedger, Audit |
| `DiagnoseAttempts` | Projetar a árvore persistida sem inventar relações ausentes | CycleJournal, Audit |
| `DiscardTerritory` | Executar sequência repetível de descarte, invalidando retomada e preservando auditoria | CycleJournal, KnowledgeState, Audit |
| `FinalizeExecution` | Aplicar condição de parada e persistir relatório/outcome separado | CycleJournal, BudgetLedger, Audit/Outbox |

## Máquina de estados C02

`state` e `phase` são dimensões separadas: a primeira tem apenas quatro valores de ciclo C02; a segunda localiza atividade corrente. Nenhuma linha abaixo cria um estado novo.

| Estado atual → próximo | Phase ilustrativa | Guarda | Registro durável |
|---|---|---|---|
| `CREATED → RUNNING` | `PLANNING` → `BUILDING` | Política explícita válida, capacidade/autoridade mínima disponíveis, reserva inicial concluída | Objetivo, política, baseline, versão e orçamento. |
| `RUNNING → PAUSED` | `VERIFYING` ou qualquer fase ativa → `AWAITING_DECISION`/`RECOVERING` | Breaker, alto risco, incerteza material, cancelamento ou dependência indisponível | Motivo, pacote/diagnóstico, chamadas em voo e incerteza; bloquear novos despachos. |
| `PAUSED → RUNNING` | `RECOVERING` → fase aplicável | Decisão para versão atual; orçamento válido; journal, CBM e efeitos reconciliados conforme C07 | Decisor/comando, revisão CAS, saldo e mapa de efeitos. |
| `RUNNING/PAUSED → COMPLETED` | fase atual → `CLEANING_UP` | Motivo definido; chamadas reconciliadas ou explicitamente incertas | Relatório/consumo duráveis; outcome de promoção separado. |

Motivos terminais são exclusivamente `ACCEPTED`, `REJECTED`, `VERIFIER_UNAVAILABLE`, `BUDGET_EXHAUSTED`, `CANCELLED`, `FAILED`, `RECOVERY_REQUIRED`. A fase `PROMOTING` não é permissão nem resultado de promoção; `COMPLETED` por si só não significa promoção. Pós-F05, checkpoint só pode conduzir a `PAUSED → RUNNING` após reconciliação e decisão aplicável. Pré-F05, diagnóstico termina o ciclo anterior e abre outro `runId` desde o objetivo.

## Transações, crash, concorrência e cancelamento

| Ponto de falha | Estado observável após recuperação | Conduta |
|---|---|---|
| Antes do commit local de intenção/reserva | Sem intenção/outbox confirmadas; nenhum despacho é autorizado por esse comando | Repetir comando somente pela mesma chave idempotente e CAS atual. |
| Após intenção durável e antes de confirmação do despacho | O efeito pode não ter começado ou pode estar em voo; a fronteira MCP/host não participa do commit local | Consultar host/tool se suportado; sem confirmação, preservar `UNKNOWN`, não tratar ausência de resultado como ausência do efeito. |
| Efeito remoto ocorreu, processo cai antes de gravar resultado | Journal mostra intenção pendente; resultado remoto é incerto | Consultar estado ou deduplicar por capacidade comprovada; se impossível, tratar como ocorrido, bloquear replay cego e escalar incerteza material. |
| Após resultado e checkpoint/outbox no commit local | Resultado confirmado com referência durável | Reentrega local é deduplicada; relatório e checkpoint referenciam o mesmo efeito. |
| Durante consulta/discard no CBM | Journal TS e território podem divergir | Reconciliar por MCP/IDs; não afirmar transação distribuída nem promoção; falha de descarte permanece `CLEANING_UP`/diagnóstico bloqueado. |

Concorrência entre workers, retry e cancelamento converge pelo CAS de revisão do ciclo e reserva atômica do ledger. O despacho verifica a revisão/estado imediatamente antes da chamada e só usa reserva/intenção duráveis. Cancelamento/pausa revoga novos despachos; chamadas já iniciadas permanecem em voo e contam no consumo pendente. Se o transporte não puder fazer cessar uma chamada, registrar a exposição/efeito residual, sem declarar cancelamento remoto efetivo. `attemptId` novo identifica uma nova tentativa; deduplicação usa chave do comando/efeito, nunca apenas número da tentativa.

## Eventos e persistência

| # / evento canônico de 001 | Alias lógico TS proposto | Disparo e dados mínimos | Persistência/consumidores |
|---|---|---|---|
| 1 — Objetivo de mudança registrado | `ObjetivoDeMudancaRegistrado` | início; `projectId`, `runId`, objetivo | Journal/outbox; resumo do ciclo. |
| 2 — Política explícita do ciclo validada | `PoliticaExplicitaDoCicloValidada` | início; política e revisão | Journal/outbox; configuração efetiva. |
| 11 — Execução de agente iniciada | `ExecucaoDeAgenteIniciada` | despacho permitido; run/attempt/agente/perfil autenticado | Journal/outbox; Operador e telemetria. |
| 12 — Iteração do Outer Loop iniciada | `IteracaoDoOuterLoopIniciada` | Builder inicia proposta; run/attempt, objetivo e baseline | Journal/outbox; Operador e Builder. |
| 13 — Efeito externo confirmado | `EfeitoExternoConfirmado` | resposta consultável; chave, resultado e referência da chamada | Journal/outbox; relatório e telemetria. |
| 14 — Efeito externo incerto registrado | `EfeitoExternoIncertoRegistrado` | efeito sem confirmação/deduplicação; chave, status `UNKNOWN` e origem | Journal/auditoria; reconciliação, sem repetição cega. |
| 15 — Incerteza material escalada ao Operador | `IncertezaMaterialEscaladaAoOperador` | efeito incerto cuja materialidade exige decisão; motivo e referência | Journal/outbox; Operador. É distinto do registro do efeito incerto (#14). |
| 16 — Consumo do ciclo contabilizado | `ConsumoDoCicloContabilizado` | resposta/reconciliação de uso; origem, unidade, qualidade e reserva | Ledger/journal/outbox; breaker, relatório e telemetria mínima. |
| 17 — Checkpoint associado ao território persistido | `CheckpointAssociadoAoTerritorioPersistido` | commit do checkpoint F05; horizonte, versões, tentativa e referências | Journal; Operador/Builder e recuperação. |
| 19 — Versão candidata identificada e selada | `VersaoCandidataIdentificadaESelada` | Builder já produziu; host congela candidato e tupla C01 antes da verificação | Journal/outbox; verificador e Gatekeeper. |
| 20 — Avaliação anterior invalidada por mudança de versão | `AvaliacaoAnteriorInvalidadaPorMudancaDeVersao` | correção altera conteúdo/revisão; versões anterior/nova e avaliações invalidadas | Journal/outbox; Builder, verificador e Operador. |
| 21 — Pacote de evidências da versão reunido | `PacoteDeEvidenciasDaVersaoReunido` | evidências foram vinculadas à versão selada; refs/status/gaps | Journal/outbox; Gatekeeper e Operador. |
| 22 — Veredicto independente emitido | `VeredictoIndependenteEmitido` | verificação termina; versão, status, origem e referência | Journal/outbox; correção ou relatório. |
| 23 — Iteração de correção agendada | `IteracaoDeCorrecaoAgendada` | veredicto leva a nova iteração; versão avaliada, motivo e tentativa | Journal/outbox; Builder e Operador. |
| 25 — Circuit breaker acionado e ciclo pausado | `CircuitBreakerAcionadoECicloPausado` | falhas configuradas atingem critério; classe, revisão, motivo e pausa | Journal/outbox; Operador e diagnóstico. |
| 26 — Novas ações bloqueadas por orçamento esgotado | `NovasAcoesBloqueadasPorOrcamentoEsgotado` | teto alcançado; saldo, reservas pendentes e qualidade | Ledger/journal/outbox; cancelamento/reconciliação. |
| 34 — Território descartado com auditoria preservada | `TerritorioDescartadoComAuditoriaPreservada` | descarte coordenado confirmado; território, checkpoints invalidados e refs de auditoria | Tombstone no journal e estado confirmado via capacidade MCP; retrospectiva conforme Q07. |
| 35 — Ciclo encerrado com motivo e evidências registrados | `CicloEncerradoComMotivoEEvidenciasRegistrados` | condição terminal; motivo, versão, evidências, consumo e incertezas | Journal/outbox; relatório e telemetria. |

Eventos TS seguem envelope mínimo C06/C01 (`eventId`, tipo/schema, projeto/run/versão aplicáveis, instante, fonte, causation/correlation, referência, chave de dedupe). Inbox deduplica por tipo+fonte+objeto+versão; outbox é transacional apenas com journal local. Sensíveis e conversas não são incluídos. Persistência proposta: ciclo e revisão CAS; política/ledger e reservas; tentativas; intenção/efeito; checkpoints; relatórios e auditoria; outbox/inbox. A projeção de telemetria pode ser reconstruída do journal; não substitui o registro de auditoria nem exige serviço externo.

## Refinamento por PBI

| PBI | Aceite refinado desta frente | Dependências e limites |
|---|---|---|
| PBI-01 | Um ciclo executa Builder → verificador independente → decisão/motivo final auditável; sucesso, reprovação, indisponibilidade e orçamento têm outcomes distinguíveis. | Q04/capacidade do runtime e autoridade mínimas são pré-condições; ADR-005 PROPOSED. Não amplia para política completa F04. |
| PBI-02 | Política válida registra tokens/custo/tentativas/tempo; reserva precede despacho; atingir teto bloqueia novos despachos, cancela o controlável e mantém custo pendente. | Q05 Deferred; nenhuma cifra/default; efetividade financeira limitada pelo que host conseguir impor e medir. |
| PBI-03 | Falhas repetidas da mesma classe segundo parâmetro configurado pausam e escalam com diagnóstico. | Parâmetro/janela são configurados; ausência de números não é preenchida pela especificação. |
| PBI-04 | Veredicto e evidência da versão atual alimentam correção; correção cria nova versão e requer nova verificação. | Verificador executável e independente depende de Q04/capacidades; indisponibilidade não é verde. |
| PBI-05 | Condição de parada produz relatório durável com motivo, evidências, iterações, custo e incertezas; estado final separado de aceitação/promoção. | Persistência de decisão/relatório é bloqueante; telemetria é o subconjunto de PBI-31. |
| PBI-19 | Checkpoint referencia histórico necessário, chamadas, tentativas, território e versão. | F05; C07; dados do host são dependência configurável e não restauram raciocínio interno por suposição. |
| PBI-20 | Recuperação pós-F05 reconcilia antes de retomar e deduplica efeito confirmado; incerto e não consultável é marcado `UNKNOWN`, tratado como ocorrido e escalado quando material. | Q04 e capacidade externa de consultar/deduplicar; sem ela, sem retry. Pré-F05 reinicia do objetivo, sem auto-retomada. |
| PBI-21 | Diagnóstico lista tentativas, relações, versão, resultado e evidência por território, incluindo lacunas. | Requer referências duráveis; não reconstruir relações/outcomes ausentes por inferência. |
| PBI-22 | Descarte invalida retomada e checkpoints associados, persiste tombstone/auditoria elegível e declara efeitos externos não desfeitos. | Capacidade de descarte geral não comprovada em 006; Q07 Deferred; falha parcial fica diagnosticável e repetível. |

## Ordered Development Tasks

Todas as tarefas abaixo são condicionais ao mapeamento de capacidades e ao spike Q04 concluído com evidência. As estimativas são sessões de 2–4 horas; `scope` lista contratos/caminhos propostos, não arquivos existentes. O spike não é executado como parte desta frente. IDs antigos permanecem estáveis; o array está ordenado pelos pré-requisitos, e a tabela `all_requires` explicita requisitos além do predecessor principal `depends_on`.

```json
[
  {
    "id": "ER-01",
    "title": "Definir contratos de ciclo e versão",
    "description": "Define tipos e invariantes do ciclo alinhados a C01/C02 antes de qualquer execução autônoma.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/execution/ExecutionCycle.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionIds.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionContracts.ts"
    ],
    "acceptance": [
      "State, phase e terminationReason mantêm enums C02 separados",
      "Mutação relevante cria versão candidata nova e invalida evidência anterior"
    ],
    "pbis": [
      "PBI-01",
      "PBI-04",
      "PBI-05"
    ],
    "depends_on": null
  },
  {
    "id": "ER-02",
    "title": "Modelar orçamento e reservas",
    "description": "Modela política e ledger para que nenhuma chamada seja despachada sem consumo reservado e identificável.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/budget/BudgetPolicy.ts",
      "packages/cbm-orchestrator/src/budget/BudgetLedger.ts",
      "packages/cbm-orchestrator/src/budget/UsageRecord.ts"
    ],
    "acceptance": [
      "Reserva contabiliza consumo confirmado e pendente sem somar unidades incompatíveis",
      "Uso desconhecido suspende novas ações quando não há limite conservador"
    ],
    "pbis": [
      "PBI-02",
      "PBI-03"
    ],
    "depends_on": "ER-01"
  },
  {
    "id": "ER-03",
    "title": "Definir registros de efeitos e idempotência",
    "description": "Modela intenção, resultado e chave idempotente para efeitos remotos, inclusive a lacuna UNKNOWN.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/recovery/ExternalEffectRecord.ts",
      "packages/cbm-orchestrator/src/recovery/EffectStatus.ts",
      "packages/cbm-orchestrator/src/recovery/IdempotencyKey.ts"
    ],
    "acceptance": [
      "Status de efeito usa apenas os valores de C07",
      "Efeito sem resultado permanece UNKNOWN e a chave deduplica o comando, não o ordinal da tentativa"
    ],
    "pbis": [
      "PBI-01",
      "PBI-02",
      "PBI-05",
      "PBI-20"
    ],
    "depends_on": "ER-01"
  },
  {
    "id": "ER-04",
    "title": "Implementar transições do ciclo",
    "description": "Aplica guardas C02 para início, pausa, retomada validada e conclusão com motivo independente da fase.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/execution/ExecutionCycle.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionTransitions.ts",
      "packages/cbm-orchestrator/src/execution/TerminationReason.ts"
    ],
    "acceptance": [
      "Somente transições C02 são aceitas",
      "COMPLETED não implica aceitação ou promoção"
    ],
    "pbis": [
      "PBI-01",
      "PBI-03",
      "PBI-05"
    ],
    "depends_on": "ER-01"
  },
  {
    "id": "ER-05",
    "title": "Definir portas de runtime e verificação",
    "description": "Expõe contratos lógicos para iniciar, cancelar, medir e verificar sem selecionar host ou alegar capacidade existente.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/ports/AgentRuntimePort.ts",
      "packages/cbm-orchestrator/src/ports/IndependentVerifierPort.ts",
      "packages/cbm-orchestrator/src/ports/KnowledgeStatePort.ts"
    ],
    "acceptance": [
      "Perfis são fornecidos por identidade confiável fora da autodeclaração do Builder",
      "Operações não mapeadas ficam explicitamente indisponíveis e bloqueiam autonomia dependente"
    ],
    "pbis": [
      "PBI-01",
      "PBI-04",
      "PBI-16"
    ],
    "depends_on": "ER-01"
  },
  {
    "id": "ER-07",
    "title": "Persistir journal mínimo do ciclo e outbox local",
    "description": "Armazena estado mínimo de F01 e eventos locais atomicamente; os checkpoints F05 são uma extensão separada.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/persistence/CycleJournal.ts",
      "packages/cbm-orchestrator/src/persistence/SqliteCycleJournal.ts",
      "packages/cbm-orchestrator/src/persistence/Outbox.ts"
    ],
    "acceptance": [
      "Atualização usa CAS e chave idempotente",
      "Journal mínimo e outbox local confirmam ou revertem juntos sem incluir checkpoint F05"
    ],
    "pbis": [
      "PBI-05",
      "PBI-31"
    ],
    "depends_on": "ER-01"
  },
  {
    "id": "ER-08",
    "title": "Implementar ledger concorrente de uso",
    "description": "Integra reservas, uso pendente e reconciliação ao journal antes de despachar trabalho.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/budget/BudgetLedgerPort.ts",
      "packages/cbm-orchestrator/src/persistence/SqliteBudgetLedger.ts",
      "packages/cbm-orchestrator/src/budget/ReconcileUsage.ts"
    ],
    "acceptance": [
      "Reserva simultânea não excede saldo por corrida de atualização",
      "Uso ausente não é lançado como zero e bloqueia novos despachos sem cobertura"
    ],
    "pbis": [
      "PBI-02",
      "PBI-03"
    ],
    "depends_on": "ER-02"
  },
  {
    "id": "ER-17",
    "title": "Integrar o primeiro host validado ao runtime",
    "description": "Reaproveita a integração do fixture SP-02 quando adequada e conecta um único host aprovado às portas e à composição real do ciclo.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/runtime/ValidatedHostAdapter.ts",
      "packages/cbm-orchestrator/src/runtime/HostComposition.ts",
      "packages/cbm-orchestrator/src/runtime/ValidatedHostAdapter.integration.test.ts"
    ],
    "acceptance": [
      "Builder e verificador iniciam pelo adapter concreto com identidade e transporte autorizados; saída/uso têm origem e versão do host validadas",
      "Cancelamento cobre os processos filhos sob sua propriedade, confirma encerramento ou registra falha/incerteza e bloqueia novos despachos",
      "Uso ausente ou saída malformada não vira zero/sucesso; erro e consumo pendente chegam ao journal/ledger e à execução"
    ],
    "pbis": [
      "PBI-01",
      "PBI-02",
      "PBI-04",
      "PBI-05",
      "PBI-16",
      "PBI-31"
    ],
    "depends_on": "ER-05"
  },
  {
    "id": "ER-06",
    "title": "Coordenar iterações após seus ports e stores",
    "description": "Coordena saída do Builder, congelamento/selagem da candidata, verificação independente e correção versionada.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/execution/RunIteration.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionCoordinator.ts",
      "packages/cbm-orchestrator/src/execution/IndependentVerdict.ts"
    ],
    "acceptance": [
      "Builder produz antes de o host capturar e selar a versão candidata",
      "Veredicto é associado à tupla exata selada; indisponibilidade não vira verde e correção exige nova selagem/verificação",
      "Execução usa o adapter concreto ER-17; produção não inicia sem S0_APPROVED, mesmo que SP-05 esteja concluída"
    ],
    "pbis": [
      "PBI-01",
      "PBI-04",
      "PBI-16"
    ],
    "depends_on": "ER-04"
  },
  {
    "id": "ER-11",
    "title": "Aplicar breaker e cancelamento",
    "description": "Pausa despachos por critério configurado e reconcilia chamadas em voo após cancelamento.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/execution/ApplyCircuitBreaker.ts",
      "packages/cbm-orchestrator/src/execution/CancelExecution.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionCoordinator.ts"
    ],
    "acceptance": [
      "Falhas repetidas da mesma classe pausam e geram escalonamento",
      "Cancelamento bloqueia novos despachos e registra custo/efeito residual"
    ],
    "pbis": [
      "PBI-02",
      "PBI-03",
      "PBI-05"
    ],
    "depends_on": "ER-06"
  },
  {
    "id": "ER-12",
    "title": "Projetar relatório final e telemetria auditáveis",
    "description": "Projeta relatório final e telemetria mínima do journal/ledger preservando desconhecidos; árvore F05 é tratada pela tarefa de checkpoint.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/execution/FinalReport.ts",
      "packages/cbm-orchestrator/src/telemetry/ExecutionEvents.ts",
      "packages/cbm-orchestrator/src/telemetry/ReportProjection.ts"
    ],
    "acceptance": [
      "Relatório distingue custo conhecido, estimado, pendente e desconhecido",
      "Conclusão do ciclo não é convertida em promoção e lacunas permanecem explícitas"
    ],
    "pbis": [
      "PBI-05",
      "PBI-31"
    ],
    "depends_on": "ER-07"
  },
  {
    "id": "ER-10",
    "title": "Diagnosticar crash pré-F05 sem retomar checkpoint",
    "description": "Fecha o run interrompido para diagnóstico e, se houver nova execução, parte do objetivo original com novo runId/parentRunId sem checkpoint ou replay cego.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/recovery/DiagnosePreF05Crash.ts",
      "packages/cbm-orchestrator/src/execution/StartExecution.ts",
      "packages/cbm-orchestrator/src/persistence/CycleJournal.ts",
      "packages/cbm-orchestrator/src/execution/FinalReport.ts"
    ],
    "acceptance": [
      "Crash pré-F05 é diagnosticado e novo ciclo, quando iniciado, tem novo runId, parentRunId e objetivo original",
      "Nenhum checkpoint é retomado ou efeito UNKNOWN repetido às cegas"
    ],
    "pbis": [
      "PBI-05"
    ],
    "depends_on": "ER-12"
  },
  {
    "id": "ER-14",
    "title": "Definir snapshot de checkpoint F05",
    "description": "Modela o snapshot recuperável da árvore de tentativas e seus vínculos sem criar dependência F05 para o journal mínimo.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/recovery/ExecutionCheckpoint.ts",
      "packages/cbm-orchestrator/src/recovery/CheckpointSnapshot.ts",
      "packages/cbm-orchestrator/src/recovery/AttemptTree.ts"
    ],
    "acceptance": [
      "Snapshot referencia run, território, tupla C01, tentativas e mapa de efeitos C07",
      "Conteúdo sensível é minimizado sem prometer restauração de raciocínio interno nem apagar referência de deduplicação"
    ],
    "pbis": [
      "PBI-19",
      "PBI-21"
    ],
    "depends_on": "ER-03"
  },
  {
    "id": "ER-15",
    "title": "Estender journal com persistência de checkpoint F05",
    "description": "Adiciona armazenamento transacional de checkpoints F05 sobre o journal mínimo já utilizável no slice S1.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/persistence/CycleJournal.ts",
      "packages/cbm-orchestrator/src/persistence/SqliteCheckpointRepository.ts",
      "packages/cbm-orchestrator/src/recovery/CheckpointSnapshot.ts"
    ],
    "acceptance": [
      "Checkpoint e referências/outbox locais são persistidos atomicamente com CAS",
      "Falha de commit preserva o último checkpoint íntegro e não exige escrita no store C"
    ],
    "pbis": [
      "PBI-19"
    ],
    "depends_on": "ER-14"
  },
  {
    "id": "ER-09",
    "title": "Persistir checkpoints e árvore diagnóstica de F05",
    "description": "Liga tentativas e checkpoints ao território no journal depois que a extensão de persistência F05 estiver disponível.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/recovery/CheckpointService.ts",
      "packages/cbm-orchestrator/src/recovery/AttemptRepository.ts",
      "packages/cbm-orchestrator/src/recovery/DiagnoseAttempts.ts",
      "packages/cbm-orchestrator/src/persistence/CycleJournal.ts"
    ],
    "acceptance": [
      "Checkpoint referencia versão, horizonte, IDs de chamadas/tentativas e mapa de efeitos C07",
      "Diagnóstico usa relações persistidas e expõe lacunas sem inventar pai, ordem causal ou resultado",
      "Crash antes do commit mantém apenas o checkpoint durável anterior"
    ],
    "pbis": [
      "PBI-19",
      "PBI-21"
    ],
    "depends_on": "ER-15"
  },
  {
    "id": "ER-16",
    "title": "Retomar execução segura pós-F05",
    "description": "Valida checkpoint, tupla e estado externo antes de reconstruir trabalho seguro e retomar por política C07.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/recovery/ResumeExecutionPostF05.ts",
      "packages/cbm-orchestrator/src/recovery/EffectReconciler.ts",
      "packages/cbm-orchestrator/src/ports/AgentRuntimePort.ts",
      "packages/cbm-orchestrator/src/ports/KnowledgeStatePort.ts"
    ],
    "acceptance": [
      "Efeito confirmado/deduplicável não é reaplicado; tupla/território divergente bloqueia retomada",
      "Efeito UNKNOWN sem consulta ou deduplicação não é repetido e escala quando material"
    ],
    "pbis": [
      "PBI-20"
    ],
    "depends_on": "ER-09"
  },
  {
    "id": "ER-13",
    "title": "Coordenar descarte e preservação de auditoria",
    "description": "Executa descarte repetível do território e checkpoints, mantendo trilha elegível para retrospectiva.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/recovery/DiscardTerritory.ts",
      "packages/cbm-orchestrator/src/recovery/AuditRecord.ts",
      "packages/cbm-orchestrator/src/ports/KnowledgeStatePort.ts"
    ],
    "acceptance": [
      "Checkpoints ficam não retomáveis e tombstone/auditoria são persistidos",
      "Falha parcial mantém diagnóstico e não afirma desfazer efeito externo"
    ],
    "pbis": [
      "PBI-22"
    ],
    "depends_on": "ER-09"
  }
]
```

### Pré-requisitos completos da task graph (`all_requires`)

`all_requires` inclui o predecessor principal informado em `depends_on`. A trilha S1 termina sem depender dos artefatos F05: ER-01/02/03/04/05/07/08/17/06/11/12 e o diagnóstico pré-F05 ER-10. ER-14/15/09/16/13 são a extensão posterior F05; ER-10 não depende de checkpoint.

| Task | `depends_on` principal | `all_requires` completo |
|---|---|---|
| ER-01 | — | — |
| ER-02 | ER-01 | ER-01 |
| ER-03 | ER-01 | ER-01 |
| ER-04 | ER-01 | ER-01 |
| ER-05 | ER-01 | ER-01 |
| ER-07 | ER-01 | ER-01, ER-03 (tipos de efeito S1; sem checkpoint) |
| ER-08 | ER-02 | ER-02 |
| ER-06 | ER-04 | ER-01, ER-02, ER-04, ER-05, ER-07, ER-08, ER-17 |
| ER-17 | ER-05 | ER-05, ER-07, ER-08, AD-02; S0_APPROVED para o host/configuração usados |
| ER-11 | ER-06 | ER-05, ER-06, ER-07, ER-08 |
| ER-12 | ER-07 | ER-07, ER-08, ER-11 |
| ER-10 | ER-12 | ER-01, ER-02, ER-04, ER-05, ER-06, ER-07, ER-08, ER-12 |
| ER-14 | ER-03 | ER-01, ER-03 |
| ER-15 | ER-14 | ER-03, ER-07, ER-14 |
| ER-09 | ER-15 | ER-03, ER-07, ER-14, ER-15 |
| ER-16 | ER-09 | ER-05, ER-07, ER-08, ER-09 |
| ER-13 | ER-09 | ER-05, ER-07, ER-09, ER-11 |

## Cenários de teste especificados

Especificação Given/When/Then apenas; não são testes executáveis nem foram executados. Cada cenário tem origem rastreável ao PBI e às regras desta frente.

### PBI-01 — ciclo verificável

<a id="er-s01"></a>

**ER-S01 — Deve selar após a produção do Builder e verificar a mesma versão** *(Functional; PBI-01; ER-R01–05)*  
**Given** capacidade/runtime e política válidos, objetivo e baseline registrados, sem versão candidata selada para esta iteração  
**When** Builder produz candidato, o host captura/congela e sela a nova tupla C01 e o verificador independente retorna GREEN para essa tupla  
**Then** ciclo termina com `state=COMPLETED` e `terminationReason=ACCEPTED`; evidências e iteração ficam registradas e outcome de promoção permanece separado.

<a id="er-s02"></a>

**ER-S02 — Deve registrar reprovação quando a condição configurada encerra após veredicto vermelho** *(Functional; PBI-01; ER-R04/09)*  
**Given** ciclo executando com condição de parada explicitamente configurada e veredicto RED para a versão atual  
**When** a condição de parada do ciclo é satisfeita sem candidato aceito  
**Then** ciclo termina com `state=COMPLETED` e `terminationReason=REJECTED`, preserva veredicto/razão e não declara promoção.

### PBI-02 — orçamento e chamadas em andamento

<a id="er-s03"></a>

**ER-S03 — Deve reservar e reconciliar consumo antes e depois do despacho** *(Integration; PBI-02/31; ER-R06/07/17)*
**Given** política válida e saldo suficiente no ledger do ciclo  
**When** uma chamada é despachada e o runtime informa uso  
**Then** a reserva precede o despacho, uso é lançado uma vez e apenas saldo não consumido é liberado.

<a id="er-s04"></a>

**ER-S04 — Deve bloquear novas ações ao teto enquanto contabiliza custo pendente** *(Integration; PBI-02/31; ER-R06/07/16)*
**Given** uma ou mais chamadas em voo e a soma de consumo confirmado com reserva alcança o teto configurado  
**When** ledger processa o limite  
**Then** novos despachos são bloqueados, cancelamento controlável é solicitado, custo residual permanece pendente até reconciliação e o ciclo termina com `state=COMPLETED` e `terminationReason=BUDGET_EXHAUSTED` quando o consumo em voo estiver reconciliado ou explicitamente incerto.

<a id="er-s05"></a>

**ER-S05 — Deve suspender novos despachos quando uso fica desconhecido** *(Integration; PBI-02/31; ER-R07)*
**Given** chamada terminou sem dado de uso e não há limite conservador válido para esse runtime  
**When** ledger reconcilia a chamada  
**Then** qualidade permanece `UNKNOWN`, consumo não vira zero e novas ações ficam suspensas.

### PBI-03 — circuit breaker

<a id="er-s06"></a>

**ER-S06 — Deve pausar e escalar após falhas repetidas da mesma classe** *(Integration; PBI-03; ER-R08)*  
**Given** limiar/janela configurados e ocorrências persistidas da mesma classe abaixo do critério  
**When** nova falha cruza o critério  
**Then** ciclo transita `RUNNING→PAUSED`, `phase=RECOVERING`, e evento com classe/motivo é durável.

<a id="er-s07"></a>

**ER-S07 — Deve manter breaker fechado quando falhas não atingem o critério da mesma classe** *(Unit; PBI-03; ER-R08)*  
**Given** falhas de classes distintas ou contagem inferior ao critério configurado  
**When** breaker avalia o histórico  
**Then** não agrega classes como repetição e não dispara pausa por uma contagem inventada.

### PBI-04 — iteração baseada no veredicto

<a id="er-s08"></a>

**ER-S08 — Deve encaminhar veredicto vermelho à correção e reverificar nova versão** *(Functional; PBI-04; ER-R01/04/09)*  
**Given** versão candidata selada e verificador retorna RED com razões/evidências  
**When** a política do ciclo permite correção, Builder produz candidato corrigido, o host congela/sela nova versão e o verificador independente retorna GREEN para essa versão  
**Then** o resultado vermelho alimenta a correção; o novo veredicto usa a tupla selada depois da produção e o ciclo pode encerrar aceito sem reutilizar evidência obsoleta.

<a id="er-s09"></a>

**ER-S09 — Deve pausar sem tratar verificador indisponível como aprovação** *(Integration; PBI-04/31; ER-R04/05)*
**Given** ciclo aguarda verificação da versão atual  
**When** verificador retorna UNAVAILABLE ou INCONCLUSIVE  
**Then** nenhum veredicto verde é sintetizado, nenhuma promoção é liberada e dependência indisponível é escalada; se encerrado, usa `state=COMPLETED` e `terminationReason=VERIFIER_UNAVAILABLE`.

### PBI-05 — condição de parada e relatório

<a id="er-s10"></a>

**ER-S10 — Deve persistir relatório completo ao encerrar ciclo** *(Integration; PBI-05/31; ER-R10/11)*
**Given** ciclo chega a condição terminal após reconciliar chamadas em voo ou marcar incerteza  
**When** `FinalizeExecution` grava o resultado  
**Then** relatório contém motivo, versões, iterações, evidências, uso e efeitos incertos e não confunde `COMPLETED` com promoção.

<a id="er-s11"></a>

**ER-S11 — Deve impedir conclusão durável quando falha a gravação do relatório** *(Integration; PBI-05; ER-R10/11)*  
**Given** ciclo precisa gravar decisão e relatório final no journal  
**When** transação local falha antes do commit  
**Then** estado/outbox não aparecem parcialmente confirmados e nenhuma ação posterior dependente da decisão, inclusive promoção, é liberada.

### PBI-19 — checkpoint

<a id="er-s12"></a>

**ER-S12 — Deve gravar checkpoint ligado ao horizonte e à árvore de tentativas** *(Integration; PBI-19; ER-R11/14)*  
**Given** tentativa ativa tem referências de mensagens/tools, versão e efeitos registrados  
**When** checkpoint é confirmado no journal  
**Then** snapshot referencia run, horizonte, tupla, tentativa e mapa de efeitos sem exigir restauração da cognição interna do host.

<a id="er-s13"></a>

**ER-S13 — Deve conservar o último checkpoint íntegro após crash antes do commit** *(Integration; PBI-19; ER-R11/14)*  
**Given** existe checkpoint anterior confirmado  
**When** processo cai durante gravação do próximo checkpoint antes do commit local  
**Then** recuperação lê o último snapshot completo e não expõe checkpoint/outbox parcial.

### PBI-20 — retomada idempotente

<a id="er-s14"></a>

**ER-S14 — Deve retomar trabalho seguro sem duplicar efeito confirmado** *(Integration; PBI-20; ER-R12/15)*  
**Given** checkpoint F05 contém comando cuja chave idempotente e efeito estão confirmados  
**When** recuperação processa o checkpoint  
**Then** efeito é reconhecido/reconciliado e não executado uma segunda vez.

<a id="er-s15"></a>

**ER-S15 — Deve tratar efeito externo não consultável como ocorrido e escalar incerteza** *(Integration; PBI-20; ER-R12/15)*  
**Given** journal tem intenção de chamada externa sem resultado e a ferramenta não oferece consulta nem deduplicação comprovável  
**When** recuperação reconcilia a tentativa  
**Then** efeito fica `UNKNOWN`, não há replay cego e incerteza material é encaminhada ao Operador.

### PBI-21 — diagnóstico da árvore

<a id="er-s16"></a>

**ER-S16 — Deve reconstruir árvore de tentativas persistida por território** *(Integration; PBI-21; ER-R14/15)*  
**Given** tentativas com IDs, pais, versões e resultados foram persistidas  
**When** Operador solicita diagnóstico por horizonte  
**Then** projeção apresenta relações e evidências armazenadas para explicar falha ou convergência.

<a id="er-s17"></a>

**ER-S17 — Deve expor lacuna quando vínculo de tentativa está ausente** *(Unit; PBI-21; ER-R14)*  
**Given** registro de tentativa não possui vínculo pai ou resultado persistido  
**When** diagnóstico projeta a árvore  
**Then** relação/outcome fica marcado como lacuna, sem inventar pai, ordenação causal ou sucesso.

### PBI-22 — descarte e auditoria

<a id="er-s18"></a>

**ER-S18 — Deve invalidar retomada e preservar auditoria ao descartar território** *(Integration; PBI-22; ER-R18/19)*  
**Given** território possui checkpoints e registros de falha ainda elegíveis para retrospectiva  
**When** descarte é confirmado pela capacidade suportada  
**Then** checkpoints não são retomáveis, tombstone referencia território e auditoria permanece consultável conforme política configurada.

<a id="er-s19"></a>

**ER-S19 — Deve manter diagnóstico repetível quando descarte parcial falha** *(Integration; PBI-22; ER-R18)*  
**Given** auditoria foi preservada mas confirmação do descarte CBM falha ou fica incerta  
**When** operação de descarte é interrompida  
**Then** fase permanece `CLEANING_UP` ou bloqueada com diagnóstico, sem tombstone de sucesso nem alegação de rollback de efeito externo.

### Cenários transversais exigidos

<a id="er-s20"></a>

**ER-S20 — Deve parar novos despachos após cancelamento e reconciliar chamadas em voo** *(Integration; PBI-02/05; ER-R07/16/17)*  
**Given** ciclo RUNNING tem chamadas ativas  
**When** Operador cancela  
**Then** nenhuma chamada nova é despachada, cancelamento controlável é solicitado, pendências são reconciliadas ou marcadas desconhecidas e, após persistência, ciclo encerra com `state=COMPLETED` e `terminationReason=CANCELLED`.

<a id="er-s21"></a>

**ER-S21 — Deve diagnosticar e iniciar do objetivo após crash pré-F05 sem checkpoint** *(Functional; PBI-05/20; ER-R12/13)*  
**Given** ciclo pré-F05 caiu e registro de chamada externa não confirma seu resultado  
**When** Operador inicia recuperação diagnóstica  
**Then** execução antiga registra anomalia e, quando encerrada, usa `state=COMPLETED` e `terminationReason=RECOVERY_REQUIRED` após reconciliar ou registrar incerteza; novo `runId` tem `parentRunId` e parte do objetivo original sem repetir o efeito incerto; descarte de estado inconsistente depende de capacidade suportada.

<a id="er-s22"></a>

**ER-S22 — Deve recusar retomada quando versão/território diverge do checkpoint** *(Integration; PBI-20; ER-R01/15)*  
**Given** checkpoint F05 aponta uma tupla de versão diferente do estado CBM atual  
**When** recuperação valida consistência  
**Then** não retoma nem promove a versão divergente e escala reconciliação/decisão.

<a id="er-s23"></a>

**ER-S23 — Deve negar promoção por chamada direta e por caminho alternativo do Builder** *(Integration; PBI-16; ER-R05)*  
**Given** identidade Builder está vinculada ao processo por host/proxy confiável e não possui credencial/canal de promoção próprio  
**When** Builder tenta chamar diretamente uma tool de promoção pelo nome e tenta alcançar a mesma operação por transporte/caminho alternativo  
**Then** autorização no despacho nega ambos sem efeito; tentativas ficam auditáveis e, se qualquer caminho não puder ser bloqueado no ambiente controlado, autonomia dependente permanece bloqueada até prova do spike.


### Integração do primeiro host validado — ER-17

<a id="er-s24"></a>

**ER-S24 — Deve executar Builder e verificador pelo adapter concreto validado** *(Functional; PBI-01/04/05/16/31; ER-R02/04/05/10)*
- **Given:** gate S0_APPROVED satisfeito para o host/configuração atuais e composição conectada ao adapter ER-17, sem substituir o processo alvo por mock
- **When:** um objetivo pequeno inicia Builder, captura/sele a saída e verifica a versão em processo independente
- **Then:** entradas, saídas, identidades, versão do host e uso têm referências duráveis; o Builder não acessa o canal de promoção e a decisão não depende só de interfaces ou do fixture isolado.

<a id="er-s25"></a>

**ER-S25 — Deve cancelar processos próprios pelo adapter real sem esconder consumo pendente** *(Integration; PBI-02/05; ER-R07/16/17)*
- **Given:** host validado executa processo e filhos rastreados sob propriedade do ciclo, com reserva de consumo em voo
- **When:** cancelamento é enviado ao adapter real
- **Then:** ele bloqueia novos despachos e confirma encerramento do processo/filhos ou registra falha/incerteza explícita; consumo ausente permanece UNKNOWN/pendente e não é lançado como zero nem liberado antes da reconciliação.

## Gaps mantidos

- Q04 segue Deferred; ADR-005 segue PROPOSED até spike futuro. Isolamento real, cancelamento, medição e recuperação por host não estão demonstrados; PBI-16 e autonomia dependente ficam bloqueados até prova.
- Q05 segue Deferred; cifras, thresholds, timeouts e defaults de negócio não foram definidos. Este desenho exige política explicitamente válida, mas isso é recomendação `model`.
- Q07 segue Deferred; não há duração de retenção. Só é vinculante no desenho preservar auditoria necessária/não consumida e configurar a política.
- Descarte genérico de território, consulta/deduplicação de efeitos e vínculo de processos runtime ao checkpoint requerem capacidade validada por mapeamento/spike. Nenhuma operação C/MCP ou endpoint novo é afirmado como implementado.
- O core C não é alterado; se a mediação necessária ao Builder não puder ser provada fora do core, retornar ao Operador para decisão explícita de escopo.
