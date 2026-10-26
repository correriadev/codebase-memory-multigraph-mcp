# Ordem de entrega e condições de compromisso

Data: 2026-10-23. Este documento sequencia a visão integral, sem implementar nem comprometer estimativas. As tasks detalhadas são propostas condicionais; tamanho de 2–4h é hipótese de decomposição, não prazo contratado. Não converter estas etapas em execução automática de desenvolvimento.

## S0 — Fechar viabilidade antes de comprometer o slice

**Entradas:** PBB Q04, ADR-005 PROPOSED, matriz 006 e contratos C01–C08.

| Trabalho de validação | Evidência exigida | Consequência de falha |
|---|---|---|
| Mapear tools realmente anunciadas, schemas e resultados do host escolhido | Contrato observado para iniciar, cancelar, isolar, medir, diagnosticar/reiniciar | Adapter não compatível mantém Q04 pendente; não presumir host genérico |
| Executar futuramente os cinco critérios do ADR-005 | Ciclo com decisão; kill/reinício consistente; cancelamento registrado; custo persistido; recusa fora do scope | ADR continua PROPOSED, revisar proposta |
| Provar isolamento mínimo PBI-16 | Builder tenta promoção por chamada direta e caminho alternativo; ambas impedidas na fronteira controlada | Sem barreira comprovada, não entregar autonomia de promoção |
| Provar vínculo candidato/evidência/decisão | Nova correção ou drift entre revisão e promoção invalida autorização; verificador ausente nunca permite promoção | Revisar contrato de snapshot/exclusão/versão; não reduzir invariante |
| Parametrizar orçamento e decidir medição | Política explícita, unidades, reserva por chamada, exposição em voo, limites efetivos do host | Não iniciar autonomia sem configuração válida/consumo controlável; números Q05 não são inventados |
| Validar efeito de promoção e descarte no tipo exato de horizonte | Estado CBM observado e journal reconciliado sem escrita direta no store C | Capacidade faltante vira gap explícito, não substituição silenciosa de tool |

S0 é spike futuro de viabilidade, não um PBI de produto concluído pelo refinamento. Aceitar ADR requer evidência e decisão apropriada após experimento; este conjunto não altera seu status. Se uma capacidade essencial só for realizável alterando core C, apresentar opções ao operador antes de comprometer o slice.

## S1 — Ciclo mínimo verificável

**Cobertura:** PBI-01–05 + PBI-16 + subconjunto PBI-31. Pré-requisito: S0 satisfatório.

O pré-requisito é o resultado `S0_APPROVED`, não a conclusão de SP-05. Seu contrato legível por máquina fica em 003: todos os critérios PASS, invariantes aprovadas, evidência para a configuração atual e ADR-005 ACCEPTED. Um spike documentado como reprovado conclui SP-05, mas mantém todos os slices de produção bloqueados.

Ordem: tipos/identidades/configuração → journal mínimo e telemetria local → host adapter/canal autorizado → reserva e reconciliação de orçamento → Builder → verificador independente → decisão determinística vinculada à versão → parada/breaker/cancelamento → relatório auditável. Aprovação humana e fast path não dispensam a invariante.

ER-17 entrega o primeiro adapter concreto para o host validado em S0, reaproveitando a integração do fixture SP-02 quando adequada. Conecta as portas ER-05 ao runtime real e à composição da aplicação; iniciar Builder/verificador, coletar uso/erros e cancelar os processos sob sua propriedade são critérios de integração. AD-03 e ER-06 dependem dessa entrega, não apenas de interfaces. Outros hosts permanecem fora desse primeiro adapter.

**Saída:** cenários de sucesso, reprovação, verificador indisponível/inconclusivo, orçamento esgotado, falhas repetidas e crash têm resultado registrado. O resultado de promoção é separado do término do ciclo. Crash pré-F05 exige novo ciclo desde objetivo, com estado inconsistente tratado e efeitos incertos preservados, sem retomada automática. A prova mínima de PBI-16 deve existir mesmo antes do isolamento completo de perfis.

## S2 — Autoridade completa e recuperação

**Cobertura:** completar PBI-15/17/18 e PBI-19–22. S1 não dispensa evidência independente; S2 amplia cobertura e recuperação, não introduz a invariante tardiamente.

Frentes podem evoluir em paralelo após congelar C01/C04/C05/C07: autorização/perfis completos de um lado; checkpoints, replay, diagnóstico e descarte coordenado do outro. Integração requer identidade de sessão/território explícita, evidências retidas e idempotência. Falha parcial entre journal/CBM não promove. Retenção Q07 condiciona política de produção; preservar registros ainda necessários à retrospectiva.

## S3 — Planejamento paralelo e reação a eventos

**Cobertura:** PBI-06–14. Análise de alcance e DAG podem ser prototipadas após S0; despacho autônomo de múltiplos agentes exige S1 e contratos de autoridade, reserva, claims/fencing e recuperação aplicáveis.

Ordem: subgrafo com cobertura/freshness → DAG/agrupamento de ciclos → claim exclusivo → seam compartilhada → verificação integrada de falha parcial. Depois: fontes verificáveis de violações/anomalias → assinaturas versionadas → inbox/outbox/dedupe → limites de fan-out/recursão/orçamento → despacho/replay controlados. Não permitir que eventos criem orçamento novo ilimitado nem que contagem de comunidades determine paralelismo automaticamente.

## S4 — Atenção humana por risco e telemetria dos slices habilitados

**Cobertura:** PBI-23–26 + cobertura de PBI-31 para os slices habilitados até S4. Telemetria de todas as frentes acompanha seus respectivos slices; S4 consolida consultas e cobertura do alvo corrente, sem adiar dados essenciais até o fim. A conclusão integral de PBI-31 inclui as fontes posteriores requeridas pela visão completa.

“Cobertura completa” em S4 refere-se às capacidades já habilitadas e ao alvo declarado. O produtor de F07 chega em S5 e exige revalidação/extensão da cobertura; a visão integral de PBI-31 só fecha com todos os produtores requeridos por F01–F08. Comparações restritas a S1 podem ocorrer antes disso, com conclusões limitadas ao protocolo S1, sem declarar F08 ou a visão integral concluídos antecipadamente.

Risco exige mais que topologia: efeitos, evidências, restrições/âncoras sensíveis e incerteza. Desconhecido escala. Fast path só com baixo risco comprovado e C04. Decisão humana precisa de versão, evidências e consequência operacional idempotente. A validação de deep link por nó/aresta na graph-ui existente pode começar em S0; se não comprovada, PBI-25 não é declarado concluído por entregar só um link de projeto. Nenhuma nova UI é prevista sem decisão de escopo.

## S5 — Evolução supervisionada

**Cobertura:** PBI-27–30. Depende de auditoria preservada, consulta autorizada e evidências do Harness; cadência Q06 e retenção Q07 permanecem decisões a fechar.

Ordem: selecionar período com cobertura e registros de falha → reconhecer padrões sustentados → propor alvo/versão com attestation de contrato quando aplicável → provar não-regressão → aprovação humana → ativação versionada e confirmação → reversão testável. Provar capacidade de ativação das skills deferred é trabalho separado de provar qualidade. Sem API/mecanismo autorizado de ativação, não substituir PBI-30 por relatório de proposta.

EV-10/11 exigem comparação e troca atômica da versão do destino ou serialização por alvo que cubra todos os escritores. Duas aprovações do mesmo baseline não podem sobrescrever-se; rollback de uma ativação antiga não pode apagar uma versão posterior. A capacidade ausente bloqueia ativação/reversão, mesmo com Harness verde e aprovação humana.

## S6 — Medir valor contra referência simples

**Cobertura:** PBI-32. Preparar protocolo antes da execução; coleta começa assim que a telemetria necessária existir. Não é necessário aguardar F07 para avaliar S1, mas a população e capacidades comparadas devem ser declaradas.

Definir com o operador a referência, tarefas/aceite equivalentes, versões/configurações, condições de repetição, custo total por entrega aceita, intervenções e regressões. Incluir fracassos, retries e trabalho de revisão; zero entregas aceitas não vira custo zero. Sem limiares previamente aprovados Q08, relatar resultados descritivos sem declarar superioridade. A conclusão pode refutar a hipótese de valor.

## Dependências transversais e revisão

| Contrato | Produtor responsável | Consumidores / gates de integração |
|---|---|---|
| C01 identidade/versão | Execução + Autoridade | Todas as frentes; proibir misturar evidências de versões |
| C02/C03 estado e orçamento | Execução | Planejamento/Eventos reserva antes de despachar; Observabilidade reproduz ledger |
| C04/C05 evidência e autoridade | Autoridade | Qualquer promoção, decisão humana, fast path e evolução |
| C06 plano/claim/evento | Planejamento e Eventos | Execução respeita fencing e dependências; Observabilidade deduplica |
| C07 journal/auditoria | Execução | Evolução lê auditoria elegível; Observabilidade projeta sem mudar autoridade |
| C08 decisão/evolução/valor | Autoridade, Evolução, Observabilidade | Ativação e comparação exigem aprovação/protocolo aplicáveis |

O critério de pronto deste refinamento é rastreabilidade integral dos 32 PBIs, contratos coerentes, cenários especificados, revisão cruzada e bloqueios explícitos. O critério de pronto do produto exige executar os testes e provas dos slices; ele não é satisfeito por esta documentação.
