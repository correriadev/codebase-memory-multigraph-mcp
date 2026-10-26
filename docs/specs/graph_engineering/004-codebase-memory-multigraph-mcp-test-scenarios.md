# Test Scenarios — codebase-memory-multigraph-mcp

**Domain:** graph_engineering  
**Project:** codebase-memory-multigraph-mcp  
**Framework:** runner TypeScript ainda não selecionado; `docs/adr/TESTS.md` descreve os testes C/Python/shell/Windows existentes, sem selecionar framework para o pacote novo.  
**Date:** 2026-10-23

Este catálogo especifica comportamento futuro; nenhum cenário de produto, spike ou benchmark foi executado neste refinamento. Fontes: [desenho tático 003](003-codebase-memory-multigraph-mcp-tactical-design.md), seus suplementos e contratos C01–C08 de [005](005-shared-contracts.md). IDs preservados permitem ligar cenário a regra/PBI e tarefa. O resumo de cobertura e as dependências estão em [008](008-traceability.md).

Cada cenário classificado Unit exige lógica isolada; Integration exercita a dependência real alvo (SQLite, processo, transporte, CBM ou graph-ui); Functional exercita o fluxo completo na interface de entrada que o pacote vier a publicar. Dublês são úteis para falhas determinísticas, mas não comprovam isolamento, cancelamento, promoção, recuperação ou navegação reais. O spike e esses cenários devem registrar ambiente, build, comando, pass/fail/skip e evidências; presença documental não implica aprovação.

**Catálogo:** 119 cenários definidos nas frentes + sete transversais definidos abaixo. As frentes são a única fonte do texto Given/When/Then; este índice mantém IDs, níveis e links estáveis, sem cópias de corpos.


## Unit

| ID / cenário canônico | Fonte única |
|---|---|
| <a id="er-s07"></a> ER-S07 — Deve manter breaker fechado quando falhas não atingem o critério da mesma classe | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s07) |
| <a id="er-s17"></a> ER-S17 — Deve expor lacuna quando vínculo de tentativa está ausente | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s17) |
| <a id="pe-s01"></a> PE-S01 — Should manter gaps de cobertura explícitos ao identificar o subgrafo | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s01) |
| <a id="pe-s02"></a> PE-S02 — Should bloquear escopo afirmativo quando a cobertura não pode ser consultada | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s02) |
| <a id="pe-s03"></a> PE-S03 — Should compilar tarefas independentes somente em DAG acíclico | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s03) |
| <a id="pe-s04"></a> PE-S04 — Should agrupar SCC cíclica e preservar tarefas independentes | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s04) |
| <a id="pe-s05"></a> PE-S05 — Should emitir fencing token monotônico em claim transferido | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s05) |
| <a id="pe-s06"></a> PE-S06 — Should recusar token antigo após expiração enquanto processo antigo permanece ativo | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s06) |
| <a id="pe-s07"></a> PE-S07 — Should reter resultados independentes aprovados em falha parcial | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s07) |
| <a id="pe-s08"></a> PE-S08 — Should não ativar dispatch com sinal sem fonte comprovada | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s08) |
| <a id="pe-s09"></a> PE-S09 — Should manter revisão imutável da assinatura usada pelo evento | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s09) |
| <a id="pe-s10"></a> PE-S10 — Should detectar duplicata lógica mesmo com eventId diferente | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s10) |
| <a id="ad-s01"></a> AD-S01 — Should autorizar operações permitidas somente para o perfil autenticado | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s01) |
| <a id="ad-s02"></a> AD-S02 — Should negar promoção ao Builder mesmo quando ele solicita a tool pelo nome | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s02) |
| <a id="ad-s03"></a> AD-S03 — Should rejeitar veredicto não verde ou evidência incompatível com a versão | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s03) |
| <a id="ad-s04"></a> AD-S04 — Should rejeitar evidência malformada ou sem origem verificável | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s04) |
| <a id="ad-s05"></a> AD-S05 — Should classificar risco desconhecido como não elegível para fast path | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s05) |
| <a id="ad-s06"></a> AD-S06 — Should invalidar aprovação quando uma correção cria nova versão | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s06) |
| <a id="ev-s03"></a> EV-S03 — Deve registrar hipótese quando poucos exemplos não sustentam recorrência ou causalidade | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s03) |
| <a id="ev-s05"></a> EV-S05 — Deve encaminhar attestation somente para contrato de skill | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s05) |
| <a id="ev-s06"></a> EV-S06 — Deve bloquear validação de skill quando attestation é ausente, inconclusiva ou de outra versão | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s06) |
| <a id="ov-s04"></a> OV-S04 — Deve manter moeda, tokens e estados de consumo distintos | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s04) |
| <a id="ov-s11"></a> OV-S11 — Deve marcar indefinido custo quando zero entregas forem aceitas | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s11) |

## Integration

| ID / cenário canônico | Fonte única |
|---|---|
| <a id="er-s03"></a> ER-S03 — Deve reservar e reconciliar consumo antes e depois do despacho | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s03) |
| <a id="er-s04"></a> ER-S04 — Deve bloquear novas ações ao teto enquanto contabiliza custo pendente | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s04) |
| <a id="er-s05"></a> ER-S05 — Deve suspender novos despachos quando uso fica desconhecido | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s05) |
| <a id="er-s06"></a> ER-S06 — Deve pausar e escalar após falhas repetidas da mesma classe | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s06) |
| <a id="er-s09"></a> ER-S09 — Deve pausar sem tratar verificador indisponível como aprovação | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s09) |
| <a id="er-s10"></a> ER-S10 — Deve persistir relatório completo ao encerrar ciclo | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s10) |
| <a id="er-s11"></a> ER-S11 — Deve impedir conclusão durável quando falha a gravação do relatório | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s11) |
| <a id="er-s12"></a> ER-S12 — Deve gravar checkpoint ligado ao horizonte e à árvore de tentativas | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s12) |
| <a id="er-s13"></a> ER-S13 — Deve conservar o último checkpoint íntegro após crash antes do commit | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s13) |
| <a id="er-s14"></a> ER-S14 — Deve retomar trabalho seguro sem duplicar efeito confirmado | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s14) |
| <a id="er-s15"></a> ER-S15 — Deve tratar efeito externo não consultável como ocorrido e escalar incerteza | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s15) |
| <a id="er-s16"></a> ER-S16 — Deve reconstruir árvore de tentativas persistida por território | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s16) |
| <a id="er-s18"></a> ER-S18 — Deve invalidar retomada e preservar auditoria ao descartar território | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s18) |
| <a id="er-s19"></a> ER-S19 — Deve manter diagnóstico repetível quando descarte parcial falha | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s19) |
| <a id="er-s20"></a> ER-S20 — Deve parar novos despachos após cancelamento e reconciliar chamadas em voo | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s20) |
| <a id="er-s22"></a> ER-S22 — Deve recusar retomada quando versão/território diverge do checkpoint | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s22) |
| <a id="er-s23"></a> ER-S23 — Deve negar promoção por chamada direta e por caminho alternativo do Builder | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s23) |
| <a id="er-s25"></a> ER-S25 — Deve cancelar processos próprios pelo adapter real sem esconder consumo pendente | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s25) |
| <a id="pe-s11"></a> PE-S11 — Should invalidar plano quando renome altera caminho ou anchor | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s11) |
| <a id="pe-s12"></a> PE-S12 — Should conceder somente um claim em aquisições concorrentes | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s12) |
| <a id="pe-s13"></a> PE-S13 — Should preservar plano anterior quando persistência do plano falha | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s13) |
| <a id="pe-s14"></a> PE-S14 — Should liberar paralelismo somente com seam compatível atual | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s14) |
| <a id="pe-s15"></a> PE-S15 — Should preservar sucesso independente e exigir verificação do conjunto integrado | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s15) |
| <a id="pe-s16"></a> PE-S16 — Should recusar evento quando watcher somente reindexa ou trace só foi ingerido | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s16) |
| <a id="pe-s17"></a> PE-S17 — Should recuperar entrega reordenada ou interrompida sem repetir efeito lógico | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s17) |
| <a id="pe-s18"></a> PE-S18 — Should registrar assinatura nova sem reescrever entrega pendente | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s18) |
| <a id="pe-s19"></a> PE-S19 — Should manter custo pendente no orçamento global antes de novo dispatch | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s19) |
| <a id="pe-s20"></a> PE-S20 — Should bloquear tempestade recursiva e preservar dead-letter para replay manual | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s20) |
| <a id="pe-s30"></a> PE-S30 — Should detectar aliases do filesystem e conflito de ranges sobrepostos | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s30) |
| <a id="pe-s31"></a> PE-S31 — Should reservar um único orçamento global sob dispatches concorrentes | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s31) |
| <a id="pe-s28"></a> PE-S28 — Should recusar publicação de assinatura sem autoridade ou contrato válido | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s28) |
| <a id="ad-s07"></a> AD-S07 — Should recusar chamada direta de promoção por Builder antes de alcançar o CBM | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s07) |
| <a id="ad-s08"></a> AD-S08 — Should recusar acesso alternativo do Builder fora do canal de tools anunciado | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s08) |
| <a id="ad-s09"></a> AD-S09 — Should manter o verificador em processo e contexto separados do Builder | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s09) |
| <a id="ad-s10"></a> AD-S10 — Should bloquear a corrida entre revalidação e chamada de promoção | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s10) |
| <a id="ad-s11"></a> AD-S11 — Should manter estado não promovido quando o verificador está indisponível ou inconclusivo | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s11) |
| <a id="ad-s12"></a> AD-S12 — Should registrar risco com topologia, evidências e restrições da mesma revisão | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s12) |
| <a id="ad-s13"></a> AD-S13 — Should admitir fast path apenas sob todas as condições explícitas e com gate verde | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s13) |
| <a id="ad-s14"></a> AD-S14 — Should pausar horizonte de alto risco e compor o pacote completo | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s14) |
| <a id="ad-s15"></a> AD-S15 — Should validar deep link exato contra a graph-ui real | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s15) |
| <a id="ad-s16"></a> AD-S16 — Should aplicar comando humano por versão e estado atuais | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s16) |
| <a id="ad-s17"></a> AD-S17 — Should impedir que aprovação humana contorne evidência ou Gatekeeper | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s17) |
| <a id="ad-s18"></a> AD-S18 — Should retomar o ciclo com nova versão após pedido de correção | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s18) |
| <a id="ad-s19"></a> AD-S19 — Should registrar rejeição sem promover e aplicar consequência suportada | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s19) |
| <a id="ad-s20"></a> AD-S20 — Should impedir chamada de promoção após falha ao persistir decisão | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s20) |
| <a id="ad-s21"></a> AD-S21 — Should reconciliar timeout da promoção antes de qualquer nova tentativa | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s21) |
| <a id="ev-s01"></a> EV-S01 — Deve reunir auditoria não consumida com cobertura rastreável | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s01) |
| <a id="ev-s02"></a> EV-S02 — Deve manter elegíveis registros quando a análise falha antes de persistir | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s02) |
| <a id="ev-s07"></a> EV-S07 — Deve manter Harness rule no store local de governança | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s07) |
| <a id="ev-s08"></a> EV-S08 — Deve validar não-regressão com execução ligada à versão proposta | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s08) |
| <a id="ev-s09"></a> EV-S09 — Deve impedir VALIDATED quando Harness falha ou não executa | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s09) |
| <a id="ev-s11"></a> EV-S11 — Deve invalidar aprovação após mudança de versão ou rejeição explícita | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s11) |
| <a id="ev-s13"></a> EV-S13 — Deve manter skills deferred bloqueadas sem admissão real | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s13) |
| <a id="ev-s14"></a> EV-S14 — Deve reconciliar timeout de ativação antes de qualquer retry | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s14) |
| <a id="ev-s16"></a> EV-S16 — Deve preservar diagnóstico quando reversão falha parcialmente | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s16) |
| <a id="ev-s17"></a> EV-S17 — Deve recusar a segunda ativação aprovada para um baseline já substituído | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s17) |
| <a id="ev-s18"></a> EV-S18 — Deve preservar evolução posterior ao receber rollback obsoleto | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s18) |
| <a id="ov-s01"></a> OV-S01 — Deve consultar o subconjunto mínimo do slice sem serviço externo | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s01) |
| <a id="ov-s02"></a> OV-S02 — Deve deduplicar replay e reconstruir a mesma projeção | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s02) |
| <a id="ov-s05"></a> OV-S05 — Deve preservar desconhecido e não inferir teste de manifest | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s05) |
| <a id="ov-s06"></a> OV-S06 — Deve negar consulta fora do escopo e minimizar payload | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s06) |
| <a id="ov-s07"></a> OV-S07 — Deve preservar auditoria não consumida sem fixar retenção | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s07) |
| <a id="ov-s08"></a> OV-S08 — Deve deixar a execução autoritativa inalterada quando a projeção falha | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s08) |
| <a id="ov-s10"></a> OV-S10 — Deve incluir tentativas falhas, retries e revisão humana na população/custo | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s10) |
| <a id="ov-s12"></a> OV-S12 — Deve impedir conclusão de sucesso com dados ou cobertura desconhecidos | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s12) |
| <a id="ov-s14"></a> OV-S14 — Deve rejeitar protocolo alterado após início da medição | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s14) |
| <a id="ov-s15"></a> OV-S15 — Deve cobrir todas as fontes habilitadas no alvo S4 sem escrita cruzada | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s15) |
| <a id="ov-s16"></a> OV-S16 — Deve marcar incompleta a fonte requerida ausente sem atrasar medição S1 | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s16) |

## Functional

| ID / cenário canônico | Fonte única |
|---|---|
| <a id="er-s01"></a> ER-S01 — Deve selar após a produção do Builder e verificar a mesma versão | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s01) |
| <a id="er-s02"></a> ER-S02 — Deve registrar reprovação quando a condição configurada encerra após veredicto vermelho | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s02) |
| <a id="er-s08"></a> ER-S08 — Deve encaminhar veredicto vermelho à correção e reverificar nova versão | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s08) |
| <a id="er-s21"></a> ER-S21 — Deve diagnosticar e iniciar do objetivo após crash pré-F05 sem checkpoint | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s21) |
| <a id="er-s24"></a> ER-S24 — Deve executar Builder e verificador pelo adapter concreto validado | [Definição Given/When/Then](fronts/01-execution-recovery.md#er-s24) |
| <a id="pe-s21"></a> PE-S21 — Should compilar plano conservador para alteração com coverage parcial | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s21) |
| <a id="pe-s22"></a> PE-S22 — Should recusar escrita de agente antigo após transferência do claim | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s22) |
| <a id="pe-s23"></a> PE-S23 — Should manter entrega conjunta sem promoção quando uma tarefa reprova | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s23) |
| <a id="pe-s24"></a> PE-S24 — Should despachar somente violação de seam comprovada à assinatura configurada | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s24) |
| <a id="pe-s25"></a> PE-S25 — Should não despachar anomalia de trace sem detector e evidência validados | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s25) |
| <a id="pe-s26"></a> PE-S26 — Should deduplicar tempestade e exigir replay com orçamento | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s26) |
| <a id="pe-s27"></a> PE-S27 — Should despachar anomalia de trace depois de evidência validada | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s27) |
| <a id="pe-s29"></a> PE-S29 — Should solicitar verificação para conjunto integrado quando todas tarefas obrigatórias passam | [Definição Given/When/Then](fronts/02-planning-events.md#pe-s29) |
| <a id="ad-s22"></a> AD-S22 — Should completar fast path baixo risco sem abrir exceção ao gate | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s22) |
| <a id="ad-s23"></a> AD-S23 — Should escalar risco desconhecido sem iniciar promoção | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s23) |
| <a id="ad-s24"></a> AD-S24 — Should manter aprovação limitada à versão avaliada em fluxo completo | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s24) |
| <a id="ad-s25"></a> AD-S25 — Should rejeitar independência quando Builder e verificador compartilham processo ou contexto | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s25) |
| <a id="ad-s26"></a> AD-S26 — Should manter PBI-25 bloqueado quando o deep link não abre o alvo exato | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s26) |
| <a id="ad-s27"></a> AD-S27 — Should iniciar do objetivo e reconciliar efeito incerto após crash pré-F05 | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s27) |
| <a id="ad-s28"></a> AD-S28 — Should compartilhar uma única reserva C03 entre promoção e retries | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s28) |
| <a id="ad-s29"></a> AD-S29 — Should selar somente depois da saída persistida do Builder | [Definição Given/When/Then](fronts/03-authority-decision.md#ad-s29) |
| <a id="ev-s04"></a> EV-S04 — Deve propor Tradição sem misturá-la a regra local ou contrato de skill | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s04) |
| <a id="ev-s10"></a> EV-S10 — Deve exigir aprovação autenticada do Operador para digest atual | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s10) |
| <a id="ev-s12"></a> EV-S12 — Deve ativar e confirmar a versão aprovada pelo adapter de target | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s12) |
| <a id="ev-s15"></a> EV-S15 — Deve reverter para versão anterior e preservar trilha | [Definição Given/When/Then](fronts/04-supervised-evolution.md#ev-s15) |
| <a id="ov-s03"></a> OV-S03 — Deve registrar gatilhos, intervenções e falhas sem duplicar o sinal | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s03) |
| <a id="ov-s09"></a> OV-S09 — Deve comparar coortes equivalentes sob protocolo pré-registrado | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s09) |
| <a id="ov-s13"></a> OV-S13 — Deve expor que a autonomia pode piorar o resultado | [Definição Given/When/Then](fronts/05-observability-value.md#ov-s13) |

## Cenários transversais de integração entre frentes

### XT-S01 — Deve impedir orçamento multiplicado pelo fan-out de eventos

- **Nível:** Integration. **Fonte:** 003 operações Despachar evento/Reconciliar consumo; C03/C06; INV-07; PBI-02/08/14/31.
- **Given:** duas tarefas e um evento derivado concorrem pelo saldo de um mesmo objetivo com reservas em voo registradas.
- **When:** os três despachos tentam reservar simultaneamente mais que o saldo disponível.
- **Then:** só reservas compatíveis com o saldo são confirmadas atomicamente, nenhuma chamada sem reserva ocorre e supressões/consumo permanecem consultáveis uma vez por comando.

### XT-S02 — Deve invalidar revisão integrada depois de mudança em contrato compartilhado

- **Nível:** Functional. **Fonte:** 003 Compilar plano/Avaliar promoção; C01/C04/C06; INV-01; PBI-09/10/18.
- **Given:** tarefas independentes produziram resultados e um contrato compartilhado foi alterado após a verificação integrada da versão anterior.
- **When:** o conjunto é preparado para promoção com evidência antiga.
- **Then:** a versão/dossiê anteriores não autorizam a promoção, seam e dependências afetadas são reavaliadas, trabalho independente pode ser preservado e o conjunto exige nova verificação.

### XT-S03 — Deve preservar prova de falha descartada até sua retrospectiva

- **Nível:** Integration. **Fonte:** 003 Recuperar/descartar e Propor evolução; C07/C08; INV-06/08; PBI-22/27/28/31.
- **Given:** horizonte descartável tem checkpoint e auditoria necessária a uma retrospectiva ainda não consumida.
- **When:** descarte, projeção de telemetria e coleta de retrospectiva intercalam commits.
- **Then:** não há retomada por checkpoint invalidado, auditoria/referências necessárias continuam consultáveis e a retrospectiva identifica sua cobertura sem atribuir efeito externo como desfeito.

### XT-S04 — Deve reconciliar promoção confirmada remotamente após queda local

- **Nível:** Integration. **Fonte:** 003 Avaliar promoção/Recuperar e C04/C07; INV-01/03/04; PBI-05/18/20/31.
- **Given:** intenção de promoção da versão exata foi persistida e CBM confirmou a mudança, mas o processo caiu antes de gravar a resposta no journal.
- **When:** recuperação consulta o estado real do território e correlaciona a intenção.
- **Then:** confirma o resultado somente com prova da versão/operação, não repete a promoção às cegas, contabiliza o consumo conhecido e preserva incerteza quando a correlação não puder ser provada.

### XT-S05 — Deve impedir ativação de evolução pela mesma identidade que a propôs

- **Nível:** Functional. **Fonte:** 003 Propor/ativar/reverter evolução; C05/C08; INV-08; PBI-15/28/29/30.
- **Given:** Meta-Agent propôs versão de contrato com resultado de qualidade e sem aprovação humana aplicável.
- **When:** sua identidade tenta chamar ativação diretamente, inclusive pelo nome da operação.
- **Then:** autorização recusa antes do efeito, qualidade não vira aprovação e nenhuma versão ativa é alterada por autodeclaração.

### XT-S06 — Deve manter resultado comparativo sem alegar valor quando o protocolo não foi aprovado

- **Nível:** Functional. **Fonte:** 003 Consultar execução/comparar valor; C03/C08; Q08; PBI-31/32.
- **Given:** execuções têm telemetria com custos medidos/estimados/desconhecidos e ainda faltam limiares humanos de sucesso.
- **When:** Operador consulta a comparação com referência simples.
- **Then:** dados descritivos e suas limitações são mostrados, falhas/retries/intervenções permanecem na população, desconhecido não vira zero e nenhuma superioridade é declarada.


### XT-S07 — Deve negar produção quando o spike foi concluído sem aprovação

- **Nível:** Unit. **Fonte:** C02; gates de entrada 003; S0/S1 em 007; PBI-01/16.
- **Given:** todas as tasks S0, inclusive SP-05, estão concluídas e um registro de critérios contém FAIL, INCONCLUSIVE, UNAVAILABLE ou campo ausente; também exercitar ADR PROPOSED e configuração divergente
- **When:** o consumidor avalia uma task de S1–S6 cujos requires já concluíram
- **Then:** S0_APPROVED é DENY em cada caso, sem despacho; só todos os cinco PASS, invariantes e correspondência de configuração verdadeiras e ADR ACCEPTED satisfazem o gate. Gate desconhecido/não suportado também é DENY.

## Segurança, limites e aplicabilidade

Autorização, integridade de evidência, isolamento, replay, minimização de dados e risco fazem parte do desenho e dos cenários das frentes. Não há requisito de SaaS multi-tenant, API HTTP pública ou formulário HTML novo; XSS de nova UI, códigos HTTP inventados e separação entre tenants não são acrescentados ao escopo. Quando o contrato de entrada for escolhido, validações de payload e erros deverão seguir a fronteira efetiva, sem shell interpolation de objetivo ou segredo em logs. Testes da UI existente só validam o alvo navegável exigido, sem assumir nova interface.
