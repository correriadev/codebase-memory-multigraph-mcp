# Desenho tático — Observabilidade e Valor

**Domínio:** Graph Engineering  
**Projeto:** `codebase-memory-multigraph-mcp`  
**Contexto:** Observabilidade e Valor (pacote proposto `packages/cbm-orchestrator/`, TypeScript)  
**Status:** refinamento `model`; contratos lógicos, sem implementação ou benchmark  
**Data:** 2026-10-23

Este suplemento detalha F08, PBI-31 e PBI-32 para consolidação posterior em 003/004. Usa o vocabulário de [001](../001-problem-space.md), o mapa de integrações de [002](../002-context-map.md), o desenho tático de [003](../003-codebase-memory-multigraph-mcp-tactical-design.md), C01–C08 de [005](../005-shared-contracts.md), a matriz estática de [006](../006-capability-matrix.md) e a ordem de entrega de [007](../007-delivery-sequence.md). Não altera C01–C08 nem cria variantes de estado, fase ou motivo terminal. Todos os paths abaixo são alvos propostos, não arquivos existentes.

## Fronteira e PBIs

| Dentro desta frente | Resultado especificado |
|---|---|
| F08 — PBI-31 | Telemetria do orquestrador registrada no journal/outbox local e consultável por `runId`, `candidateVersion` e `attemptId`; inclui veredictos por iteração, custo por ciclo, gatilhos, orçamento e intervenções humanas. |
| F08 — PBI-32 | Protocolo de comparação pré-registrado e relatório comparável contra uma referência mais simples, usando custo por entrega aceita, intervenções humanas e regressões. Pode demonstrar ou refutar a hipótese de valor. |
| Slice inicial S1 | Subconjunto necessário de PBI-31: veredictos, custos e consumo de orçamento por ciclo, com fonte própria no journal/outbox e consulta local. Não depende de stack externa de métricas. |
| Complemento S4/S6 | `OV-07` integra cobertura das fontes habilitadas e incluídas no alvo declarado; cada nova capacidade habilitada amplia esse alvo e exige revalidação. Um alvo S4 restrito não conclui PBI-31 integral nem exclui F07 da visão integral dos PBIs. `OV-05/06` comparam somente as capacidades/métricas e a população declaradas no protocolo. Nenhuma dessas tasks posterga nem é pré-requisito para o mínimo S1 (`OV-01–04`). |
| Fora da fronteira | Autorização, risco, promoção ou mudança de estado da execução; telemetria não concede autoridade. Este documento não executa benchmark, define baseline humano nem prova capacidade do runtime. |

## Refinement Questions and Answers

| ID | Categoria | Pergunta | Recomendação registrada | Resposta final / fonte |
|---|---|---|---|---|
| Q04 | Integração | O runtime informa custo, tokens e efeitos com qualidade suficiente? | Medir por contrato e host no spike; dados sem fonte confiável permanecem `UNKNOWN`. | Unresolved / Deferred; ADR-005 permanece PROPOSED. |
| Q05 | Orçamento | Quais tetos, tarifas e limites são padrão? | Não definir número ou taxa por inferência; usar valores explícitos com origem e unidade. | Unresolved / Deferred; sem defaults humanos. |
| Q07 | Retenção | Por quanto tempo reter registros de auditoria descartados? | Proteger auditoria ainda não consumida pela retrospectiva e manter política configurável. | Unresolved / Deferred; sem prazo inferido. |
| Q08 | Aceitação | Qual referência, população e limiar comprovam valor? | Pré-registrar tarefas/aceite equivalentes, métricas e critérios com o Operador antes da medição. | Unresolved / Deferred; referência e limiares não fixados. |

Q06 também continua Deferred, mas não é resolvida nem necessária para esta frente. O PBI-32 pode produzir resultados descritivos; sem os critérios humanos prévios de Q08, não deve declarar sucesso ou superioridade.

## Regras de Observabilidade e Valor

As regras são propostas locais mapeadas a C01–C08 e às invariantes INV-01–08. A matriz 006 é a autoridade para o estado observado das capacidades; logs e ledgers existentes não equivalem à telemetria deste produto.

| ID | Regra | Base |
|---|---|---|
| OV-R01 | A fonte de telemetria são os journals/outboxes TS dos contextos produtores. Cada produtor persiste seus fatos na mesma transação local que seu evento de negócio; Observabilidade só lê e projeta essa fonte. Não altera ciclo, risco, decisão, território ou promoção. | C06/C07/C08, PBI-31 |
| OV-R02 | O slice mínimo registra, no mínimo, veredictos por iteração, custo/uso por ciclo e consumo/reservas do orçamento. A coleta nasce junto às frentes que produzem os fatos; PBI-31 mínimo não aguarda S4 nem exige serviço externo de métricas. | C02/C03/C08, PBI-31 |
| OV-R03 | A cobertura de um alvo inclui gatilhos recebidos/correspondidos/despachados/suprimidos, resultados/falhas, intervenções, decisões de risco, retries/revisão e resultados de ciclo para cada capacidade habilitada e fonte incluída nesse alvo. A projeção identifica fonte, atualidade e escopo. Alvo S1/S4 restrito não encerra PBI-31 integral nem retira fontes F07 da visão integral dos PBIs; habilitação futura amplia o alvo e exige revalidar cobertura. | C02/C03/C06/C08, PBI-31 |
| OV-R04 | Eventos usam envelope C06 e correlacionam `projectId`, `runId`, `candidateVersion`, `attemptId` e IDs de evento/causa quando aplicáveis, segundo C01. Não criar uma segunda linguagem de estados, fases ou motivos de C02. | C01/C02/C06, PBI-31 |
| OV-R05 | Entrega ao menos uma vez é deduplicada usando os identificadores/chaves do contrato C06. Replay de outbox ou reprocessamento de lote não duplica contagens, soma de uso nem intervenções. Não prometer exactly-once remoto. | C06/C07, PBI-31 |
| OV-R06 | Projeções podem ser descartadas e reconstruídas a partir dos fatos retidos na fonte. Checkpoint/cursor da projeção é idempotente; rebuild marca a revisão consultada e compara cobertura, sem editar fatos de origem. Fonte ausente ou retida fora do período solicitado aparece como lacuna. | C06/C07/C08, PBI-31 |
| OV-R07 | Consultas permitem filtrar e agrupar por execução, candidato e tentativa; correlacionam projeto, período, evento, papel e resultado conforme dados autorizados. Totais não misturam execuções, versões candidatas ou tentativas por ordinal. | C01/C08, PBI-31 |
| OV-R08 | Qualidade do uso segue exatamente `MEASURED|ESTIMATED|UNKNOWN` de C08. `MEASURED` exige fonte observada; `ESTIMATED` inclui método/origem/revisão; dado ausente ou impossível de reconciliar fica `UNKNOWN`. Não adicionar outro valor de qualidade. | C03/C08, PBI-31/32 |
| OV-R09 | Ausência, timeout, falha de projeção, uso não informado ou resultado declarado sem prova nunca vira zero, sucesso ou custo medido. Campos desconhecidos são reportados como desconhecidos e não escondidos em totais. | C03/C04/C08, INV-07, PBI-31/32 |
| OV-R10 | Tokens, tempo, tentativas, custo monetário por moeda e intervenções são dimensões separadas. Moedas distintas não são somadas; conversão exige taxa, origem e revisão explicitamente versionadas. Tokens nunca são somados a valores monetários. | C03/C08, PBI-31/32 |
| OV-R11 | Consumo confirmado, reserva/chamada em voo, estimativa conservadora e pendência permanecem distinguíveis conforme C03. Projeção de custo não apaga reserva antes da reconciliação e não concede novo teto a cada agente ou retry. | C02/C03/C08, PBI-02/31 |
| OV-R12 | Retry e tentativa falha são observações/custos próprios, vinculados a IDs C01. Replay do mesmo evento ou lançamento não duplica custo; uma nova tentativa real não é deduplicada como se fosse repetição de registro. | C01/C03/C06, PBI-31/32 |
| OV-R13 | Gatilho mede pelo menos recebimento, correspondência, despacho, supressão, falha e resultado quando disponível. Evento recursivo/duplicado é contado como supressão, não como novo agente executado; estado de origem prevalece sobre número projetado. | C06/C08, PBI-31 |
| OV-R14 | Intervenção humana é comando consequente do Operador, vinculado ao ator autenticado, ciclo/versão, tipo de decisão e resultado. Abrir ou consultar relatório não é intervenção de revisão. Duração/custo de revisão só é contado quando medido ou estimado com origem. | C01/C05/C08, PBI-31/32 |
| OV-R15 | Veredicto e resultado de teste identificam executor/origem, versão e evidência observável. Campos de manifest como `tdd_oracles_passed` não provam execução nem passam a ser métrica de teste concluído. | C04/C08, PBI-31/32 |
| OV-R16 | Projeção é read model. Ela pode estar atrasada ou incompleta e deve expor cobertura/atualidade; falha ou replay do projetor não altera a fonte nem retroativamente prova o que um runtime fez. | C02/C06/C07/C08, PBI-31 |
| OV-R17 | Payloads guardam eventos tipados, IDs, resultados, unidades, origem e referências/digests mínimos; não copiam prompts, respostas completas, cadeia de raciocínio, segredos ou payloads de tools sem necessidade justificada. | C06/C07/C08, PBI-31 |
| OV-R18 | Consulta/exportação respeita identidade e escopo de projeto do solicitante segundo a fronteira C05, aplica acesso mínimo e registra acessos sensíveis. Não pressupõe RBAC nativo completo nem expõe o canal de promoção à telemetria. | C01/C05, PBI-31 |
| OV-R19 | Fatos ligados a território descartado e evidências requeridas pela retrospectiva permanecem protegidos até consumo. Q07 não define prazo; não se inventa retenção, exclusão, anonimização irreversível ou expurgo automático. | C07/C08, INV-06, PBI-22/27/31 |
| OV-R20 | Antes da primeira medição PBI-32, Operador registra e congela versão do protocolo: referência, população/tarefas, aceite equivalente, ambiente/configuração, métricas, regra de inclusão, plano de amostra/parada e critério de sucesso. Q08 continua Deferred; nenhum valor, limiar ou tamanho de amostra é inventado. | C08, Q08, PBI-32 |
| OV-R21 | Referência e grupo orquestrado só são comparáveis quando escopo e aceite foram definidos como equivalentes antes da execução. Registrar versões/configurações conhecidas e diferenças; incompatibilidade separa os resultados ou marca a comparação incompleta, sem ajuste seletivo após observar resultados. | C01/C08, Q08, PBI-32 |
| OV-R22 | A população protocolada inclui todos os ciclos elegíveis, inclusive falhas, rejeições, retries, interrupções e trabalho de revisão. Toda exclusão precisa constar da regra pré-registrada; lacuna ou truncamento da fonte torna a cobertura incompleta, não uma amostra silenciosamente melhor. | C02/C03/C08, PBI-32 |
| OV-R23 | O protocolo reporta separadamente custo monetário por entrega aceita, tokens/tempo como dimensões adicionais, número de intervenções e regressões observadas. Custos de tentativas falhas, retries e revisão pertencem ao numerador da população quando houver origem conhecida. | C03/C08, PBI-32 |
| OV-R24 | Se a população não tiver entregas aceitas, custo por entrega aceita é indefinido/`UNKNOWN`, nunca zero. Se o custo ou a contagem de entregas aceita for desconhecido, resultado não vira zero nem permite comparação de custo conclusiva. | C08, PBI-32 |
| OV-R25 | Limiar de sucesso e plano amostral são fornecidos pelo Operador antes de coletar dados para a afirmação correspondente. Sem eles, emitir apenas descrição/limitações; não inferir que a autonomia vale seu custo. Relatório pode mostrar resultado pior e refutar a hipótese. | Q08, C08, PBI-32 |
| OV-R26 | Métrica, protocolo ou relatório não autoriza, pausa, retoma, promove ou altera execução/evolução. Nenhuma observação posterior converte retroativamente projeção em evidência de runtime ou substitui C04/C05. | C04/C05/C08, INV-01–05, PBI-31/32 |

## Estrutura, tipos, ports e use cases

Todos os nomes e arquivos desta seção são contratos lógicos propostos para `packages/cbm-orchestrator/`; os estados e campos canônicos seguem C01–C08.

### Elementos principais

| Elemento | Camada/tipo | Invariantes e responsabilidade |
|---|---|---|
| `TelemetryFact` | Fato imutável — origem produtora | Evento C06 e correlação C01 com medidas normalizadas/minimizadas; sua gravação pertence ao contexto produtor, Observabilidade só o lê. |
| `TelemetryProjection` | Read model — Observabilidade | Reproduz contagens/somas filtráveis a partir dos fatos retidos; declara revisão, cursor e cobertura. Não é fonte de autoridade. |
| `UsageObservation` | Value object — Observabilidade | Mantém dimensão/unidade, valor, qualidade C08, origem e revisão da taxa/fórmula quando estimada. Não agrega moeda e tokens. |
| `InterventionObservation` | Fato projetado | Correlaciona comando humano autenticado, tipo, alvo/versão e resultado; evita armazenar conteúdo conversacional. |
| `ComparisonProtocol` | Aggregate Root — Valor | Registro prévio, versionado e imutável após início da coleta; recebe referências, aceite, população, métricas e critérios do Operador. |
| `ComparisonObservation` | Entidade — Valor | Registra cada execução/tentativa da população, grupo de referência, outcome, custos e cobertura, incluindo falhas e retries. |
| `ComparisonReport` | Read model — Valor | Apresenta métricas separadas, denominadores, qualidade, lacunas e critérios protocolados. Pode refutar valor; não concede aprovação. |

### Tipos e contratos

| Nome | Camada | Regra de validação |
|---|---|---|
| `ProjectId`, `RunId`, `CandidateVersion`, `AttemptId` | Identidade (C01) | Usar os IDs e a tupla canônica; não substituí-los por basename, ordem de tentativas ou texto do agente. |
| `TelemetryFactEnvelope` | Integração (C06) | Usa `eventId`, tipo/schema, origem, projeto, correlação/causa e versão quando aplicáveis; sem payload secreto ou transcrição. |
| `UsageObservation` | Métrica (C03/C08) | Quantidade e dimensão separadas; moeda explícita; qualidade apenas `MEASURED|ESTIMATED|UNKNOWN`; unknown não se converte em zero. |
| `ProjectionCursor`, `ProjectionCoverage` | Projeção | Referencia fonte/intervalo/revisão e lacunas; cursor não se torna versão de ciclo nem enum de estado. |
| `MetricQuery` | Consulta | Aceita filtros por `runId`, `candidateVersion`, `attemptId`, projeto e período; autorização valida escopo antes de projetar. |
| `ComparisonProtocol` | Valor (C08) | Registra referência, tarefa/coorte, equivalência de aceite, ambiente/configuração, métricas, população, amostra/parada e critério; números vêm do Operador. |
| `ComparisonReport` | Read model | Mantém denominadores e qualidade por dimensão; custo por zero entregas aceitas é indefinido, custo desconhecido permanece unknown. |

Não se adicionam valores a `ExecutionState`, `ExecutionPhase`, `TerminationReason`, status de evidência, decisão ou ciclo de evolução. Medidas de origem preservam as distinções de C02/C03/C04/C08; nomes de mensagens locais apenas mapeiam eventos de 001.

### Ports e adapters

| Port/adaptador | Operações lógicas | Limite |
|---|---|---|
| `TelemetrySourcePort` | Ler journal/outbox e fatos autorizados; acompanhar cursor de leitura | Somente leitura para Observabilidade; não chamar ferramentas de promoção nem editar fato de origem. |
| `DomainEventPort` com mapeamento de telemetria | Contexto produtor publica fato sanitizado junto ao seu evento/outbox local | Cada fonte grava apenas seus dados; Observabilidade não escreve em journal de outro contexto nem presume transação com CBM/runtime. |
| `TelemetryProjectionPort` | Deduplicar, projetar, guardar cursor/cobertura e reconstruir read model | Operações idempotentes; rebuild não modifica journal/outbox. |
| `TelemetryQueryPort` | Consultar por IDs, período, origem e dimensão | Aplica identidade/escopo de C01/C05; expõe qualidade e atualidade. |
| `ComparisonProtocolPort` | Registrar, versionar, congelar e recuperar protocolo aprovado | Antes da coleta; sem baseline/limiar/tamanho de amostra default. |
| `ComparisonCohortPort` | Carregar população de referência e orquestrada conforme protocolo | Mantém inclusões, falhas, retries, revisões e lacunas; fonte e aceite precisam ser comparáveis. |
| `ComparisonReportPort` | Calcular/exibir métricas e relatório descritivo | Somente projeção; não conclui valor sem critérios prévios nem altera execução. |

Adapters propostos: leitor do `CycleJournal`/outbox local; repositório de projeção em SQLite TS; consulta autenticada do operador; exportador de relatório. Stack de observabilidade externa, SaaS ou envio remoto não é pré-requisito do slice e exige decisão própria de privacidade/escopo.

### Use cases

| Operação | Responsabilidade | Coordena |
|---|---|---|
| `MapTelemetryEvents` | Traduzir fatos de origem para projeções sem alterar sua semântica | Mapeamento C06, cursor e eventos de 001 |
| `ProjectTelemetryBatch` | Consumir pelo menos uma vez, deduplicar e atualizar projeção local | TelemetrySource, inbox, projection store |
| `RebuildTelemetryProjection` | Recriar projeção selecionada a partir de fatos ainda retidos e produzir cobertura | Journal/outbox, projection store, cursor |
| `QueryTelemetry` | Consultar ciclo/candidato/tentativa e apresentar qualidade/origem | Query port, access policy, projection/report |
| `RegisterComparisonProtocol` | Validar pré-registro imutável antes da primeira coleta comparável | Operador autenticado, protocolo, cohort definitions |
| `EvaluateComparison` | Reconciliar população completa e calcular métricas separadas | Comparison cohort, telemetry, protocol, report |
| `PublishComparisonReport` | Expor limitações e resultados, inclusive quando refutam a hipótese | Report port, Operador |

Nenhum use case recebe permissão para iniciar agente, alterar risco, aplicar decisão, admitir evolução ou promover território. Erro de telemetria deve afetar consulta/medição, não a decisão autoritativa já persistida.

## Eventos, projeções e persistência

| Fato/evento de origem | Produtor | Projeções/consumidores F08 |
|---|---|---|
| Política e orçamento validados; consumo contabilizado; novas ações bloqueadas | Execução | Saldo confirmado/reservado/pendente por ciclo, unidade e qualidade |
| Iteração iniciada; veredicto independente emitido; ciclo encerrado | Execução/Autoridade | Iterações, resultado, motivo C02 e vínculo de evidência, por versão |
| Evento recebido; assinatura correspondida; verificação despachada; disparo suprimido | Planejamento e Eventos | Volumes de gatilho, supressões, despachos e falhas de entrega |
| Efeito incerto escalado; decisão humana registrada; risco avaliado | Execução/Autoridade | Intervenções, motivo/resultado e incertezas por ciclo/candidato |
| Auditoria elegível; resultado de não-regressão; proposta ativada/revertida | Execução/Evolução | Fonte para retrospectiva e indicadores de capacidade, sem confundir proposta com ativação |
| Comparação de valor calculada sob critério prévio | Observabilidade e Valor | Relatório protocolado e cobertura da população |

A ingestão não inventa evento de negócio ao observar apenas log do host. Manifest declarativo não se converte em resultado de teste. Eventos locais conservam `eventId`, envelope, causalidade e revisão de origem conforme C06; os projetores podem receber duplicatas e fora de ordem.

| Recurso lógico | Responsabilidade | Garantia proposta |
|---|---|---|
| `journal_event` / evento fonte | Fonte durável de fatos mínimos e referências, mantida pelo contexto produtor | Commit local conjunto com o evento/outbox daquele contexto; sem escrita cruzada ou transação com host/CBM |
| `outbox` / `inbox` | Entrega e dedupe de eventos para projetores | Pelo menos uma vez; ack/replay não apaga a fonte durável nem duplica valores |
| `telemetry_projection` / `projection_cursor` | Read model e posição de rebuild | Recriável; guarda origem, versão de projeção, cursor e lacunas conhecidas |
| `comparison_protocol` | Referência e critérios congelados | Criado antes da primeira medição; nova definição gera versão/protocolo novo |
| `comparison_population` | Runs/attempts incluídos e situação de cobertura | Lista população por regra pré-registrada, incluindo fracassos/retries/revisão |
| `comparison_report` | Métricas por grupo e protocolo | Preserva unidade, moeda, qualidade, denominator, total conhecido/pendente/unknown e limitações |

Os nomes são sugestão lógica, não schema SQLite implementado. Outbox pode ser compactada após entrega somente se os fatos fonte necessários para rebuild continuarem duráveis segundo a retenção aprovada. Q07 protege registros de auditoria ainda não consumidos; prazo não é fixado aqui. Se os fatos necessários foram retidos/expurgados, rebuild ou comparação marca lacuna e não afirma cobertura completa.

## Refinamento por PBI

### PBI-31 — Registrar e consultar telemetria própria

**Critério de aceite OV-31.1:** cada ciclo deixa fatos consultáveis de veredicto por iteração, custo/uso, orçamento e, conforme os slices que os produzem, gatilhos, falhas e intervenções. Fatos correlacionam `runId`, versão/`candidateVersion` e `attemptId` quando aplicáveis e preservam origem/qualidade.

**Critério de aceite OV-31.2:** leitura local funciona sem serviço externo de métricas, distingue medido/estimado/desconhecido, tokens/moedas/tempo/reservas e retries; eventos duplicados ou rebuild não duplicam resultados.

**Critério de aceite OV-31.3:** telemetria é somente projeção; falha de projetor não muda ciclo ou promoção. Acesso respeita projeto/identidade, conteúdo sensível é minimizado e auditoria necessária segue protegida sem prazo Q07 inventado.

**Critério de aceite OV-31.4:** cobertura declara o alvo de capacidades e fontes. S4 pode fechar a cobertura do seu slice, mas esse recorte não conclui PBI-31 integral nem remove fontes F07 da visão integral dos PBIs. A conclusão integral exige todos os produtores requeridos na visão final; uma fonte futura não é contada como coberta, e sua habilitação amplia o alvo e exige revalidação antes de declarar PBI-31 integral.

**Dependências:** C01/C02/C03/C06/C07/C08; journal/outbox e ledger de execução (ER-07/ER-08); fontes produtoras à medida que seus slices forem entregues. Q04 afeta qualidade de medição do host, não autoriza converter falta de dado em zero. S1 implementa fatos mínimos; S4 cobre seu alvo disponível sem antecipar fontes S5. A visão integral do PBI-31 continua aberta até todos os produtores requeridos estarem cobertos, com revalidação quando F07 for habilitado.

### PBI-32 — Comparar autonomia com referência simples

**Critério de aceite OV-32.1:** protocolo versionado é aprovado/fixado pelo Operador antes de começar medição e define referência, tarefas/aceite equivalentes, ambientes/configuração, métricas, população, inclusão, plano amostral/parada e limiares para a conclusão pretendida. Q08 continua Deferred até decisão; nenhuma opção numérica é presumida.

**Critério de aceite OV-32.2:** relatório inclui todos os ciclos elegíveis, entregas aceitas e não aceitas, falhas, cancelamentos, retries e trabalho de revisão; registra lacunas. Custo por entrega aceita, contagem de intervenções e regressões são apresentados como métricas separadas.

**Critério de aceite OV-32.3:** tokens, tempo e custos monetários são separados por unidade/moeda e qualidade. Custo faltante/desconhecido permanece desconhecido; população com zero aceitas tem custo por aceita indefinido, não zero.

**Critério de aceite OV-32.4:** sem equivalência, cobertura ou critérios prévios suficientes, o resultado é descritivo/inconclusivo; com critérios válidos, relatório pode mostrar que a orquestração piorou. Um protocolo limitado às métricas/capacidades S1 pode ser medido antes de F07, mas a conclusão fica restrita a esse alvo, não conclui PBI-31 integral nem representa valor integral do produto. Essa restrição do protocolo não remove F07 da visão integral dos PBIs. Não produzir escore inventado de “valor”, sucesso padrão nem recomendação baseada só em média agregada.

**Dependências:** telemetria PBI-31 suficiente para as métricas/fontes incluídas no protocolo, C01/C03/C08, dados comparáveis de referência e decisão do Operador para Q08. S6 pode medir o alvo S1 sem F07; só exige fontes S4/S5 quando o protocolo inclui as respectivas capacidades/métricas. Não executar benchmark nesta fase.

## Ordered Development Tasks

Tasks condicionais para decomposição futura. Estimativas de 2–4h são hipóteses de sessão e precisam recalibração depois das dependências; `scope` identifica 2–4 alvos propostos. Não autorizam implementação, benchmark ou mudança de ADR.

```json
[
  {
    "id": "OV-01",
    "title": "Mapear e instrumentar fatos mínimos na origem",
    "description": "Mapeia eventos de domínio aos fatos PBI-31 do slice S1 para que cada contexto produtor os publique junto ao próprio journal/outbox; preserva fonte independente de stack externa.",
    "estimate_hours": 4,
    "scope": ["packages/cbm-orchestrator/src/events/DomainEventPort.ts", "packages/cbm-orchestrator/src/telemetry/TelemetryFact.ts", "packages/cbm-orchestrator/src/telemetry/TelemetryEventMapping.ts"],
    "acceptance": ["Fatos de veredicto, custo/uso e orçamento são vinculados a IDs C01 e cada produtor os grava com sua transação local", "Outbox pode ser reentregue sem eliminar os fatos duráveis para consulta/rebuild"],
    "pbis": ["PBI-31"],
    "depends_on": "ER-07"
  },
  {
    "id": "OV-02",
    "title": "Projetar e reconstruir telemetria de forma idempotente",
    "description": "Consome fatos/outbox com dedupe e cria projeção local que pode ser reconstituída da fonte ainda retida.",
    "estimate_hours": 3,
    "scope": ["packages/cbm-orchestrator/src/telemetry/ProjectTelemetryBatch.ts", "packages/cbm-orchestrator/src/telemetry/RebuildTelemetryProjection.ts", "packages/cbm-orchestrator/src/persistence/TelemetryProjectionRepository.ts"],
    "acceptance": ["Replay do mesmo evento não duplica valores nem contagens", "Rebuild da mesma fonte produz a mesma projeção e registra lacunas de cobertura"],
    "pbis": ["PBI-31"],
    "depends_on": "OV-01"
  },
  {
    "id": "OV-03",
    "title": "Consultar telemetria por ciclo, candidato e tentativa",
    "description": "Expõe read models locais, separa dimensões e qualidade, e apresenta atualidade/cobertura.",
    "estimate_hours": 3,
    "scope": ["packages/cbm-orchestrator/src/telemetry/QueryTelemetry.ts", "packages/cbm-orchestrator/src/telemetry/TelemetryReport.ts", "packages/cbm-orchestrator/src/telemetry/UsageObservation.ts"],
    "acceptance": ["Filtros por runId/candidateVersion/attemptId não cruzam candidatos ou execuções", "Tokens, cada moeda e tempo permanecem separados; UNKNOWN não vira zero"],
    "pbis": ["PBI-31"],
    "depends_on": "OV-02"
  },
  {
    "id": "OV-04",
    "title": "Proteger consultas e auditoria minimizada",
    "description": "Aplica escopo autenticado, minimização de payload e proteção para registros requeridos por retrospectiva.",
    "estimate_hours": 2,
    "scope": ["packages/cbm-orchestrator/src/telemetry/TelemetryAccessPolicy.ts", "packages/cbm-orchestrator/src/telemetry/TelemetryRedactor.ts"],
    "acceptance": ["Solicitante fora do escopo não consulta projeto alheio", "Projeção não persiste conversa/segredo e não apaga auditoria não consumida; política temporal fica sem valor inventado"],
    "pbis": ["PBI-31"],
    "depends_on": "OV-03"
  },
  {
    "id": "OV-05",
    "title": "Registrar protocolo de comparação antes da coleta",
    "description": "Valida e congela referência, aceite equivalente, população, métricas, plano de amostra e critérios fornecidos pelo Operador.",
    "estimate_hours": 3,
    "scope": ["packages/cbm-orchestrator/src/measurement/ComparisonProtocol.ts", "packages/cbm-orchestrator/src/measurement/RegisterComparisonProtocol.ts", "packages/cbm-orchestrator/src/persistence/ComparisonProtocolRepository.ts"],
    "acceptance": ["Medição comparativa não começa sem protocolo pré-registrado para a conclusão pretendida", "Protocolo não preenche automaticamente baseline, limite ou tamanho amostral de Q08"],
    "pbis": ["PBI-32"],
    "depends_on": "OV-04"
  },
  {
    "id": "OV-06",
    "title": "Calcular comparação completa e reportar limites",
    "description": "Compara a população pré-registrada, incluindo falhas/retries/revisões, e produz resultados que podem refutar a hipótese.",
    "estimate_hours": 4,
    "scope": ["packages/cbm-orchestrator/src/measurement/EvaluateComparison.ts", "packages/cbm-orchestrator/src/measurement/ComparisonReport.ts", "packages/cbm-orchestrator/src/persistence/ComparisonRepository.ts"],
    "acceptance": ["Todas as tentativas elegíveis e custos conhecidos/estimados/pendentes entram com unidade e qualidade", "Zero entregas aceitas ou custo desconhecido não vira custo zero", "Conclusão PBI-32 exige cobertura completa das fontes definidas no protocolo; população incompleta gera somente descrição/limitações e pode refutar a hipótese"],
    "pbis": ["PBI-32"],
    "depends_on": "OV-05"
  },
  {
    "id": "OV-07",
    "title": "Integrar cobertura integral da telemetria S4",
    "description": "Projeta fontes produtoras incluídas no alvo S4; cobertura desse recorte não conclui PBI-31 integral. Revalida a visão integral quando fontes futuras, inclusive F07, forem habilitadas.",
    "estimate_hours": 4,
    "scope": ["packages/cbm-orchestrator/src/telemetry/TelemetrySourceRegistry.ts", "packages/cbm-orchestrator/src/telemetry/ProjectTelemetryCoverage.ts", "packages/cbm-orchestrator/src/telemetry/TelemetryCoverageReport.ts"],
    "acceptance": ["Cada fonte habilitada e incluída no alvo S4 aponta para o produtor, revisão e fatos de gatilho, risco, decisão/intervenção, resultado e falha que entrega; F07/S5 exige revalidação quando habilitado e incluído", "Projeção é somente leitura: cada contexto grava apenas em seu próprio journal/outbox", "Fonte requerida pelo alvo e ausente marca essa cobertura incompleta sem impedir consultas locais S1; cobertura de alvo restrito não conclui PBI-31 integral nem exclui produtores futuros da visão integral"],
    "pbis": ["PBI-31"],
    "depends_on": "OV-04"
  }
]
```

`OV-07` é o complemento S4 do PBI-31, em ramo separado das tasks S6 `OV-05/06`. As referências abaixo são condições cruzadas no DAG global, não um `requires` conjuntivo: só as fontes habilitadas e incluídas no escopo declarado são pré-requisitos dessa projeção. A fonte EV-12 pertence ao slice S5 e não pode ser requisito incondicional de OV-07 em S4. Nada aqui bloqueia o mínimo S1 `OV-01–04`.

| Alvo/capacidade | Task fonte | Condição no DAG global | Dados / limite |
|---|---|---|---|
| Cobertura S4 habilitada | ER-12, PE-10/PE-12, AD-05/AD-06/AD-07 | OV-07 requer cada produtor somente quando a capacidade correspondente estiver habilitada e incluída no alvo declarado. | Veredictos/resultados de execução; gatilhos recebidos, correspondidos, suprimidos e despachados; risco e decisão/intervenção humana disponíveis no S4. |
| Eventos de evolução F07 | EV-12 | OV-07 requer este produtor somente se F07 estiver habilitado e seus eventos estiverem incluídos; como EV-12 está em S5, nunca é requisito incondicional de OV-07/S4. Habilitar F07 exige revalidar cobertura integral. | Proposta/ativação/reversão quando a fonte de evolução fizer parte do alvo; até então, PBI-31 integral continua aberto. |
| Comparação restrita ao slice S1 | OV-01–OV-04 | OV-06 depende de OV-05 e das fontes S1 incluídas no protocolo; não depende de OV-07 nem de EV-12. O resultado só sustenta conclusões para as métricas e população S1 declaradas; não conclui PBI-31 integral nem remove F07 da visão integral dos PBIs. | A ausência de métricas de slices futuros limita o escopo da conclusão, não bloqueia a medição S1. |
| Comparação com métricas/capacidades S4/S5 | OV-07 e produtores das métricas solicitadas | OV-06 exige a cobertura OV-07 somente quando o protocolo inclui fontes S4; EV-12 é exigido apenas se a população inclui eventos F07 habilitados. | Ausência de fonte requerida torna a conclusão integral indisponível, sem transformar a lacuna em zero. |

O vínculo cross-front é com os produtores de fatos duráveis, não com uma escrita centralizada por Observabilidade. Se qualquer fonte habilitada e requerida pelo alvo/protocolo estiver ausente, a cobertura dessa população fica parcial/incompleta e não sustenta conclusão integral. Capacidades ainda não habilitadas ficam explicitamente fora do alvo corrente, mas continuam no escopo da visão integral do PBI-31 quando requeridas pelo conjunto final; sua habilitação futura amplia a cobertura e exige revalidação. Nenhuma dessas condições afeta o funcionamento ou a medição restrita ao mínimo S1.

## Cenários de teste especificados

Cenários Given/When/Then propostos; não são testes executáveis nem foram executados. Cada PBI tem cenário de sucesso e falha, origem em regra e nível de validação.

### PBI-31 — telemetria consultável

<a id="ov-s01"></a>

**OV-S01 — Deve consultar o subconjunto mínimo do slice sem serviço externo** *(Integration; PBI-31; OV-R01–04)*  
**Given** journal/outbox local configurado e ciclo com veredicto, uso e reservas registrados  
**When** Operador consulta por `runId`, versão candidata e tentativa  
**Then** relatório mostra veredicto por iteração e consumo por unidade/qualidade a partir da fonte local, sem depender de backend externo.

<a id="ov-s02"></a>

**OV-S02 — Deve deduplicar replay e reconstruir a mesma projeção** *(Integration; PBI-31; OV-R05–07)*  
**Given** uma fonte durável com evento já projetado e cursor gravado  
**When** o projetor recebe novamente o evento e depois executa rebuild da mesma faixa  
**Then** total de uso/intervenções não cresce por duplicata e a projeção reconstruída identifica a mesma cobertura.

<a id="ov-s03"></a>

**OV-S03 — Deve registrar gatilhos, intervenções e falhas sem duplicar o sinal** *(Functional; PBI-31; OV-R03/13/14)*  
**Given** um sinal foi suprimido como duplicado e o Operador solicitou correção em versão específica  
**When** fatos de despacho e decisão entram no journal/outbox  
**Then** projeção conta supressão e intervenção com a origem/versão, sem criar despachos ou ações humanas que não ocorreram.

<a id="ov-s04"></a>

**OV-S04 — Deve manter moeda, tokens e estados de consumo distintos** *(Unit; PBI-31; OV-R08–12)*  
**Given** uma tentativa reporta tokens medidos, custo estimado em USD e outra chamada ainda está reservada/em voo  
**When** relatório agrega o ciclo  
**Then** dimensões continuam separadas, custo estimado conserva origem, reserva não aparece como consumo confirmado e lançamento repetido não duplica total.

<a id="ov-s05"></a>

**OV-S05 — Deve preservar desconhecido e não inferir teste de manifest** *(Integration; PBI-31; OV-R09/11/15)*  
**Given** runtime não informa custo e um manifest contém `tdd_oracles_passed: true` sem saída executada  
**When** fatos são projetados  
**Then** uso fica `UNKNOWN`, novas métricas não tratam valor como zero e o manifest não se converte em teste verde.

<a id="ov-s06"></a>

**OV-S06 — Deve negar consulta fora do escopo e minimizar payload** *(Integration; PBI-31; OV-R17/18)*  
**Given** evento de origem contém referência a conteúdo sensível e solicitante não tem escopo do projeto consultado  
**When** fato é persistido e a consulta é tentada  
**Then** payload bruto/segredo não aparece na telemetria e a consulta não autorizada é recusada sem expor valores do projeto.

<a id="ov-s07"></a>

**OV-S07 — Deve preservar auditoria não consumida sem fixar retenção** *(Integration; PBI-31; OV-R06/19)*  
**Given** registro de auditoria de território descartado ainda não foi consumido pela retrospectiva  
**When** a projeção é rebuild ou a política de expurgo é avaliada  
**Then** evidência necessária permanece disponível e nenhuma data de expiração inventada é aplicada; lacuna prévia fica declarada.

<a id="ov-s08"></a>

**OV-S08 — Deve deixar a execução autoritativa inalterada quando a projeção falha** *(Integration; PBI-31; OV-R16/26)*  
**Given** ciclo e decisão foram persistidos no journal mas o projetor está indisponível  
**When** a publicação/rebuild falha  
**Then** ciclo, promoção e decisão não mudam; consulta indica defasagem e a projeção pode ser refeita da fonte.

### PBI-32 — protocolo e comparação de valor

<a id="ov-s09"></a>

**OV-S09 — Deve comparar coortes equivalentes sob protocolo pré-registrado** *(Functional; PBI-32; OV-R20/21)*  
**Given** Operador congelou versão do protocolo com referência, mesma tarefa/aceite, ambiente/configuração, métricas, população e critério antes da coleta  
**When** todas as execuções elegíveis são reconciliadas  
**Then** relatório compara custos por entrega aceita, intervenções e regressões em dimensões próprias e vincula resultados à versão do protocolo.

<a id="ov-s10"></a>

**OV-S10 — Deve incluir tentativas falhas, retries e revisão humana na população/custo** *(Integration; PBI-32; OV-R22/23)*  
**Given** coorte tem entregas aceitas e rejeitadas, retry e revisão com valores medidos/estimados  
**When** custo por entrega aceita é calculado  
**Then** custos de toda tentativa elegível e revisão com fonte entram no numerador, cada unidade/qualidade é exposta e nenhum resultado é filtrado por outcome após a coleta.

<a id="ov-s11"></a>

**OV-S11 — Deve marcar indefinido custo quando zero entregas forem aceitas** *(Unit; PBI-32; OV-R09/24)*  
**Given** protocolo cobre a população completa e nenhuma execução produziu entrega aceita  
**When** custo por entrega aceita é calculado  
**Then** métrica fica indefinida/`UNKNOWN`, nunca zero, e relatório preserva custos incorridos.

<a id="ov-s12"></a>

**OV-S12 — Deve impedir conclusão de sucesso com dados ou cobertura desconhecidos** *(Integration; PBI-32; OV-R08/21/24/25)*  
**Given** uma coorte tem custo ou cobertura ausente e Q08 não tem limite aprovado  
**When** relatório compara os grupos  
**Then** valores desconhecidos não viram zero, resultado é descritivo/inconclusivo e não declara superioridade.

<a id="ov-s13"></a>

**OV-S13 — Deve expor que a autonomia pode piorar o resultado** *(Functional; PBI-32; OV-R20/25/26)*  
**Given** protocolo válido e resultados mostram mais custo por aceita, mais intervenções ou regressões que a referência segundo critérios pré-registrados  
**When** comparação é publicada  
**Then** relatório preserva resultado desfavorável como possível refutação e não muda autoridade ou política do produto.

<a id="ov-s14"></a>

**OV-S14 — Deve rejeitar protocolo alterado após início da medição** *(Integration; PBI-32; OV-R20–22)*  
**Given** coleta começou sob versão imutável de protocolo  
**When** alguém tenta mudar baseline, aceite, população ou limiar para favorecer resultados observados  
**Then** alteração é recusada para aquela medição e exige protocolo/versão nova antes de uma nova coleta; registros existentes não são reclassificados silenciosamente.

<a id="ov-s15"></a>

**OV-S15 — Deve cobrir todas as fontes habilitadas no alvo S4 sem escrita cruzada** *(Integration; PBI-31; OV-R01/03/06/16)*  
**Given** o alvo declara as capacidades habilitadas e seus outboxes produtores entregues, com IDs C01/C06 e revisões consultáveis; eventos F07 só entram quando a capacidade S5 estiver habilitada e incluída  
**When** `ProjectTelemetryCoverage` processa a população declarada  
**Then** consulta/reconstrói os fatos de cada fonte incluída, identifica o escopo e a cobertura alcançados, preserva resultados/falhas/intervenções e nenhum write de Observabilidade altera journal/outbox de outro contexto; cobertura S4 restrita não conclui PBI-31 integral, que aguarda todos os produtores da visão final e revalidação após habilitação de F07.

<a id="ov-s16"></a>

**OV-S16 — Deve marcar incompleta a fonte requerida ausente sem atrasar medição S1** *(Integration; PBI-31/32; OV-R01/02/03/06/16/24)*  
**Given** o protocolo/alvo declara uma fonte como habilitada e necessária, mas o outbox ainda não foi entregue; o protocolo S1 local tem suas fontes mínimas disponíveis  
**When** projeção S1, comparação restrita S1 e `OV-07` são consultadas  
**Then** projeção e medição S1 continuam disponíveis no escopo restrito; a cobertura que requereu uma fonte ausente fica parcial/incompleta e nomeia a lacuna. Fonte F07 ainda não habilitada aparece como pendente da visão integral, não é contada como coberta nem removida do escopo; habilitá-la exige revalidação, e nenhuma ausência vira zero.

## Gaps mantidos

- Q04 segue Deferred e ADR-005 segue PROPOSED. Qualidade medida de custo/uso, cancelamento e isolamento do runtime precisa da capacidade demonstrada; telemetria não supre a ausência dela.
- Q05 segue Deferred. Não há tarifa, moeda padrão, timeout, limite de custo, sample size ou valor numérico deduzido neste suplemento.
- Q07 segue Deferred. Auditoria não consumida fica protegida, mas nenhuma duração é escolhida. Rebuild só cobre fatos ainda retidos; lacunas precisam ser mostradas.
- Q08 segue Deferred. Referência e limiares de sucesso devem vir do Operador antes da medição pretendida; tamanho de amostra e stop criteria não recebem default inventado. Sem critérios, publicar só descrição, sem conclusão de valor.
- Nenhum benchmark, teste, spike, implementação ou integração com provedor de métricas foi executado. Esta frente especifica regras e cenários para trabalho futuro; não altera ADRs, entradas PBB, core C ou o journal existente.
