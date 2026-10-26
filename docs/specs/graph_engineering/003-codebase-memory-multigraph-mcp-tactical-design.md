# Tactical Design — codebase-memory-multigraph-mcp

**Domain:** graph_engineering. **Project:** codebase-memory-multigraph-mcp. **Date:** 2026-10-23. **Status:** especificação proposta; sem implementação ou spike executado. Este documento é o ponto de entrada tático por projeto; os cinco suplementos de frente integram sua especificação. [001](001-problem-space.md) governa vocabulário, [002](002-context-map.md) governa integrações e [005](005-shared-contracts.md) governa contratos comuns.

## Refinement Questions and Answers

Applies_to de todas as entradas: `codebase-memory-multigraph-mcp`. Registro completo e proveniência: [DECISIONS.md](DECISIONS.md). As respostas humanas são preservadas; recomendações não encerram questões adiadas.

| ID | Category | Question | Recommendation | Final Answer | Answered By |
|---|---|---|---|---|---|
| Q01 | boundary | Visão completa ou protótipo? | Visão integral com slice mínimo priorizado. | Visão completa + slice inicial priorizado. | human |
| Q02 | architecture | Onde vive a camada TS? | Pacote novo consumindo MCP; sem escrever store C. | Novo pacote TS neste repositório. | human |
| Q03 | business | Agentes são personas? | Operador e agentes como atores do produto. | Humanos + agentes como personas. | human |
| Q04 | integration | Qual host/contrato inicia, cancela, isola, mede e recupera? | Validar proposta ADR-005 e fronteira mediada em spike. | Unresolved; Deferred, ADR-005 PROPOSED. | unresolved |
| Q05 | performance | Quais defaults de orçamento e parada? | Política explícita validada até existirem medições; não fixar números por inferência. | Unresolved; Deferred. | unresolved |
| Q06 | business | Qual cadência da retrospectiva? | Semanal é hipótese do briefing; manter configurável e aguardar decisão. | Unresolved; Deferred. | unresolved |
| Q07 | data | Qual retenção da auditoria? | Preservar registros ainda necessários; prazo configurável a decidir. | Unresolved; Deferred. | unresolved |
| Q08 | acceptance | Qual referência e limiar demonstram valor? | Tarefas/aceite equivalentes e protocolo aprovado antes de medir. | Unresolved; Deferred. | unresolved |

## Section 1 — Main Structure

O projeto existente é backend C nativo com UI opcional. O desenho aplica módulos TS ao novo pacote proposto `packages/cbm-orchestrator/`, sem converter core em agregados DDD, escolher framework/runner ou implementar UI nova. Todos os caminhos desse pacote nos suplementos são alvos futuros, não arquivos existentes.

| Elemento | Camada / tipo | Invariantes e responsabilidade | Contrato / suplemento |
|---|---|---|---|
| Ciclo de execução | Execução / agregado | Objetivo, estados, fase, término e referências; só avança com orçamento/autoridade válidos | C01/C02; frente 1 |
| Orçamento do ciclo | Execução / agregado | Reserva atômica compartilhada, consumo em voo e breaker; não duplica custo por replay | C03; frente 1 |
| Checkpoint de execução | Execução / registro versionado | Retomada só F05; associa efeitos confirmados/incertos ao território correto | C07; frente 1 |
| Plano de trabalho | Planejamento / agregado | Dependências, claims e integração parcial; comunidade não implica independência | C06; frente 2 |
| Claim de trabalho | Planejamento / recurso de exclusão | Aquisição atômica e fencing da escrita; não é claim documental Union | C06; frente 2 |
| Assinatura de evento | Eventos / agregado | Regra versionada, fonte válida, dedupe e orçamento global | C06; frente 2 |
| Versão candidata e dossiê | Autoridade / modelo imutável | Conteúdo e contexto exatos para revisão/decisão; mutação invalida aprovação | C01/C04; frente 3 |
| Gatekeeper | Autoridade / serviço determinístico | Sem verde independente atual não promove; Builder não alcança canal privilegiado | C04/C05; frente 3 |
| Decisão humana | Autoridade / comando versionado | Aprovar, rejeitar, corrigir com efeito operacional e compare-and-set | C08; frente 3 |
| Proposta de evolução | Evolução / agregado | Evidência de falha, qualidade, aprovação e capacidade de ativação independentes | C08; frente 4 |
| Telemetria e experimento comparável | Observabilidade / projeções e protocolo | Fonte journal/outbox; qualidade de uso explícita; sucesso depende de Q08 | C03/C08; frente 5 |

Forma ilustrativa dos contratos comuns (não implementação):

```text
CicloDeExecucao: runId, state, phase, terminationReason?
CandidateVersion: projectId, horizonId, artifactDigest, revisions
OrcamentoDoCiclo: policy, confirmed, reserved, uncertain
```

## Section 2 — Value Objects / Types / Interfaces

| Tipo | Contexto | Validação / semântica |
|---|---|---|
| RunId / TaskId / AttemptId | Comum | Não vazios, identidades diferentes; correlação não dá autorização |
| CandidateVersion | Comum | Igualdade pela tupla C01; conteúdo e baseline identificados; serialize/hash canônico a validar |
| EvidenceRecord / IndependentVerdict | Autoridade | Origem, papel, versão, resultado e digest verificáveis; texto autodeclarado não é prova |
| BudgetPolicy / UsageRecord | Execução | Unidades e limites válidos; uso measured/estimated/unknown; teto não reinicia por crash |
| WorkClaim / FencingToken | Planejamento | Recurso e baseline canônicos; token revogado/antigo impede escrita |
| EventEnvelope | Eventos | Fonte, versão, causa, correlação, dedupe e hops; sem segredo/conversa no payload |
| ExternalEffectRecord | Execução | Chave idempotente e estados PENDING/CONFIRMED/FAILED/UNKNOWN; confirmação externa necessária |
| OperatorDecision | Autoridade | Ator autenticado, versão e estado esperados; aprovação antiga não reaproveitável |
| EvolutionVersion | Evolução | Alvo explícito tema/regra/contrato; qualidade não equivale a ativação |
| ComparisonProtocol | Observabilidade | Referência/tarefas/aceite/limiares prévios e população completa; Q08 pendente |

Interfaces especializadas e validações detalhadas ficam nos suplementos. Não há contrato de seleção de fornecedor/modelo fixado pelo refinamento; os modelos desta equipe de análise não são escolha do runtime do produto.

Nomes locais como `AgentRuntimePort`, `TrustedAgentHostPort` e `AgentHostPort` são visões de um mesmo contrato de host Q04: ciclo de vida/uso, identidade/mediação e integração. Não são três runtimes nem três fontes independentes de identidade. ER-17 compõe essas capacidades no primeiro adapter concreto do host validado, reaproveitando o fixture quando adequado e preservando interfaces mínimas por consumidor; AD-03/ER-06 o usam na composição real. Da mesma forma, `AuthorityJournal` e `CycleJournal` são boundaries lógicos no journal TS; não impõem bancos físicos separados ou duplicação de ledger/outbox. C01–C08 são a autoridade sobre os nomes e a semântica compartilhada.

## Section 3 — Domain Services / Use Cases / Actions

| Operação | Coordenação e regra | Contrato |
|---|---|---|
| Iniciar ciclo | Validar política/capacidade, registrar objetivo/baseline e reservar primeiro despacho | C01–C03 |
| Executar iteração | Iniciar Builder, congelar versão, obter evidência independente, corrigir ou encerrar | C02/C04/C05 |
| Reconciliar consumo | Consolidar uso/estimativas e liberar reservas sem duplicar débito | C03 |
| Compilar plano | Consultar subgrafo/cobertura, ordenar dependências e alocar claims | C06 |
| Despachar evento | Validar fonte/assinatura, deduplicar, limitar propagação e reservar orçamento | C03/C06 |
| Avaliar promoção | Revalidar versão/contexto sob exclusão, aplicar risco/decisão e gate C existente | C04/C05 |
| Aplicar decisão humana | Registrar ator/versão e aprovar condicionado, rejeitar ou devolver à correção | C08 |
| Recuperar / descartar | Consultar journal e CBM, tratar efeitos incertos e preservar auditoria antes de invalidar retomada | C07 |
| Propor / ativar / reverter evolução | Separar origem, qualidade, aprovação e habilitação real de capacidade | C08 |
| Consultar execução / comparar valor | Projetar fonte própria, preservar qualidade do uso e aplicar protocolo equivalente | C03/C08 |

## Section 4 — Events / Messages / Async Flows

Os eventos de domínio estão em 001 e o envelope C06 em 005; cada suplemento detalha payload e consumidores. Nomes locais de transporte devem mapear explicitamente o evento de domínio, sem criar uma segunda linguagem de estados.

| Família | Produtor | Payload mínimo além de C06 | Consumidores |
|---|---|---|---|
| Ciclo/iteração/consumo/parada | Execução | Fase/estado/motivo, referências e qualidade de uso | Autoridade, Observabilidade |
| Plano/claim/tarefa/integração | Planejamento e Eventos | Plano/claim/token/versão e resultado parcial | Execução, Observabilidade |
| Sinal/assinatura/despacho/supressão | Planejamento e Eventos | Fonte, regra, motivo e reserva de orçamento | Execução, Observabilidade |
| Veredicto/risco/decisão/promoção | Autoridade e Decisão | Tupla candidata, evidência, ator, motivo, resultado | Execução, Observabilidade |
| Checkpoint/efeito/descarte/auditoria | Execução | Território, chave de efeito, estado conhecido/incerto e referências retidas | Evolução, Observabilidade |
| Proposta/qualidade/ativação/reversão | Evolução Supervisionada | Alvo/versão, origem, aprovação e versão ativa confirmada | Observabilidade, Operador |
| Protocolo/medição/comparação | Observabilidade e Valor | População, referência, métricas, qualidade e critérios prévios | Operador |

Outbox/inbox TS oferece atomicidade local e entrega ao menos uma vez; consumidores são idempotentes. Falha de projeção não muda decisão de origem. Evento tardio de versão antiga não libera promoção. Despacho não nasce de texto do briefing nem de reindexação tomada como prova de violação.

## Section 5 — Persistence / Repository / Data Access Interfaces

| Recurso / port | Ações lógicas propostas | Resultado / estado esperado |
|---|---|---|
| RunRepository / BudgetLedger | Criar/carregar/avançar por versão; reservar/reconciliar | Transação SQLite TS e compare-and-set; falha não despacha efeito |
| CheckpointRepository / EffectJournal | Gravar intenção/resultado, consultar pendências e invalidar retomada | Journal consistente, efeitos UNKNOWN explícitos |
| PlanRepository / ClaimRepository | Versionar plano; adquirir/renovar/revogar/transferir claim | Aquisição atômica; fencing obrigatório |
| EventInbox / Outbox | Persistir/deduplicar/ack/retry/dead-letter | Sem exactly-once remoto; orçamento e replay controlados |
| EvidenceRepository / DecisionRepository | Selar evidência/registrar decisão e intenção de promoção | Vínculo C01; origem/versão/política atuais |
| AuditRepository / EvolutionRepository | Reter/projetar auditoria; controlar proposta/ativação/reversão | Proteção de registros não consumidos; Q06/Q07 pendentes |
| MetricsProjection / ProtocolRepository | Consultar execução e registrar protocolo/comparação | Fonte própria rastreável; sem conclusão de valor sem Q08 |
| AgentHostPort | Iniciar/cancelar/isolar/medir/diagnosticar-reiniciar; recuperar apenas com F05 | Contrato Q04 a provar, não API existente do CBM |
| CbmPort | Chamar tools reais por ACL e interpretar resultados | Validar schemas/erros/revisões; sem escrita direta no SQLite C |
| PromotionPort / OperatorPort | Canal privilegiado e decisão autenticada | Builder sem acesso; gate determinístico existente preservado |
| GraphNavigationPort / EvolutionActivationPort | Validar alvo navegável; ativar/reverter versão autorizada | Capacidade precisa prova; sem fallback que declare PBI concluído |

O journal lógico pertence à Execução; outros contextos possuem suas tabelas/projeções e acessam por contrato. Compartilhar um arquivo SQLite TS futuramente não autoriza escrita cruzada nem implica múltiplos bancos obrigatórios. As fronteiras de consistência são locais; não há transação atômica com o CBM ou ferramenta externa. Para promoção, confirmar efeito no CBM antes de marcar sucesso; timeout é incerteza a reconciliar.

## Suplementos por frente

| Frente | Escopo normativo do suplemento | Documento |
|---|---|---|
| 1 | F01/F05, PBI-01–05 e 19–22 | [Execução e recuperação](fronts/01-execution-recovery.md) |
| 2 | F02/F03, PBI-06–14 | [Planejamento e eventos](fronts/02-planning-events.md) |
| 3 | F04/F06, PBI-15–18 e 23–26 | [Autoridade e decisão](fronts/03-authority-decision.md) |
| 4 | F07, PBI-27–30 | [Evolução supervisionada](fronts/04-supervised-evolution.md) |
| 5 | F08, PBI-31–32 | [Observabilidade e valor](fronts/05-observability-value.md) |

Os suplementos são partes deste desenho por projeto, não cinco projetos fictícios. Divergências devem ser reconciliadas com 005; não se escolhe silenciosamente uma versão diferente de um contrato. O registro de revisão identifica correções.

## Condições para desenvolvimento

O [sequenciamento](007-delivery-sequence.md) define S0–S6 e dependências cruzadas. Tasks são backlog técnico proposto, não compromisso antes de S0. Estimativas devem ser recalibradas depois do spike; uma task que exceder a sessão prevista deve ser dividida antes de implementação. Q04–Q08 continuam pendentes; domínio/políticas não ganham certeza por aparecerem na lista de tarefas.

O JSON consolida as tasks com IDs numéricos e preserva source_task_id. depends_on é o predecessor principal; **todos** os IDs de requires são pré-requisitos conjuntivos. requires_by_target adiciona somente o adapter do alvo escolhido; requires_by_capability e requires_by_protocol adicionam fontes conforme as capacidades habilitadas e o protocolo declarado. Uma fonte futura não bloqueia medição de S1, mas sua habilitação exige revalidar a cobertura; alvo parcial não conclui PBI-31 integral. delivery_stage é slice, não phase do ciclo; revalidation_stage_by_capability explicita extensões posteriores. A ordem dos pré-requisitos incondicionais é topológica, sem obrigar serializar tarefas independentes. SP-01–05 descrevem o spike futuro; produção exige S0_APPROVED além de SP-05 concluída, sem prova realizada. Estimativas são hipóteses de sessão; aceitação operacional exige todos os pré-requisitos aplicáveis.

## Section 6 — Ordered Development Tasks

```json
[
  {
    "id": "01",
    "title": "Modelar versão candidata e evidência vinculada",
    "description": "Define tipos imutáveis que vinculam artefato, baseline, revisões e evidências à identidade exata C01.",
    "scope": [
      "CandidateVersionRef (C01)",
      "EvidenceRecord (C04)",
      "IndependentVerdict (C04)"
    ],
    "acceptance": [
      "Qualquer mudança de artefato ou revisão relevante exige uma versão nova",
      "Evidência não pode ser vinculada sem identidade, digest e tupla C01"
    ],
    "depends_on": null,
    "source_task_id": "AD-01",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S0",
    "estimate_hours": 3,
    "pbis": [
      "PBI-18",
      "PBI-23",
      "PBI-24",
      "PBI-26"
    ],
    "requires": []
  },
  {
    "id": "02",
    "title": "Observar contratos reais do host e do MCP",
    "description": "Registra comandos, schemas, limites e respostas das cinco operações Q04 antes de construir o fixture do spike.",
    "scope": [
      "Protocolo do spike ADR-005",
      "Relatório de capacidade host/MCP",
      "Fixture isolado sem mutação do core"
    ],
    "acceptance": [
      "Cada operação tem comando/resposta e limite rastreável ou gap explícito",
      "Nenhum alias do briefing é tratado como endpoint real"
    ],
    "depends_on": null,
    "source_task_id": "SP-01",
    "source_document": "006-capability-matrix.md / 007-delivery-sequence.md",
    "delivery_stage": "S0",
    "estimate_hours": 3,
    "pbis": [
      "PBI-01",
      "PBI-16"
    ],
    "requires": []
  },
  {
    "id": "03",
    "title": "Montar fixture mínimo com decisão e custo duráveis",
    "description": "Exercita Builder, verificador e gate existente sobre mudança trivial com journal separado e medição observável.",
    "scope": [
      "Fixture TS isolado do spike",
      "Journal mínimo do experimento",
      "Captura de saída/uso do host"
    ],
    "acceptance": [
      "Ciclo registra versão, veredicto, decisão e resultado CBM",
      "Uso/custo têm origem e qualidade persistidas"
    ],
    "depends_on": "02",
    "source_task_id": "SP-02",
    "source_document": "006-capability-matrix.md / 007-delivery-sequence.md",
    "delivery_stage": "S0",
    "estimate_hours": 4,
    "pbis": [
      "PBI-01",
      "PBI-05",
      "PBI-31"
    ],
    "requires": [
      "01",
      "02"
    ]
  },
  {
    "id": "04",
    "title": "Provar fronteira de identidade e despacho por perfil",
    "description": "Executa a parte de autoridade do spike Q04 para verificar identidade confiável e recusa de chamadas diretas ou alternativas do Builder, sem substituir os demais critérios do ADR-005.",
    "scope": [
      "TrustedAgentHostPort (proposta)",
      "ProfilePermissionPolicy (C05)",
      "Configuração de transporte e credenciais do ambiente controlado"
    ],
    "acceptance": [
      "Evidência reproduzível demonstra recusa de promoção direta e das rotas alternativas cobertas sem efeito no CBM",
      "Qualquer rota alcançável sem autorização deixa Q04 aberto e bloqueia autonomia",
      "O resultado desta fatia não marca Q04 concluído sem os demais critérios do ADR-005"
    ],
    "depends_on": "01",
    "source_task_id": "AD-02",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S0",
    "estimate_hours": 4,
    "pbis": [
      "PBI-15",
      "PBI-16",
      "PBI-17"
    ],
    "requires": [
      "01",
      "03"
    ]
  },
  {
    "id": "05",
    "title": "Exercitar kill reinício e cancelamento do fixture",
    "description": "Coleta evidência de reinício consistente e cancelamento controlável sem assumir retomada F05.",
    "scope": [
      "Fixture de falha de processo",
      "Journal e estado CBM antes/depois",
      "Relatório de cancelamento"
    ],
    "acceptance": [
      "Kill/reinício não corrompe estado nem repete efeito incerto",
      "Cancelamento efetivo ou limitação é demonstrado e registrado"
    ],
    "depends_on": "03",
    "source_task_id": "SP-03",
    "source_document": "006-capability-matrix.md / 007-delivery-sequence.md",
    "delivery_stage": "S0",
    "estimate_hours": 4,
    "pbis": [
      "PBI-01",
      "PBI-05"
    ],
    "requires": [
      "03"
    ]
  },
  {
    "id": "06",
    "title": "Exercitar escopo autoridade e mudança de versão",
    "description": "Tenta escrita fora do intent scope, promoção por Builder e promoção com evidência antiga para provar as invariantes do slice.",
    "scope": [
      "Fixture de autorização do spike",
      "Transporte de agente controlado",
      "Evidência de versão e recusas"
    ],
    "acceptance": [
      "Operação fora do scope e promoção proibida não produzem efeito",
      "Mudança de versão e verificador indisponível impedem promoção"
    ],
    "depends_on": "05",
    "source_task_id": "SP-04",
    "source_document": "006-capability-matrix.md / 007-delivery-sequence.md",
    "delivery_stage": "S0",
    "estimate_hours": 4,
    "pbis": [
      "PBI-01",
      "PBI-16",
      "PBI-18"
    ],
    "requires": [
      "04",
      "05"
    ]
  },
  {
    "id": "07",
    "title": "Consolidar os cinco critérios de viabilidade",
    "description": "Reconcilia resultados reproduzíveis do spike e propõe o destino do ADR sem comprometer produção diante de falha.",
    "scope": [
      "Relatório dos cinco critérios ADR-005",
      "Matriz de capacidades atualizada",
      "Registro da decisão apropriada sobre ADR-005"
    ],
    "acceptance": [
      "Os cinco critérios e invariantes têm resultado/evidência explícitos, inclusive FAIL, INCONCLUSIVE e UNAVAILABLE",
      "Resultado negativo conclui o registro SP-05, mas mantém S0_APPROVED bloqueado; conclusão da tarefa não autoriza produção"
    ],
    "depends_on": "06",
    "source_task_id": "SP-05",
    "source_document": "006-capability-matrix.md / 007-delivery-sequence.md",
    "delivery_stage": "S0",
    "estimate_hours": 2,
    "pbis": [
      "PBI-01",
      "PBI-16",
      "PBI-31"
    ],
    "requires": [
      "04",
      "05",
      "06"
    ]
  },
  {
    "id": "08",
    "title": "Definir contratos de ciclo e versão",
    "description": "Define tipos e invariantes do ciclo alinhados a C01/C02 antes de qualquer execução autônoma.",
    "scope": [
      "packages/cbm-orchestrator/src/execution/ExecutionCycle.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionIds.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionContracts.ts"
    ],
    "acceptance": [
      "State, phase e terminationReason mantêm enums C02 separados",
      "Mutação relevante cria versão candidata nova e invalida evidência anterior"
    ],
    "depends_on": "07",
    "source_task_id": "ER-01",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-01",
      "PBI-04",
      "PBI-05"
    ],
    "requires": [
      "07"
    ]
  },
  {
    "id": "09",
    "title": "Modelar orçamento e reservas",
    "description": "Modela política e ledger para que nenhuma chamada seja despachada sem consumo reservado e identificável.",
    "scope": [
      "packages/cbm-orchestrator/src/budget/BudgetPolicy.ts",
      "packages/cbm-orchestrator/src/budget/BudgetLedger.ts",
      "packages/cbm-orchestrator/src/budget/UsageRecord.ts"
    ],
    "acceptance": [
      "Reserva contabiliza consumo confirmado e pendente sem somar unidades incompatíveis",
      "Uso desconhecido suspende novas ações quando não há limite conservador"
    ],
    "depends_on": "08",
    "source_task_id": "ER-02",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-02",
      "PBI-03"
    ],
    "requires": [
      "07",
      "08"
    ]
  },
  {
    "id": "10",
    "title": "Definir registros de efeitos e idempotência",
    "description": "Modela intenção, resultado e chave idempotente para efeitos remotos, inclusive a lacuna UNKNOWN.",
    "scope": [
      "packages/cbm-orchestrator/src/recovery/ExternalEffectRecord.ts",
      "packages/cbm-orchestrator/src/recovery/EffectStatus.ts",
      "packages/cbm-orchestrator/src/recovery/IdempotencyKey.ts"
    ],
    "acceptance": [
      "Status de efeito usa apenas os valores de C07",
      "Efeito sem resultado permanece UNKNOWN e a chave deduplica o comando, não o ordinal da tentativa"
    ],
    "depends_on": "08",
    "source_task_id": "ER-03",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-01",
      "PBI-02",
      "PBI-05",
      "PBI-20"
    ],
    "requires": [
      "07",
      "08"
    ]
  },
  {
    "id": "11",
    "title": "Implementar transições do ciclo",
    "description": "Aplica guardas C02 para início, pausa, retomada validada e conclusão com motivo independente da fase.",
    "scope": [
      "packages/cbm-orchestrator/src/execution/ExecutionCycle.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionTransitions.ts",
      "packages/cbm-orchestrator/src/execution/TerminationReason.ts"
    ],
    "acceptance": [
      "Somente transições C02 são aceitas",
      "COMPLETED não implica aceitação ou promoção"
    ],
    "depends_on": "08",
    "source_task_id": "ER-04",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 4,
    "pbis": [
      "PBI-01",
      "PBI-03",
      "PBI-05"
    ],
    "requires": [
      "07",
      "08"
    ]
  },
  {
    "id": "12",
    "title": "Definir portas de runtime e verificação",
    "description": "Expõe contratos lógicos para iniciar, cancelar, medir e verificar sem selecionar host ou alegar capacidade existente.",
    "scope": [
      "packages/cbm-orchestrator/src/ports/AgentRuntimePort.ts",
      "packages/cbm-orchestrator/src/ports/IndependentVerifierPort.ts",
      "packages/cbm-orchestrator/src/ports/KnowledgeStatePort.ts"
    ],
    "acceptance": [
      "Perfis são fornecidos por identidade confiável fora da autodeclaração do Builder",
      "Operações não mapeadas ficam explicitamente indisponíveis e bloqueiam autonomia dependente"
    ],
    "depends_on": "08",
    "source_task_id": "ER-05",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-01",
      "PBI-04",
      "PBI-16"
    ],
    "requires": [
      "07",
      "08"
    ]
  },
  {
    "id": "13",
    "title": "Persistir journal mínimo do ciclo e outbox local",
    "description": "Armazena estado mínimo de F01 e eventos locais atomicamente; os checkpoints F05 são uma extensão separada.",
    "scope": [
      "packages/cbm-orchestrator/src/persistence/CycleJournal.ts",
      "packages/cbm-orchestrator/src/persistence/SqliteCycleJournal.ts",
      "packages/cbm-orchestrator/src/persistence/Outbox.ts"
    ],
    "acceptance": [
      "Atualização usa CAS e chave idempotente",
      "Journal mínimo e outbox local confirmam ou revertem juntos sem incluir checkpoint F05"
    ],
    "depends_on": "08",
    "source_task_id": "ER-07",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 4,
    "pbis": [
      "PBI-05",
      "PBI-31"
    ],
    "requires": [
      "07",
      "08",
      "10"
    ]
  },
  {
    "id": "14",
    "title": "Implementar ledger concorrente de uso",
    "description": "Integra reservas, uso pendente e reconciliação ao journal antes de despachar trabalho.",
    "scope": [
      "packages/cbm-orchestrator/src/budget/BudgetLedgerPort.ts",
      "packages/cbm-orchestrator/src/persistence/SqliteBudgetLedger.ts",
      "packages/cbm-orchestrator/src/budget/ReconcileUsage.ts"
    ],
    "acceptance": [
      "Reserva simultânea não excede saldo por corrida de atualização",
      "Uso ausente não é lançado como zero e bloqueia novos despachos sem cobertura"
    ],
    "depends_on": "09",
    "source_task_id": "ER-08",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-02",
      "PBI-03"
    ],
    "requires": [
      "07",
      "09",
      "13"
    ]
  },
  {
    "source_task_id": "ER-17",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "title": "Integrar o primeiro host validado ao runtime",
    "description": "Reaproveita a integração do fixture SP-02 quando adequada e conecta um único host aprovado às portas e à composição real do ciclo.",
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
    "estimate_hours": 4,
    "pbis": [
      "PBI-01",
      "PBI-02",
      "PBI-04",
      "PBI-05",
      "PBI-16",
      "PBI-31"
    ],
    "id": "15",
    "requires": [
      "04",
      "07",
      "12",
      "13",
      "14"
    ],
    "depends_on": "12"
  },
  {
    "id": "16",
    "title": "Separar verificação e validar proveniência da evidência",
    "description": "Conecta um verificador em processo/contexto separado e traduz saídas observadas para o dossiê da versão candidata.",
    "scope": [
      "IndependentVerificationPort (proposta)",
      "EvidenceDossier",
      "ACL do Harness/runtime"
    ],
    "acceptance": [
      "Saída consultável identifica versão, origem e status, e indisponibilidade ou inconclusão nunca produz GREEN",
      "Família de modelo, se diferente, é registrada como hipótese experimental e não como prova de isolamento"
    ],
    "depends_on": "04",
    "source_task_id": "AD-03",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-17",
      "PBI-18"
    ],
    "requires": [
      "01",
      "04",
      "07",
      "15"
    ]
  },
  {
    "id": "17",
    "title": "Implementar política determinística antes do gate C",
    "description": "Aplica a invariante comum de evidência verde e atual antes de solicitar a operação ao gate C existente.",
    "scope": [
      "PromotionDecision (C04)",
      "DeterministicGatekeeper",
      "ProfilePermissionPolicy (C05)"
    ],
    "acceptance": [
      "Só a tupla C01 atual com veredicto independente GREEN e âncoras válidas pode resultar em ALLOW",
      "ALLOW autoriza apenas a solicitação e não substitui nem presume aceitação pelo gate C",
      "Aprovação humana e fast path não criam exceção à invariante"
    ],
    "depends_on": "16",
    "source_task_id": "AD-04",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-15",
      "PBI-16",
      "PBI-18"
    ],
    "requires": [
      "04",
      "07",
      "16"
    ]
  },
  {
    "id": "18",
    "title": "Persistir intenção e resultado de promoção",
    "description": "Grava intenção versionada e outbox local antes da chamada; reconcilia resultado externo sem tratar timeout como sucesso.",
    "scope": [
      "AuthorityJournal (boundary lógico C07)",
      "PromotionIntent",
      "OutboxEnvelope"
    ],
    "acceptance": [
      "Intenção versionada e outbox são duráveis com CAS antes da chamada externa",
      "Falha de persistência bloqueia o despacho; timeout posterior permanece UNKNOWN até reconciliação"
    ],
    "depends_on": "01",
    "source_task_id": "AD-07",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-16",
      "PBI-18"
    ],
    "requires": [
      "01",
      "04",
      "07",
      "13",
      "16",
      "17"
    ]
  },
  {
    "id": "19",
    "title": "Integrar promoção e reconciliar corrida de versão",
    "description": "Revalida e congela a versão antes da tool MCP observada, confirmando o resultado ou preservando estado incerto.",
    "scope": [
      "PromotionPort",
      "Adapter da tool promote_horizon",
      "Reconciliador de estado e resultado",
      "GlobalBudgetPort C03"
    ],
    "acceptance": [
      "Mudança entre verificação e chamada cancela a promoção; sem prova de exclusão de mutações não há despacho",
      "Reserva do ledger global antecede a chamada e timeout mantém custo/efeito pendente até reconciliação",
      "Nenhum timeout é repetido às cegas"
    ],
    "depends_on": "18",
    "source_task_id": "AD-09",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S1",
    "estimate_hours": 4,
    "pbis": [
      "PBI-16",
      "PBI-18"
    ],
    "requires": [
      "04",
      "07",
      "14",
      "16",
      "17",
      "18"
    ]
  },
  {
    "id": "20",
    "title": "Coordenar iterações após seus ports e stores",
    "description": "Coordena saída do Builder, congelamento/selagem da candidata, verificação independente e correção versionada.",
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
    "depends_on": "11",
    "source_task_id": "ER-06",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 4,
    "pbis": [
      "PBI-01",
      "PBI-04",
      "PBI-16"
    ],
    "requires": [
      "04",
      "07",
      "08",
      "09",
      "11",
      "12",
      "13",
      "14",
      "15",
      "16",
      "17"
    ]
  },
  {
    "id": "21",
    "title": "Aplicar breaker e cancelamento",
    "description": "Pausa despachos por critério configurado e reconcilia chamadas em voo após cancelamento.",
    "scope": [
      "packages/cbm-orchestrator/src/execution/ApplyCircuitBreaker.ts",
      "packages/cbm-orchestrator/src/execution/CancelExecution.ts",
      "packages/cbm-orchestrator/src/execution/ExecutionCoordinator.ts"
    ],
    "acceptance": [
      "Falhas repetidas da mesma classe pausam e geram escalonamento",
      "Cancelamento bloqueia novos despachos e registra custo/efeito residual"
    ],
    "depends_on": "20",
    "source_task_id": "ER-11",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-02",
      "PBI-03",
      "PBI-05"
    ],
    "requires": [
      "07",
      "12",
      "13",
      "14",
      "20"
    ]
  },
  {
    "id": "22",
    "title": "Projetar relatório final e telemetria auditáveis",
    "description": "Projeta relatório final e telemetria mínima do journal/ledger preservando desconhecidos; árvore F05 é tratada pela tarefa de checkpoint.",
    "scope": [
      "packages/cbm-orchestrator/src/execution/FinalReport.ts",
      "packages/cbm-orchestrator/src/telemetry/ExecutionEvents.ts",
      "packages/cbm-orchestrator/src/telemetry/ReportProjection.ts"
    ],
    "acceptance": [
      "Relatório distingue custo conhecido, estimado, pendente e desconhecido",
      "Conclusão do ciclo não é convertida em promoção e lacunas permanecem explícitas"
    ],
    "depends_on": "13",
    "source_task_id": "ER-12",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-05",
      "PBI-31"
    ],
    "requires": [
      "07",
      "13",
      "14",
      "21"
    ]
  },
  {
    "id": "23",
    "title": "Diagnosticar crash pré-F05 sem retomar checkpoint",
    "description": "Fecha o run interrompido para diagnóstico e, se houver nova execução, parte do objetivo original com novo runId/parentRunId sem checkpoint ou replay cego.",
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
    "depends_on": "22",
    "source_task_id": "ER-10",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S1",
    "estimate_hours": 4,
    "pbis": [
      "PBI-05"
    ],
    "requires": [
      "07",
      "08",
      "09",
      "11",
      "12",
      "13",
      "14",
      "20",
      "22"
    ]
  },
  {
    "id": "24",
    "title": "Mapear e instrumentar fatos mínimos na origem",
    "description": "Mapeia eventos de domínio aos fatos PBI-31 do slice S1 para que cada contexto produtor os publique junto ao próprio journal/outbox; preserva fonte independente de stack externa.",
    "scope": [
      "packages/cbm-orchestrator/src/events/DomainEventPort.ts",
      "packages/cbm-orchestrator/src/telemetry/TelemetryFact.ts",
      "packages/cbm-orchestrator/src/telemetry/TelemetryEventMapping.ts"
    ],
    "acceptance": [
      "Fatos de veredicto, custo/uso e orçamento são vinculados a IDs C01 e cada produtor os grava com sua transação local",
      "Outbox pode ser reentregue sem eliminar os fatos duráveis para consulta/rebuild"
    ],
    "depends_on": "13",
    "source_task_id": "OV-01",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S1",
    "estimate_hours": 4,
    "pbis": [
      "PBI-31"
    ],
    "requires": [
      "07",
      "13",
      "14"
    ]
  },
  {
    "id": "25",
    "title": "Projetar e reconstruir telemetria de forma idempotente",
    "description": "Consome fatos/outbox com dedupe e cria projeção local que pode ser reconstituída da fonte ainda retida.",
    "scope": [
      "packages/cbm-orchestrator/src/telemetry/ProjectTelemetryBatch.ts",
      "packages/cbm-orchestrator/src/telemetry/RebuildTelemetryProjection.ts",
      "packages/cbm-orchestrator/src/persistence/TelemetryProjectionRepository.ts"
    ],
    "acceptance": [
      "Replay do mesmo evento não duplica valores nem contagens",
      "Rebuild da mesma fonte produz a mesma projeção e registra lacunas de cobertura"
    ],
    "depends_on": "24",
    "source_task_id": "OV-02",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-31"
    ],
    "requires": [
      "07",
      "24"
    ]
  },
  {
    "id": "26",
    "title": "Consultar telemetria por ciclo, candidato e tentativa",
    "description": "Expõe read models locais, separa dimensões e qualidade, e apresenta atualidade/cobertura.",
    "scope": [
      "packages/cbm-orchestrator/src/telemetry/QueryTelemetry.ts",
      "packages/cbm-orchestrator/src/telemetry/TelemetryReport.ts",
      "packages/cbm-orchestrator/src/telemetry/UsageObservation.ts"
    ],
    "acceptance": [
      "Filtros por runId/candidateVersion/attemptId não cruzam candidatos ou execuções",
      "Tokens, cada moeda e tempo permanecem separados; UNKNOWN não vira zero"
    ],
    "depends_on": "25",
    "source_task_id": "OV-03",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S1",
    "estimate_hours": 3,
    "pbis": [
      "PBI-31"
    ],
    "requires": [
      "07",
      "25"
    ]
  },
  {
    "id": "27",
    "title": "Proteger consultas e auditoria minimizada",
    "description": "Aplica escopo autenticado, minimização de payload e proteção para registros requeridos por retrospectiva.",
    "scope": [
      "packages/cbm-orchestrator/src/telemetry/TelemetryAccessPolicy.ts",
      "packages/cbm-orchestrator/src/telemetry/TelemetryRedactor.ts"
    ],
    "acceptance": [
      "Solicitante fora do escopo não consulta projeto alheio",
      "Projeção não persiste conversa/segredo e não apaga auditoria não consumida; política temporal fica sem valor inventado"
    ],
    "depends_on": "26",
    "source_task_id": "OV-04",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S1",
    "estimate_hours": 2,
    "pbis": [
      "PBI-31"
    ],
    "requires": [
      "04",
      "07",
      "26"
    ]
  },
  {
    "id": "28",
    "title": "Definir snapshot de checkpoint F05",
    "description": "Modela o snapshot recuperável da árvore de tentativas e seus vínculos sem criar dependência F05 para o journal mínimo.",
    "scope": [
      "packages/cbm-orchestrator/src/recovery/ExecutionCheckpoint.ts",
      "packages/cbm-orchestrator/src/recovery/CheckpointSnapshot.ts",
      "packages/cbm-orchestrator/src/recovery/AttemptTree.ts"
    ],
    "acceptance": [
      "Snapshot referencia run, território, tupla C01, tentativas e mapa de efeitos C07",
      "Conteúdo sensível é minimizado sem prometer restauração de raciocínio interno nem apagar referência de deduplicação"
    ],
    "depends_on": "10",
    "source_task_id": "ER-14",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S2",
    "estimate_hours": 3,
    "pbis": [
      "PBI-19",
      "PBI-21"
    ],
    "requires": [
      "07",
      "08",
      "10"
    ]
  },
  {
    "id": "29",
    "title": "Estender journal com persistência de checkpoint F05",
    "description": "Adiciona armazenamento transacional de checkpoints F05 sobre o journal mínimo já utilizável no slice S1.",
    "scope": [
      "packages/cbm-orchestrator/src/persistence/CycleJournal.ts",
      "packages/cbm-orchestrator/src/persistence/SqliteCheckpointRepository.ts",
      "packages/cbm-orchestrator/src/recovery/CheckpointSnapshot.ts"
    ],
    "acceptance": [
      "Checkpoint e referências/outbox locais são persistidos atomicamente com CAS",
      "Falha de commit preserva o último checkpoint íntegro e não exige escrita no store C"
    ],
    "depends_on": "28",
    "source_task_id": "ER-15",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S2",
    "estimate_hours": 3,
    "pbis": [
      "PBI-19"
    ],
    "requires": [
      "07",
      "10",
      "13",
      "28"
    ]
  },
  {
    "id": "30",
    "title": "Persistir checkpoints e árvore diagnóstica de F05",
    "description": "Liga tentativas e checkpoints ao território no journal depois que a extensão de persistência F05 estiver disponível.",
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
    "depends_on": "29",
    "source_task_id": "ER-09",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S2",
    "estimate_hours": 3,
    "pbis": [
      "PBI-19",
      "PBI-21"
    ],
    "requires": [
      "07",
      "10",
      "13",
      "28",
      "29"
    ]
  },
  {
    "id": "31",
    "title": "Coordenar descarte e preservação de auditoria",
    "description": "Executa descarte repetível do território e checkpoints, mantendo trilha elegível para retrospectiva.",
    "scope": [
      "packages/cbm-orchestrator/src/recovery/DiscardTerritory.ts",
      "packages/cbm-orchestrator/src/recovery/AuditRecord.ts",
      "packages/cbm-orchestrator/src/ports/KnowledgeStatePort.ts"
    ],
    "acceptance": [
      "Checkpoints ficam não retomáveis e tombstone/auditoria são persistidos",
      "Falha parcial mantém diagnóstico e não afirma desfazer efeito externo"
    ],
    "depends_on": "30",
    "source_task_id": "ER-13",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S2",
    "estimate_hours": 4,
    "pbis": [
      "PBI-22"
    ],
    "requires": [
      "07",
      "12",
      "13",
      "21",
      "30"
    ]
  },
  {
    "id": "32",
    "title": "Retomar execução segura pós-F05",
    "description": "Valida checkpoint, tupla e estado externo antes de reconstruir trabalho seguro e retomar por política C07.",
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
    "depends_on": "30",
    "source_task_id": "ER-16",
    "source_document": "fronts/01-execution-recovery.md",
    "delivery_stage": "S2",
    "estimate_hours": 4,
    "pbis": [
      "PBI-20"
    ],
    "requires": [
      "07",
      "12",
      "13",
      "14",
      "30"
    ]
  },
  {
    "id": "33",
    "title": "Definir snapshot de topologia e cobertura",
    "description": "Preserva proveniência, revisões, anchors consultáveis e lacunas antes de compilar trabalho.",
    "scope": [
      "AffectedSubgraphSnapshot",
      "TopologyReadPort",
      "CoverageState"
    ],
    "acceptance": [
      "Snapshot sempre identifica projeto, fonte, revisões e gaps",
      "PARTIAL/STALE/UNKNOWN nunca são normalizados para COMPLETE"
    ],
    "depends_on": "07",
    "source_task_id": "PE-01",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 3,
    "pbis": [
      "PBI-06"
    ],
    "requires": [
      "07"
    ],
    "capability_gates": [
      "Adapter MCP e cobertura acessível mapeados; anchors permanecem condicionais à capacidade comprovada"
    ]
  },
  {
    "id": "34",
    "title": "Compilar plano DAG versionado",
    "description": "Produz tarefas e dependências a partir do snapshot sem inferir paralelismo por contagem de comunidades.",
    "scope": [
      "PlanSnapshot",
      "TaskDependency",
      "CompileWorkPlan"
    ],
    "acceptance": [
      "SCC cíclica vira unidade serial bloqueada de replanejamento; componentes acíclicos não são descartados",
      "Independência não é inferida de edge ausente ou cobertura incompleta"
    ],
    "depends_on": "33",
    "source_task_id": "PE-02",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 4,
    "pbis": [
      "PBI-06",
      "PBI-08"
    ],
    "requires": [
      "07",
      "33"
    ],
    "capability_gates": [
      "Política de concorrência explícita e ledger global C03 disponível"
    ]
  },
  {
    "id": "35",
    "title": "Implementar claim com lease, sobreposição e fencing",
    "description": "Coordena recursos efetivos no store TS e impede escrita por conflito de escopo, alias ou token revogado.",
    "scope": [
      "WorkClaim",
      "WorkClaimStorePort",
      "CanonicalResourceRef",
      "FencingToken"
    ],
    "acceptance": [
      "Aquisição concorrente de escopos sobrepostos concede no máximo um token corrente",
      "Boundary revalida alias/escopo e transferência revoga token antigo antes de novo grant"
    ],
    "depends_on": "33",
    "source_task_id": "PE-03",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 4,
    "pbis": [
      "PBI-07"
    ],
    "requires": [
      "04",
      "07",
      "13",
      "33"
    ],
    "capability_gates": [
      "Q04/ADR-005 comprova store compartilhado e mediação de todo caminho de escrita"
    ]
  },
  {
    "id": "36",
    "title": "Revalidar anchors após drift",
    "description": "Invalida escopo e claims quando path, baseline, âncora ou cobertura muda.",
    "scope": [
      "AnchorValidationPort",
      "RevalidatePlanSnapshot",
      "PlanSnapshotInvalidated"
    ],
    "acceptance": [
      "Renome/drift não mantém âncora válida por suposição",
      "Sem capacidade de revalidação, plano reduz paralelismo ou bloqueia"
    ],
    "depends_on": "35",
    "source_task_id": "PE-04",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 3,
    "pbis": [
      "PBI-06",
      "PBI-07"
    ],
    "requires": [
      "07",
      "35"
    ],
    "capability_gates": [
      "Matriz 006 atualizada com superfície de validação acessível"
    ]
  },
  {
    "id": "37",
    "title": "Bloquear seam sem evidência atual",
    "description": "Exige evidência consultável para liberar trabalhos que tocam contrato comum.",
    "scope": [
      "SeamEvidenceRef",
      "SeamEvidencePort",
      "EvaluateSharedSeam"
    ],
    "acceptance": [
      "Somente evidência atual compatível libera a política de paralelismo",
      "Ausente/inconclusiva/stale mantém serialização"
    ],
    "depends_on": "34",
    "source_task_id": "PE-05",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 3,
    "pbis": [
      "PBI-09"
    ],
    "requires": [
      "07",
      "34",
      "36"
    ],
    "capability_gates": [
      "Capacidade parcial do Harness validada no adapter; sem equivalência semântica presumida"
    ]
  },
  {
    "id": "38",
    "title": "Integrar resultados de tarefas com verificação do conjunto",
    "description": "Preserva sucesso parcial e impede que composição incompleta seja tratada como candidata verificada.",
    "scope": [
      "IntegrateTaskResults",
      "ParallelResultsIntegrated",
      "CandidateVersionRef"
    ],
    "acceptance": [
      "Falha de tarefa não apaga resultado independente aprovado",
      "Integração não libera promoção sem veredicto C04 atual"
    ],
    "depends_on": "34",
    "source_task_id": "PE-06",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 4,
    "pbis": [
      "PBI-10"
    ],
    "requires": [
      "07",
      "16",
      "17",
      "34",
      "37"
    ],
    "capability_gates": [
      "Contrato de evidência C04 e port de verificação da frente F04"
    ]
  },
  {
    "id": "39",
    "title": "Validar origem de sinais de grafo",
    "description": "Normaliza somente sinais com semântica e evidência comprovadas em eventos acionáveis.",
    "scope": [
      "EventSourcePort",
      "ValidateGraphSignal",
      "EventEnvelope"
    ],
    "acceptance": [
      "Watcher reindex isolado não gera violação de seam",
      "Trace ingerido sem anomalia provada não gera dispatch"
    ],
    "depends_on": "07",
    "source_task_id": "PE-07",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 3,
    "pbis": [
      "PBI-11",
      "PBI-12"
    ],
    "requires": [
      "07"
    ],
    "capability_gates": [
      "Fonte e detector específicos validados; runtime de agente Q04 não assumido"
    ]
  },
  {
    "id": "40",
    "title": "Versionar assinaturas de evento",
    "description": "Mantém filtros, evidência e alvo imutáveis por revisão e autoridade do operador.",
    "scope": [
      "EventSubscription",
      "SignatureRegistryPort",
      "SignatureRevisionRef"
    ],
    "acceptance": [
      "Mudança cria revisão nova autorizada",
      "Entrega enfileirada conserva a revisão originalmente avaliada"
    ],
    "depends_on": "07",
    "source_task_id": "PE-08",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 3,
    "pbis": [
      "PBI-13"
    ],
    "requires": [
      "07"
    ],
    "capability_gates": [
      "Identidade/autorização C05 validada para quem publica; runtime de dispatch é pré-requisito apenas para ativar entrega"
    ]
  },
  {
    "id": "41",
    "title": "Persistir inbox e outbox deduplicadas",
    "description": "Torna a entrega repetível após crash sem duplicar o mesmo gatilho lógico.",
    "scope": [
      "EventJournalPort",
      "EventDelivery",
      "LogicalDedupeKey"
    ],
    "acceptance": [
      "Inbox compara eventId e chave lógica tipo+fonte+objeto+versão",
      "Outbox é durável no journal lógico C07, sem afirmar atomicidade remota nem exigir SQLite físico separado"
    ],
    "depends_on": "13",
    "source_task_id": "PE-09",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 4,
    "pbis": [
      "PBI-11",
      "PBI-12",
      "PBI-14"
    ],
    "requires": [
      "07",
      "13"
    ],
    "capability_gates": [
      "Journal TS C07 e estratégia CAS/idemência disponíveis"
    ]
  },
  {
    "id": "42",
    "title": "Aplicar orçamento e limites globais ao dispatch",
    "description": "Debita no C03 antes de cada gatilho, retry ou replay e limita recursão, hops e fan-out.",
    "scope": [
      "DispatchBoundEvent",
      "GlobalBudgetPort",
      "EventDeliveryLimits"
    ],
    "acceptance": [
      "Nenhum dispatch ocorre sem reserva no ledger GLOBAL",
      "Loop/hops/fan-out excedido é suprimido ou dead-letterado com registro"
    ],
    "depends_on": "41",
    "source_task_id": "PE-10",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 4,
    "pbis": [
      "PBI-14"
    ],
    "requires": [
      "07",
      "14",
      "39",
      "40",
      "41"
    ],
    "capability_gates": [
      "Política explícita Q05 válida e ledger compartilhado C03; sem defaults inventados"
    ]
  },
  {
    "id": "43",
    "title": "Registrar dead-letter e exigir replay explícito",
    "description": "Preserva diagnóstico e permite nova tentativa autorizada, revalidada e debitada.",
    "scope": [
      "GraphEventDeadLettered",
      "ReplayDeadLetter",
      "GraphEventReplayRequested"
    ],
    "acceptance": [
      "Dead-letter nunca reenvia automaticamente",
      "Replay valida fonte/revisão/idempotência e reserva C03 antes do efeito"
    ],
    "depends_on": "42",
    "source_task_id": "PE-11",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 3,
    "pbis": [
      "PBI-14"
    ],
    "requires": [
      "07",
      "41",
      "42"
    ],
    "capability_gates": [
      "Permissão de Operador C05 e reconciliação pós-timeout de C07"
    ]
  },
  {
    "id": "44",
    "title": "Despachar evento ao perfil configurado",
    "description": "Entrega evento comprovado a runtime autorizado preservando correlação e status incerto.",
    "scope": [
      "AgentDispatchPort",
      "GraphEventDispatched",
      "EventDispatchReconciliation"
    ],
    "acceptance": [
      "Perfil corresponde à assinatura e identidade autenticada",
      "Crash após aceitação remota reconcilia antes de retry não deduplicável"
    ],
    "depends_on": "42",
    "source_task_id": "PE-12",
    "source_document": "fronts/02-planning-events.md",
    "delivery_stage": "S3",
    "estimate_hours": 4,
    "pbis": [
      "PBI-11",
      "PBI-12",
      "PBI-13",
      "PBI-14"
    ],
    "requires": [
      "04",
      "07",
      "12",
      "39",
      "40",
      "41",
      "42"
    ],
    "capability_gates": [
      "Spike Q04 comprova identidade, isolamento, cancelamento/medição relevantes e rota de dispatch"
    ]
  },
  {
    "id": "45",
    "title": "Avaliar risco e elegibilidade de fast path",
    "description": "Combina fatores disponíveis com política explícita e escala estado desconhecido ou evidência incompleta.",
    "scope": [
      "RiskAssessment",
      "RiskPolicyRevision",
      "FastPathEligibility"
    ],
    "acceptance": [
      "Avaliação registra topologia, evidências, restrições e incertezas sob a política usada",
      "Fast path exige todos os predicados PBI-24 e ainda passa pelo gate AD-04"
    ],
    "depends_on": "01",
    "source_task_id": "AD-05",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S4",
    "estimate_hours": 4,
    "pbis": [
      "PBI-23",
      "PBI-24"
    ],
    "requires": [
      "01",
      "07",
      "16",
      "17"
    ]
  },
  {
    "id": "46",
    "title": "Registrar comandos humanos e montar pacote de decisão",
    "description": "Aplica aprovar, rejeitar ou solicitar correção à versão atual e apresenta evidências e impactos para o Operador.",
    "scope": [
      "OperatorDecision (C08)",
      "DecisionPackage (C08)",
      "CompareAndSet de estado e versão"
    ],
    "acceptance": [
      "Cada comando registra ator, versão e consequência; comando para versão obsoleta é recusado",
      "Correção cria versão nova e invalida decisão/evidência anteriores"
    ],
    "depends_on": "45",
    "source_task_id": "AD-06",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S4",
    "estimate_hours": 3,
    "pbis": [
      "PBI-25",
      "PBI-26"
    ],
    "requires": [
      "07",
      "45"
    ]
  },
  {
    "id": "47",
    "title": "Validar deep link na graph-ui existente",
    "description": "Comprova que o alvo do pacote abre o nó ou aresta crítica exatos na interface real antes de declarar PBI-25 entregue.",
    "scope": [
      "DecisionNavigationPort (proposta)",
      "Adapter graph-ui",
      "Validação de seleção do alvo"
    ],
    "acceptance": [
      "Teste de navegação na UI confirma o mesmo nó/aresta esperado",
      "Se a UI não suportar o alvo exato, PBI-25 permanece bloqueado sem link substituto anunciado como direto"
    ],
    "depends_on": "46",
    "source_task_id": "AD-08",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S4",
    "estimate_hours": 3,
    "pbis": [
      "PBI-25"
    ],
    "requires": [
      "07",
      "46"
    ]
  },
  {
    "id": "48",
    "title": "Persistir comandos humanos versionados",
    "description": "Registra decisão, versão esperada e consequência no journal C07 com compare-and-set e outbox local.",
    "scope": [
      "OperatorDecision (C08)",
      "AuthorityJournal (C07)",
      "OutboxEnvelope"
    ],
    "acceptance": [
      "Comando autenticado só altera estado/versão esperados",
      "Falha no commit não avança estado nem dispara efeito externo"
    ],
    "depends_on": "18",
    "source_task_id": "AD-10",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S4",
    "estimate_hours": 3,
    "pbis": [
      "PBI-26"
    ],
    "requires": [
      "07",
      "18",
      "46"
    ]
  },
  {
    "id": "49",
    "title": "Aplicar decisão humana à versão corrente",
    "description": "Encaminha aprovação ao mesmo Gatekeeper; rejeição e correção aplicam somente consequências suportadas.",
    "scope": [
      "OperatorDecision",
      "CompareAndSet",
      "RequestCurrentPromotion"
    ],
    "acceptance": [
      "APPROVE não contorna evidência verde, Gatekeeper nem gate C; REJECT não promove",
      "REQUEST_CORRECTION cria nova versão e invalida aprovação/veredicto anteriores"
    ],
    "depends_on": "48",
    "source_task_id": "AD-11",
    "source_document": "fronts/03-authority-decision.md",
    "delivery_stage": "S4",
    "estimate_hours": 3,
    "pbis": [
      "PBI-26"
    ],
    "requires": [
      "07",
      "19",
      "48"
    ]
  },
  {
    "id": "50",
    "title": "Integrar cobertura integral da telemetria S4",
    "description": "Projeta fontes produtoras incluídas no alvo S4; cobertura desse recorte não conclui PBI-31 integral. Revalida a visão integral quando fontes futuras, inclusive F07, forem habilitadas.",
    "scope": [
      "packages/cbm-orchestrator/src/telemetry/TelemetrySourceRegistry.ts",
      "packages/cbm-orchestrator/src/telemetry/ProjectTelemetryCoverage.ts",
      "packages/cbm-orchestrator/src/telemetry/TelemetryCoverageReport.ts"
    ],
    "acceptance": [
      "Cada fonte habilitada e incluída no alvo S4 aponta para o produtor, revisão e fatos de gatilho, risco, decisão/intervenção, resultado e falha que entrega; F07/S5 exige revalidação quando habilitado e incluído",
      "Projeção é somente leitura: cada contexto grava apenas em seu próprio journal/outbox",
      "Fonte requerida pelo alvo e ausente marca essa cobertura incompleta sem impedir consultas locais S1; cobertura de alvo restrito não conclui PBI-31 integral nem exclui produtores futuros da visão integral"
    ],
    "depends_on": "27",
    "source_task_id": "OV-07",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S4",
    "estimate_hours": 4,
    "pbis": [
      "PBI-31"
    ],
    "requires": [
      "07",
      "27"
    ],
    "requires_by_capability": {
      "execution_in_scope": [
        "22"
      ],
      "graph_events_enabled_and_in_scope": [
        "42",
        "44"
      ],
      "risk_human_decisions_enabled_and_in_scope": [
        "45",
        "46",
        "18",
        "48",
        "49"
      ],
      "evolution_enabled_and_in_scope": [
        "62"
      ]
    },
    "revalidation_stage_by_capability": {
      "evolution_enabled_and_in_scope": "S5; revalidar cobertura depois de habilitar F07, sem bloquear entrega S4 anterior"
    }
  },
  {
    "id": "51",
    "title": "Definir contratos de retrospectiva e evolução",
    "description": "Modela IDs, estágios C08, targets distintos e referências de evidência para impedir mistura entre qualidade e ativação.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/EvolutionProposal.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionVersion.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionContracts.ts",
      "packages/cbm-orchestrator/src/ports/EvolutionTargetPorts.ts"
    ],
    "acceptance": [
      "Estágios mantêm exatamente C08",
      "Target distingue Tradição, regra Harness e contrato skill com ports separados"
    ],
    "depends_on": "07",
    "source_task_id": "EV-01",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-27",
      "PBI-28",
      "PBI-29",
      "PBI-30"
    ],
    "requires": [
      "07"
    ]
  },
  {
    "id": "52",
    "title": "Selecionar auditorias elegíveis com cobertura",
    "description": "Reúne refs de auditoria por período solicitado sem definir cadência nem reter/apagar a fonte.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/CollectEligibleAudits.ts",
      "packages/cbm-orchestrator/src/evolution/RetrospectiveBatch.ts",
      "packages/cbm-orchestrator/src/ports/AuditReadPort.ts"
    ],
    "acceptance": [
      "Lote informa cobertura e refs omitidas com motivo",
      "Consumo só é marcado depois de resultado persistido"
    ],
    "depends_on": "51",
    "source_task_id": "EV-02",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-27"
    ],
    "requires": [
      "07",
      "31",
      "51"
    ]
  },
  {
    "id": "53",
    "title": "Registrar padrões com provenance",
    "description": "Agrupa observações de falha preservando fonte, contraexemplos e limites da amostra.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/PatternFinding.ts",
      "packages/cbm-orchestrator/src/evolution/AnalyzeRecurringPatterns.ts",
      "packages/cbm-orchestrator/src/evolution/PatternRepository.ts"
    ],
    "acceptance": [
      "Poucos exemplos/cobertura parcial permanecem hipótese sem alegação causal",
      "Cada padrão referencia auditorias e lacunas consultáveis"
    ],
    "depends_on": "52",
    "source_task_id": "EV-03",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-27",
      "PBI-28"
    ],
    "requires": [
      "07",
      "52"
    ]
  },
  {
    "id": "54",
    "title": "Criar propostas por target tipado",
    "description": "Gera proposta versionada e encaminha attestation somente quando o artefato alvo é contrato de skill.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ProposeEvolution.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionTarget.ts",
      "packages/cbm-orchestrator/src/ports/SkillContractAttestorPort.ts"
    ],
    "acceptance": [
      "Tradição, Harness rule e skill contract mantêm target/provenance próprios",
      "Attestation ausente ou ligada a digest diferente não valida contrato de skill"
    ],
    "depends_on": "53",
    "source_task_id": "EV-04",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 4,
    "pbis": [
      "PBI-28"
    ],
    "requires": [
      "07",
      "53"
    ]
  },
  {
    "id": "55",
    "title": "Persistir evidência executada de não-regressão",
    "description": "Executa Harness para baseline/candidato e grava saída que valida qualidade independentemente da admissão do target.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ValidateNonRegression.ts",
      "packages/cbm-orchestrator/src/evolution/HarnessRunEvidence.ts",
      "packages/cbm-orchestrator/src/ports/HarnessValidationPort.ts"
    ],
    "acceptance": [
      "PASS fica vinculado ao digest, baseline, suite e ambiente exatos",
      "Bundle, fast-path eligibility e campos declarados não contam como execução"
    ],
    "depends_on": "54",
    "source_task_id": "EV-05",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 4,
    "pbis": [
      "PBI-29"
    ],
    "requires": [
      "07",
      "54"
    ]
  },
  {
    "id": "56",
    "title": "Registrar decisão humana versionada",
    "description": "Aplica aprovação/rejeição autenticada por CAS à versão e target exatos da proposta validada.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ApprovalRecord.ts",
      "packages/cbm-orchestrator/src/evolution/ApproveEvolution.ts",
      "packages/cbm-orchestrator/src/ports/OperatorDecisionPort.ts"
    ],
    "acceptance": [
      "Meta-Agent não consegue emitir aprovação",
      "Mudança de digest invalida decisão anterior e requer nova avaliação"
    ],
    "depends_on": "55",
    "source_task_id": "EV-06",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-30"
    ],
    "requires": [
      "07",
      "18",
      "55"
    ]
  },
  {
    "id": "57",
    "title": "Definir adapter de Tradição com gate de capacidade",
    "description": "Encapsula consulta, ativação versionada e reversão do catálogo sem tratar DRAFT como tema ativo.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ThemeCatalogAdapter.ts",
      "packages/cbm-orchestrator/src/ports/ThemeCatalogPort.ts",
      "packages/cbm-orchestrator/src/evolution/TargetCapability.ts"
    ],
    "acceptance": [
      "Operação só é habilitada após contrato/autorização do target validados",
      "Estado DRAFT ou resposta incerta não é reportado ACTIVE"
    ],
    "depends_on": "51",
    "source_task_id": "EV-07",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-28",
      "PBI-30"
    ],
    "requires": [
      "07",
      "51"
    ]
  },
  {
    "id": "58",
    "title": "Definir adapter de regras locais do Harness",
    "description": "Isola o fluxo de proposta, revisão, ativação e reversão no store de governança local.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/HarnessRuleAdapter.ts",
      "packages/cbm-orchestrator/src/ports/HarnessRulePort.ts",
      "packages/cbm-orchestrator/src/evolution/TargetCapability.ts"
    ],
    "acceptance": [
      "Regra local não é publicada como Tradição",
      "Schema/revisão incompletos deixam target bloqueado até capacidade comprovada"
    ],
    "depends_on": "51",
    "source_task_id": "EV-08",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-28",
      "PBI-30"
    ],
    "requires": [
      "07",
      "51"
    ]
  },
  {
    "id": "59",
    "title": "Condicionar adapter de admissão de skills",
    "description": "Especifica a fronteira de admissão sem habilitar skills deferred na ausência de mecanismo validado.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/SkillAdmissionAdapter.ts",
      "packages/cbm-orchestrator/src/ports/SkillAdmissionPort.ts",
      "packages/cbm-orchestrator/src/evolution/TargetCapability.ts"
    ],
    "acceptance": [
      "Constellation/level continuam bloqueadas sem capacidade real de admissão",
      "Relatório ou attestation não é tratado como instalação/ativação"
    ],
    "depends_on": "51",
    "source_task_id": "EV-09",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-28",
      "PBI-30"
    ],
    "requires": [
      "07",
      "51"
    ]
  },
  {
    "id": "60",
    "title": "Orquestrar ativação e reconciliação",
    "description": "Grava intenção e troca a versão do destino sob CAS/serialização confiável por alvo, reconciliando incerteza sem marcar ACTIVE antecipadamente.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ActivateEvolution.ts",
      "packages/cbm-orchestrator/src/evolution/ActivationRecord.ts",
      "packages/cbm-orchestrator/src/evolution/ReconcileEvolutionEffect.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionJournal.ts"
    ],
    "acceptance": [
      "Ativação incerta não é repetida às cegas nem marcada ACTIVE",
      "Resultado e digest ativo confirmados são persistidos com idempotência",
      "Baseline aprovado é comparado atomicamente no destino; conflito ou ausência de CAS/exclusão de todos os escritores bloqueia a ativação"
    ],
    "depends_on": "56",
    "source_task_id": "EV-10",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 4,
    "pbis": [
      "PBI-30"
    ],
    "requires": [
      "07",
      "51",
      "52",
      "53",
      "54",
      "55",
      "56"
    ],
    "requires_by_target": {
      "THEME_TRADITION": [
        "57"
      ],
      "HARNESS_RULE": [
        "58"
      ],
      "SKILL_CONTRACT": [
        "59"
      ]
    }
  },
  {
    "id": "61",
    "title": "Reverter evolução para versão anterior",
    "description": "Restaura a versão anterior somente se a versão corrente do destino ainda for a instalada por esta ativação, sob guarda atômica e confirmação.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/RevertEvolution.ts",
      "packages/cbm-orchestrator/src/evolution/ReversalRecord.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionJournal.ts"
    ],
    "acceptance": [
      "REVERTED só é registrado após confirmar a versão alvo restaurada",
      "Falha parcial preserva a versão corrente reportada e o diagnóstico",
      "Reversão obsoleta recusa com TARGET_VERSION_CONFLICT e preserva a versão posterior, sem marcá-la REVERTED"
    ],
    "depends_on": "60",
    "source_task_id": "EV-11",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 4,
    "pbis": [
      "PBI-30"
    ],
    "requires": [
      "07",
      "60"
    ],
    "requires_by_target": {
      "THEME_TRADITION": [
        "57"
      ],
      "HARNESS_RULE": [
        "58"
      ],
      "SKILL_CONTRACT": [
        "59"
      ]
    }
  },
  {
    "id": "62",
    "title": "Publicar eventos de evolução e consumo",
    "description": "Persiste eventos correlacionados para auditoria e telemetria sem apagar as fontes de Execução.",
    "scope": [
      "packages/cbm-orchestrator/src/evolution/EvolutionEvents.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionOutbox.ts",
      "packages/cbm-orchestrator/src/evolution/RetrospectiveBatchRepository.ts"
    ],
    "acceptance": [
      "Eventos de Harness, aprovação, rejeição, ativação e reversão carregam a versão/digest exatos e refs consultáveis",
      "Consumo é deduplicado e só confirmado junto ao lote/análise persistidos"
    ],
    "depends_on": "52",
    "source_task_id": "EV-12",
    "source_document": "fronts/04-supervised-evolution.md",
    "delivery_stage": "S5",
    "estimate_hours": 3,
    "pbis": [
      "PBI-27",
      "PBI-28",
      "PBI-29",
      "PBI-30"
    ],
    "requires": [
      "07",
      "52",
      "53",
      "54",
      "55",
      "56",
      "60",
      "61"
    ]
  },
  {
    "id": "63",
    "title": "Registrar protocolo de comparação antes da coleta",
    "description": "Valida e congela referência, aceite equivalente, população, métricas, plano de amostra e critérios fornecidos pelo Operador.",
    "scope": [
      "packages/cbm-orchestrator/src/measurement/ComparisonProtocol.ts",
      "packages/cbm-orchestrator/src/measurement/RegisterComparisonProtocol.ts",
      "packages/cbm-orchestrator/src/persistence/ComparisonProtocolRepository.ts"
    ],
    "acceptance": [
      "Medição comparativa não começa sem protocolo pré-registrado para a conclusão pretendida",
      "Protocolo não preenche automaticamente baseline, limite ou tamanho amostral de Q08"
    ],
    "depends_on": "27",
    "source_task_id": "OV-05",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S6",
    "estimate_hours": 3,
    "pbis": [
      "PBI-32"
    ],
    "requires": [
      "07",
      "27"
    ]
  },
  {
    "id": "64",
    "title": "Calcular comparação completa e reportar limites",
    "description": "Compara a população pré-registrada, incluindo falhas/retries/revisões, e produz resultados que podem refutar a hipótese.",
    "scope": [
      "packages/cbm-orchestrator/src/measurement/EvaluateComparison.ts",
      "packages/cbm-orchestrator/src/measurement/ComparisonReport.ts",
      "packages/cbm-orchestrator/src/persistence/ComparisonRepository.ts"
    ],
    "acceptance": [
      "Todas as tentativas elegíveis e custos conhecidos/estimados/pendentes entram com unidade e qualidade",
      "Zero entregas aceitas ou custo desconhecido não vira custo zero",
      "Conclusão PBI-32 exige cobertura completa das fontes definidas no protocolo; população incompleta gera somente descrição/limitações e pode refutar a hipótese"
    ],
    "depends_on": "63",
    "source_task_id": "OV-06",
    "source_document": "fronts/05-observability-value.md",
    "delivery_stage": "S6",
    "estimate_hours": 4,
    "pbis": [
      "PBI-32"
    ],
    "requires": [
      "07",
      "63"
    ],
    "requires_by_protocol": {
      "S1_metrics": [
        "24",
        "25",
        "26",
        "27"
      ],
      "S4_metrics": [
        "50"
      ],
      "F07_metrics": [
        "50",
        "62"
      ]
    }
  }
]
```

## Gates de entrada dos slices

Contrato declarativo central: antes de despachar qualquer task, o consumidor resolve stage_entry_gates para seu delivery_stage e exige todas as condições all_of com igualdade de valor e tipo. Requisito desconhecido/não suportado ou evidência ausente é DENY; não ignorar o gate porque os requires já concluíram. Concluir SP-05 pode significar documentar FAIL e não libera S1–S6. A correspondência de configuração inclui host/versão, build CBM e política/transporte sob teste; mudança invalida a evidência aplicável. current_evaluation registra apenas que o experimento não foi realizado nesta entrega. Este bloco é contrato de autorização da entrega futura, não um executor implementado.

```json
{
  "schema_version": 1,
  "stage_entry_gates": {
    "S1": [
      "S0_APPROVED"
    ],
    "S2": [
      "S0_APPROVED"
    ],
    "S3": [
      "S0_APPROVED"
    ],
    "S4": [
      "S0_APPROVED"
    ],
    "S5": [
      "S0_APPROVED"
    ],
    "S6": [
      "S0_APPROVED"
    ]
  },
  "gates": {
    "S0_APPROVED": {
      "result_task_source_id": "SP-05",
      "all_of": [
        {
          "field": "spike.criteria.end_to_end",
          "equals": "PASS"
        },
        {
          "field": "spike.criteria.crash_restart",
          "equals": "PASS"
        },
        {
          "field": "spike.criteria.cancellation",
          "equals": "PASS"
        },
        {
          "field": "spike.criteria.cost_persisted",
          "equals": "PASS"
        },
        {
          "field": "spike.criteria.intent_scope_refusal",
          "equals": "PASS"
        },
        {
          "field": "spike.invariants_passed",
          "equals": true
        },
        {
          "field": "spike.evidence_matches_current_configuration",
          "equals": true
        },
        {
          "field": "adr005.status",
          "equals": "ACCEPTED"
        }
      ],
      "on_missing_unknown_or_unsupported": "DENY",
      "current_evaluation": "S0_APPROVED"
    }
  }
}
```
