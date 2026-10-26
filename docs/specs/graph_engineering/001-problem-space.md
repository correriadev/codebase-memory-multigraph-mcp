# 001 — Espaço do problema: Graph Engineering

**Produto:** `cbm-orchestrator` (nome provisório), camada nova de orquestração em TypeScript neste repositório. **Escopo:** PBB v3 integral, F01–F08 e PBI-01–32. Este documento é o modelo de domínio proposto para orientar os refinamentos seguintes; não afirma que as capacidades descritas já existem.

## 1. Event Storming — fluxo de negócio proposto

O fluxo cobre o ciclo desde o objetivo até a decisão, a auditoria e a medição. Disparos de eventos, tarefas paralelas e verificações podem intercalar-se em execução; cada linha abaixo continua sendo um evento distinto. A identidade e atualidade da versão seguem C01; evidência, veredicto e promoção seguem C04. Correção produz uma versão nova e invalida avaliação e aprovação anteriores. C01–C08 em [`005-shared-contracts.md`](./005-shared-contracts.md) definem os contratos canônicos; esta análise não cria variantes desses contratos.

| # | Evento de domínio (passado) | Comando (gatilho) | Agregado | Sistemas externos | Read models consumidos por usuário ou sistema externo |
|---:|---|---|---|---|---|
| 1 | Objetivo de mudança registrado | Registrar objetivo | Ciclo de execução | — | Resumo do ciclo para Operador |
| 2 | Política explícita do ciclo validada | Configurar orçamento, limites e condição de parada | Ciclo de execução | Runtime de agentes, para estimar e reportar consumo (contrato Q04 pendente) | Configuração efetiva do ciclo para Operador |
| 3 | Subgrafo afetado identificado | Analisar alcance da mudança | Plano de trabalho | CBM via MCP, como fonte de conhecimento consultável | Alcance por comunidades, dependências e âncoras para Operador e Builder |
| 4 | Plano de tarefas compilado | Compilar plano a partir do subgrafo e das dependências | Plano de trabalho | CBM via MCP | Plano e dependências para Operador e Builders |
| 5 | Claim exclusivo de trabalho concedido | Reservar âncoras para uma tarefa | Claim de trabalho | CBM via MCP pode fornecer contexto; a exclusividade é contrato novo da camada TS (C06) | Responsável e território da tarefa para Operador e Builder |
| 6 | Evento de grafo recebido | Receber mudança de grafo ou sinal de anomalia | Despacho de eventos | CBM via MCP; origem e semântica do evento precisam de contrato | Sinal recebido para o consumidor externo de eventos, se configurado |
| 7 | Assinatura de evento correspondida | Avaliar regra de despacho | Despacho de eventos | — | Gatilho e perfil selecionado para Operador |
| 8 | Disparo duplicado ou recursivo suprimido | Deduplicar, limitar ou encerrar propagação | Despacho de eventos | — | Contagem de supressões para Operador |
| 9 | Verificação de evento despachada | Acionar perfil configurado para o sinal | Despacho de eventos | Runtime de agentes (Q04 permanece Deferred) | Estado do disparo para Operador e perfil acionado |
| 10 | Tarefa liberada após dependências satisfeitas | Liberar tarefa pronta | Plano de trabalho | Runtime de agentes | Estado de execução do plano para Operador e Builder |
| 11 | Execução de agente iniciada | Iniciar sessão de agente com perfil e limites | Ciclo de execução | Runtime/host de agentes; identidade de host e modelo não foram escolhidos | Estado e identidade operacional da execução para Operador |
| 12 | Iteração do Outer Loop iniciada | Solicitar proposta ou correção | Ciclo de execução | Runtime de agentes; Harness/CBM via MCP para verificação quando aplicável | Número e estado da iteração para Operador e Builder |
| 13 | Efeito externo confirmado | Registrar resultado confirmado de chamada ou ação | Registro de execução | Runtime de agentes e ferramenta externa invocada | Efeito confirmado para Operador |
| 14 | Efeito externo incerto registrado | Registrar resultado que não pode ser confirmado | Registro de execução | Runtime de agentes e ferramenta externa invocada | Efeito incerto para Operador |
| 15 | Incerteza material escalada ao Operador | Solicitar decisão sem repetir efeito às cegas | Escalonamento | Runtime/ferramenta externa que recebeu a chamada | Ação incerta, justificativa e decisão pendente para Operador |
| 16 | Consumo do ciclo contabilizado | Registrar tokens, custo, tempo e tentativas | Orçamento do ciclo | Runtime de agentes, como fonte de medição segundo contrato a definir | Consumo e saldo do orçamento para Operador |
| 17 | Checkpoint associado ao território persistido | Gravar estado recuperável da execução | Checkpoint de execução | — | Árvore de tentativas e checkpoint por território para Operador e Builder |
| 18 | Resultado de tarefa integrada | Integrar resultados independentes e registrar falha parcial | Entrega integrada | Runtime de agentes; Harness/CBM via MCP para verificação | Estado integrado e tarefas reprovadas para Operador e Builders |
| 19 | Versão candidata identificada e selada | Congelar o candidato antes de coletar evidência | Versão candidata | Host/repositório de código e CBM via MCP | Identidade da versão para Operador, Builder e Gatekeeper |
| 20 | Avaliação anterior invalidada por mudança de versão | Vincular correção à nova versão e invalidar evidência/aprovação antiga, quando houver | Versão candidata | Host/repositório de código | Estado atual da avaliação para Operador, Builder e Gatekeeper |
| 21 | Pacote de evidências da versão reunido | Consolidar evidências da versão selada | Dossiê de evidências | Harness/CBM via MCP e runtime Adversarial | Pacote de decisão para Operador e Gatekeeper |
| 22 | Veredicto independente emitido | Avaliar a versão candidata | Evidência de verificação | Harness/CBM via MCP e runtime Adversarial, conforme capacidades mapeadas | Veredicto, evidências e lacunas para Operador e Gatekeeper |
| 23 | Iteração de correção agendada | Encaminhar veredicto ao Builder para nova iteração | Ciclo de execução | Runtime de agentes | Motivo e tarefa da próxima iteração para Builder e Operador; retorna ao evento 12 |
| 24 | Risco da versão avaliado | Combinar topologia, evidências e restrições adicionais | Avaliação de risco | CBM via MCP; registro de restrições recebido por contrato | Classe, fatores e incertezas do risco para Operador |
| 25 | Circuit breaker acionado e ciclo pausado | Suspender após falhas repetidas da mesma classe | Ciclo de execução | — | Motivo da pausa e pacote de diagnóstico para Operador |
| 26 | Novas ações bloqueadas por orçamento esgotado | Aplicar teto considerando consumo pendente | Orçamento do ciclo | Runtime de agentes | Estado do orçamento para Operador |
| 27 | Escalonamento de alto risco ou risco desconhecido aberto | Pausar horizonte e preparar decisão humana | Escalonamento | graph-ui existente, se houver link válido para o alvo | Pacote de decisão com mudança, evidências, motivo do risco e impacto para Operador |
| 28 | Decisão humana sobre horizonte registrada | Aprovar, rejeitar, solicitar correção ou retomada | Decisão de operador | graph-ui para navegação, quando suportado; Operador como ator | Decisão e consequência aplicada para Operador e Builder |
| 29 | Fast path admitido para avaliação de promoção | Confirmar baixo risco comprovado e evidência íntegra | Decisão de rota | — | Justificativa do fast path para Operador |
| 30 | Promoção autorizada para versão atual | Validar invariante e solicitar promoção | Decisão de promoção | CBM via MCP recebe solicitação; pré-condições adicionais pertencem à camada TS | Resultado e versão promovida para Operador |
| 31 | Promoção recusada pela Autoridade e Decisão | Recusar quando verificador falhou, está indisponível, inconclusivo ou a evidência não é atual | Decisão de promoção | — | Motivo de recusa e lacunas para Operador e Gatekeeper |
| 32 | Solicitação de promoção recusada pelo CBM | Registrar recusa do gate C ou de pré-condição do território | Decisão de promoção | CBM via MCP | Motivo e estado consultável para Operador e Gatekeeper |
| 33 | Território consolidado no grafo após autorização | Solicitar admissão da versão autorizada | Território de conhecimento | CBM via MCP | Estado de admissão para Operador e consumidores externos do grafo |
| 34 | Território descartado com auditoria preservada | Descartar estado retomável e manter evidência necessária | Território de execução | CBM via MCP para o território; efeitos externos não são desfeitos pelo descarte | Estado de descarte e trilha retida para Operador e Meta-Agent |
| 35 | Ciclo encerrado com motivo e evidências registrados | Aplicar condição objetiva de parada e motivo terminal de C02 | Ciclo de execução | Runtime de agentes; Harness/CBM via MCP, se houve verificação | Relatório final do ciclo para Operador |
| 36 | Lote de auditoria elegível para retrospectiva reunido | Selecionar registros não consumidos do período | Retrospectiva | — | Cobertura e registros incluídos para Meta-Agent e Operador |
| 37 | Padrão recorrente de falha identificado | Analisar auditorias elegíveis | Retrospectiva | — | Padrões e evidências de origem para Meta-Agent e Operador |
| 38 | Proposta de evolução versionada criada | Propor Tradição, ajuste de regra local do Harness ou contrato de skill | Proposta de evolução | Catálogo de temas do CBM; Harness/attestor conforme alvo da proposta | Proposta, origem, alvo e versão para Operador |
| 39 | Não-regressão da proposta avaliada | Rodar avaliação exigida antes da ativação | Proposta de evolução | Harness/CBM via MCP | Resultado e evidências de não-regressão para Operador |
| 40 | Proposta de evolução aprovada explicitamente | Aprovar a proposta avaliada | Decisão de evolução | Operador | Decisão e versão aprovada para Operador |
| 41 | Proposta de evolução rejeitada explicitamente | Rejeitar a proposta avaliada | Decisão de evolução | Operador | Motivo e versão rejeitada para Operador e Meta-Agent |
| 42 | Evolução aprovada ativada em versão identificada | Ativar versão aprovada no catálogo ou destino correspondente | Evolução ativa | Catálogo versionado de temas/Tradições do CBM ou alvo de skill/contrato, conforme capacidade validada | Estado e versão ativa para Operador e Meta-Agent |
| 43 | Evolução revertida para versão anterior | Reverter evolução após decisão ou regressão | Evolução ativa | Catálogo de temas do CBM ou alvo de skill/contrato, conforme capacidade validada | Versão revertida e motivo para Operador |
| 44 | Telemetria do orquestrador consultável registrada | Consultar métricas de execução e risco | Observabilidade | — | Veredictos, gatilhos, custo, orçamento e intervenções para Operador |
| 45 | Comparação de valor calculada sob critério prévio | Comparar entregas equivalentes com referência definida | Medição de valor | Runtime simples de referência e fonte de entregas aceitas | Custo por entrega aceita, intervenções e regressões para Operador |

**Ramos do fluxo:** após o veredicto (#22), reprovação com orçamento e dependências ainda válidos pode agendar correção (#23) e voltar a uma nova iteração (#12); isso não encerra o ciclo. Após risco (#24), baixo risco comprovado pode seguir ao fast path (#29); risco alto/desconhecido ou evidência incompleta escala (#27) e recebe decisão humana (#28). Correção volta ao loop com nova versão; rejeição leva ao descarte (#34). Fast path e decisão humana ainda passam pela autorização (#30); recusa da Autoridade (#31) ou do CBM (#32) não promove. Promoção seguida de consolidação (#33), descarte, breaker (#25) ou orçamento esgotado (#26) podem levar ao encerramento. O ciclo só produz o evento terminal #35 com o motivo de C02; `COMPLETED` não implica promoção. Q04–Q08 seguem Deferred. Até F05, uma queda exige diagnóstico e novo ciclo desde o objetivo; não há retomada automática nem repetição cega de efeitos incertos. O limite do orçamento deve considerar chamadas em voo. Gatekeeper é um componente determinístico fora do LLM; escrever o modelo não prova que o mecanismo exista nem que o isolamento tenha sido demonstrado.

## 2. Classificação de subdomínios

| Subdomínio | Tipo | Justificativa |
|---|---|---|
| Execução e Recuperação | Core | O ciclo verificável, limitado por orçamento e recuperável transforma agentes avulsos em execução governada. |
| Planejamento e Eventos | Core | Compilar trabalho a partir do subgrafo e reagir a sinais com controle de dependências é o uso distintivo do conhecimento do CBM. |
| Autoridade e Decisão | Core | A separação entre geração, verificação independente, gate determinístico e decisão humana define quando a autonomia pode agir. |
| Evolução Supervisionada | Supporting | Converte falhas em mudanças controladas do harness; depende de evidências e registros produzidos pelos subdomínios Core. |
| Observabilidade e Valor | Supporting | Registra operação e permite refutar ou demonstrar o valor da autonomia, sem autorizar execução ou promoção. |

A integração com hosts/modelos de agentes é uma dependência configurável, não um subdomínio de negócio. Q04 mantém o contrato de iniciar, cancelar, isolar, medir e recuperar sem resolução; nenhum fornecedor foi selecionado.

## 3. Glossário de linguagem ubíqua

| Termo | Definição | Notas |
|---|---|---|
| Objetivo de mudança | Resultado que o Operador pede ao ciclo para alcançar. | Deve ser registrado antes da execução autônoma. |
| Ciclo de execução | Unidade que reúne objetivo, política, iterações, consumo, evidências e motivo de encerramento. | Estados e motivos terminais seguem C02; orçamento segue C03. Novo ciclo após crash não equivale à retomada de checkpoint. |
| Plano de tarefas | Decomposição ordenada do objetivo em trabalho atribuído conforme dependências e concorrência permitida. | Segue PlanSnapshot de C06; derivado do subgrafo, não de uma contagem fixa de comunidades. |
| Subgrafo afetado | Parte do conhecimento do projeto relevante para uma mudança, incluindo comunidades, dependências e âncoras. | Resultado consultado; a fonte e a cobertura precisam acompanhar a evidência. |
| Claim de trabalho | Compromisso exclusivo proposto para um agente editar um conjunto delimitado de âncoras. | Segue WorkClaim de C06; claims Union observados não são locks sobre AST. |
| Território | Área de trabalho especulativa associada a uma execução e à sua auditoria. | Horizonte e sessão especulativa têm semânticas distintas; C07 governa descarte/auditoria; descarte não desfaz efeito externo. |
| Versão candidata | Identidade do candidato imutável conforme C01, usada para vincular avaliação e decisão. | Correção cria nova versão e invalida avaliação anterior conforme C01/C04. |
| Evidência independente | Resultado de verificação produzido por papel separado do gerador e associado à versão candidata atual. | Formato e resultados seguem C04; indisponibilidade, inconclusão ou evidência declarada sem prova não equivale a verde. |
| Gatekeeper | Componente determinístico que valida pré-condições de promoção fora do LLM. | Permissões seguem C05; não é um LLM nem indica RBAC nativo completo já disponível. |
| Invariante de promoção | Regra de que nenhuma rota promove sem evidência independente verde e atual. | Aprovação humana e fast path não são exceções; correção invalida aprovação anterior. |
| Fast path | Rota autônoma condicionada a baixo risco comprovado, fingerprint íntegro e ausência de marcadores de risco. | Risco desconhecido ou evidência incompleta vai para escalonamento. |
| Pacote de decisão | Conjunto enviado ao Operador com mudança, evidências, motivo do risco e impacto esperado. | Link de navegação é auxiliar e sua precisão no graph-ui ainda deve ser comprovada. |
| Registro de auditoria | Evidência preservada de execução, falhas e efeitos, inclusive após descarte do estado retomável. | Segue C07; retenção permanece Q07 Deferred, sem prazo inventado. |
| Retrospectiva | Análise de registros elegíveis para encontrar falhas recorrentes e propor evolução. | Fluxo segue C08; cadência Q06 Deferred; registros não consumidos devem permanecer elegíveis. |
| Tradição | Item do catálogo/tema versionado do CBM que registra padrões de ofício aprovados para orientar o trabalho. | É distinto de regra local do Harness e de contrato de skill. C08 governa proposta, aprovação e reversão; a integração normativa do catálogo é parcial (006), então a alteração requer validar tool e autoridade. Não tratar toda Tradição como capacidade futura. |
