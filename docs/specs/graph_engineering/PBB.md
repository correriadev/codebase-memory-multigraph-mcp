# PBB — CBM System Intelligence Engine

Produto derivado de [`BRIEFING.md`](./BRIEFING.md) (Graph Engineering + Loop Engineering, outubro de 2026).
Data: 2026-10-23 · Idioma: pt-BR · Restrição técnica declarada pelo operador: **TypeScript** como linguagem da camada de orquestração.

> **v2** — incorpora o parecer da 1ª rodada de sparring (Codex `gpt-6.1-sol`, effort `low`) e a validação cruzada do consultor: slice inicial delimitado, plano por dependências, Gatekeeper com requisito próprio, decisão humana completa, evolução versionada/reversível, contradição descarte×retenção resolvida, proteção contra tempestade de eventos, risco além de topologia, telemetria e medição de valor (F08).
>
> **v3** — incorpora o parecer da 2ª rodada de sparring: invariante de promoção unificada (transversal a F04/F06), fronteira explícita do slice inicial com dependências de entrega, comportamento definido para efeitos incertos na retomada (PBI-20), default conservador para risco desconhecido (PBI-24), pacote de decisão no escalonamento (PBI-25) e critério de comparação convertido em open question (Q08).
>
> **v4** — incorpora o parecer da 3ª rodada de sparring (Codex `gpt-6.1-sol` + validação cruzada): condição terminal de sucesso do slice inicial delimitada explicitamente como **"Candidato Aprovado com Pacote de Evidências Auditável"** (sem mutação física em Base no primeiro slice), formalização da identidade operacional atômica da Candidata (C01 com vinculação canônica de baseline, regras e grafo) e retificação dos limites de autorização entre o core C11 e o mediador MCP no [ADR-005](../../adr/ADR-005-AGENT-EXECUTION-CONTRACT.md).

---

## 1. Product

**Nome (provisório):** cbm-orchestrator — camada de orquestração do CBM (System Intelligence Engine).

**Intenção:** transformar o CBM de um servidor MCP passivamente consultado por um agente em um motor de inteligência de sistema que usa o Knowledge Multigraph para organizar tarefas (plano de agentes com dependências e concorrência controlada), coordenar papéis isolados com permissões distintas, persistir o estado de execução dos agentes, executar loops autônomos verificados (Outer Loop), rotear a atenção humana por risco real e evoluir o próprio harness a partir de falhas observadas — conectando as três camadas: **Conhecimento persistente → Orquestração de agentes → Execução verificável**, com valor demonstrável por métricas.

**Dentro do escopo:**
- Controlador de Outer Loop (propor → verificar → corrigir, com orçamento, circuit breaker e condição de parada).
- Plano de agentes compilado a partir da topologia do código (comunidades, dependências, âncoras AST), respeitando dependências e limites de concorrência.
- Despacho de eventos do grafo para agentes (violações de seam, anomalias de traces), com proteção contra tempestade de eventos.
- Isolamento de papéis e restrição de permissões por perfil de agente (Builder / Adversarial / Gatekeeper), com recusa efetiva de operações proibidas.
- Checkpoints de execução dos agentes atrelados aos territórios, com retomada idempotente e preservação de registro de auditoria.
- Roteamento de atenção humana por risco da mudança (topologia + evidências + restrições adicionais), com decisão humana registrada.
- Loop de evolução supervisionada (retrospectiva que propõe Tradições/ajustes de contrato com não-regressão, aprovação, versionamento e reversão).
- Telemetria do orquestrador e medição comparativa de valor contra uma referência mais simples.

**Fora do escopo:**
- Reescrita do core determinístico em C11 (o gate permanece o existente).
- Nova UI de visualização (usa o `graph-ui` existente para deep links de escalonamento).
- Treinamento ou fine-tuning de modelos; escolha de fornecedores de LLM.

---

## 2. Problems

| ID | Problema (situação indesejável) |
| --- | --- |
| P01 | O desenvolvedor humano precisa tomar cada decisão intermediária do trabalho do agente; quando o ciclo propor → testar → corrigir falha, nada o reexecuta automaticamente. |
| P02 | O mesmo agente, na mesma sessão, gera a mudança, audita a própria mudança e pode acionar o portão de promoção (role confusion). |
| P03 | Erros e recusas de agentes (`refusal-discipline`, recusas de promoção) morrem na sessão; nenhum mecanismo aprende com horizontes descartados para melhorar restrições e skills. |
| P04 | Loops autônomos produzem mais mudanças do que o humano consegue revisar; a revisão vira carimbo e exige ler dezenas de arquivos em vez das poucas arestas arquiteturais que importam. |
| P05 | O watcher atualiza o Knowledge Graph, mas violações de seam/contrato e anomalias em traces ingeridos não disparam nenhuma ação automática. |
| P06 | O grafo de tarefas (quem faz o quê, em paralelo) é desenhado à mão pelo humano; o conhecimento estrutural do código (comunidades, dependências, âncoras) não organiza o trabalho dos agentes. |
| P07 | O estado intermediário de execução dos agentes (histórico de chamadas, árvore de tentativas) não é persistido nem atrelado aos territórios; a queda de uma sessão perde o histórico de execução. |

---

## 3. Expectations

| ID | Expectação | Endereça |
| --- | --- | --- |
| E01 | O ciclo propor → verificar → corrigir reexecuta autonomamente até uma condição de parada objetiva, dentro de orçamento (tokens, custo, tentativas) e com circuit breaker. | P01 |
| E02 | Geração, ataque adversarial e autorização de promoção ocorrem em papéis isolados; a promoção a Base só ocorre com evidência independente verde referente à versão candidata atual, e verificador indisponível ou inconclusivo resulta em não-promoção. | P02, P04 |
| E03 | Falhas e recusas recorrentes alimentam uma retrospectiva que propõe evolução supervisionada (Tradições, contratos) com não-regressão provada, aprovação explícita, versionamento e reversão antes de valer. | P03 |
| E04 | O humano revisa apenas mudanças de alto risco real (topologia combinada com evidências e restrições adicionais), decide o destino com registro e consequência operacional; mudanças de baixo risco comprovado seguem fast path autônomo. | P04 |
| E05 | Violações de seam/contrato e anomalias de traces disparam verificação automática por agentes, sem espera humana e sem tempestade de eventos. | P05 |
| E06 | A topologia do código determina o plano de agentes: subgrafo afetado → tarefas ordenadas por dependência, com concorrência apenas onde há independência real, claims exclusivos e verificação integrada do resultado. | P06 |
| E07 | Execuções de agentes têm checkpoints recuperáveis atrelados aos territórios; retomada idempotente, diagnóstico por horizonte e descarte conjunto de território+execução preservando o registro de auditoria. | P07 |
| E08 | O valor da autonomia é demonstrável com métricas comparativas (custo por entrega aceita, intervenções humanas, regressões) contra uma referência mais simples, a partir de telemetria própria do orquestrador. | P01, P04 |

---

## 4. Personas

### 4.1 Operador / Arquiteto (humano)
- **Meta principal:** validar apenas decisões arquiteturais; confiar o resto ao sistema — e poder conferir, com métricas, que essa confiança se paga.
- **Interações:** define objetivo, orçamento e condições de parada dos loops; recebe escalonamentos com links diretos ao nó crítico e decide o destino (aprovar/rejeitar/corrigir/retomar); valida Tradições propostas com ativação versionada e reversível; consome relatórios e métricas.
- **Problemas/expectações relevantes:** P01/E01, P03/E03, P04/E04, P01+P04/E08.

### 4.2 Builder Agent (gerador)
- **Meta principal:** implementar mudanças no território Speculative/Horizon com evidências.
- **Interações:** recebe claim exclusivo sobre âncoras do seu módulo (com regras de conflito, abandono e transferência); escreve no horizonte; itera sobre o resultado do verificador; registra checkpoints; participa de verificação integrada com outros builders.
- **Problemas/expectações relevantes:** P02/E02, P06/E06, P07/E07.

### 4.3 Adversarial Agent (verificador independente)
- **Meta principal:** derrubar horizontes com evidências, provando que a mudança quebra contratos ou tradições.
- **Interações:** monitora novos horizontes e eventos do grafo; audita traces; emite bloqueios fundamentados ou veredicto inconclusivo; roda em processo/instância separada.
- **Problemas/expectações relevantes:** P02/E02, P04/E02, P05/E05.

### 4.4 Gatekeeper (portão determinístico)
- **Meta principal:** garantir que a promoção a Base só ocorra com evidência independente verde referente à versão candidata atual.
- **Interações:** exige o pacote de evidências da versão candidata; recusa promoção quando o verificador está indisponível, inconclusivo ou bloqueia; aplica a mutação determinística fora do LLM.
- **Problemas/expectações relevantes:** P02/E02, P04/E02.

### 4.5 Meta-Agent de Retrospectiva (evolução)
- **Meta principal:** transformar falhas passadas em propostas de evolução validadas (Tradições, ajustes de contrato).
- **Interações:** lê os registros de auditoria dos horizontes descartados do período; identifica padrões de erro recorrentes; propõe mudadas com não-regressão provada; abre proposta versionada para o operador.
- **Problemas/expectações relevantes:** P03/E03.

---

## 5. Functionalities

| ID | Funcionalidade | Persona principal | Valor reconhecível |
| --- | --- | --- | --- |
| F01 | Executar Outer Loop autônomo (agente + verificação) | Operador | Ciclos convergem sozinhos, dentro de orçamento e com parada objetiva e auditável. |
| F02 | Compilar plano de agentes a partir da topologia do código | Operador, Builder | O próprio código desenha a organização do trabalho: dependências respeitadas, concorrência só onde há independência, resultado integrado verificado. |
| F03 | Despachar eventos do grafo para agentes | Adversarial | Violações e anomalias disparam verificação automática, sem tempestade de eventos. |
| F04 | Isolar papéis e restringir permissões por perfil | Operador, Gatekeeper | Geração, ataque e autorização ficam separados; operações proibidas são de fato recusadas. |
| F05 | Gerenciar checkpoints de execução dos agentes | Builder, Operador | Execuções sobrevivem a quedas, retomam sem duplicar efeitos e descartam junto com o território — preservando a memória de falhas. |
| F06 | Rotear atenção humana por risco da mudança | Operador | Revisão humana proporcional ao risco real: fast path para baixo risco comprovado, decisão registrada para alto risco. |
| F07 | Evoluir o sistema a partir de falhas (hill-climbing supervisionado) | Meta-Agent, Operador | Falhas recorrentes viram restrições melhores, com não-regressão, aprovação, versionamento e reversão. |
| F08 | Medir valor e observar a execução | Operador | Autonomia vira hipótese mensurável; relatórios, decisões de risco e retrospectiva têm fonte de dados própria. |

### Invariante de promoção (transversal a F04/F06)

Nenhum caminho de promoção — Gatekeeper (PBI-18), fast path (PBI-24) ou aprovação humana (PBI-26) — dispensa evidência independente válida referente à **versão candidata atual**. Correção posterior à aprovação **invalida a aprovação anterior**, que deve ser reavaliada antes de qualquer promoção. Risco desconhecido ou evidência incompleta nunca autoriza fast path: o default é escalar ao Operador.

---

## 6. Product Backlog

### Fronteira do slice inicial (ordem de entrega)

O primeiro slice priorizado é o ciclo mínimo verificável, composto por:
- **PBI-01..05** — ciclo, orçamento, circuit breaker, iteração sobre veredicto e relatório final;
- **PBI-16** — o gerador não promove: isolamento mínimo exigido pela invariante de promoção;
- **Telemetria mínima** (subconjunto do PBI-31: veredictos, custo e consumo de orçamento por ciclo) — o relatório (PBI-05) e o circuit breaker (PBI-03) dependem dela;
- **Contrato mínimo de execução de agentes** (Q04: iniciar, cancelar, isolar, medir, recuperar) e o **mapeamento nomes×capacidades** (premissa 6) — ambos pré-requisitos a resolver na quebra técnica antes de comprometer o slice. O contrato tem decisão registrada no [ADR-005](../../adr/ADR-005-AGENT-EXECUTION-CONTRACT.md) — **PROPOSED até o spike de viabilidade** validar os critérios de aceite.

**Condição terminal de sucesso do slice inicial:** O primeiro slice termina estritamente em **"Candidato Aprovado com Pacote de Evidências Auditável" (Decisão Auditada)**, sem executar mutação/promoção física direta na Base. A mutação atômica em Base é postergada até o Gatekeeper Pleno (F04 / PBI-18) e a prova de autorização física de processos. Isso elimina o risco de corrupção ou efeitos irreversíveis na Base antes de o isolamento e a reconciliação de crash estarem consolidados.

**Identidade operacional da candidata (C01):** Toda versão candidata possui identidade determinística atômica `versionId = sha256(canonicalTuple)` vinculando canonicamente: `artifactDigest`, `baselineDigest`, `rulesRevision`, `coverageRevision`, `graphGeneration`, `contractVersion` e `policyRevision`. Qualquer alteração na Base, nas regras de teste ou na geração do grafo invalida imediatamente as evidências vinculadas.

**Semântica de crash do slice inicial:** até F05 chegar, o slice **não tem retomada automática** — queda no meio de um ciclo significa reinício/reexecução desde o objetivo original, com território/estado inconsistente descartado e a anomalia registrada no relatório. Checkpoints, retomada idempotente e descarte coordenado (PBI-19..22) são entregas posteriores (F05).

### F01 — Executar Outer Loop autônomo

- **PBI-01** — Como Operador, quero executar ponta-a-ponta uma mudança pequena e verificável — um Builder Agent, uma verificação independente e uma decisão final auditável com candidato selado (sem mutação física em Base no primeiro slice) — com cenários de resultado definidos (sucesso, reprovação, verificador indisponível, orçamento esgotado), para validar o ciclo mínimo antes de ampliar a autonomia. *(primeiro slice; deliberadamente enxuto — a política completa de promoção física e isolamento de processos vem em F04)*
- **PBI-02** — Como Operador, quero definir orçamento (tokens, custo, tentativas, tempo) com política para chamadas em andamento — novas ações bloqueadas ao atingir o teto e consumo pendente contabilizado — para que nenhum custo fuja do previsto sem controle.
- **PBI-03** — Como Operador, quero um circuit breaker que suspenda o loop e me escale após falhas repetidas do mesmo tipo, para que o sistema peça ajuda em vez de girar infinito.
- **PBI-04** — Como Builder Agent, quero que o veredicto do verificador alimente minha próxima iteração (propor → testar no Harness → corrigir), para convergir autonomamente até a evidência ficar verde.
- **PBI-05** — Como Operador, quero uma condição de encerramento objetiva com relatório final (motivo da parada, evidências coletadas, custo, iterações), para saber exatamente por que e como o loop terminou — inclusive para diagnóstico post-mortem.

### F02 — Compilar plano de agentes a partir da topologia do código

- **PBI-06** — Como Operador, quero que uma solicitação de mudança identifique o subgrafo afetado (comunidades, dependências, âncoras), para conhecer o alcance real antes de instanciar agentes.
- **PBI-07** — Como Builder Agent, quero um claim exclusivo sobre as âncoras do meu módulo, com comportamento definido para conflito, abandono e transferência de claim, para editar sem sobreposição e sem zona cinzenta de responsabilidade.
- **PBI-08** — Como Operador, quero um plano de tarefas derivado da topologia que respeite dependências e limites de concorrência, paralelizando apenas trabalho independente, para que o paralelismo venha da estrutura real do código — não de uma regra fixa de contagem de comunidades.
- **PBI-09** — Como Operador, quero que dois agentes que precisem tocar o mesmo contrato sejam obrigados a passar pela avaliação de seam, para que edições no mesmo contrato nunca aconteçam silenciosamente em paralelo.
- **PBI-10** — Como Operador, quero que os resultados de agentes paralelos sejam integrados e verificados em conjunto, com tratamento de falha parcial (um agente reprova sem invalidar cegamente os demais), para aceitar ou reprovar a entrega como um todo.

### F03 — Despachar eventos do grafo para agentes

- **PBI-11** — Como Adversarial Agent, quero ser acionado automaticamente quando um commit violar um seam/contrato, para atacar a mudança com evidências sem esperar o humano.
- **PBI-12** — Como Operador, quero que anomalias detectadas em traces ingeridos disparem verificação automática, para capturar regressões de comportamento cedo.
- **PBI-13** — Como Operador, quero regras de assinatura de eventos configuráveis (qual evento dispara qual agente/perfil), para evoluir gatilhos sem alterar código.
- **PBI-14** — Como Operador, quero proteção contra tempestade de eventos — sem disparo duplicado, recursivo ou fora de orçamento — para que os gatilhos não consumam o orçamento que o Outer Loop controla.

### F04 — Isolar papéis e restringir permissões por perfil

- **PBI-15** — Como Operador, quero que cada perfil (builder / adversarial / gatekeeper) tenha operações proibidas de fato **recusadas quando chamadas diretamente** — não apenas ausentes da lista de ferramentas — para que a restrição seja de autorização, não de cosmética.
- **PBI-16** — Como Operador, quero garantir que o agente gerador nunca possa invocar a promoção a Base, para separar fisicamente geração de autorização.
- **PBI-17** — Como Adversarial Agent, quero rodar em processo/instância separada do Builder — usando modelo de família diferente como **hipótese de diversidade de avaliação a validar, não garantia** — para reduzir compartilhamento de contexto e viés operacional.
- **PBI-18** — Como Gatekeeper, quero liberar a promoção somente com evidência independente verde referente à **versão candidata atual**, com resultado definido (não-promoção) quando o verificador estiver indisponível ou inconclusivo, para que nenhuma promoção ocorra por lacuna ou ambiguidade.

### F05 — Gerenciar checkpoints de execução dos agentes

- **PBI-19** — Como Builder Agent, quero checkpoint de execução (histórico de mensagens, chamadas de tools, árvore de tentativas) atrelado ao meu horizonte, para sobreviver à queda da sessão.
- **PBI-20** — Como Operador, quero retomar uma execução de forma **idempotente** a partir do último checkpoint — sem duplicar efeitos de chamadas cujo efeito já ocorreu antes da gravação — para não repetir nem duplicar trabalho após falha. Quando uma tool externa não permitir deduplicar nem confirmar se o efeito ocorreu, o default é **tratar como ocorrido**, sinalizar a incerteza no checkpoint e escalar ao Operador se a incerteza for material — nunca reexecutar às cegas.
- **PBI-21** — Como Operador, quero diagnóstico da árvore de tentativas por horizonte, para entender por que uma execução falhou ou convergiu.
- **PBI-22** — Como Operador, quero que o descarte de um território leve junto os checkpoints de execução a ele associados, **mas preserve o registro de auditoria** (evidência das falhas) exigido pela retrospectiva — e sinalize efeitos externos de tools como não-desfeitos — para que conhecimento e execução voltem juntos sem destruir a memória de falhas.

### F06 — Rotear atenção humana por risco da mudança

- **PBI-23** — Como Operador, quero uma métrica de risco da mudança que **combine topologia** (folha vs. aresta inter-comunidades vs. âncora protegida por ADR) **com evidências e restrições adicionais** (ex.: nós marcados como sensíveis), para decidir autonomia vs. escalonamento sem confundir posição no grafo com risco real.
- **PBI-24** — Como Operador, quero fast path autônomo somente para mudanças de baixo risco comprovado (nó folha + fingerprint de efeito intacto + ausência de marcadores de risco), para reduzir o volume de revisão sem abrir exceções perigosas. Risco desconhecido ou evidência incompleta **escala por default** — nunca segue fast path.
- **PBI-25** — Como Operador, quero que mudanças de alto risco paussem o horizonte e me enviem o link direto ao nó/aresta crítica no graph-ui, para revisar as arestas críticas em vez de 40 arquivos. O escalonamento entrega um **pacote de decisão** — mudança, evidências coletadas, motivo do risco e impacto — e não apenas o link.
- **PBI-26** — Como Operador, quero decidir o destino de um horizonte pausado — aprovar, rejeitar, ou solicitar correção e retomar o loop — com a decisão registrada e sua consequência operacional aplicada (promoção condicionada à invariante de promoção e ao gate do PBI-18, descarte ou retorno ao ciclo), e com a aprovação **válida apenas para a versão candidata avaliada** — correção posterior invalida a aprovação anterior —, para que a intervenção humana tenha efeito e rastro.

### F07 — Evoluir o sistema a partir de falhas (hill-climbing supervisionado)

- **PBI-27** — Como Meta-Agent, quero ler os registros de auditoria dos horizontes descartados de um período, para identificar padrões de erro recorrentes dos agentes.
- **PBI-28** — Como Meta-Agent, quero propor uma nova Tradição ou ajuste de contrato de skill (validado por skill-contract-attestor), para melhorar restrições com base em evidência de falha.
- **PBI-29** — Como Operador, quero não-regressão provada no Harness como condição de qualidade para qualquer proposta valer, para evoluir o sistema sem degradá-lo.
- **PBI-30** — Como Operador, quero aprovação explícita, ativação controlada e versionada, e reversão da evolução aplicada — com a habilitação das skills hoje diferidas (`constellation-individuator`, `level-graduator`) tratada como habilitação de capacidade, separada do resultado de qualidade — para que a evolução do sistema seja supervisionada e reversível.

### F08 — Medir valor e observar a execução

- **PBI-31** — Como Operador, quero telemetria do orquestrador — veredictos por iteração, custo por ciclo, gatilhos disparados, consumo de orçamento, intervenções humanas — registrada e consultável, para que relatórios de loop (PBI-05), decisões de risco (PBI-23..26) e a retrospectiva (PBI-27) tenham fonte de dados própria.
- **PBI-32** — Como Operador, quero comparar o sistema contra uma referência mais simples (ex.: agente único sem orquestração) usando custo por entrega aceita, intervenções humanas e regressões, para demonstrar — ou refutar — que a autonomia orquestrada vale seu custo.

---

## 7. Traceability

| PBI | Funcionalidade | Persona | Problema / Expectação |
| --- | --- | --- | --- |
| PBI-01 | F01 | Operador, Builder | P01, P02 / E01, E02 |
| PBI-02 | F01 | Operador | P01 / E01 |
| PBI-03 | F01 | Operador | P01, P04 / E01, E04 |
| PBI-04 | F01 | Builder | P01 / E01 |
| PBI-05 | F01 | Operador | P01, P07 / E01, E07 |
| PBI-06 | F02 | Operador | P06 / E06 |
| PBI-07 | F02 | Builder | P06, P02 / E06, E02 |
| PBI-08 | F02 | Operador | P06 / E06 |
| PBI-09 | F02 | Operador | P06, P02 / E06, E02 |
| PBI-10 | F02 | Operador, Builder | P06 / E06 |
| PBI-11 | F03 | Adversarial | P05 / E05 |
| PBI-12 | F03 | Operador | P05 / E05 |
| PBI-13 | F03 | Operador | P05 / E05 |
| PBI-14 | F03 | Operador | P05, P01 / E05, E01 |
| PBI-15 | F04 | Operador | P02 / E02 |
| PBI-16 | F04 | Operador | P02 / E02 |
| PBI-17 | F04 | Adversarial | P02 / E02 |
| PBI-18 | F04 | Gatekeeper | P02, P04 / E02 |
| PBI-19 | F05 | Builder | P07 / E07 |
| PBI-20 | F05 | Operador | P07 / E07 |
| PBI-21 | F05 | Operador | P07 / E07 |
| PBI-22 | F05 | Operador | P07, P03 / E07, E03 |
| PBI-23 | F06 | Operador | P04 / E04 |
| PBI-24 | F06 | Operador | P04 / E04 |
| PBI-25 | F06 | Operador | P04 / E04 |
| PBI-26 | F06 | Operador | P04 / E04 |
| PBI-27 | F07 | Meta-Agent | P03 / E03 |
| PBI-28 | F07 | Meta-Agent | P03 / E03 |
| PBI-29 | F07 | Operador | P03 / E03 |
| PBI-30 | F07 | Operador | P03 / E03 |
| PBI-31 | F08 | Operador | P01, P04 / E08, E01 |
| PBI-32 | F08 | Operador | P01, P04 / E08 |

Tabela por PBI (não por intervalo): cada linha declara todas as relações, inclusive secundárias (ex.: PBI-01 também endereça P02; PBI-03 inclui escalonamento humano; PBI-05 serve ao diagnóstico de P07; PBI-22 fecha a ponte F05↔F07 exigida por E03).

---

## 8. Assumptions

1. **Linguagem:** a camada de orquestração será escrita em **TypeScript** (decisão explícita do operador); o core determinístico em C11 permanece inalterado.
2. **Local:** a camada TS vive como **novo pacote neste repositório**, consumindo o CBM via MCP.
3. **Personas de agentes:** Builder, Adversarial, Gatekeeper e Meta-Agent são personas de primeira classe do backlog (decisão validada pelo operador).
4. **Gate determinístico:** o portão de mutação/promoção reaproveita a verificação determinística existente em C; o PBB não assume novos gates.
5. **graph-ui:** a UI 3D existente pode receber deep links para nós/arestas (usada no escalonamento do PBI-25).
6. **Nomes × capacidades:** nomes citados no briefing (`get_ast_anchors`, `evaluate_seam_map`, `apply_mutation_gate`, etc.) podem não existir verbatim na interface atual do CBM. **Atenção:** a diferença pode ser de *capacidade*, não apenas de nome — se o mapeamento (na quebra técnica) revelar capacidade faltante, a fronteira "core inalterado" pode ser afetada e deve ser reavaliada antes de comprometer o primeiro slice.
7. **Skills diferidas:** `constellation-individuator` e `level-graduator` estão hoje em estado deferred (análise apenas); a habilitação com admissão real é parte do escopo de F07 (PBI-30), separada do resultado de qualidade (PBI-29).
8. **Telemetria e comparação:** as métricas de F08 usam dados produzidos pelo próprio orquestrador (PBI-31); a definição da referência comparável e dos critérios de sucesso da medição de valor (PBI-32) é tratada como questão de negócio em Q08.

---

## 9. Open Questions

### Q01 — O PBB cobre a visão completa ou apenas o protótipo Outer Loop?

**Suggested answer:** PBB completo cobrindo o motor inteiro (8 funcionalidades), com o slice mínimo verificável (PBI-01..05) como primeiro passo priorizado.

**Rationale:** permite quebrar em tasks e priorizar a visão toda sem perder o primeiro passo prático proposto pelo briefing.

**Resolved answer:** Visão completa + slice inicial priorizado.

**Answered by:** human

**Status:** Human validated

### Q02 — Onde vive a camada de orquestração em TypeScript?

**Suggested answer:** Novo pacote TS neste repositório, consumindo o core C via MCP.

**Rationale:** mantém versionamento, testes e evolução do core e da orquestração juntos; o briefing pressupõe integração íntima com o SQLite WAL e o watcher.

**Resolved answer:** Novo pacote TS neste repositório.

**Answered by:** human

**Status:** Human validated

### Q03 — Agentes são personas de primeira classe no PBB?

**Suggested answer:** Sim — o produto existe para coordená-los; suas metas e permissões modelam o backlog.

**Rationale:** as funcionalidades F02–F05 só são expressáveis se Builder/Adversarial/Gatekeeper/Meta-Agent forem personas com metas próprias.

**Resolved answer:** Humanos + agentes como personas.

**Answered by:** human

**Status:** Human validated

### Q04 — Quais hosts/modelos executam os agentes (Builder, Adversarial)? E qual o contrato mínimo de execução de agentes?

**Suggested answer:** agnóstico: a orquestração trata agentes como sessões MCP com perfil; o host e o modelo (inclusive família diferente no Adversarial, PBI-17) são configuração, não código. Antes de comprometer o primeiro slice, definir o contrato mínimo de execução de agentes — iniciar, cancelar, isolar, medir (custo/tokens) e recuperar — independente de fornecedor.

**Rationale:** preserva o escopo sem amarrar o backlog a um fornecedor; o contrato mínimo é pré-requisito para PBI-01..05 e PBI-17 e deve ser resolvido na quebra técnica, não presumido.

**Encaminhamento:** decisão registrada no [ADR-005](../../adr/ADR-005-AGENT-EXECUTION-CONTRACT.md) (contrato de execução: processos MCP perfis builder/adversarial, journal próprio em SQLite WAL, coordenação journal×territórios) — **PROPOSED até o spike de viabilidade** validar os cinco critérios de aceite.

**Resolved answer:** Unresolved

**Answered by:** unresolved

**Status:** Deferred

### Q05 — Quais são os valores padrão de orçamento e condições de parada do Outer Loop?

**Suggested answer:** parâmetros configuráveis por ciclo com defaults conservadores (nº máximo de tentativas por veredicto, teto de tokens/custo, timeout por ciclo, política de consumo pendente); valores exatos definidos na quebra técnica a partir de medições.

**Rationale:** o briefing exige orçamento e circuit breaker, mas não fixa números; defaults conservadores protegem custo enquanto permitem medir.

**Resolved answer:** Unresolved

**Answered by:** unresolved

**Status:** Deferred

### Q06 — Qual a cadência do loop de retrospectiva/evolução (F07)?

**Suggested answer:** semanal, conforme proposto no briefing ("loop semanal de retrospectiva").

**Rationale:** é a única cadência mencionada pela fonte; mensal ou on-demand seriam reduções de valor sem ganho de escopo.

**Resolved answer:** Unresolved

**Answered by:** unresolved

**Status:** Deferred

### Q07 — Qual a política de retenção do registro de auditoria de horizontes descartados (PBI-22/PBI-27)?

**Suggested answer:** retenção configurável por período (ex.: ciclos ou semanas), suficiente para cobrir a retrospectiva semanal sem crescimento ilimitado; descarte do registro de auditoria apenas após a retrospectiva correspondente consumi-lo.

**Rationale:** resolve a tensão descarte×retenção sem fixar números; alinha-se à cadência semanal sugerida em Q06.

**Resolved answer:** Unresolved

**Answered by:** unresolved

**Status:** Deferred

### Q08 — Qual a referência comparável e os critérios de sucesso da medição de valor (PBI-32)?

**Suggested answer:** comparar contra um agente único sem orquestração em entregas de escopo equivalente (mesma tarefa, mesmo critério de aceite), medindo custo por entrega aceita, intervenções humanas e regressões; critérios de sucesso definidos pelo Operador **antes** do início da medição.

**Rationale:** sem equivalência de entregas e critério prévio de sucesso, a comparação pode ser conduzida para confirmar qualquer conclusão; definir antes preserva o valor demonstrativo que o briefing exige ("demonstrar, com métricas e testes, que essa arquitetura resolve problemas melhor do que abordagens mais simples").

**Resolved answer:** Unresolved

**Answered by:** unresolved

**Status:** Deferred
