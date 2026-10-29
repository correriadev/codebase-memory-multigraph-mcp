# PBB — Fluxo Alpha Interativo: Chat Conversacional (Etapa 1) e Streaming de Logs (Etapas 2+)

Documento canônico de Product Backlog Building (PBB) definindo o fluxo Alpha simplificado do CBM Orchestrator. Substitui a complexidade de TUI full-screen por interação linear via console padrão (CMD/PowerShell/Bash).

- **Data:** 2026-10-28
- **Versão:** 1.0 (Alpha Simplificado)
- **Domínio:** `orchestrator`
- **Contexto:** CBM System Intelligence Engine / Terminal Alpha Interaction
- **Referência:** Filosofia UNIX, CLI interativa (`readline`), streaming de logs (`stdout`) e portão humano conversacional

---

### Avaliação Inicial do Produto (Advisory Posture)

A tentativa anterior de construir uma TUI full-screen com buffer alternativo e gerenciamento de janelas introduziu complexidade desnecessária para um produto em estágio Alpha. `[Certain]`
A divisão em duas fases — Chat na Etapa 1 e Logs nas Etapas 2+ — elimina os problemas de emulação ConPTY no Windows, quebra de scrollback e desconfiguração de terminal. `[Certain]`
Risco residual aceito: o formato linear não possui painéis fixos de telemetria; os dados de custo e status são emitidos como mensagens e sumários no stdout. `[Certain]`

---

## 1. Produto

* **Nome:** `cbm-alpha-flow` — Fluxo Interativo Alpha com Chat de Alinhamento e Execução Orientada a Logs.
* **Intenção:** Proporcionar uma experiência de uso rápida, robusta e acessível diretamente no console padrão (CMD, PowerShell, terminais de IDE ou SSH). Na Etapa 1, operador e agente refinam e aprovam o plano via diálogo natural linear. Nas Etapas 2+, o motor executa de forma autônoma emitindo logs cronológicos estruturados no console padrão, mantendo intervenção sob demanda via `Ctrl+C`.
* **Restrições:** 
  * Não utilizar buffers alternativos (`alternate screen buffer`) ou bibliotecas pesadas de desenho ANSI full-screen.
  * Manter compatibilidade com qualquer emulador de terminal com suporte a `stdin`/`stdout`.
  * Garantir interrupção segura de processos e liberação de locks SQLite em `Ctrl+C`.
* **Resultado Pretendido:** Operador conduz tarefas completas com fricção zero de interface, aprovando planos em conversa e acompanhando a execução em tempo real.
* **Sinal de Sucesso:** Operador inicia tarefa, alinha escopo no chat em até 3 iterações e obtém execução completa com logs legíveis e sem travamentos.
* **Recomendação:** Implementar imediatamente este fluxo Alpha como padrão do orquestrador, depreciando o buffer alternativo.

### Limites do Escopo

#### In Scope
* Prompt conversacional linear para a Etapa 1 (Etapa de Alinhamento e Planejamento) usando `node:readline`.
* Comando explícito de confirmação ou gatilho de aprovação para transição da Etapa 1 para a Etapa 2.
* Modo de streaming de logs estruturados (timestamp, subsistema, mensagem limpa) para as Etapas 2+ (Builder, Sealer, Verifier).
* Captura de sinal `SIGINT` (`Ctrl+C`) para pausa controlada com menu de contingência (Continuar, Instruir, Abortar).
* Diagnóstico silencioso e automático de ambiente (verificação de locks e daemon) antes da abertura do prompt.
* Veredito final limpo com resumo de arquivos modificados, custo monetário e consumo de tokens.

#### Out of Scope
* Telas divididas, barras de status persistentes ou viewport delimitado (TUI full-screen).
* Renderização de gráficos de nós ou navegadores web interativos (atendidos pelo `graph-ui`).
* Conversas multimodais (áudio/imagem) no terminal.

### Quatro Riscos do Produto

| Risco | Evidência / Premissa | Status | Consequência se Errado | Validação Mínima | Decisão Afetada |
|---|---|---|---|---|---|
| **Valor** | Operadores preferem chat simples e logs a menus numéricos e telas fixas. | `Supported` | O produto continua sendo considerado complexo demais para uso diário. | Sessão de teste exploratório com desenvolvedor em ambiente real. | Manter fluxo estritamente linear. |
| **Usabilidade** | A transição de chat para logs contínuos pode desorientar o usuário sem aviso visual claro. | `Unresolved` | Operador não percebe que o diálogo pausou e a execução iniciou. | Inserir divisor visual explícito de início de execução (`=== INÍCIO DA EXECUÇÃO ===`). | Formatação de transição de fase. |
| **Viabilidade** | Interceptar `Ctrl+C` sem abortar imediatamente processos filhos no Windows. `[Likely]` | `Supported` | Encerramento abrupto corrompe estado do SQLite WAL ou deixa processos zumbis. | Teste de cancelamento e pausa já validado em `s1-orchestrator.test.ts`. | Tratamento unificado de `SIGINT`. |
| **Viabilidade do Negócio** | Chat aberto na Etapa 1 pode gastar orçamento com conversas prolixas antes de codificar. | `Supported` | Custos operacionais elevados antes mesmo de gerar uma linha de código. | Incluir estimativa de custo antes da confirmação final do plano. | Bloqueio de execução sem teto de custo. |

### Decisões
* Decisão: Eliminar completamente a dependência de buffer alternativo (`TerminalTui`) no modo padrão.
* Decisão: A Etapa 1 é obrigatoriamente conversacional via `readline` padrão.
* Decisão: As Etapas 2+ são exclusivamente emissoras de logs estruturados no `stdout`.
* Decisão: O `Ctrl+C` durante as Etapas 2+ aciona a pausa sob demanda e reabre o prompt interativo.

---

## 2. Problemas

| ID | Problema (Situação Indesejável) | Ator Afetado | Consequência | Referência |
|---|---|---|---|---|
| `P01` | A interface em tela cheia sequestra o terminal, quebrando scrollback e atalhos habituais do desenvolvedor. | Operador | Frustração, perda de contexto e abandono da ferramenta. | `A1` |
| `P02` | Falta de diálogo prévio na Etapa 1 impede o operador de alinhar nuances antes do agente alterar código. | Operador | Soluções erradas geradas pelo Builder, desperdiçando tokens e tempo. | `A2` |
| `P03` | Saída contínua de ferramentas MCP e agentes gera poluição visual ilegível se não formatada. | Operador | Sobrecarga cognitiva ao tentar entender o progresso da execução. | `A3` |
| `P04` | Finalização forçada via `Ctrl+C` padrão mata o runtime sem liberar locks ou fechar o SQLite com segurança. | Operador | Travamento de execuções subsequentes por `cycle.lock` pendente. | `A4` |

---

## 3. Expectativas

| ID | Expectativa de Resultado | Problema Vinculado | Sinal Observável de Sucesso |
|---|---|---|---|
| `E01` | A ferramenta inicia no console padrão sem limpar a tela nem sequestrar o buffer. | `P01` | Mensagens do terminal anterior permanecem visíveis acima do prompt. |
| `E02` | A Etapa 1 opera como chat simples onde o operador refina o plano até dar o comando de execução. | `P02` | Agente ajusta proposta com base no feedback antes de acionar o Builder. |
| `E03` | As Etapas 2+ emitem logs compactos de 1 linha com timestamp e badge de subsistema. | `P03` | O terminal exibe progresso limpo sem despejos gigantes de JSON. |
| `E04` | O operador pode pausar a execução a qualquer momento via `Ctrl+C` e retomar o diálogo. | `P04` | Pressionar `Ctrl+C` congela agentes e abre prompt de decisão sem abortar o processo. |

---

## 4. Personas

### `PER1`: Desenvolvedor / Operador de IA
* **Papel:** Conduz tarefas de codificação, refatoração e correção de bugs com auxílio do CBM Orchestrator.
* **Objetivo:** Discutir a tarefa brevemente com o agente, autorizar a implementação e acompanhar o progresso sem atritos de interface.
* **Interações:** Digita instruções no chat, lê o plano proposto, autoriza a execução e intervém se notar desvio.
* **Problemas vinculados:** `P01`, `P02`, `P03`, `P04`.
* **Expectativas vinculadas:** `E01`, `E02`, `E03`, `E04`.

---

## 5. Funcionalidades

| ID | Nome da Funcionalidade | Personas | Expectativas | Descrição Resumida |
|---|---|---|---|---|
| `F01` | Chat Conversacional de Alinhamento (Etapa 1) | `PER1` | `E01`, `E02` | Loop de `readline` padrão no console para definição e ajuste do plano. |
| `F02` | Portão de Confirmação de Plano | `PER1` | `E02` | Barreira explícita que exige validação humana antes de disparar mutações. |
| `F03` | Emissor de Streaming de Logs Estruturados (Etapas 2+) | `PER1` | `E03` | Formatação compacta de eventos dos agentes (`[BUILDER]`, `[VERIFIER]`) no stdout. |
| `F04` | Pausa sob Demanda com Intervenção Conversacional | `PER1` | `E04` | Interceptação de `SIGINT` para pausar ciclo e reabrir prompt de orientação. |
| `F05` | Finalizador e Relatório de Resumo de Ciclo | `PER1` | `E01`, `E03` | Veredito final limpo contendo métricas, arquivos alterados e custo. |

---

## 6. Backlog do Produto (Product Backlog)

### `PBI-01`: Implementar Chat Conversacional para a Etapa 1 via `node:readline`
* **Funcionalidade:** `F01` | **Persona:** `PER1`
* **Resultado:** Operador inicia o orquestrador e entra diretamente em diálogo no console padrão para refinar o objetivo.
* **Critérios de Aceite:**
  1. Ao executar `cbm` ou `npm run start`, exibe cabeçalho conciso (projeto, modelo, teto de custo) e prompt `CBM> `.
  2. Suporta diálogo de múltiplas rodadas onde o agente propõe arquivos e abordagem.
  3. Não utiliza buffer alternativo ou comandos de limpar tela (`\x1b[2J`).
* **Dependências:** Nenhuma | **Prontidão:** `Ready for refinement` | **Premissas:** `A1`, `A2`

### `PBI-02`: Criar Portão de Confirmação e Gatilho de Transição para Execução
* **Funcionalidade:** `F02` | **Persona:** `PER1`
* **Resultado:** O agente só avança para o Builder quando o operador confirmar expressamente a proposta no chat.
* **Critérios de Aceite:**
  1. Ao apresentar o plano, o agente exibe: resumo de ações, alvos (arquivos) e comando para prosseguir.
  2. Reconhece confirmação via `s`, `sim`, `aprovar` ou tecla `Enter`.
  3. Se o operador digitar texto adicional, interpreta como instrução de correção (steer) e reapresenta o plano.
  4. Se o operador digitar `cancelar` ou `q`, encerra a sessão sem mutações.
* **Dependências:** `PBI-01` | **Prontidão:** `Ready for refinement` | **Premissas:** `A2`

### `PBI-03`: Implementar Formatação e Streaming de Logs para as Etapas 2+
* **Funcionalidade:** `F03` | **Persona:** `PER1`
* **Resultado:** Execução do Builder, Sealer e Verifier emite linhas cronológicas simples e legíveis diretamente no console.
* **Critérios de Aceite:**
  1. Exibe divisor visual de transição: `=== INÍCIO DA EXECUÇÃO (MODO LOG) ===`.
  2. Cada evento é impresso em linha única: `[HH:MM:SS] [SUBSISTEMA] Mensagem informativa`.
  3. Mensagens com JSON longo ou stacks são suprimidas do stdout padrão e gravadas em `.cbm-orchestrator/session.log`.
  4. Flag `--verbose` reativa a exibição completa se solicitada pelo operador.
* **Dependências:** `PBI-02` | **Prontidão:** `Ready for refinement` | **Premissas:** `A3`

### `PBI-04`: Interceptar `Ctrl+C` para Pausa sob Demanda e Retorno ao Chat
* **Funcionalidade:** `F04` | **Persona:** `PER1`
* **Resultado:** Operador pressiona `Ctrl+C` durante os logs e ganha a opção de redirecionar o agente ou abortar com segurança.
* **Critérios de Aceite:**
  1. Durante a emissão de logs, o primeiro `Ctrl+C` pausa os subprocessos sem encerrar a aplicação.
  2. Imprime: `[PAUSA] Execução congelada. Digite uma correção ou [1] Continuar, [2] Abortar: `.
  3. Se o operador fornecer uma instrução textual, ela é injetada no contexto do Builder para a próxima iteração.
  4. Um segundo `Ctrl+C` imediato realiza cancelamento forçado, libera locks e sai com código `130`.
* **Dependências:** `PBI-03` | **Prontidão:** `Ready for refinement` | **Premissas:** `A4`

### `PBI-05`: Emissão do Veredito e Resumo Operacional de Conclusão
* **Funcionalidade:** `F05` | **Persona:** `PER1`
* **Resultado:** Ao término do ciclo, o operador visualiza o resultado final consolidado.
* **Critérios de Aceite:**
  1. Em caso de sucesso: exibe banner verde, lista de arquivos alterados, tempo de execução e custo total em USD.
  2. Em caso de recusa/falha: exibe justificativa do Adversarial/Verifier e comando sugerido para depuração.
  3. Processo encerra retornando o cursor ao prompt do sistema hospedeiro.
* **Dependências:** `PBI-03` | **Prontidão:** `Ready for refinement` | **Premissas:** `A1`

---

## 7. Matriz de Rastreabilidade

| Problema | Expectativa | Persona | Funcionalidade | PBI |
|---|---|---|---|---|
| `P01` | `E01` | `PER1` | `F01` | `PBI-01` |
| `P02` | `E02` | `PER1` | `F01`, `F02` | `PBI-01`, `PBI-02` |
| `P03` | `E03` | `PER1` | `F03`, `F05` | `PBI-03`, `PBI-05` |
| `P04` | `E04` | `PER1` | `F04` | `PBI-04` |

---

## 8. Premissas e Evidências

| ID | Tag | Declaração | Proveniência | Status da Evidência | Método de Confirmação | PBIs Dependentes |
|---|---|---|---|---|---|---|
| `A1` | `[Certain]` | Terminais padrão suportam `node:readline` sem dependências nativas C++ ou ConPTY complexo. | Documentação do Node.js | Suportada | Execução direta de script `readline` no CMD. | `PBI-01`, `PBI-05` |
| `A2` | `[Certain]` | O motor já possui a lógica de aprovação `AWAITING_PLANNING` no Stage 1 (`s1-orchestrator.ts`). | Código existente | Suportada | Testes unitários `s1-orchestrator.test.ts`. | `PBI-01`, `PBI-02` |
| `A3` | `[Likely]` | Logs de 1 linha com badge de subsistema reduzem a fadiga cognitiva do operador. | Práticas padrão de CLI | Suportada | Revisão do formato de saída dos logs. | `PBI-03` |
| `A4` | `[Certain]` | A infraestrutura do journal SQLite WAL suporta cancelamento e retenção de estado sob `SIGINT`. | Código existente | Suportada | Testes de concorrência em `journal.test.ts`. | `PBI-04` |

---

## 9. Perguntas Abertas

| ID | Pergunta | Decisão Afetada | Resposta Sugerida | Racional | Status |
|---|---|---|---|---|---|
| `Q01` | O comando de inicialização padrão na raiz do repositório deve ser `npm start` ou script `cbm`? | Empacotamento / Deploy | `npm start` que invoca `cbm` internamente | Garante execução imediata sem exigir instalação global prévia. | `Provisional` |
| `Q02` | No modo de logs, devemos exibir a contagem de tokens consumidos a cada linha de log ou apenas no sumário final? | `PBI-03` | Apenas nos marcos de fase e no sumário final | Evita poluição visual durante o streaming de ações do agente. | `Resolved` |
