# Desenho tático — Evolução Supervisionada

**Domínio:** Graph Engineering
**Projeto:** `codebase-memory-multigraph-mcp`
**Contexto:** Evolução Supervisionada — F07
**Status:** refinamento `model`; nenhum contrato abaixo declara capacidade de admissão implementada
**Data:** 2026-10-23

Este suplemento detalha PBI-27–30 para consolidação posterior em 003/004. Usa a linguagem e a sequência de eventos de `001-problem-space.md`, as fronteiras de `002-context-map.md`, C07/C08 de `005-shared-contracts.md`, `006-capability-matrix.md`, `DECISIONS.md`, `007-delivery-sequence.md`, `docs/feature/tradition.md`, `docs/feature/harness_v2.md` e `skills/README.md`. Evolução Supervisionada mantém propostas e seu registro; não é dona das auditorias de Execução, do catálogo de temas/Tradições, das regras locais do Harness nem de contratos de skill. Não altera core C nem afirma que escrever esta especificação prova qualidade ou capacidade.

## Fronteira e PBIs

| PBI | Fronteira desta frente | Dependência / limite |
|---|---|---|
| PBI-27 | Selecionar auditorias de territórios descartados em um período, apresentar cobertura e identificar padrões recorrentes com proveniência. | Lê referências/cópias autorizadas; Execução continua dona da fonte. Q06 (cadência) e Q07 (retenção) seguem Deferred. |
| PBI-28 | Produzir proposta versionada de Tradição, regra local do Harness ou contrato de skill; aplicar `skill-contract-attestor` apenas ao último alvo. | Meta-Agent propõe, não aprova nem ativa. Cada alvo tem semântica e adaptador próprios. |
| PBI-29 | Exigir evidência executada de não-regressão no Harness vinculada ao artefato e baseline exatos antes de validar qualquer proposta. | Qualidade é independente de existir capacidade para ativar o alvo. `evaluate_fast_path`, manifest declarativo ou attestation não provam execução de regressão. |
| PBI-30 | Registrar aprovação/rejeição explícita do Operador; ativar versão exata após confirmação do destino; reverter para versão ativa anterior com confirmação. | Sem mecanismo real e autorizado para o alvo, a proposta não fica `ACTIVE`; relatório/proposta não substitui ativação. As skills deferred não têm admissão demonstrada. |

Q04 permanece Deferred e ADR-005 PROPOSED. Não é executado spike nesta frente. A capacidade de acesso ao Harness, ao catálogo e a qualquer ativador de skill deve ser mapeada antes de comprometer implementação. Core C e stores dos contextos alvo permanecem sob suas fronteiras.

## Perguntas de refinamento

| ID | Categoria | Pergunta | Recomendação registrada | Resposta final / fonte |
|---|---|---|---|---|
| Q06 | Negócio | Qual a cadência da retrospectiva/evolução? | Suportar acionamento configurável; a frequência semanal do briefing é hipótese, não requisito fixado. | Unresolved / Deferred; não há cadência escolhida. PBB e DECISIONS. |
| Q07 | Dados | Por quanto tempo reter auditoria descartada? | Política configurável e proteção dos registros ainda não consumidos pela retrospectiva. | Unresolved / Deferred; sem prazo, unidades ou regra de expiração inventados. PBB e DECISIONS. |

## Regras de evolução

As regras locais abaixo especializam C07/C08; não redefinem estados compartilhados, autoridades ou contratos de evidência.

| ID | Regra | Base |
|---|---|---|
| EV-R01 | Um lote de retrospectiva registra período solicitado, filtros, origem, instante de leitura, IDs de auditorias de horizontes descartados elegíveis nessa janela, referências consultadas, registros omitidos e motivo de omissão. Cobertura incompleta é visível; não se descreve o lote como completo sem prova da cobertura consultada. | C07/C08, PBI-27 |
| EV-R02 | Q06 não fixa execução semanal, agendada ou por evento. O sistema suporta período solicitado/configurado sem pressupor frequência de produto. | DECISIONS Q06, PBI-27 |
| EV-R03 | Q07 não fixa expiração. Retenção é configurável; auditoria ainda não consumida ou necessária para retrospectiva não pode ser eliminada pela Evolução Supervisionada. O contexto registra referências/consumo localmente sem apagar nem reescrever o journal de Execução. | C07, DECISIONS Q07, PBI-27 |
| EV-R04 | Seleção é idempotente: uma auditoria fonte entra uma vez por referência/versionamento, com status de consumo pertencente ao contexto de evolução. Só marcar consumo depois que lote e resultado da análise/proposta estiverem duráveis; falha antes disso deixa a auditoria elegível. | C07/C08, PBI-27 |
| EV-R05 | Cada padrão/proposta preserva provenance consultável: IDs e versões de auditoria, horizonte/run, instante/período, origem do evento/falha, perfil, outcome, evidências/digests, cobertura e transformação analítica aplicada. Agregação não substitui a fonte nem esconde contraexemplos. | C01/C07/C08, PBI-27/28 |
| EV-R06 | Padrões recorrentes não autorizam inferência causal. Com amostra escassa, cobertura parcial, fontes divergentes ou contraexemplos sem explicação, registrar observação/hipótese e suas lacunas; não declarar causa, padrão confirmado ou melhoria validada. Nenhum limite numérico de amostra/confiança é inventado. | PBI-27/28, Q06/Q07 Deferred |
| EV-R07 | Meta-Agent pode ler dados autorizados, analisar e propor; não aprova, ativa ou substitui a própria proposta. A decisão/ação exige identidade autenticada do Operador. | C05, C08, PBI-27/28/30 |
| EV-R08 | `EvolutionProposal` tem alvo tipado e não intercambiável: `THEME_TRADITION`, `HARNESS_RULE` ou `SKILL_CONTRACT`. Artefato, destino, regras de validação, autorização e mecanismo de ativação são próprios do alvo. | C08, 001/002, PBI-28/30 |
| EV-R09 | Tradição é conhecimento de ofício externo, versionado no catálogo de temas do CBM. Não é regra de projeto nem instrução de skill. Binding `NORMATIVE` ou `CONSULTED` e alteração normativa exigem autoridade/validação do Operador; sessão de projeto não escreve tema automaticamente. | `docs/feature/tradition.md`, `skills/README.md`, 006, PBI-28/30 |
| EV-R10 | Regra local do Harness é restrição de governança do projeto no store do Harness, distinta do catálogo de temas. Proposta `UNVERIFIED_CANDIDATE` e manifest não equivalem a revisão/ativação; caminhos de revisão anunciados têm lacunas de schema/autoridade que precisam validação. | `docs/feature/harness_v2.md`, 006, PBI-28–30 |
| EV-R11 | Contrato de skill define procedimento universal; normas de ofício específicas permanecem em temas/Tradições. `skill-contract-attestor` verifica a separação/forma do contrato quando o alvo é skill; não aprova conteúdo de craft nem substitui Harness, Operador ou admissão no host. | `skills/README.md`, PBI-28 |
| EV-R12 | Attestation é aplicável apenas a `SKILL_CONTRACT`. Seu resultado e proveniência devem estar ligados ao digest da versão submetida. Ausência, execução declarada sem evidência consultável, ou resultado inconclusivo não valida a proposta. | C04/C08, `skills/README.md`, PBI-28 |
| EV-R13 | PBI-29 exige não-regressão realmente executada no Harness por executor/validação separados da autoria da proposta, com baseline, artefato/digest, configuração/ambiente, resultado e evidências rastreáveis. Resultado PASS aplica-se somente àquela versão e suite; alteração cria nova versão e invalida resultado anterior. | C01/C04/C08, `docs/feature/harness_v2.md`, PBI-29 |
| EV-R14 | Qualidade e capacidade de ativação são gates independentes. Harness verde pode validar qualidade mesmo quando um destino não pode ser ativado; capacidade ausente impede `ACTIVE` e não pode ser substituída por relatório, proposal, attestation ou botão/manifest. | C08, DECISIONS INV-08, 007 S5, PBI-29/30 |
| EV-R15 | Estados compartilhados da proposta permanecem exatamente `PROPOSED→VALIDATED→APPROVED→ACTIVE→REVERTED` (C08). Evidência reprovada/indisponível impede progressão de qualidade; decisão rejeitada não cria ativação; falha/ausência de capacidade não autoriza marcar `ACTIVE`. Outros fatos de operação ficam em registros de decisão/efeito, sem inventar estados de proposta. | C08, PBI-29/30 |
| EV-R16 | O Operador aprova ou rejeita explicitamente versão/digest/alvo exatos, com identidade, instante, política e consequência. Compare-and-set verifica proposta ainda `VALIDATED`, digest atual e versão do destino; edição/rebase exige nova proposta e nova aprovação. Meta-Agent não aprova. | C01/C05/C08, INV-08, PBI-30 |
| EV-R17 | Ativação requer `APPROVED`, Harness verde da mesma versão, autorização e adapter/capacidade validada para aquele alvo. `ACTIVE` só após confirmação observada de que o destino está na versão/digest aprovada; não basta uma chamada aceita ou um relatório. | C08, 006, 007 S5, PBI-30 |
| EV-R18 | Registro de ativação/reversão usa intenção durável, chamada ao sistema alvo e resultado durável; stores não compartilham transação distribuída. Timeout ou crash deixa efeito `UNKNOWN`; consultar versão ativa antes de retry, e nunca repetir cegamente. Falha parcial preserva diagnóstico, aprovação e proveniência. | C07/C08, INV-03/04/08, PBI-30 |
| EV-R19 | Reversão aponta para uma versão ativa anterior identificada e preserva histórico de ambas as versões, motivo, Operador e evidência. `REVERTED` só após confirmação de restauração do alvo. Rollback não apaga auditoria nem desfaz efeitos históricos. | C08, INV-08, PBI-30 |
| EV-R20 | `constellation-individuator` e `level-graduator` permanecem deferred/analysis-only até haver mecanismo real de admissão. Preparar proposta ou evidência por esses procedimentos não as instala, ativa, habilita nem substitui. `level-graduator` não substitui o Harness exigido por PBI-29. | PBB §8, `skills/README.md`, 006, PBI-29/30 |
| EV-R21 | O catálogo de temas, store local do Harness, repositório de arquivos/contratos de skill e journal TS são fontes separadas. O orquestrador coordena por ports e referências; não escreve diretamente no store C ou no arquivo/catálogo como se houvesse atomicidade comum. | 002, C07/C08, PBI-28/30 |
| EV-R22 | Ativação/reversão comparam atomicamente expectedTargetVersion no destino antes de trocar para desiredTargetVersion, com targetId e operationId. Na ativação, exigir o baseline aprovado; no rollback, exigir a versão instalada por aquela ativação. Drift recusa com TARGET_VERSION_CONFLICT e mantém a versão corrente. Alternativa: serialização confiável por alvo cobrindo todos os escritores durante a comparação/troca; apenas CAS no journal ou leitura seguida de escrita não serve. Sem garantia, bloquear a operação. | C08, EV-R16–19, PBI-30 |

## Estrutura, tipos, ports e use cases

Os caminhos e nomes desta seção são contratos lógicos propostos para `packages/cbm-orchestrator/`; não são arquivos nem APIs existentes.

### Elementos principais

| Elemento | Camada/tipo | Invariantes e responsabilidade |
|---|---|---|
| `RetrospectiveBatch` | Aggregate Root — Evolução | Guarda período/filtros, cobertura, referências elegíveis/omitidas e progresso de consumo; não possui a auditoria fonte. |
| `PatternFinding` | Entidade — Análise | Preserva observações, hipótese, evidências, cobertura e contraexemplos; correlação nunca é causalidade implícita. |
| `EvolutionProposal` | Aggregate Root — Evolução | Tem alvo, artefato/digest, provenance, versão e status C08; Meta-Agent só propõe. |
| `EvolutionVersion` | Value/entity — Evolução | Identifica digest imutável, target revision e predecessor ativo; qualquer edição cria versão nova. |
| `NonRegressionEvidence` | Value record — Qualidade | Liga baseline e Harness realmente executado à versão; não é capacidade de ativação. |
| `ApprovalRecord` | Entity — Decisão | Guarda aprovação/rejeição autenticada do Operador para digest/alvo exatos. |
| `ActivationRecord` / `ReversalRecord` | Entity — Integração | Guarda intenção, chave idempotente, versão antes/depois, resultado C07 e confirmação externa. |

### Tipos

| Nome | Camada | Validação |
|---|---|---|
| `AuditRef` | ACL / leitura de execução | ID da auditoria mais run/horizon/version e origem; não carrega conversa ou segredo desnecessário. |
| `AuditWindow` e `CoverageReport` | Retrospectiva | Período solicitado/configurado, filtros, cobertura conhecida e omissões; sem cadência/retention default. |
| `PatternEvidence` | Análise | Referências fonte, observações, contraexemplos e lacunas; separa hipótese de relação causal comprovada. |
| `EvolutionTarget` | Proposta | Exatamente `THEME_TRADITION`, `HARNESS_RULE` ou `SKILL_CONTRACT`; dita adapter e validações próprias. |
| `EvolutionStage` | Proposta | Exatamente estados de C08; não codifica capacidade de destino como status. |
| `HarnessRunEvidence` | Qualidade | Baseline, digest, suite/configuração, executor/ambiente, resultado e referências consultáveis. |
| `SkillContractAttestation` | Qualidade de contrato | Digest avaliado, versão do procedimento do attestor, resultado e razões; válido somente para skill contract. |
| `TargetCapability` | Integração | Indica evidência/versão do contrato do adapter disponível; capacidade não deriva de `APPROVED` nem de Harness PASS. |
| `ActivationOutcome` | Integração (C07) | Reusa `PENDING|CONFIRMED|FAILED|UNKNOWN` do registro de efeito, sem ampliar estados C08. |

### Ports, adapters e repositórios

| Port/repository | Operações lógicas | Fronteira |
|---|---|---|
| `AuditReadPort` | consultar registros elegíveis por período/projeto/origem e devolver referências/coverage | ACL de leitura ao journal de Execução; não apaga nem reescreve fonte. |
| `RetrospectiveBatchRepository` | persistir lote, referências selecionadas/omitidas e marcadores locais de consumo | Storage do contexto de evolução a escolher; consumo só após análise/proposta persistida. |
| `PatternRepository` | salvar achados e sua provenance/contraevidência | Não aceita relação causal sem evidência declarada e revisável. |
| `EvolutionProposalRepository` | guardar versão, digest, alvo, stage C08, avaliações e referências | Registro próprio; não escreve no store do alvo por conta própria. |
| `HarnessValidationPort` | executar suite de não-regressão com baseline/candidato e capturar saída | Não implementado por `evaluate_fast_path` nem pelo campo declarativo de manifest; execução real deve ser mapeada. |
| `SkillContractAttestorPort` | avaliar formato/procedimento do contrato para alvo `SKILL_CONTRACT` | Skill é orientação, não pipeline automático; prova de invocação e resposta é necessária. |
| `ThemeCatalogPort` | consultar e, após aprovação, solicitar nova versão/estado/reversão de Tradição | Tool de catálogo e autorização normativa têm capacidades parciais; validar contratos. `founding_decide` aceito retorna DRAFT, não ACTIVE. |
| `HarnessRulePort` | criar/revisar/ativar/reverter regra local por fluxo autorizado | Store local difere do catálogo; proposta `UNVERIFIED_CANDIDATE` e schema/revisão não provam publicação. |
| `SkillAdmissionPort` | solicitar e confirmar admissão/ativação/reversão de contrato de skill | Capacidade pública de registro/admissão não comprovada; skills deferred seguem bloqueadas até capacidade real. |
| `OperatorDecisionPort` | autenticar aprovação/rejeição e vincular a digest/versão | Meta-Agent não pode simular ou emitir decisão do Operador. |
| `EvolutionJournalPort` / `EventOutboxPort` | commit de proposta/decisão/outbox local; CAS e deduplicação | Atomicidade apenas local; chamadas ao Harness/destino são reconciliadas por C07. |

### Use cases

| Operação | Responsabilidade | Coordena |
|---|---|---|
| `CollectEligibleAudits` | Capturar lote pelo período solicitado, cobertura, omissões e referências não consumidas | AuditRead, batch repository |
| `AnalyzeRecurringPatterns` | Produzir observações/hipóteses com evidências, recorrência e contraexemplos | PatternRepository, AuditRead |
| `ProposeEvolution` | Criar artefato versionado para target tipado e anexar provenance | EvolutionProposalRepository, PatternRepository, attestor quando skill |
| `ValidateNonRegression` | Executar Harness contra baseline/candidato e persistir saída independente da admissão | HarnessValidationPort, proposal repository |
| `ApproveEvolution` / `RejectEvolution` | Aplicar decisão autenticada compare-and-set à versão exata | OperatorDecisionPort, proposal repository |
| `ActivateEvolution` | Verificar aprovação/qualidade/capacidade; chamar adapter certo e confirmar digest ativo | ThemeCatalog, HarnessRule ou SkillAdmission, journal |
| `RevertEvolution` | Repor versão anterior por adapter correspondente e confirmar resultado | Adapter alvo, journal |
| `ReconcileEvolutionEffect` | Consultar destino após timeout/crash e reconciliar tentativa UNKNOWN antes de reenvio | Adapter alvo, journal |

## Máquina de evolução e falhas parciais

| Stage atual → próximo | Guarda | Registro durável |
|---|---|---|
| `PROPOSED → VALIDATED` | Harness retorna não-regressão PASS para digest exato; attestation PASS adicional somente para target skill; provenance e cobertura revisadas | Baseline/candidato/suite, saída observada, gaps e attestation aplicável. |
| `VALIDATED → APPROVED` | Operador autenticado aprova digest/alvo/versão atual | Decisão, identidade, instante e revisão compare-and-set. |
| `VALIDATED → VALIDATED` (sem progressão) | Operador rejeita; Harness falha/inconclusivo; provenance ou target sem validação | Decisão/motivo e evidências; nenhuma ativação. C08 não exige inventar um novo stage de rejeição. |
| `APPROVED → ACTIVE` | Adapter e capacidade do alvo validados; Harness ainda válido; destino confirma versão/digest aprovada | Intenção, chamada, resposta/estado consultado, versão anterior e nova; evento de ativação só após confirmação. |
| `ACTIVE → REVERTED` | Operador ou política aplicável pede reversão da versão ativa, e adapter confirma a anterior | Versões antes/depois, motivo, decisão e confirmação. |

Se a ativação ficar `UNKNOWN`, consultar o alvo antes de qualquer retry; proposta continua sem `ACTIVE` até confirmação do digest exato. Se consulta não existir, manter aprovação e diagnóstico sem afirmar sucesso; isso bloqueia PBI-30. Se ativação de versão nova for confirmada e reversão falhar, preservar estado que o destino realmente reporta, registrar falha/uncertainty e não declarar `REVERTED`. Stores do journal, Harness, catálogo e skills não participam da mesma transação. A escrita no target é condicionada à identidade do Operador e a capacidade real; não é efeito automático do Meta-Agent.

Todos os adapters de target recebem expectedTargetVersion, desiredTargetVersion e operationId conforme EV-R22. APPROVED não reserva a versão do destino. A guarda precisa atuar na escrita de ativação e na de reversão; uma consulta antes da chamada ou a confirmação posterior não impedem sobrescrita concorrente. Sem CAS ou exclusão confiável de todos os escritores, PBI-30 permanece bloqueado para aquele destino.

## Eventos e persistência

Os eventos abaixo mapeiam os números e nomes canônicos finais de 001 a aliases lógicos TypeScript; payload é o mínimo de rastreio proposto.

| # / evento canônico de 001 | Alias TS proposto | Disparo / payload mínimo | Persistência e consumidor |
|---|---|---|---|
| 36 — Lote de auditoria elegível para retrospectiva reunido | `LoteDeAuditoriaElegivelParaRetrospectivaReunido` | seleção; batch ID, janela/filtros, refs, coverage e omissões | Evolução journal/outbox; Operador e Meta-Agent. |
| 37 — Padrão recorrente de falha identificado | `PadraoRecorrenteDeFalhaIdentificado` | análise; pattern ID, refs, observações, lacunas/contraexemplos, nível hipótese/padrão | Evolução journal/outbox; Meta-Agent e Operador. Não declara causalidade por si só. |
| 38 — Proposta de evolução versionada criada | `PropostaDeEvolucaoVersionadaCriada` | proposta; proposal ID/version/digest, target, fonte, attestation quando aplicável | Proposal repository/outbox; Operador. |
| 39 — Não-regressão da proposta avaliada | `NaoRegressaoDaPropostaAvaliada` | Harness executado; digest, baseline, suite, status, output refs | Evidência imutável/outbox; gate de validação. |
| 40 — Proposta de evolução aprovada explicitamente | `PropostaDeEvolucaoAprovadaExplicitamente` | aprovação autenticada; Operador, digest e target | Approval repository/outbox; ativação condicional. |
| 41 — Proposta de evolução rejeitada explicitamente | `PropostaDeEvolucaoRejeitadaExplicitamente` | rejeição autenticada; Operador, digest e motivo | Approval repository/outbox; sem ativação. |
| 42 — Evolução aprovada ativada em versão identificada | `EvolucaoAprovadaAtivadaEmVersaoIdentificada` | confirmação no destino; target, versão ativa, digest e versão anterior | Activation record/outbox após confirmação; Operador. |
| 43 — Evolução revertida para versão anterior | `EvolucaoRevertidaParaVersaoAnterior` | confirmação de rollback; target, versão restaurada, versão revertida e motivo | Reversal record/outbox; Operador. |

Persistência do contexto: lote e cobertura; referências/consumo de auditoria; padrão/proveniência; proposta e digest; relatório de Harness/attestor; decisão autenticada; intenção/resultado de ativação; histórico de reversão; outbox e CAS. Dados fonte da auditoria permanecem em Execução; target mantém seu próprio estado. Evento de outbox é local/at-least-once, deduplicado por ID sem prometer atomicidade ou exactly-once externo. Registros ainda elegíveis não são apagados por um lote incompleto. Política temporal segue Q07.

## Refinamento por PBI

| PBI | Aceite refinado | Dependências |
|---|---|---|
| PBI-27 | Meta-Agent consulta auditoria descartada do período solicitado; lote mostra cobertura, exclusões e provenance, e análise separa observação, hipótese e padrão sustentado sem causalidade automática. | Journal/consulta autorizada de Execução (C07); Q06/Q07 Deferred; retenção/consulta não podem ser presumidas completas. |
| PBI-28 | Proposta contém alvo tipado, versão/digest, hipótese, referências fonte e plano de validação. Attestor é aplicado e registrado apenas em `SKILL_CONTRACT`; Tradição, regra Harness e contrato de skill seguem adapters distintos. | Padrão auditável PBI-27; `skill-contract-attestor` com resultado comprovável quando skill; capacidades dos destinos continuam condicionais. |
| PBI-29 | Harness executa não-regressão contra baseline e candidato exatos; saída observada, digest, suite/ambiente e resultado ficam consultáveis, antes de qualquer ativação. | Prova Harness vinculada à versão; `evaluate_fast_path`, manifest, attestation ou relatório declarativo não substituem execução. Sem Harness disponível, proposta não valida. |
| PBI-30 | Operador aprova/rejeita versão exata; ativação só ocorre após gate de qualidade/capacidade e confirmação do target; reversão restaura versão anterior confirmada com trilha preservada. | PBI-29 + autorização C08 + adapter real para o alvo. Skills deferred exigem capacidade de admissão separada; sem ela PBI-30 não é concluído nem reduzido a um relatório. |

## Ordered Development Tasks

Tasks condicionais à proveniência de auditoria, saída real do Harness, autorização do Operador e capacidade confirmada do target. Estimativas de 2–4h são hipóteses de sessão, não prazos contratados; caminhos são contratos futuros, não arquivos existentes. Ativadores de tema, regra local e skill devem ser separados. A ausência de capacidade mantém a etapa bloqueada. O array segue dependências principais; `all_requires` abaixo lista os pré-requisitos adicionais completos. Para ativação/reversão, EV-10 e EV-11 exigem o adapter do target escolhido: exatamente um entre EV-07 (Tradição), EV-08 (Harness rule) e EV-09 (skill).

```json
[
  {
    "id": "EV-01",
    "title": "Definir contratos de retrospectiva e evolução",
    "description": "Modela IDs, estágios C08, targets distintos e referências de evidência para impedir mistura entre qualidade e ativação.",
    "estimate_hours": 3,
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
    "pbis": [
      "PBI-27",
      "PBI-28",
      "PBI-29",
      "PBI-30"
    ],
    "depends_on": null
  },
  {
    "id": "EV-02",
    "title": "Selecionar auditorias elegíveis com cobertura",
    "description": "Reúne refs de auditoria por período solicitado sem definir cadência nem reter/apagar a fonte.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/CollectEligibleAudits.ts",
      "packages/cbm-orchestrator/src/evolution/RetrospectiveBatch.ts",
      "packages/cbm-orchestrator/src/ports/AuditReadPort.ts"
    ],
    "acceptance": [
      "Lote informa cobertura e refs omitidas com motivo",
      "Consumo só é marcado depois de resultado persistido"
    ],
    "pbis": [
      "PBI-27"
    ],
    "depends_on": "EV-01"
  },
  {
    "id": "EV-03",
    "title": "Registrar padrões com provenance",
    "description": "Agrupa observações de falha preservando fonte, contraexemplos e limites da amostra.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/PatternFinding.ts",
      "packages/cbm-orchestrator/src/evolution/AnalyzeRecurringPatterns.ts",
      "packages/cbm-orchestrator/src/evolution/PatternRepository.ts"
    ],
    "acceptance": [
      "Poucos exemplos/cobertura parcial permanecem hipótese sem alegação causal",
      "Cada padrão referencia auditorias e lacunas consultáveis"
    ],
    "pbis": [
      "PBI-27",
      "PBI-28"
    ],
    "depends_on": "EV-02"
  },
  {
    "id": "EV-04",
    "title": "Criar propostas por target tipado",
    "description": "Gera proposta versionada e encaminha attestation somente quando o artefato alvo é contrato de skill.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ProposeEvolution.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionTarget.ts",
      "packages/cbm-orchestrator/src/ports/SkillContractAttestorPort.ts"
    ],
    "acceptance": [
      "Tradição, Harness rule e skill contract mantêm target/provenance próprios",
      "Attestation ausente ou ligada a digest diferente não valida contrato de skill"
    ],
    "pbis": [
      "PBI-28"
    ],
    "depends_on": "EV-03"
  },
  {
    "id": "EV-05",
    "title": "Persistir evidência executada de não-regressão",
    "description": "Executa Harness para baseline/candidato e grava saída que valida qualidade independentemente da admissão do target.",
    "estimate_hours": 4,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ValidateNonRegression.ts",
      "packages/cbm-orchestrator/src/evolution/HarnessRunEvidence.ts",
      "packages/cbm-orchestrator/src/ports/HarnessValidationPort.ts"
    ],
    "acceptance": [
      "PASS fica vinculado ao digest, baseline, suite e ambiente exatos",
      "Bundle, fast-path eligibility e campos declarados não contam como execução"
    ],
    "pbis": [
      "PBI-29"
    ],
    "depends_on": "EV-04"
  },
  {
    "id": "EV-06",
    "title": "Registrar decisão humana versionada",
    "description": "Aplica aprovação/rejeição autenticada por CAS à versão e target exatos da proposta validada.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ApprovalRecord.ts",
      "packages/cbm-orchestrator/src/evolution/ApproveEvolution.ts",
      "packages/cbm-orchestrator/src/ports/OperatorDecisionPort.ts"
    ],
    "acceptance": [
      "Meta-Agent não consegue emitir aprovação",
      "Mudança de digest invalida decisão anterior e requer nova avaliação"
    ],
    "pbis": [
      "PBI-30"
    ],
    "depends_on": "EV-05"
  },
  {
    "id": "EV-07",
    "title": "Definir adapter de Tradição com gate de capacidade",
    "description": "Encapsula consulta, ativação versionada e reversão do catálogo sem tratar DRAFT como tema ativo.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/ThemeCatalogAdapter.ts",
      "packages/cbm-orchestrator/src/ports/ThemeCatalogPort.ts",
      "packages/cbm-orchestrator/src/evolution/TargetCapability.ts"
    ],
    "acceptance": [
      "Operação só é habilitada após contrato/autorização do target validados",
      "Estado DRAFT ou resposta incerta não é reportado ACTIVE"
    ],
    "pbis": [
      "PBI-28",
      "PBI-30"
    ],
    "depends_on": "EV-01"
  },
  {
    "id": "EV-08",
    "title": "Definir adapter de regras locais do Harness",
    "description": "Isola o fluxo de proposta, revisão, ativação e reversão no store de governança local.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/HarnessRuleAdapter.ts",
      "packages/cbm-orchestrator/src/ports/HarnessRulePort.ts",
      "packages/cbm-orchestrator/src/evolution/TargetCapability.ts"
    ],
    "acceptance": [
      "Regra local não é publicada como Tradição",
      "Schema/revisão incompletos deixam target bloqueado até capacidade comprovada"
    ],
    "pbis": [
      "PBI-28",
      "PBI-30"
    ],
    "depends_on": "EV-01"
  },
  {
    "id": "EV-09",
    "title": "Condicionar adapter de admissão de skills",
    "description": "Especifica a fronteira de admissão sem habilitar skills deferred na ausência de mecanismo validado.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/SkillAdmissionAdapter.ts",
      "packages/cbm-orchestrator/src/ports/SkillAdmissionPort.ts",
      "packages/cbm-orchestrator/src/evolution/TargetCapability.ts"
    ],
    "acceptance": [
      "Constellation/level continuam bloqueadas sem capacidade real de admissão",
      "Relatório ou attestation não é tratado como instalação/ativação"
    ],
    "pbis": [
      "PBI-28",
      "PBI-30"
    ],
    "depends_on": "EV-01"
  },
  {
    "id": "EV-10",
    "title": "Orquestrar ativação e reconciliação",
    "description": "Grava intenção e troca a versão do destino sob CAS/serialização confiável por alvo, reconciliando incerteza sem marcar ACTIVE antecipadamente.",
    "estimate_hours": 4,
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
    "pbis": [
      "PBI-30"
    ],
    "depends_on": "EV-06"
  },
  {
    "id": "EV-11",
    "title": "Reverter evolução para versão anterior",
    "description": "Restaura a versão anterior somente se a versão corrente do destino ainda for a instalada por esta ativação, sob guarda atômica e confirmação.",
    "estimate_hours": 4,
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
    "pbis": [
      "PBI-30"
    ],
    "depends_on": "EV-10"
  },
  {
    "id": "EV-12",
    "title": "Publicar eventos de evolução e consumo",
    "description": "Persiste eventos correlacionados para auditoria e telemetria sem apagar as fontes de Execução.",
    "estimate_hours": 3,
    "scope": [
      "packages/cbm-orchestrator/src/evolution/EvolutionEvents.ts",
      "packages/cbm-orchestrator/src/evolution/EvolutionOutbox.ts",
      "packages/cbm-orchestrator/src/evolution/RetrospectiveBatchRepository.ts"
    ],
    "acceptance": [
      "Eventos de Harness, aprovação, rejeição, ativação e reversão carregam a versão/digest exatos e refs consultáveis",
      "Consumo é deduplicado e só confirmado junto ao lote/análise persistidos"
    ],
    "pbis": [
      "PBI-27",
      "PBI-28",
      "PBI-29",
      "PBI-30"
    ],
    "depends_on": "EV-02"
  }
]
```

### Pré-requisitos completos da task graph (`all_requires`)

`all_requires` inclui `depends_on`. Na linha EV-10, a escolha de adapter é uma alternativa determinada pelo alvo, não uma dependência dos três adapters.

| Task | `depends_on` principal | `all_requires` completo |
|---|---|---|
| EV-01 | — | — |
| EV-02 | EV-01 | EV-01 |
| EV-03 | EV-02 | EV-02 |
| EV-04 | EV-03 | EV-03 |
| EV-05 | EV-04 | EV-04 |
| EV-06 | EV-05 | EV-05 |
| EV-07 | EV-01 | EV-01 |
| EV-08 | EV-01 | EV-01 |
| EV-09 | EV-01 | EV-01 |
| EV-10 | EV-06 | EV-01, EV-02, EV-03, EV-04, EV-05, EV-06 e exatamente um adapter EV-07/08/09 compatível com o target |
| EV-11 | EV-10 | EV-10 e o mesmo adapter EV-07/08/09 usado para o target |
| EV-12 | EV-02 | EV-02, EV-03, EV-04, EV-05, EV-06, EV-10, EV-11 |

## Cenários de teste especificados

Especificação Given/When/Then; nenhum cenário é teste executável ou prova de capacidade. A aprovação e qualidade se referem a versão/digest exatos.

### PBI-27 — auditoria elegível e padrões

<a id="ev-s01"></a>

**EV-S01 — Deve reunir auditoria não consumida com cobertura rastreável** *(Integration; PBI-27; EV-R01–05)*

- **Given:** Execução contém referências autorizadas de horizontes descartados no período solicitado, com algumas já consumidas e outras elegíveis
- **When:** Meta-Agent coleta um lote
- **Then:** lote registra janela, critérios, refs incluídas/omitidas e cobertura; somente refs elegíveis entram e permanecem consultáveis na fonte.

<a id="ev-s02"></a>

**EV-S02 — Deve manter elegíveis registros quando a análise falha antes de persistir** *(Integration; PBI-27; EV-R03/04)*

- **Given:** lote carregou referências não consumidas
- **When:** persistência da análise/proposta falha antes do commit local
- **Then:** nenhuma referência fonte é apagada nem marcada como consumida e a próxima tentativa pode reexaminar o mesmo conjunto de forma idempotente.

<a id="ev-s03"></a>

**EV-S03 — Deve registrar hipótese quando poucos exemplos não sustentam recorrência ou causalidade** *(Unit; PBI-27; EV-R05/06)*

- **Given:** lote tem poucos exemplos ou cobertura parcial e inclui evidências divergentes
- **When:** análise agrupa ocorrências semelhantes
- **Then:** resultado é apresentado como observação/hipótese com refs e lacunas, sem inventar mínimo numérico nem atribuir causa.

### PBI-28 — proposta por alvo

<a id="ev-s04"></a>

**EV-S04 — Deve propor Tradição sem misturá-la a regra local ou contrato de skill** *(Functional; PBI-28; EV-R07–11)*

- **Given:** padrão sustentado possui evidências, fonte e hipótese ligadas a uma possível prática de ofício
- **When:** Meta-Agent abre proposta com alvo `THEME_TRADITION`
- **Then:** proposta fica versionada no journal de Evolução, referencia o catálogo-alvo e aguarda avaliação/aprovação; não altera catálogo, regra Harness, skill nem binding normativo automaticamente.

<a id="ev-s05"></a>

**EV-S05 — Deve encaminhar attestation somente para contrato de skill** *(Unit; PBI-28; EV-R08/11/12)*

- **Given:** proposta versionada tem alvo `SKILL_CONTRACT` e contém digest do contrato
- **When:** fluxo chama `skill-contract-attestor` e captura sua resposta rastreável
- **Then:** attestation é vinculada ao digest e julga procedimento universal/separação de craft norms, sem substituir Harness ou admissão.

<a id="ev-s06"></a>

**EV-S06 — Deve bloquear validação de skill quando attestation é ausente, inconclusiva ou de outra versão** *(Unit; PBI-28; EV-R12/14)*

- **Given:** contrato de skill tem artefato modificado após attestation ou não há saída verificável
- **When:** proposal tenta avançar
- **Then:** permanece `PROPOSED` e não reutiliza resultado de outro digest.

<a id="ev-s07"></a>

**EV-S07 — Deve manter Harness rule no store local de governança** *(Integration; PBI-28; EV-R08/10/21)*

- **Given:** proposta visa uma restrição específica deste projeto
- **When:** target é `HARNESS_RULE`
- **Then:** adapter selecionado é o do Harness/store local, não o catálogo de Tradições nem o registro de skills.

### PBI-29 — não-regressão no Harness

<a id="ev-s08"></a>

**EV-S08 — Deve validar não-regressão com execução ligada à versão proposta** *(Integration; PBI-29; EV-R13/14)*

- **Given:** baseline e candidato possuem digest, suite e configuração identificados
- **When:** Harness executa a avaliação de não-regressão e retorna PASS
- **Then:** evidência consultável fica ligada ao candidato exato e permite `PROPOSED→VALIDATED`, ainda que a capacidade de ativação esteja indisponível.

<a id="ev-s09"></a>

**EV-S09 — Deve impedir VALIDATED quando Harness falha ou não executa** *(Integration; PBI-29; EV-R13/14)*

- **Given:** proposta possui somente manifest declarativo, bundle de elegibilidade ou resultado Harness FAILED/UNAVAILABLE/INCONCLUSIVE
- **When:** qualidade é avaliada
- **Then:** não-regressão não é considerada provada, proposta não avança a `VALIDATED` e nenhuma declaração `tdd_oracles_passed` substitui a execução observada.

### PBI-30 — decisão, ativação e reversão

<a id="ev-s10"></a>

**EV-S10 — Deve exigir aprovação autenticada do Operador para digest atual** *(Functional; PBI-30; EV-R07/15/16)*

- **Given:** proposta `VALIDATED` tem Harness PASS para digest D
- **When:** Operador autenticado aprova exatamente target/versão/digest D
- **Then:** decisão é durável e proposta avança a `APPROVED`; Meta-Agent não pode emitir essa aprovação.

<a id="ev-s11"></a>

**EV-S11 — Deve invalidar aprovação após mudança de versão ou rejeição explícita** *(Integration; PBI-30; EV-R15/16)*

- **Given:** aprovação registrada para digest D ou decisão rejeitada para a versão atual
- **When:** artefato muda para digest D2, ou proposta é rejeitada pelo Operador
- **Then:** D não autoriza D2, a rejeição fica auditável e nenhuma versão é ativada sem novo ciclo de qualidade/aprovação.

<a id="ev-s12"></a>

**EV-S12 — Deve ativar e confirmar a versão aprovada pelo adapter de target** *(Functional; PBI-30; EV-R14–18)*

- **Given:** proposta `APPROVED`, Harness PASS atual e adapter autorizado confirmou capacidade para o target
- **When:** Operador solicita ativação e o destino confirma o digest aprovado como ativo
- **Then:** registro guarda versão anterior/nova e proposta torna-se `ACTIVE` somente após confirmação.

<a id="ev-s13"></a>

**EV-S13 — Deve manter skills deferred bloqueadas sem admissão real** *(Integration; PBI-30; EV-R14/17/20)*

- **Given:** proposta de ativação para `constellation-individuator` ou `level-graduator`, com qualidade descrita mas sem API de admissão validada
- **When:** fluxo verifica capacidade de destino
- **Then:** proposta não fica `ACTIVE`, skill permanece deferred e relatório não substitui a habilitação real exigida por PBI-30.

<a id="ev-s14"></a>

**EV-S14 — Deve reconciliar timeout de ativação antes de qualquer retry** *(Integration; PBI-30; EV-R17/18)*

- **Given:** destino pode ter aplicado versão aprovada, mas resposta se perdeu
- **When:** registro de efeito fica `UNKNOWN`
- **Then:** adapter consulta versão ativa antes de novo comando; sem confirmação, proposta não é marcada `ACTIVE` e não há replay cego.

<a id="ev-s15"></a>

**EV-S15 — Deve reverter para versão anterior e preservar trilha** *(Functional; PBI-30; EV-R18/19)*

- **Given:** evolução está `ACTIVE` e versão anterior V1 é identificada
- **When:** Operador solicita reversão e target confirma V1
- **Then:** evento de reversão liga versões/motivo/Operador, stage torna-se `REVERTED` e histórico da versão ativa anterior permanece.

<a id="ev-s16"></a>

**EV-S16 — Deve preservar diagnóstico quando reversão falha parcialmente** *(Integration; PBI-30; EV-R18/19)*

- **Given:** reversão foi registrada e chamada ao destino falha ou fica incerta
- **When:** adapter não confirma a versão restaurada
- **Then:** proposta não é marcada `REVERTED`, versão corrente é consultada ou registrada como desconhecida e decisão continua auditável sem alegar rollback concluído.


### Concorrência no destino — PBI-30

<a id="ev-s17"></a>

**EV-S17 — Deve recusar a segunda ativação aprovada para um baseline já substituído** *(Integration; PBI-30; EV-R16/17/18/22)*
- **Given:** propostas A e B aprovadas para o mesmo target e baseline V1
- **When:** A troca atomicamente V1 por V2 e B tenta trocar V1 por V3
- **Then:** B recebe TARGET_VERSION_CONFLICT e não sobrescreve V2; o registro mantém a versão observada e exige nova avaliação/aprovação sobre o baseline atual. Sem CAS ou exclusão confiável, nenhuma troca é despachada.

<a id="ev-s18"></a>

**EV-S18 — Deve preservar evolução posterior ao receber rollback obsoleto** *(Integration; PBI-30; EV-R18/19/22)*
- **Given:** A instalou V2 sobre V1 e outra ativação válida instalou V3 depois
- **When:** rollback de A tenta trocar expectedTargetVersion=V2 por desiredTargetVersion=V1
- **Then:** a guarda atômica recusa TARGET_VERSION_CONFLICT, V3 permanece e A não é marcada REVERTED por uma restauração que não ocorreu.

## Gaps mantidos

- Q06 cadence e Q07 retenção seguem Deferred; nenhuma frequência, janela padrão, prazo ou limiar numérico foi criado.
- Não há base para afirmar causalidade a partir de uma amostra pequena. A quantidade mínima e a calibração de confiança permanecem sem decisão; resultados insuficientes são hipóteses com provenance.
- Harness bundle/fast-path e manifest não comprovam execução de não-regressão. O port/runner e evidência independente exata ainda precisam validação; qualidade não depende de target estar habilitado.
- Catálogo de Tradições tem ferramentas versionadas, mas o fluxo de mutação/ativação e autoridade é parcial: `founding_decide` aceito retorna `DRAFT`, e lacunas de schema/credencial exigem validação. Regras locais do Harness têm store distinto e lacunas de revisão/schema.
- `skill-contract-attestor` especifica análise do contrato; não existe admissão pública demonstrada. `constellation-individuator` e `level-graduator` continuam deferred até mecanismo real de habilitação/rollback ser provado.
- Ativação/reversão em qualquer target não compartilha transação com o journal TS. Resposta perdida exige consulta/reconciliação; sem consulta ou confirmação, manter status não concluído e bloquear PBI-30.
- ADR-005 continua PROPOSED; o core C fica inalterado. Uma capacidade que exigir mudança no core volta ao Operador como gap de escopo.
