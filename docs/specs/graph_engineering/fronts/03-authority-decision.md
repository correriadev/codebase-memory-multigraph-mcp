# Frente 03 — Refinamento tático e cenários: Autoridade e Decisão

**Domínio:** Graph Engineering  
**Projeto:** `codebase-memory-multigraph-mcp`  
**Pacote proposto:** `packages/cbm-orchestrator/` (recomendação, não implementação existente)  
**Frente:** F04/F06 — PBI-15–18 e PBI-23–26  
**Data:** 2026-10-23  
**Status:** proposta `model`; não comprova implementação ou capacidade existente.

Este suplemento alimenta a consolidação do desenho tático e dos cenários por projeto. A estrutura segue os limites do pacote TypeScript novo e os contratos compartilhados C01–C08; nomes de tipos e ports abaixo são nomes lógicos propostos, não APIs disponíveis. As referências válidas são `001-problem-space.md`, `002-context-map.md`, `005-shared-contracts.md`, `006-capability-matrix.md`, `DECISIONS.md` e PBB v3. O core C e seu banco permanecem fora da propriedade deste contexto.

## Respostas de refinamento aplicáveis

| ID | Categoria | Questão | Recomendação | Resposta final / fonte | Respondido por |
|---|---|---|---|---|---|
| Q03 | business | Agentes são personas? | Tratar Operador e perfis de agente como atores distintos do fluxo. | Humanos + agentes como personas. PBB Q03; não define por si só identidade autenticada. | human |
| Q04 | integration | Qual contrato executa os agentes e garante isolamento/medição? | Manter a fronteira mediada como proposta e exigir evidência do spike para qualquer autonomia. | Deferred; ADR-005 segue PROPOSED. Não há runtime, isolamento ou autorização por perfil comprovados. | unresolved |
| Q07 | data | Por quanto tempo reter auditoria? | Configurar retenção e proteger registros ainda não consumidos pela retrospectiva. | Deferred; nenhum prazo é fixado por este desenho. | unresolved |

Q04 é bloqueio para comprometer autoridade por perfil e execução autônoma: o spike futuro deve provar a recusa de chamada direta e de rotas alternativas no ambiente controlado. Q07 impede inventar TTL de evidência ou prazo de auditoria; atualidade é decidida pela igualdade das revisões e pela revalidação de C01/C04, não por um intervalo temporal arbitrário.

## 1. Estrutura principal

`AgentRuntimePort`, `TrustedAgentHostPort` e `AgentHostPort` são visões do mesmo contrato Q04 de ciclo de vida, identidade/mediação e integração, não runtimes ou fontes de identidade independentes. `AuthorityJournal` também é boundary lógico dentro do journal TypeScript C07; não impõe banco físico separado.

| Elemento | Camada / tipo | Invariantes e regras técnicas | Contrato |
|---|---|---|---|
| `VersaoCandidata` | Raiz lógica do contexto | Imutável depois de selada; qualquer mudança no artefato ou revisão relevante cria nova identidade. `candidateVersion` é identidade/token de referência para comparação, não credencial bearer nem autoridade. Mantém projeto, território, digest do candidato e baseline e revisões relevantes. | C01 |
| `DossieDeEvidencias` | Raiz lógica do contexto | Contém evidências autenticadas vinculadas à tupla exata da versão; separa quem produz de quem valida. Declaração de manifest não é saída de execução. | C01/C04 |
| `VeredictoIndependente` | Modelo de evidência | `GREEN` só se verificações aplicáveis terminarem para a versão atual; `RED`, `INCONCLUSIVE` e `UNAVAILABLE` não podem virar verde por timeout ou interpretação. | C04/C05 |
| `AvaliacaoDeRisco` | Modelo de decisão | Combina fatores topológicos, estado das evidências e restrições adicionais sob `policyRevision`; registra fatores e incertezas sem inventar pesos, limites ou score padrão. | C01/C08 |
| `DecisaoDePromocao` | Raiz lógica do contexto | Resultado determinístico `ALLOW`, `DENY` ou `ESCALATE` da política TS; `ALLOW` só permite solicitar a tool ao CBM, cujo gate C existente ainda executa e pode recusar. | C04 |
| `PerfilDePermissao` | Política de autorização proposta | Usa perfil emitido pelo host confiável; autoriza cada despacho, inclusive chamada direta por nome, e não confia em papel autodeclarado pelo agente. | C01/C05 |
| `DecisaoDoOperador` | Registro de comando humano | Aprovar, rejeitar ou solicitar correção com identidade autenticada, versão e comparação de estado; aprovação não dispensa o gate. | C08 |
| `PacoteDeDecisao` | Read model para revisão | Mostra mudança, versão, evidências, fatores e motivo de risco, impacto e alvo de navegação. Link de nó/aresta só é funcional após validação na UI real. | C08 |

Os roots são fronteiras lógicas propostas, compatíveis com o mapa de contextos; não exigem uma migração DDD do core. Exemplos ilustrativos de shape, cada um limitado a quatro linhas:

```text
VersaoCandidata: projectId + horizonId + candidateVersion
artifactDigest + baselineDigest + revisões C01
imutável após selagem; mudança cria outra versão
digest identifica integridade, não qualidade
```

```text
VeredictoIndependente: candidateVersion + verifierId
status: GREEN | RED | INCONCLUSIVE | UNAVAILABLE
evidenceRefs + razões + tupla C01
GREEN requer verificações aplicáveis concluídas
```

```text
DecisaoDoOperador: decisionId + actorId + command
command: APPROVE | REJECT | REQUEST_CORRECTION
candidateVersion + estado esperado para compare-and-set
correção cria versão nova e invalida aprovação anterior
```

### Regras da frente

| ID | Regra proposta | Base |
|---|---|---|
| AD-R01 | Depois que o Builder produz e persiste uma saída candidata, a versão é selada antes da coleta final de evidências; o dossiê registra a identidade emitida e a tupla C01 completa. Alteração posterior cria outra versão e invalida veredicto, fast path e aprovação anteriores. | PBB/001, C01; evento “Versão candidata identificada e selada” |
| AD-R02 | Identidade e perfil vêm do host confiável e são vinculados à sessão/processo. O Builder não escolhe seu próprio perfil nem recebe material de operador, conexão direta ao CBM ou credencial capaz de contornar o proxy. | C01/C05; D02; Q04 |
| AD-R03 | A política de perfil é aplicada no ponto de despacho a cada chamada. Ocultar uma tool da descoberta não é autorização; chamadas diretas por nome e caminhos alternativos disponíveis no ambiente controlado devem ser recusados. | PBI-15/16, C05, D02 |
| AD-R04 | Evidência deve identificar produtor/origem autenticada, papel, tipo, resultado, ambiente ou comando quando aplicável, digest e referência consultável. Evidência malformada, declarada sem prova, de outra versão ou com revisão divergente é rejeitada. | C01/C04 |
| AD-R05 | Verificador independente não é o Builder e executa em processo/instância separados, sem compartilhar o contexto de geração. Família de modelo diferente continua hipótese a validar e não substitui separação nem constitui garantia. | PBI-17, C04/C05 |
| AD-R06 | Toda rota, inclusive humana e fast path, usa a mesma política Gatekeeper determinística TS, fora do LLM: a versão atual precisa de evidência independente `GREEN`, âncoras válidas e revisão de risco/política atual. `ALLOW` é pré-autorização de chamada; em seguida o gate C existente aplica suas próprias pré-condições e ainda pode recusar. | INV-01, C04, PBI-18/24/26 |
| AD-R07 | Verificador ausente, indisponível, inconclusivo, timeout, evidência incompleta ou risco desconhecido nunca libera promoção. O gate retorna `DENY` ou `ESCALATE` segundo a condição; não converte ambiguidade em sucesso. | INV-01/05, C04, PBI-18/24 |
| AD-R08 | Fast path requer baixo risco comprovado, nó folha, fingerprint de efeito intacto e ausência de marcadores de risco. Além desses requisitos próprios, mantém todas as condições de AD-R06. | PBI-24, C04 |
| AD-R09 | Avaliação de risco combina posição/topologia, evidências e restrições adicionais, incluindo âncora protegida e marcador sensível quando fornecidos. Política sem configuração válida ou dado desconhecido não recebe classificação baixa por omissão. Nenhum peso ou limiar é fixado nesta frente. | PBI-23/24; não há parâmetro humano definido para pesos/limites |
| AD-R10 | Decisão humana é autenticada, registrada e aplicada por compare-and-set contra estado e versão atuais. `APPROVE` solicita avaliação do gate; `REJECT` não promove; `REQUEST_CORRECTION` retorna ao ciclo com nova versão. O resultado operacional de descarte depende da capacidade suportada e não é simulado por fechar uma sessão Union. | PBI-26, C08, C07 |
| AD-R11 | Antes de solicitar promoção, revalidar tupla, âncoras, evidência e política sob uma fronteira que exclua mutações concorrentes. Se não for possível comprovar essa exclusão/atualidade entre verificação e chamada, não invocar promoção. | C04, INV-01, D04 |
| AD-R12 | `sessionId` de Union e `horizonId`/overlay são identidades distintas e vinculadas explicitamente. Fechar uma sessão Union não é API genérica de descarte de horizonte; claim Union não é lock de escrita. | C01/C07, PBI-22, matriz 006 |
| AD-R13 | O pacote de decisão inclui mudança, evidências, fatores/motivos de risco e impacto. Só anuncia link direto após comprovar na graph-ui a abertura do nó/aresta exato; link de projeto, texto ou coordenada não cumpre PBI-25. | PBI-25, C08, matriz 006 |
| AD-R14 | Journal TypeScript e estado do CBM permanecem boundaries separados, sem exigir bancos físicos distintos. Intenção de promoção, chamada e resultado são registros distintos. Antes de F05, crash não retoma checkpoint automaticamente: inicia novo ciclo desde o objetivo, reconcilia efeitos em voo e mantém timeout posterior ao despacho como `UNKNOWN`, sem repetição cega. | C02/C07, INV-03/04 |
| AD-R15 | `restricted` significa restrição por contrato de skill desconhecido e classes específicas de efeito; não é RBAC dos perfis. Processo separado, sozinho, também não prova isolamento nem bloqueia credenciais, endpoints, arquivos ou subprocessos. | ORIENTATION, C05, matriz 006 |
| AD-R16 | Chamadas de verificação, runtime, promoção e retry debitam/reservam no único ledger global C03 do ciclo antes do efeito. Consumo em voo/desconhecido permanece pendente; uso desconhecido suspende novos despachos sem cobertura conservadora. Q05 não fornece defaults ou limites numéricos nesta frente. | C03, Q05, PBI-16/18 |

### Limite de ameaça e confiança

| Fronteira / ameaça | Controle modelado | Limite da garantia |
|---|---|---|
| Builder acidental ou malicioso tenta promover diretamente | Proxy/host confiável verifica identidade e permissão por despacho, inclusive nome literal de tool; negar antes de qualquer efeito. | Só vale no ambiente controlado coberto pelo spike. Se o Builder alcança MCP por outro canal, PBI-16 permanece bloqueado. |
| Perfil falsificado pelo próprio LLM ou por mensagem | Identidade de agente/perfil emitida pelo host e persistida com a execução; entrada do LLM não altera a identidade. | O mecanismo de identidade do host é Q04 e ainda não foi selecionado nem comprovado. |
| Builder e adversarial compartilham processo/contexto | Executar verificador em processo/instância separada e fornecer candidato congelado e evidência autorizada. | Diversidade de família é hipótese experimental, não prova de independência. Não alegar resistência contra operador malicioso fora da fronteira. |
| Evidência falsa, declarada, antiga, truncada ou adulterada | ACL valida origem, papel, status, digest, referência e tupla atual C01; rejeita se não puder consultar ou autenticar. | Digest prova integridade do conteúdo referenciado, não qualidade. Formato canônico fica para spike. |
| Alteração entre avaliação e promoção (TOCTOU) | Selar candidato, impedir mutações durante revalidação/operação e comparar versão/revisões antes de invocar; divergência cancela a decisão. | Mecanismo de exclusão/CAS externo ainda não foi comprovado; sem ele não há promoção autônoma segura. |
| Timeout na chamada externa ou resposta malformada | Normalizar resposta via ACL, persistir intenção e consultar estado antes de concluir/repetir. | Não há transação distribuída com o CBM; estado não confirmável permanece incerto e escala. |
| Operador aprova apesar de evidência ausente ou gate vermelho | Comando humano registra intenção; gate determinístico continua obrigatório. | Não existe exceção humana à INV-01. |

### Tipos e ports lógicos

Esses são limites internos sugeridos; nomes, métodos, esquemas e adapters dependem do spike e do mapeamento de capacidade. Não se declara que tais interfaces já existam.

| Tipo / port proposto | Responsabilidade | Tipo e validações | Autoridade / integração |
|---|---|---|---|
| `CandidateVersionRef` | Identificar exatamente candidato e contexto avaliado | Tupla C01; imutável; `projectId` resolvido pelo CBM, sem usar basename | C01; host/repositório de código e CBM via ACL |
| `AuthenticatedAgentIdentity` | Vincular agente, perfil, processo e execução | Emissão confiável pelo host; builder/adversarial/gatekeeper/meta não é campo autodeclarado | C01/C05; depende de Q04 |
| `EvidenceRecord` | Representar uma prova consultável | Status `PASS/FAIL/INCONCLUSIVE/UNAVAILABLE`, origem, papel, tempo, digest e tupla | C04; Harness/verificador via ACL |
| `CandidateContextPort` | Ler estado/revisões e validar atualidade do candidato | Snapshot da geração, regras, cobertura, território e âncoras; cobertura/gaps acompanham a resposta | CBM/MCP, conforme mapeamento 006 |
| `IndependentVerificationPort` | Solicitar e receber verificação separada | Produz status e referências ligadas à versão; timeout não produz `GREEN` | Harness/runtime Adversarial; Q04 |
| `TrustedAgentHostPort` (visão de `AgentHostPort`) | Iniciar/isolar perfis e mediar todas as tools | Identidade emitida pelo host; controla transporte e credenciais, não só descoberta | Q04/ADR-005; proposta D02 |
| `PromotionPort` | Solicitar operação real de promoção e ler resultado | Adapter para tool MCP observada `promote_horizon`; Gatekeeper TS pré-autoriza, mas não substitui o gate C nem presume aceitação do core | CBM/MCP; sem escrita direta no SQLite C |
| `GlobalBudgetPort` | Reservar e reconciliar chamadas do ciclo | Reserva atômica no ledger global C03 antes de dispatch; custo em voo/desconhecido fica pendente; sem defaults Q05 | Journal lógico do ciclo; não cria teto por perfil |
| `DecisionNavigationPort` | Validar alvo e abrir deep link do graph-ui | Nó/aresta esperados devem corresponder ao selecionado na UI | graph-ui; capacidade não comprovada em 006 |

Tipos que especializam C01/C04/C05/C08 devem ser propostos ao CTO se alterarem os contratos; esta frente não redefine tupla, estados, motivos ou permissões compartilhadas.

### Matriz lógica de perfis

| Perfil | Pode fazer na política proposta | Proibições que o despacho deve impor |
|---|---|---|
| Builder | Ler contexto autorizado; editar apenas escopo alocado; produzir candidata. | Promover; conceder autoridade; emitir veredicto independente para sua candidata; ativar evolução. |
| Adversarial | Ler candidata congelada e evidências; produzir contestação/veredicto independente. | Alterar candidata; promover; remover bloqueio próprio sem evidência. |
| Gatekeeper | Validar tupla/evidências e aplicar política determinística TS; pré-autorizar solicitação pelo port autorizado ao gate C existente. | Gerar código; confiar em perfil autodeclarado; dispensar evidência, invariante ou decisão do gate C. |
| Meta-Agent | Ler auditoria autorizada; propor evolução versionada. | Aprovar, ativar ou substituir a própria proposta. |
| Operador humano | Enviar comando autenticado de aprovação, rejeição ou correção para estado/versão aplicáveis. | Pular o gate, transformar evidência ausente em verde ou aprovar versão diferente da avaliada. |

Essa matriz é uma política TS proposta que precisa passar pela fronteira de host/proxy. Não descreve RBAC nativo do CBM e não é satisfeita por remover ferramentas da lista de descoberta.

**Composição dos gates:** o Gatekeeper é uma camada de política determinística TypeScript, anterior ao gate C existente. Ele verifica identidade/perfil, versão, evidência e rota, e pode autorizar apenas a solicitação pela integração MCP. O handler/função de promoção do CBM continua aplicando as pré-condições nativas do território; recusa do CBM é um resultado distinto, não um sucesso do Gatekeeper nem motivo para contornar o core.

### Casos de uso e transições

| Operação lógica | Responsabilidade | Coordena |
|---|---|---|
| `SealCandidateVersion` | Capturar e selar a identidade C01 antes de avaliar a mudança | Fonte do artefato, CBM/ACL, journal TS |
| `AssembleEvidenceDossier` | Reunir evidências consultáveis e validar origem, papel, status, digest e versão | `CandidateContextPort`, `IndependentVerificationPort` |
| `AssessPromotionRisk` | Combinar fatores disponíveis sob uma revisão de política explícita e preservar incerteza | Snapshot de grafo, evidências, restrições adicionais |
| `EvaluatePromotionRoute` | Aplicar política pre-gate TS e decidir `ALLOW`, `DENY` ou `ESCALATE` | Dossiê, risco, perfil autenticado, política C04/C05 |
| `BuildDecisionPackage` | Preparar revisão humana com alvo, impacto, risco, evidências e link validável | Read model da decisão, `DecisionNavigationPort` |
| `RecordOperatorDecision` | Gravar comando autenticado contra versão/estado esperado e aplicar sua transição | Identidade do Operador, journal/outbox, ciclo de execução |
| `RequestCurrentPromotion` | Revalidar sob exclusão de mutação, obter pré-autorização TS, registrar intenção e chamar a integração MCP | Gatekeeper determinístico, `PromotionPort`, journal TS; gate C subsequente |
| `ReconcilePromotionOutcome` | Confirmar, recusar ou manter resultado incerto após a chamada | Estado consultável do CBM, chave idempotente e auditoria |

Transições propostas, alinhadas a C02/C04/C08: (1) Builder produz e persiste saída candidata; (2) selar a versão; (3) reunir evidências e emitir veredicto dessa versão; (4) avaliar risco com política e dados explícitos; (5) executar fast path só se baixo risco comprovado e gate verde, ou mudar `state` de `RUNNING` para `PAUSED` com `phase=AWAITING_DECISION` e montar pacote; (6) decisão aplicável e compare-and-set permitem `PAUSED → RUNNING`; (7) revalidar versão e evidências; (8) Gatekeeper TS pré-autoriza a solicitação pela capacidade real; (9) definir `phase=PROMOTING`, chamar o gate C e confirmar ou reconciliar o resultado. `state`, `phase` e motivo terminal são campos separados e permanecem exatamente os de C02; `COMPLETED` não significa promoção. Correção cria nova versão antes de nova coleta. Se persistir decisão falhar, a promoção não é chamada. A resposta do CBM não é inferida a partir da aceitação do comando.

## 2. Eventos e persistência

O evento usa o envelope C06 sem redefini-lo. Os eventos abaixo derivam do Event Storming de 001; os campos indicados são o mínimo específico além do envelope e das referências C01.

| Evento canônico de 001 | Gatilho | Payload adicional mínimo | Consumidores |
|---|---|---|---|
| #19 — Versão candidata identificada e selada | Saída do Builder foi persistida e congelada para avaliação | `candidateVersion`, digest de artefato/baseline e revisões C01 | Dossiê, risco, Operador, Gatekeeper |
| #21 — Pacote de evidências da versão reunido | Evidências consultáveis foram vinculadas à versão selada | Referências, status e gaps | Avaliação de risco, Gatekeeper, Operador |
| #22 — Veredicto independente emitido | Verificador distinto conclui ou registra resultado não conclusivo para a versão exata | Status, origem autenticada, evidências e tupla C01 | Dossiê, risco, Gatekeeper, Operador |
| #24 — Risco da versão avaliado | Avaliação sob política corrente é registrada | Revisão de política, fatores, classe/resultado e incertezas | Fast path, escalonamento, telemetria |
| #27 — Escalonamento de alto risco ou risco desconhecido aberto | Rota exige revisão humana | Pacote ou referência, versão, fatores e alvo de navegação validado ou pendente | Operador, UI, telemetria |
| #28 — Decisão humana sobre horizonte registrada | Comando passa compare-and-set | Ator, comando, versão, estado anterior/novo e consequência selecionada | Execução, Gatekeeper, auditoria |
| #20 — Avaliação anterior invalidada por mudança de versão | Nova versão é criada | Versão anterior/nova e referências invalidadas | Dossiê, Gatekeeper, Operador |
| #29 — Fast path admitido para avaliação de promoção | Predicados de baixo risco comprovado passam pela política | Revisão de política, fatores e versão | Gatekeeper determinístico |
| #30 — Promoção autorizada para versão atual | Política Gatekeeper TS emite `ALLOW` e autoriza uma solicitação para a tupla corrente | Versão, evidências, decisão, revisão de política e chave idempotente; isso ainda não confirma consolidação pelo CBM | Adapter MCP, Operador, auditoria |
| #31 — Promoção recusada pela Autoridade e Decisão | Política TS emite `DENY`/`ESCALATE` sem chamar promoção | Versão, status/gap, motivo e referências | Operador, Gatekeeper, auditoria |
| #32 — Solicitação de promoção recusada pelo CBM | Gate C ou pré-condição do território recusa a solicitação já autorizada pela política TS | Versão, resposta consultável e motivo retornado | Operador, Gatekeeper, auditoria |
| #33 — Território consolidado no grafo após autorização | Resposta do CBM confirma consolidação | Identificador do território/versão e referência de resposta | Operador, execução, telemetria |

| Recurso / adapter | Operações conceituais | Persistência / estado esperado |
|---|---|---|
| `AuthorityJournal` (boundary lógico TS/C07) | Registrar versão selada, dossiê, risco, decisão, intenção e reconciliação; ler por ciclo/versão | Transação local e comparação otimista; estado + outbox atômicos. Pode compartilhar infraestrutura física do journal TS; sem transação distribuída com CBM/runtime. |
| `CandidateContextPort` | Consultar horizonte/geração/regras/cobertura/âncoras e validar alvo de promoção | Fonte de conhecimento continua CBM; resposta traduzida e validada pela ACL. |
| `IndependentVerificationPort` | Solicitar execução do verificador e recuperar saída/referência | Origem e saída observadas; `UNAVAILABLE`/`INCONCLUSIVE` duráveis, sem preencher status por declaração. |
| `PromotionPort` | Invocar a tool existente pelo canal autorizado; consultar resultado/estado | Intenção durável antes do despacho; gate C continua independente; resultado confirmado, recusado pelo CBM ou `UNKNOWN`, com reconciliação. |
| `DecisionNavigationPort` | Resolver/abrir alvo do pacote na graph-ui | Só retorna link marcado funcional depois de comprovar seleção exata na UI real. |

O adapter MCP traduz capacidades observadas, sem inventar aliases. `promote_horizon` existe, mas não prova identidade do perfil, veredicto independente atual, exclusão de mutações ou segurança contra bypass. `evaluate_fast_path` e `record_admission_manifest` não substituem evidência independente; `tdd_oracles_passed` é declaração. A sessão Union, o horizonte e o overlay permanecem objetos distintos; `union_session_close` não cumpre descarte genérico. Descarte exigido por uma decisão depende da capacidade suportada e dos registros de auditoria de C07; sem operação comprovada, o trabalho correspondente fica bloqueado.

## 3. Critérios de aceite e rastreabilidade por PBI

As relações de problema/expectativa P02/E02/P04/E04 são mantidas conforme a rastreabilidade do PBB; não são dependências de implementação. “Dependência refinada” indica contrato ou capacidade necessária, não mudança de prioridade nem alegação de implementação.

| PBI | Critério observável de aceite do refinamento | Problemas / expectativas (PBB) | Dependência refinada / bloqueio |
|---|---|---|---|
| PBI-15 | Tentativa direta de operação proibida por perfil é recusada no despacho e não produz efeito; a permissão é testada no canal de chamada, não apenas na lista de tools. | P02 / E02 | Identidade confiável e proxy/host Q04; provar chamada direta e rotas alternativas. `restricted` não satisfaz o aceite. |
| PBI-16 | Builder não consegue invocar promoção por tool direta ou caminho alternativo no ambiente controlado; Gatekeeper autorizado usa a mesma invariante do PBI-18. | P02 / E02 | Faz parte do slice inicial; autonomia permanece bloqueada até prova do spike Q04 e estado efetivo incompatível com `CBM_ALLOW_LEGACY_PROMOTION=1`. |
| PBI-17 | Adversarial roda em processo/instância e contexto separados do Builder; sua origem/veredicto não é autoatribuída pelo LLM. A família de modelo diferente é hipótese registrada, não substituto da separação. | P02 / E02 | Q04/ADR-005 e integração Harness/runtime; separar processo sozinho não demonstra barreira de transporte. |
| PBI-18 | Só `GREEN` independente, completo e atual permite a política Gatekeeper TS avaliar `ALLOW`; versão ou revisão divergente, `RED`, `INCONCLUSIVE` e `UNAVAILABLE` resultam em não promoção. `ALLOW` só autoriza solicitar a operação; o gate C existente ainda pode recusar. | P02, P04 / E02 | C01/C04/C05; precisa de verificador separado, revisão de versão atual e prova de revalidação/exclusão antes da operação MCP. |
| PBI-23 | Avaliação registra contribuição de topologia, evidência e restrições adicionais para a versão, sem tratar posição isolada como risco completo. Dados ausentes ficam explícitos. | P04 / E04 | Snapshot, coverage/freshness e política revisada; limiares/ponderações permanecem configuração explícita, sem valores inventados. |
| PBI-24 | Fast path aceita somente baixo risco comprovado com nó folha, fingerprint intacto, sem marcador de risco, evidência íntegra e verde; qualquer incerteza escala e toda promoção passa pelo gate PBI-18. | P04 / E04 | PBI-23, C04 e política explícita; sem score/configuração válida ou com evidência incompleta, não há fast path. |
| PBI-25 | Alto risco pausa o horizonte; pacote inclui mudança, evidências, motivo e impacto; deep link abre o nó/aresta crítica exatos na graph-ui. | P04 / E04 | Validar a navegação na UI existente. A capacidade não comprovada mantém PBI-25 bloqueado; link textual/projeto não atende. |
| PBI-26 | Aprovar/rejeitar/corrigir é autenticado, versionado, aplicado e auditável; aprovação vale só para a versão avaliada e continua condicionada à invariante PBI-18. | P04 / E04 | C08/C07, PBI-18 e reconciliação. Descarte e retorno ao ciclo precisam capacidade operacional confirmada; fechar sessão Union não é substituto. |

**Invariante de promoção transversal:** nenhuma aprovação humana nem fast path contorna evidência independente verde para a versão corrente. Uma alteração invalida veredicto e aprovação. **Limite do slice:** PBI-16 está incluído, mas a prova de autoridade é precondição para ativar autonomia; documentação não resolve Q04 nem aceita ADR-005.

## Ordered Development Tasks

Lista condicional de implementação futura; estimativas são sessões de trabalho de 2–4h e não autorização para executar o spike nesta etapa. A sequência mantém tipos, regra/gate, casos de uso, persistência e integrações externas. `scope` enumera 2–4 interfaces/tipos a tocar; `acceptance` descreve evidência revisável. PBI-16 não fica concluído se AD-02 não demonstrar recusa no ambiente controlado.

```json
[
  {
    "id": "AD-01",
    "title": "Modelar versão candidata e evidência vinculada",
    "description": "Define tipos imutáveis que vinculam artefato, baseline, revisões e evidências à identidade exata C01.",
    "estimate_hours": 3,
    "pbis": ["PBI-18", "PBI-23", "PBI-24", "PBI-26"],
    "scope": ["CandidateVersionRef (C01)", "EvidenceRecord (C04)", "IndependentVerdict (C04)"],
    "acceptance": ["Qualquer mudança de artefato ou revisão relevante exige uma versão nova", "Evidência não pode ser vinculada sem identidade, digest e tupla C01"],
    "depends_on": null
  },
  {
    "id": "AD-02",
    "title": "Provar fronteira de identidade e despacho por perfil",
    "description": "Executa a parte de autoridade do spike Q04 para verificar identidade confiável e recusa de chamadas diretas ou alternativas do Builder, sem substituir os demais critérios do ADR-005.",
    "estimate_hours": 4,
    "pbis": ["PBI-15", "PBI-16", "PBI-17"],
    "scope": ["TrustedAgentHostPort (proposta)", "ProfilePermissionPolicy (C05)", "Configuração de transporte e credenciais do ambiente controlado"],
    "acceptance": ["Evidência reproduzível demonstra recusa de promoção direta e das rotas alternativas cobertas sem efeito no CBM", "Qualquer rota alcançável sem autorização deixa Q04 aberto e bloqueia autonomia", "O resultado desta fatia não marca Q04 concluído sem os demais critérios do ADR-005"],
    "depends_on": "AD-01"
  },
  {
    "id": "AD-03",
    "title": "Separar verificação e validar proveniência da evidência",
    "description": "Conecta um verificador em processo/contexto separado e traduz saídas observadas para o dossiê da versão candidata.",
    "estimate_hours": 3,
    "pbis": ["PBI-17", "PBI-18"],
    "scope": ["IndependentVerificationPort (proposta)", "EvidenceDossier", "ACL do Harness/runtime"],
    "acceptance": ["Saída consultável identifica versão, origem e status, e indisponibilidade ou inconclusão nunca produz GREEN", "Família de modelo, se diferente, é registrada como hipótese experimental e não como prova de isolamento"],
    "depends_on": "AD-02"
  },
  {
    "id": "AD-04",
    "title": "Implementar política determinística antes do gate C",
    "description": "Aplica a invariante comum de evidência verde e atual antes de solicitar a operação ao gate C existente.",
    "estimate_hours": 3,
    "pbis": ["PBI-15", "PBI-16", "PBI-18"],
    "scope": ["PromotionDecision (C04)", "DeterministicGatekeeper", "ProfilePermissionPolicy (C05)"],
    "acceptance": ["Só a tupla C01 atual com veredicto independente GREEN e âncoras válidas pode resultar em ALLOW", "ALLOW autoriza apenas a solicitação e não substitui nem presume aceitação pelo gate C", "Aprovação humana e fast path não criam exceção à invariante"],
    "depends_on": "AD-03"
  },
  {
    "id": "AD-05",
    "title": "Avaliar risco e elegibilidade de fast path",
    "description": "Combina fatores disponíveis com política explícita e escala estado desconhecido ou evidência incompleta.",
    "estimate_hours": 4,
    "pbis": ["PBI-23", "PBI-24"],
    "scope": ["RiskAssessment", "RiskPolicyRevision", "FastPathEligibility"],
    "acceptance": ["Avaliação registra topologia, evidências, restrições e incertezas sob a política usada", "Fast path exige todos os predicados PBI-24 e ainda passa pelo gate AD-04"],
    "depends_on": "AD-01"
  },
  {
    "id": "AD-06",
    "title": "Registrar comandos humanos e montar pacote de decisão",
    "description": "Aplica aprovar, rejeitar ou solicitar correção à versão atual e apresenta evidências e impactos para o Operador.",
    "estimate_hours": 3,
    "pbis": ["PBI-25", "PBI-26"],
    "scope": ["OperatorDecision (C08)", "DecisionPackage (C08)", "CompareAndSet de estado e versão"],
    "acceptance": ["Cada comando registra ator, versão e consequência; comando para versão obsoleta é recusado", "Correção cria versão nova e invalida decisão/evidência anteriores"],
    "depends_on": "AD-05"
  },
  {
    "id": "AD-07",
    "title": "Persistir intenção e resultado de promoção",
    "description": "Grava intenção versionada e outbox local antes da chamada; reconcilia resultado externo sem tratar timeout como sucesso.",
    "estimate_hours": 3,
    "pbis": ["PBI-16", "PBI-18"],
    "scope": ["AuthorityJournal (boundary lógico C07)", "PromotionIntent", "OutboxEnvelope"],
    "acceptance": ["Intenção versionada e outbox são duráveis com CAS antes da chamada externa", "Falha de persistência bloqueia o despacho; timeout posterior permanece UNKNOWN até reconciliação"],
    "depends_on": "AD-01"
  },
  {
    "id": "AD-08",
    "title": "Validar deep link na graph-ui existente",
    "description": "Comprova que o alvo do pacote abre o nó ou aresta crítica exatos na interface real antes de declarar PBI-25 entregue.",
    "estimate_hours": 3,
    "pbis": ["PBI-25"],
    "scope": ["DecisionNavigationPort (proposta)", "Adapter graph-ui", "Validação de seleção do alvo"],
    "acceptance": ["Teste de navegação na UI confirma o mesmo nó/aresta esperado", "Se a UI não suportar o alvo exato, PBI-25 permanece bloqueado sem link substituto anunciado como direto"],
    "depends_on": "AD-06"
  },
  {
    "id": "AD-09",
    "title": "Integrar promoção e reconciliar corrida de versão",
    "description": "Revalida e congela a versão antes da tool MCP observada, confirmando o resultado ou preservando estado incerto.",
    "estimate_hours": 4,
    "pbis": ["PBI-16", "PBI-18"],
    "scope": ["PromotionPort", "Adapter da tool promote_horizon", "Reconciliador de estado e resultado", "GlobalBudgetPort C03"],
    "acceptance": ["Mudança entre verificação e chamada cancela a promoção; sem prova de exclusão de mutações não há despacho", "Reserva do ledger global antecede a chamada e timeout mantém custo/efeito pendente até reconciliação", "Nenhum timeout é repetido às cegas"],
    "depends_on": "AD-07"
  },
  {
    "id": "AD-10",
    "title": "Persistir comandos humanos versionados",
    "description": "Registra decisão, versão esperada e consequência no journal C07 com compare-and-set e outbox local.",
    "estimate_hours": 3,
    "pbis": ["PBI-26"],
    "scope": ["OperatorDecision (C08)", "AuthorityJournal (C07)", "OutboxEnvelope"],
    "acceptance": ["Comando autenticado só altera estado/versão esperados", "Falha no commit não avança estado nem dispara efeito externo"],
    "depends_on": "AD-07"
  },
  {
    "id": "AD-11",
    "title": "Aplicar decisão humana à versão corrente",
    "description": "Encaminha aprovação ao mesmo Gatekeeper; rejeição e correção aplicam somente consequências suportadas.",
    "estimate_hours": 3,
    "pbis": ["PBI-26"],
    "scope": ["OperatorDecision", "CompareAndSet", "RequestCurrentPromotion"],
    "acceptance": ["APPROVE não contorna evidência verde, Gatekeeper nem gate C; REJECT não promove", "REQUEST_CORRECTION cria nova versão e invalida aprovação/veredicto anteriores"],
    "depends_on": "AD-10"
  }
]
```

`depends_on` registra apenas o predecessor principal; pré-requisitos adicionais abaixo são específicos da operação e não formam cadeia artificial. AD-02 demonstra somente parte do spike Q04, sem concluir ADR-005.

| Task | Pré-requisito adicional | Por quê |
|---|---|---|
| AD-03 | AD-01, AD-02 e ER-17. | Usa identidade/versionamento de evidência e o adapter concreto do host validado; a porta abstrata isoladamente não inicia o verificador. |
| AD-04 | AD-02 e AD-03. | Política de promoção exige identidade de despacho e veredicto independente ligado à versão. |
| AD-05 | AD-03 para avaliar evidência; AD-04 somente para ativar uma rota de fast path. | Avaliação de risco não depende do gate, embora fast path continue passando por ele. |
| AD-07 | AD-02, AD-03 e AD-04 para ativar promoção; nenhuma é prova completa de Q04. | A persistência é modelável sem runtime, mas despacho só pode ocorrer com host, evidência e política válidos. |
| AD-09 | AD-02, AD-03 e AD-04, além do journal AD-07. | Integra o caminho mínimo do PBI-16 sem depender do pacote humano/deep link de F06. |
| AD-08 | UI graph-ui real e pacote AD-06. | Prova de deep link é própria do PBI-25 e não pré-requisito da promoção. |
| AD-10 | AD-06 para comandos/estado; reutiliza o journal de AD-07. | Persistência da decisão humana estende o modelo, mas não bloqueia o slice PBI-16. |
| AD-11 | AD-09 para o ramo APPROVE; rotas REJECT/REQUEST_CORRECTION não solicitam promoção. | Aprovação usa o caminho de promoção comum; demais comandos aplicam efeitos suportados. |

## 5. Cenários de teste especificados

Especificação Given/When/Then derivada das regras AD-R01–AD-R16 e PBIs desta frente; nenhum cenário é teste executável nem afirma resultado de runtime já obtido. A validação de deep link só pode passar com a graph-ui real disponível. Evidência sobre host/proxy só pode passar no spike do ambiente controlado.

**Domínio:** Graph Engineering  
**Projeto:** `codebase-memory-multigraph-mcp`  
**Pacote proposto:** `packages/cbm-orchestrator/`  
**Framework:** não selecionado para o novo pacote TypeScript; `docs/adr/TESTS.md` descreve tooling existente C/Python/shell/Windows e não escolhe runner TypeScript.

### Unit

<a id="ad-s01"></a>

#### AD-S01 — Should autorizar operações permitidas somente para o perfil autenticado

- **PBI / regra:** PBI-15; AD-R02/03.
- **Given:** uma identidade Builder emitida pelo host e uma política de operações explícita para o perfil.
- **When:** o gate avalia uma operação permitida no escopo alocado e outra proibida.
- **Then:** a primeira pode seguir a política e a segunda resulta em recusa antes de qualquer despacho externo; papel declarado pelo LLM não altera a decisão.

<a id="ad-s02"></a>

#### AD-S02 — Should negar promoção ao Builder mesmo quando ele solicita a tool pelo nome

- **PBI / regra:** PBI-15/16; AD-R03/06.
- **Given:** identidade Builder autenticada e a operação real `promote_horizon` registrada como proibida para esse perfil.
- **When:** o Builder tenta solicitar a operação diretamente pelo nome literal.
- **Then:** o ponto de despacho recusa a chamada e não há efeito de promoção no CBM.

<a id="ad-s03"></a>

#### AD-S03 — Should rejeitar veredicto não verde ou evidência incompatível com a versão

- **PBI / regra:** PBI-18; AD-R01/04/06/07.
- **Given:** um dossiê com status `RED`, `INCONCLUSIVE` ou `UNAVAILABLE`, ou com digest/revisão diferente da versão selada.
- **When:** o Gatekeeper avalia promoção.
- **Then:** o resultado não é `ALLOW` e identifica a evidência ou revisão que impede a decisão.

<a id="ad-s04"></a>

#### AD-S04 — Should rejeitar evidência malformada ou sem origem verificável

- **PBI / regra:** PBI-18; AD-R04/05.
- **Given:** uma evidência sem origem autenticada, digest, versão ou referência consultável.
- **When:** o dossiê tenta aceitar a evidência.
- **Then:** ela é rejeitada; um campo declarativo de manifest, como `tdd_oracles_passed`, não substitui saída observada.

<a id="ad-s05"></a>

#### AD-S05 — Should classificar risco desconhecido como não elegível para fast path

- **PBI / regra:** PBI-23/24; AD-R08/09.
- **Given:** snapshot sem um fator topológico, restrição ou evidência requerida pela política ativa.
- **When:** a rota é avaliada.
- **Then:** a incerteza fica registrada e o resultado escala ou nega fast path; nenhuma classe baixa é inferida por omissão.

<a id="ad-s06"></a>

#### AD-S06 — Should invalidar aprovação quando uma correção cria nova versão

- **PBI / regra:** PBI-26; AD-R01/10.
- **Given:** uma aprovação e veredicto vinculados a uma versão selada.
- **When:** uma correção altera o artefato ou revisão relevante.
- **Then:** cria-se uma nova identidade de versão e a aprovação/veredicto anteriores não podem autorizar a nova versão.

### Integration

<a id="ad-s07"></a>

#### AD-S07 — Should recusar chamada direta de promoção por Builder antes de alcançar o CBM

- **PBI / regra:** PBI-15/16; AD-R03/15.
- **Given:** host/proxy Q04 configurado no ambiente controlado, sem canal direto ou credencial CBM disponível ao Builder.
- **When:** o Builder envia `promote_horizon` diretamente por uma conexão de tool.
- **Then:** a mediação recusa a chamada e a consulta posterior confirma ausência de efeito; qualquer rota que alcance o CBM sem autorização deixa PBI-16 bloqueado.

<a id="ad-s08"></a>

#### AD-S08 — Should recusar acesso alternativo do Builder fora do canal de tools anunciado

- **PBI / regra:** PBI-15/16; AD-R02/03/15.
- **Given:** a matriz de rotas de transporte, credenciais, endpoints, arquivos e subprocessos do host controlado.
- **When:** o Builder tenta usar uma rota alternativa coberta para invocar promoção.
- **Then:** credenciais ou canal não estão acessíveis ou a mediação recusa sem efeito no CBM; se um caminho alternativo continuar alcançável, o critério Q04 falha e autonomia não é liberada.

<a id="ad-s09"></a>

#### AD-S09 — Should manter o verificador em processo e contexto separados do Builder

- **PBI / regra:** PBI-17/18; AD-R05.
- **Given:** um candidato selado e um Builder em execução.
- **When:** o verificador Adversarial é iniciado para produzir evidência independente.
- **Then:** o runtime mostra processo/instância e contexto separados, identidade verificada e saída vinculada à mesma versão; apenas usar família diferente de modelo não satisfaz o cenário.

<a id="ad-s10"></a>

#### AD-S10 — Should bloquear a corrida entre revalidação e chamada de promoção

- **PBI / regra:** PBI-18/26; AD-R01/11.
- **Given:** veredicto verde atual e revalidação final concluída para uma versão selada, mas o transporte ainda não demonstrou exclusão de mutações até a tool.
- **When:** uma mutação concorrente do artefato/baseline/revisão tenta ocorrer entre a revalidação e a chamada MCP.
- **Then:** a escrita é excluída ou a divergência é detectada antes do efeito e a chamada é cancelada; se a exclusão não puder ser comprovada, nenhuma promoção é despachada e a versão precisa de nova avaliação.

<a id="ad-s11"></a>

#### AD-S11 — Should manter estado não promovido quando o verificador está indisponível ou inconclusivo

- **PBI / regra:** PBI-18; AD-R06/07.
- **Given:** verificador que retorna timeout, indisponibilidade ou resultado inconclusivo.
- **When:** o Gatekeeper avalia a candidata para promoção.
- **Then:** registra não promoção (`DENY` ou `ESCALATE` conforme política explícita) e não chama a tool de promoção.

<a id="ad-s12"></a>

#### AD-S12 — Should registrar risco com topologia, evidências e restrições da mesma revisão

- **PBI / regra:** PBI-23; AD-R09.
- **Given:** snapshot de grafo consultável com cobertura/revisões e política de risco configurada explicitamente.
- **When:** `AssessPromotionRisk` avalia a candidata.
- **Then:** o registro identifica fatores topológicos, evidências e restrições usados e suas revisões; nenhum peso ou limiar ausente é inventado.

<a id="ad-s13"></a>

#### AD-S13 — Should admitir fast path apenas sob todas as condições explícitas e com gate verde

- **PBI / regra:** PBI-24/18; AD-R06/08/09.
- **Given:** nó folha, fingerprint íntegro, ausência de marcadores de risco, baixo risco comprovado e veredicto independente verde atual.
- **When:** a rota autônoma é avaliada.
- **Then:** fast path pode prosseguir somente para o gate determinístico, que revalida a versão antes da promoção.

<a id="ad-s14"></a>

#### AD-S14 — Should pausar horizonte de alto risco e compor o pacote completo

- **PBI / regra:** PBI-25; AD-R09/13.
- **Given:** risco alto produzido pela política e snapshot da candidata com referências de evidência.
- **When:** a rota é avaliada.
- **Then:** horizonte fica pausado para Operador e pacote contém mudança, evidências, fatores/motivo de risco e impacto, sem transformar o link em autorização.

<a id="ad-s15"></a>

#### AD-S15 — Should validar deep link exato contra a graph-ui real

- **PBI / regra:** PBI-25; AD-R13.
- **Given:** um pacote de decisão com alvo de nó ou aresta crítica e graph-ui existente na configuração avaliada.
- **When:** o link é aberto na UI.
- **Then:** a UI seleciona o alvo exato do pacote; se a navegação for ausente ou apontar para outro alvo, PBI-25 não passa e o pacote não apresenta link substituto como direto.

<a id="ad-s16"></a>

#### AD-S16 — Should aplicar comando humano por versão e estado atuais

- **PBI / regra:** PBI-26; AD-R06/10.
- **Given:** horizonte pausado, versão atual, decisão autenticada e dossiê verde atual.
- **When:** Operador aprova e o estado passa compare-and-set.
- **Then:** decisão é registrada e encaminhada ao Gatekeeper TS; só o resultado `ALLOW` pré-autoriza a solicitação, que ainda precisa ser aceita pelo gate C existente para confirmar promoção.

<a id="ad-s17"></a>

#### AD-S17 — Should impedir que aprovação humana contorne evidência ou Gatekeeper

- **PBI / regra:** PBI-18/26; AD-R06/07/10.
- **Given:** verificador indisponível, evidência antiga ou resultado não verde.
- **When:** Operador aprova a candidata.
- **Then:** a intenção é auditada, mas a decisão de promoção é recusada e nenhuma tool de promoção é chamada.

<a id="ad-s18"></a>

#### AD-S18 — Should retomar o ciclo com nova versão após pedido de correção

- **PBI / regra:** PBI-26; AD-R01/10.
- **Given:** candidato pausado com aprovação ou dossiê da versão anterior.
- **When:** Operador solicita correção e o Builder altera o candidato.
- **Then:** uma nova versão é selada, a decisão anterior é invalidada e evidências precisam ser coletadas novamente antes de qualquer promoção.

<a id="ad-s19"></a>

#### AD-S19 — Should registrar rejeição sem promover e aplicar consequência suportada

- **PBI / regra:** PBI-26; AD-R10/12/14.
- **Given:** horizonte pausado e comando autenticado `REJECT` para a versão atual.
- **When:** o comando é aplicado.
- **Then:** a recusa é durável e não há promoção; descarte só é confirmado por capacidade suportada, caso contrário a consequência permanece bloqueada e visível, sem fingir que fechar Union descartou o overlay.

<a id="ad-s20"></a>

#### AD-S20 — Should impedir chamada de promoção após falha ao persistir decisão

- **PBI / regra:** PBI-18/26; AD-R10/14.
- **Given:** storage TS indisponível ao persistir a decisão/intenção ou a outbox.
- **When:** Gatekeeper tenta avançar para despacho.
- **Then:** nenhuma chamada MCP é feita e o estado requer reconciliação explícita.

<a id="ad-s21"></a>

#### AD-S21 — Should reconciliar timeout da promoção antes de qualquer nova tentativa

- **PBI / regra:** PBI-18/26; AD-R14.
- **Given:** chamada de promoção foi despachada, mas o resultado não foi persistido por timeout.
- **When:** o sistema recupera a execução.
- **Then:** estado permanece `UNKNOWN` até consulta suportada ao CBM; não repete às cegas e registra divergência se resultado não puder ser confirmado.

### Functional

<a id="ad-s22"></a>

#### AD-S22 — Should completar fast path baixo risco sem abrir exceção ao gate

- **PBI / regra:** PBI-16/18/24; AD-R03/06/08/11.
- **Given:** Builder concluiu candidato elegível e evidência independente verde é atual, com fronteira Q04 comprovada.
- **When:** o ciclo escolhe fast path e solicita promoção.
- **Then:** chamada só é emitida por identidade Gatekeeper após revalidar tupla/âncoras; resultado promovido só é registrado após confirmação do CBM.

<a id="ad-s23"></a>

#### AD-S23 — Should escalar risco desconhecido sem iniciar promoção

- **PBI / regra:** PBI-23/24/25; AD-R07/09/13.
- **Given:** candidata com restrição ou fator de topologia desconhecido, ou evidência incompleta.
- **When:** o ciclo calcula a rota.
- **Then:** horizonte pausa e o Operador recebe pacote com as incertezas e impacto conhecidos; fast path e promoção não são chamados.

<a id="ad-s24"></a>

#### AD-S24 — Should manter aprovação limitada à versão avaliada em fluxo completo

- **PBI / regra:** PBI-18/26; AD-R01/06/10/11.
- **Given:** Operador aprovou uma candidata e, antes da operação, uma edição gera outra versão.
- **When:** Gatekeeper recebe a autorização atrasada para a versão antiga.
- **Then:** compare-and-set recusa a decisão obsoleta, invalida a aprovação anterior e exige nova avaliação; a edição não chega ao CBM por essa autorização.

<a id="ad-s25"></a>

#### AD-S25 — Should rejeitar independência quando Builder e verificador compartilham processo ou contexto

- **PBI / regra:** PBI-17/18; AD-R05.
- **Given:** Builder e verificador compartilham processo, instância ou contexto de geração, embora usem famílias de modelo diferentes.
- **When:** a saída do verificador é apresentada como evidência independente.
- **Then:** a separação não é reconhecida, a evidência não é `GREEN` para fins de promoção e a hipótese de família diversa não mascara o compartilhamento.

<a id="ad-s26"></a>

#### AD-S26 — Should manter PBI-25 bloqueado quando o deep link não abre o alvo exato

- **PBI / regra:** PBI-25; AD-R13.
- **Given:** graph-ui não confirma seleção exata do nó/aresta crítica identificada no pacote.
- **When:** o escalonamento prepara navegação para o Operador.
- **Then:** o link não é declarado funcional nem substituído por URL de projeto ou coordenada textual; o critério de aceite de PBI-25 permanece pendente.

<a id="ad-s27"></a>

#### AD-S27 — Should iniciar do objetivo e reconciliar efeito incerto após crash pré-F05

- **Nível:** Functional.
- **PBI / regra:** PBI-16/18; AD-R14.
- **Given:** intenção de promoção foi persistida e chamada despachada, mas processo cai antes do resultado durável; F05 ainda não está habilitado.
- **When:** nova execução é iniciada após diagnóstico da execução anterior.
- **Then:** execução começa do objetivo, sem retomar checkpoint automaticamente; estado CBM é consultado e efeito permanece `UNKNOWN` se inconclusivo, sem nova chamada de promoção cega.

<a id="ad-s28"></a>

#### AD-S28 — Should compartilhar uma única reserva C03 entre promoção e retries

- **Nível:** Functional.
- **PBI / regra:** PBI-16/18; AD-R16.
- **Given:** uma chamada de verificação/promoção do ciclo mantém uso em voo ainda desconhecido e outro trigger ou retry tenta consumir o mesmo orçamento do objetivo.
- **When:** ledger global recebe a nova reserva.
- **Then:** reserva/custo pendente não é liberado, uso desconhecido suspende novo dispatch sem cobertura conservadora e nenhum perfil recebe um teto próprio duplicado.

<a id="ad-s29"></a>

#### AD-S29 — Should selar somente depois da saída persistida do Builder

- **Nível:** Functional.
- **PBI / regra:** PBI-18; AD-R01.
- **Given:** ciclo em `phase=BUILDING` sem saída candidata persistida do Builder.
- **When:** uma solicitação tenta selar candidato ou iniciar verificação independente.
- **Then:** ambas as ações ficam bloqueadas; após persistir a saída, uma nova versão é selada e só então evidências/veredicto dessa tupla podem ser coletados.

## 6. Bloqueios e limites para a consolidação

- **Q04 / ADR-005 PROPOSED:** o proxy/host TypeScript é uma proposta. Até o spike provar ciclo, cancelamento, medição, recuperação e sobretudo recusa de chamada direta fora do intent scope, não declarar isolamento, RBAC ou autonomia segura. Falha em impedir rota alternativa bloqueia PBI-15/16 e o slice que depende de PBI-16.
- **Corrida entre revisão e promoção:** C04 exige exclusão de mutação e revalidação, mas o mecanismo que dá essa garantia não está provado. Se a tool ou host não permite fechar essa janela, a regra é não promover e retornar o gap; não ampliar o core C implicitamente.
- **Deep link:** PBI-25 só passa quando navegação real por nó/aresta é comprovada. A matriz 006 não confirma esse contrato.
- **Capacidade de descarte:** fechar sessão Union é diferente de descartar horizonte/overlay. F05/PBI-22 e a capacidade do CBM precisam resolver o efeito selecionado por rejeição; não inventar endpoint geral.
- **Parâmetros pendentes:** não fixar score, peso, limiar, família de modelo obrigatória, idade máxima ou retenção. Q04, Q05, Q06, Q07 e Q08 permanecem Deferred conforme `DECISIONS.md`; esta frente não os resolve.
- **Limite de confiança:** a garantia proposta cobre agente e rotas do host controlado após prova. Não afirmar defesa completa contra operador malicioso, nem usar `restricted`, prompt, lista de tools ou processo separado como prova suficiente.
