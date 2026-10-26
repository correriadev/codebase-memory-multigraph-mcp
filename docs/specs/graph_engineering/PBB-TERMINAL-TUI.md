# PBB — Reformulação do Terminal com TUI Full-Screen e Observabilidade de Logs Intercalados (CBM Orchestrator)

Documento canônico de Product Backlog Building (PBB) revisado e refinado após auditoria crítica de arquitetura (Codex `gpt-6.1-sol` e validação cruzada).

- **Data:** 2026-10-26
- **Versão:** 2.0 (Pós-auditoria)
- **Contexto:** CBM System Intelligence Engine / Interactive Terminal Interface
- **Referência:** Padrões de TUI moderna (OpenCode, OpenAI Codex CLI, Google Antigravity), Contratos de Terminal Virtual e Observabilidade Unificada

---

## 1. Product

**Nome:** `cbm-terminal-tui` — Interface de Terminal TUI com Viewport Delimitado e Observabilidade de Logs sob Demanda.

**Intenção:** Proporcionar ao operador do CBM Orchestrator um ambiente de interação visualmente estável, com altura de tela delimitada e sem rolagem vertical descontrolada no console hospedeiro. A interface organiza a interação em áreas fixas (cabeçalho de metadados, viewport rolável de conversa, barra de telemetria e linha de comando) e disponibiliza uma alternância dinâmica (*toggle*) para exibir ou ocultar os logs operacionais internos do motor (probes MCP, locks, selagem de candidatas e contagem de tokens) intercalados no fluxo cronológico do chat em estilo atenuado (*dim*) com identificadores textuais de subsistema. A separação garante leitura limpa dos raciocínios dos agentes com diagnóstico técnico profundo imediato sob demanda.

**Limites do Produto:**
- **Dentro do escopo:**
  - Gerenciamento de ciclo de vida em tela cheia com alternate screen buffer do terminal, restaurando o cursor, modo canônico e o histórico original do shell em qualquer saída (normal, `:quit`, erro recuperável ou interrupção do operador).
  - Regra de coordenador único de saída (*single screen writer*): interceptação de `stdout`, `stderr` e saídas de subprocessos para evitar colisões e sobrescrita desordenada no frame.
  - Layout dividido em zonas fixas:
    - *Cabeçalho Fixo (Topo):* Projeto CBM, workspace (com truncamento suave de caminhos longos), tupla ativa `(provider, model, effort)`, cap de tokens e orçamento monetário.
    - *Viewport Central de Conversação:* Histórico de interações, intenções, sínteses de raciocínio e marcos do ciclo autônomo, contido na altura visível da tela.
    - *Barra de Status e Telemetria (Base superior):* Indicador da fase do ciclo (`IDLE`, `BUILDING`, `SEALING`, `VERIFYING`, `APPROVAL`), consumo acumulado de tokens e USD, estado do lock e status do toggle (`[Logs: ON/OFF]`).
    - *Linha de Entrada de Comando (Base inferior):* Prompt de instrução com histórico de comandos digitados e edição por cursor, isolado de novas mensagens que chegam no viewport.
  - Mecanismo dinâmico de alternância de logs (*toggle logs* via atalho dedicado, ex: `F2`, `Ctrl+O` ou comando `:logs`):
    - *Toggle ON:* Logs operacionais retidos no buffer são renderizados retrospectivamente em suas posições cronológicas estáveis no chat, com estilo atenuado (*dim* / cinza) e prefixos `[cbm:*]`.
    - *Toggle OFF:* Logs operacionais são ocultados do viewport, mantendo puramente as mensagens do agente, marcos de ciclo e alertas acionáveis de erro.
    - *Sem Efeito Colateral Operacional:* Alternar a visibilidade é uma operação puramente projetiva; não dispara chamadas de ferramentas, não reprocessa tokens e não duplica persistência.
  - Navegação no histórico de mensagens com controle de foco e auto-scroll inteligente:
    - Rolagem retroativa pausa o acompanhamento automático e exibe indicador visual de histórico pausado.
    - Retornar ao final do histórico reativa o auto-scroll para novos eventos.
  - Reatividade ao redimensionamento da janela do terminal baseada em eventos do stream (`stdout.on('resize')`), recalculando proporções com limites mínimos protegidos (mínimo: 60 colunas × 15 linhas).
  - Fallback automático para modo sequencial linear contínuo quando a entrada ou a saída não forem TTYs interativos (pipes, CI/CD, execução em lote).
  - Suporte a `NO_COLOR`: a TUI opera normalmente em modo monocromático sem forçar desativação da interface.
- **Fora do escopo:**
  - Interface gráfica web ou visualizador em navegador (atendido pelo `graph-ui`).
  - Alterações nas regras de negócio do Outer Loop, Gatekeeper S0 ou persistência do banco SQLite WAL.
  - Ferramenta de busca regex forense em arquivos de log históricos no disco (coberta pelas ferramentas existentes de auditoria).

---

## 2. Problems

| ID | Problema (situação indesejável observada) |
| --- | --- |
| P01 | O modelo atual de terminal escreve linhas continuamente na saída padrão, provocando rolagem vertical descontrolada que empurra mensagens anteriores para fora da tela e perde o foco do operador durante ciclos autônomos longos. |
| P02 | O operador precisa diagnosticar passos internos do motor (probes MCP, locks, selagem de candidatas e contagem de tokens), mas atualmente esses eventos ou poluem a conversação em texto bruto ou ficam escondidos em logs em disco sem correlação temporal direta com as mensagens do chat. |
| P03 | Redimensionamentos da janela do terminal desalinham as bordas desenhadas e misturam caracteres residuais quebrados no histórico do console hospedeiro. |
| P04 | Falta uma diferenciação visual clara entre as mensagens de diálogo/raciocínio dos agentes e os eventos de infraestrutura do orquestrador, aumentando a carga cognitiva na leitura. |
| P05 | A rolagem manual no terminal padrão entra em conflito com a saída contínua do processo, fazendo a tela saltar automaticamente para o final da página e interrompendo a leitura de blocos longos de código e diffs. |

---

## 3. Expectations

| ID | Expectação de Resultado | Endereça |
| --- | --- | --- |
| E01 | A interface opera em modo tela cheia delimitada nas dimensões suportadas (≥ 60 colunas × 15 linhas), sem causar rolagem na janela do terminal hospedeiro e restaurando a sessão original ao encerrar. | P01 |
| E02 | O operador pode alternar dinamicamente (toggle) a visibilidade dos logs operacionais do orquestrador no chat, mantendo correlação temporal estável entre eventos de sistema e respostas dos agentes sem efeitos colaterais de execução. | P02 |
| E03 | A interface adapta-se de forma suave e estável a alterações de tamanho da janela do terminal sem corrupção de layout ou vazamento de caracteres. | P03 |
| E04 | Eventos de infraestrutura do motor são apresentados em estilo visual atenuado (*dim*) e com identificadores textuais de subsistema, distinguindo-se claramente das interações do agente mesmo em terminais sem suporte a cores. | P04 |
| E05 | O operador navega livremente pelo histórico de mensagens e logs sem que novas saídas forcem saltos involuntários na tela enquanto lê trechos anteriores. | P05 |
| E06 | Ambientes não interativos (CI, scripts em lote, pipes) executam de forma transparente em modo streaming linear sequencial, enquanto preferências de ausência de cor (`NO_COLOR`) operam em TUI monocromática. | P01, P03 |

---

## 4. Personas

### Persona 1: Operador de Engenharia / Desenvolvedor Autônomo (Dev-Operator)
- **Papel:** Conduz sessões interativas com o CBM Orchestrator, despachando intenções de código, avaliando ciclos e validando decisões arquiteturais.
- **Objetivo Principal:** Interagir com os agentes em uma tela estável e organizada, acompanhando raciocínios com clareza e inspecionando logs de baixo nível sob demanda sem perder a continuidade visual.
- **Interações Relevantes:**
  - Digita objetivos e confirma alvos de ciclo.
  - Acompanha respostas dos agentes e resumos de auditoria.
  - Usa tecla de atalho de toggle para checar eventos de infraestrutura e diagnosticar anomalias temporárias.
  - Rola o histórico para revisar trechos anteriores de conversação e diffs propostos.
- **Problemas e Expectações Relacionados:** P01, P02, P03, P04, P05 ➔ E01, E02, E03, E04, E05.

### Persona 2: Engenheiro de Plataforma / Auditor de Sessão (Auditor/DevOps)
- **Papel:** Inspeciona integridade do harness, portas de governança, conexões MCP e execuções em ambientes controlados.
- **Objetivo Principal:** Obter telemetria contínua (tokens, custos, bloqueios de lock, probe de ferramentas) e garantir compatibilidade da ferramenta tanto em terminais ricos quanto em pipelines automatizados.
- **Interações Relevantes:**
  - Observa a barra de status fixa para monitorar custos e consumo de recursos.
  - Ativa logs detalhados para verificar latências de ferramentas MCP e selagem de candidatas.
  - Executa o orquestrador em esteiras automatizadas ou terminais remotos (SSH / headless).
- **Problemas e Expectações Relacionados:** P01, P02, P03, P04 ➔ E01, E02, E04, E06.

---

## 5. Functionalities

- **F01: Visualizar Interface em Viewport Delimitado** (apresentação em tela cheia com buffer alternativo, contenção visual sem rolagem externa e restauração consistente do terminal).
- **F02: Navegar no Histórico de Conversação** (área central rolável para mensagens, raciocínios e diffs, com suporte a rolagem manual via teclado e auto-scroll inteligente).
- **F03: Alternar Observabilidade de Logs do Orquestrador (Toggle Logs)** (mecanismo dinâmico para exibir ou ocultar logs internos do orquestrador intercalados no chat com replay estável do buffer).
- **F04: Distinguir Eventos de Infraestrutura por Atenuação e Identificadores** (formatação com estilo atenuado/dim e badges textuais de subsistema legíveis em temas claros, escuros e monocromáticos).
- **F05: Exibir Barra de Status e Telemetria Contínua** (painel fixo de cabeçalho e rodapé exibindo projeto, tupla `(provider, model, effort)`, fase do ciclo, custos, tokens e atalhos).
- **F06: Gerenciar Linha de Entrada e Atalhos Globais** (captura de comandos, histórico de digitação, cancelamento e isolamento de foco entre input e navegação de histórico).
- **F07: Adaptar Visualização a Redimensionamentos da Janela** (recalculo instantâneo de proporções com proteção de geometria mínima em eventos de resize).
- **F08: Degradação Graciosa para Modo Linear (Fallback)** (detecção de ausência de TTY de entrada ou saída, garantindo streaming sequencial em automações e preservando TUI monocromática com `NO_COLOR`).

---

## 6. Product Backlog

### PBI-01: Inicialização e Restauração Segura do Buffer de Tela Cheia
- **Funcionalidade:** F01
- **Persona:** Dev-Operator
- **Descrição:**
  *As a* Dev-Operator,
  *I want* que a aplicação inicialize em um buffer alternativo de tela e restaure o terminal original em qualquer encerramento,
  *So that* o histórico do meu console hospedeiro permaneça limpo e utilizável após o uso.
- **Critérios de Aceite:**
  1. A abertura da interface ativa o buffer de tela cheia sem que a saída anterior do shell seja apagada.
  2. Qualquer encerramento tratável (saída pelo menu `[0]`, comando `:quit`, erro recuperável do sistema ou interrupção por cancelamento) restaura o buffer original, a visibilidade do cursor e os modos canônicos de digitação do shell.
  3. Toda escrita de texto do processo (incluindo subprocessos e traces de erro) passa obrigatoriamente pelo coordenador da TUI, prevenindo corrupção direta da tela.

### PBI-02: Painel Fixo de Cabeçalho com Metadados e Truncamento Suave
- **Funcionalidade:** F05
- **Persona:** Dev-Operator
- **Descrição:**
  *As a* Dev-Operator,
  *I want* visualizar no topo da tela os dados essenciais da sessão com proteção contra caminhos longos,
  *So that* eu saiba o projeto e a configuração ativa sem que o cabeçalho invada a área útil de leitura.
- **Critérios de Aceite:**
  1. O cabeçalho ocupa linhas fixas no topo da janela e não rola com o conteúdo.
  2. Exibe: Nome do projeto CBM, caminho do workspace (com truncamento suave `.../subpasta` caso ultrapasse a largura disponível), tupla `(provider, model, effort)` e limites de tokens/custo.
  3. Valores ainda não inicializados ou ausentes são representados explicitamente como `N/A` ou `[não configurado]`, sem quebras de layout.
  4. Modificações na tupla de provedor ou configuração são refletidas imediatamente no cabeçalho.

### PBI-03: Viewport Central Rolável com Proteção de Geometria Mínima
- **Funcionalidade:** F01, F02
- **Persona:** Dev-Operator
- **Descrição:**
  *As a* Dev-Operator,
  *I want* uma área central de leitura contida nas dimensões da janela e protegida contra redução extrema de tela,
  *So that* os diálogos não causem rolagem externa no terminal e permaneçam legíveis.
- **Critérios de Aceite:**
  1. A altura do viewport é calculada estritamente com base nas linhas visíveis restantes entre o cabeçalho superior e o rodapé de status/input.
  2. Textos longos sofrem quebra de linha visual controlada, respeitando caracteres de largura dupla (emojis e símbolos asiáticos) e larguras de coluna do terminal.
  3. Se a janela do terminal for reduzida abaixo da geometria mínima suportada (60 colunas × 15 linhas), a interface exibe um aviso central de terminal insuficiente em vez de renderizar layout corrompido.
  4. O buffer de memória da sessão é limitado por contagem e por volume (máximo de 1.000 entradas e 2 MB totais, com truncamento de linhas individuais que excedam 10 KB), evitando consumo excessivo de RAM.

### PBI-04: Navegação no Histórico e Regra de Foco entre Input e Viewport
- **Funcionalidade:** F02, F06
- **Persona:** Dev-Operator
- **Descrição:**
  *As a* Dev-Operator,
  *I want* rolar o histórico com teclas dedicadas sem colidir com as setas do campo de digitação,
  *So that* eu possa ler mensagens antigas enquanto redijo uma nova instrução.
- **Critérios de Aceite:**
  1. As teclas `PageUp`, `PageDown` (ou atalho alternativo quando em foco no input) rolam a visualização do histórico para cima e para baixo.
  2. As teclas de seta `↑` e `↓` no campo de comando são reservadas para navegação no histórico de instruções digitadas pelo usuário.
  3. Durante a navegação retroativa no histórico, novas mensagens do sistema não forçam a tela a saltar para o fim, e um indicador `[HISTÓRICO - AUTO-SCROLL PAUSADO]` é exibido.
  4. Ao rolar de volta até a base mais recente ou pressionar `Esc` / `End`, o auto-scroll é reativado para novas mensagens.

### PBI-05: Mecanismo de Toggle de Logs com Replay Estável do Buffer
- **Funcionalidade:** F03
- **Persona:** Dev-Operator / Auditor
- **Descrição:**
  *As a* Dev-Operator,
  *I want* alternar a exibição dos logs operacionais do orquestrador via atalho dedicado com reconstrução cronológica estável,
  *So that* eu veja o histórico detalhado sob demanda sem alterar a execução em andamento.
- **Critérios de Aceite:**
  1. A interface inicializa com o toggle de logs em estado `OFF` por padrão.
  2. O acionamento do atalho dedicado (`F2`, `Ctrl+O` ou comando `:logs`) alterna o estado de exibição entre `ON` e `OFF` instantaneamente.
  3. Ao mudar de `OFF` para `ON`, os eventos operacionais ainda retidos no buffer são renderizados na ordem cronológica estável de recebimento na sessão, intercalados entre as mensagens do chat.
  4. Caso entradas antigas tenham sido descartadas pela política de limite de memória, um aviso visual `[... histórico anterior de logs descartado ...]` é exibido no topo do buffer.
  5. A alternância de logs é estritamente uma operação de visualização; ela não reexecuta ferramentas, não duplica registros na persistência e não altera contadores de tokens ou custos.
  6. A barra de status reflete claramente o estado atual: `[Logs: ON]` ou `[Logs: OFF]`.

### PBI-06: Estilização Atenuada e Prefixo de Subsistema para Logs Operacionais
- **Funcionalidade:** F04
- **Persona:** Dev-Operator / Auditor
- **Descrição:**
  *As a* Dev-Operator,
  *I want* que os logs do orquestrador sejam apresentados com estilo atenuado e prefixos de subsistema,
  *So that* eu distinga imediatamente eventos do motor de mensagens de diálogo do agente.
- **Critérios de Aceite:**
  1. Linhas de log utilizam estilo visual atenuado (*dim* / cinza suave), garantindo contraste adequado tanto em temas claros quanto escuros de terminal.
  2. Cada entrada de log traz um identificador textual de subsistema: `[cbm:engine]`, `[cbm:mcp]`, `[cbm:lock]`, `[cbm:token]`, `[cbm:seal]`, sustentando a distinção mesmo em ambientes monocromáticos (`NO_COLOR`).
  3. Erros acionáveis do sistema, pedidos de aprovação e bloqueios de governança permanecem visíveis no fluxo principal mesmo quando o toggle de logs estiver em `OFF`.
  4. O estilo visual de uma linha de log é resetado ao término da mesma, impedindo contaminação de cor ou intensidade nas mensagens conversacionais adjacentes.

### PBI-07: Barra de Status Inferior com Telemetria e Distinção de Estados
- **Funcionalidade:** F05
- **Persona:** Auditor/DevOps
- **Descrição:**
  *As a* Auditor/DevOps,
  *I want* uma barra de status fixa na base informando a fase operacional corrente, tokens consumidos, custos e atalhos disponíveis,
  *So that* eu monitore o progresso do ciclo e os recursos gastos com precisão.
- **Critérios de Aceite:**
  1. Barra fixa posicionada imediatamente acima da linha de entrada de comandos.
  2. Exibe o estado da máquina de estados do ciclo: `IDLE`, `BUILDING`, `SEALING`, `VERIFYING`, `APPROVAL`.
  3. Apresenta métricas acumuladas da sessão, distinguindo claramente valores desconhecidos (`--`), desatualizados ou calculados.
  4. Exibe o estado do lock de ciclo (`LIVRE` / `BLOQUEADO`) e a legenda dos atalhos contextuais principais (`[F2/:logs] Logs`, `[PgUp/PgDn] Histórico`, `[Ctrl+C] Abortar`, `[0] Menu/Sair`).

### PBI-08: Linha de Entrada de Comandos com Preservação de Rascunho
- **Funcionalidade:** F06
- **Persona:** Dev-Operator
- **Descrição:**
  *As a* Dev-Operator,
  *I want* digitar comandos e instruções em uma linha dedicada sem perder meu rascunho quando novas mensagens chegarem,
  *So that* minha digitação não seja interrompida pelo fluxo de saída do orquestrador.
- **Critérios de Aceite:**
  1. Campo de input posicionado na última linha da tela, com indicador visual de prompt (`❯ `).
  2. Suporte a edição de texto básica (inserção, exclusão com Backspace/Delete, movimentação com setas laterais `←` e `→`).
  3. Novas mensagens ou logs emitidos pelo sistema no viewport central não apagam, sobrescrevem nem deslocam o texto em edição no prompt.
  4. Histórico de comandos anteriores é acessível via setas `↑` e `↓` no prompt, preservando o rascunho atual caso o usuário desça de volta à linha vazia.

### PBI-09: Reatividade ao Redimensionamento da Janela (Stream Resize)
- **Funcionalidade:** F07
- **Persona:** Dev-Operator
- **Descrição:**
  *As a* Dev-Operator,
  *I want* que a TUI se adapte imediatamente ao redimensionamento da janela do terminal,
  *So that* o layout permaneça íntegro sem travamentos ou quebras de texto.
- **Critérios de Aceite:**
  1. A interface captura eventos de alteração de geometria a partir do listener de redimensionamento do stream de saída (`stdout.on('resize')`).
  2. O frame é recalculado e redesenhado dentro das novas dimensões de linhas e colunas.
  3. O rascunho de texto que estava sendo digitado no prompt e a referência visual relativa de leitura no histórico são preservados durante o redimensionamento.

### PBI-10: Fallback Automático para Ambientes não Interativos e Respeito a `NO_COLOR`
- **Funcionalidade:** F08
- **Persona:** Auditor/DevOps
- **Descrição:**
  *As a* Auditor/DevOps,
  *I want* que a aplicação detecte automaticamente a ausência de terminal interativo e degrade para streaming linear, operando em modo monocromático caso `NO_COLOR` esteja ativo,
  *So that* pipelines de CI/CD, redirecionamentos de arquivo e preferências visuais sejam respeitados sem falhas.
- **Critérios de Aceite:**
  1. Caso a saída padrão ou a entrada padrão não sejam um TTY interativo (`!stdout.isTTY || !stdin.isTTY`), a aplicação desativa automaticamente a TUI e opera em modo streaming sequencial contínuo.
  2. A presença da variável `NO_COLOR` desativa cores ANSI, mas **não** desativa a TUI em tela cheia caso o terminal seja um TTY interativo, mantendo a organização de viewport em modo monocromático com separadores textuais.
  3. A compatibilidade com testes automatizados existentes (`interactive.test.ts`, `cli.test.ts`) é mantida sem regressões.

---

## 7. Traceability

| Problema / Expectação | Persona | Funcionalidade | PBI |
| --- | --- | --- | --- |
| P01 / E01 | Dev-Operator | F01 (Visualizar em Viewport Delimitado) | PBI-01 (Buffer Seguro), PBI-03 (Viewport Central) |
| P02 / E02 | Dev-Operator, Auditor | F03 (Toggle Logs) | PBI-05 (Toggle Logs com Replay Estável) |
| P03 / E03 | Dev-Operator | F07 (Adaptar Visualização a Resize) | PBI-09 (Reatividade Stream Resize) |
| P04 / E04 | Dev-Operator, Auditor | F04 (Distinguir Eventos de Infraestrutura) | PBI-06 (Estilização Atenuada e Prefixos) |
| P05 / E05 | Dev-Operator | F02 (Navegar no Histórico) | PBI-03 (Viewport Central), PBI-04 (Navegação no Histórico) |
| P01, P04 / E01, E04 | Dev-Operator, Auditor | F05 (Barra de Status e Telemetria) | PBI-02 (Cabeçalho Fixo), PBI-07 (Barra de Status) |
| P01 / E01 | Dev-Operator | F06 (Gerenciar Linha de Entrada) | PBI-04 (Regra de Foco), PBI-08 (Linha de Entrada de Comandos) |
| P01, P03 / E06 | Auditor/DevOps | F08 (Degradação Graciosa / Fallback) | PBI-10 (Fallback Automático e `NO_COLOR`) |

---

## 8. Assumptions

1. **Terminal Virtual no Windows:** A TUI assume terminais com suporte nativo a sequências VT100/ANSI e alternate buffer (Windows Terminal, VS Code Terminal Integrado, Alacritty, WezTerm, iTerm2). Ambientes legados sem emulação VT são roteados para o modo de fallback linear.
2. **Buffer de Memória Delimitado:** O buffer circular retido em memória pela TUI é delimitado por contagem (máximo de 1.000 entradas) e por volume (máximo de 2 MB), aplicando truncamento em linhas individuais gigantes para proteger o processo Node.js contra estouro de heap em sessões de longa duração.
3. **Isolamento entre Visualização e Persistência:** O toggle de logs controla estritamente a projeção na tela; a gravação canônica de auditoria no SQLite WAL e nos arquivos de log em disco ocorre de forma contínua e desacoplada, sem sofrer impacto com a alternância da exibição.
4. **Coordenador Único de Escrita na Tela (*Single Screen Writer*):** A TUI encapsula a escrita de terminal, de modo que saídas acidentais de bibliotecas ou subprocessos não quebrem o frame desenhado.
5. **Classificação entre Log Operacional e Evento de Governança:** Eventos do motor (traces MCP, locks, hashes de candidata) são categorizados como logs operacionais alternáveis, enquanto avisos de recusa, perguntas de aprovação e erros acionáveis são eventos de ciclo sempre visíveis.

---

## 9. Open Questions

### Q01 — Qual deve ser o estado padrão da exibição dos logs na inicialização da TUI?

**Suggested answer:** Iniciar com o toggle de logs em estado **OFF** (oculto) por padrão, exibindo uma interface limpa com foco nas interações dos agentes, mantendo avisos acionáveis de erro sempre visíveis e permitindo ativação instantânea com a tecla de atalho.

**Rationale:** O objetivo primário do operador é acompanhar o progresso dos agentes e as mudanças de código. Iniciar com logs ocultos reduz a poluição visual imediata, enquanto o atalho na barra de status garante acesso rápido a qualquer momento.

**Resolved answer:** Iniciar com o toggle de logs em estado OFF por padrão, mantendo alertas críticos visíveis e traces detalhados ativáveis via atalho.

**Answered by:** human (validado via auditoria Codex / Antigravity)

**Status:** Human validated

---

### Q02 — Qual deve ser a tecla de atalho primária para alternar a exibição de logs?

**Suggested answer:** Adotar **`F2`** (tecla funcional direta) ou **`Ctrl+O`** como atalho primário de alternância, oferecendo também o comando textual **`:logs`** no prompt de comando, evitando conflitos com `Tab` (foco/completamento) e `Ctrl+L` (limpeza canônica de tela).

**Rationale:** A tecla `Tab` é necessária para navegação de foco ou preenchimento de comandos, e `Ctrl+L` é o atalho canônico de redesenho em terminais Unix/Windows. `F2` e `:logs` evitam ambiguidades com o campo de digitação de texto.

**Resolved answer:** `F2` como atalho rápido de tecla única, `Ctrl+O` como alternativa e `:logs` como comando explícito no prompt.

**Answered by:** human (validado via auditoria Codex / Antigravity)

**Status:** Human validated

---

### Q03 — Como o histórico de logs deve se comportar ao alternar de OFF para ON?

**Suggested answer:** Ao ativar o toggle (`ON`), as linhas de log retidas no buffer circular em memória são renderizadas retrospectivamente em sua ordem estável de recebimento, intercaladas entre as mensagens do chat, preservando o ponto de leitura do operador e exibindo indicador caso entradas antigas tenham sido descartadas.

**Rationale:** A alternância deve ser uma projeção puramente visual sobre o buffer de eventos já coletados na sessão, sem disparar novas chamadas a ferramentas, sem alterar contadores de tokens/custos e sem duplicar registros persistidos.

**Resolved answer:** Projeção puramente visual dos logs retidos em ordem estável de chegada, com badge visual de histórico descartado se houver evicção, sem qualquer efeito colateral de execução.

**Answered by:** human (validado via auditoria Codex / Antigravity)

**Status:** Human validated
