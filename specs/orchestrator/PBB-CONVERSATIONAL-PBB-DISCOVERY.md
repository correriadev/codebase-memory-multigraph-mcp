# PBB — Descoberta Conversacional Contínua (PBB Gate) e Observabilidade Verbose

Documento canônico de Product Backlog Building (PBB) definindo o fluxo de descoberta conversacional obrigatória e os níveis de observabilidade detalhada para a versão Alpha do CBM Orchestrator.

- **Data:** 2026-10-28
- **Versão:** 1.0 (Alpha Discovery Gate & Verbose Logging)
- **Domínio:** `orchestrator`
- **Contexto:** CBM System Intelligence Engine / Alpha CLI Interaction
- **Referência:** PBB Design Protocol, Codex CLI Multi-turn Session, Observabilidade Gradual (`--verbose`)

---

### Avaliação Inicial do Produto (Advisory Posture)

A execução autônoma direta a partir de comandos em texto livre na CLI sem diálogo de alinhamento provoca falhas silenciosas e desperdício de tokens, como demonstrado na recusa opaca de objetivos com alvos desalinhados (ex.: "updates docs folder" contra um target rígido em `cli.ts`). `[Certain]`
A exigência de que **toda interação na versão Alpha** passe por uma fase conversacional de descoberta (PBB Gate) assegura que o modelo e o operador convirjam formalmente em objetivos, arquivos afetados e critérios de aceite antes de qualquer mutação de código. `[Certain]`
Risco residual aceito: o operador assume a latência e o consumo acumulado de tokens de uma sessão multi-turno preparatória mesmo para tarefas curtas na versão Alpha, em troca de precisão e rastreabilidade total. `[Certain]`

---

## 1. Produto

* **Nome:** `cbm-discovery-verbose` — Discovery Conversacional Contínuo com PBB Gate e Observabilidade Multinível.
* **Intenção:** Prover uma experiência de alinhamento conversacional guiado na CLI antes de qualquer execução de código. Toda solicitação do operador na versão Alpha abre um diálogo socrático contínuo com o modelo (Codex CLI), que esclarece o problema, delimita os alvos no repositório e gera um resumo estruturado de especificação (PBB). Uma vez aprovado pelo operador, o ciclo autônomo é acionado. Adicionalmente, níveis configuráveis de verbosidade (`--verbose low|medium|high`) garantem transparência sobre chamadas MCP, eventos internos e diagnósticos de erro.
* **Restrições:**
  * Toda interação na versão Alpha deve passar obrigatoriamente pelo discovery conversacional antes de tocar arquivos.
  * O encadeamento de turnos não pode quebrar o isolamento de segurança dos processos nem vazar credenciais MCP.
  * O modo silencioso (`--verbose none` ou padrão limpo) deve preservar a legibilidade do console; níveis mais altos (`low`, `medium`, `high`) devem canalizar detalhes no stderr ou em linhas demarcadas.
* **Resultado Pretendido:** Operador e agente compreendem mutuamente a mudança antes do primeiro arquivo ser alterado, e qualquer falha no pipeline é acompanhada de causa-raiz imediata e inteligível.
* **Sinal de Sucesso:** 100% dos ciclos na versão Alpha iniciam com alvos confirmados via PBB Gate; falhas silenciosas são eliminadas por completo.
* **Recomendação:** Implementar na CLI Alpha como etapa mandatória pré-execução, acompanhada da flag `--verbose`.

### Limites do Escopo

#### In Scope
* Sessão conversacional multi-turno contínua no Alpha Chat (`interactive.ts`) preservando o contexto entre turnos com o Codex CLI.
* Construtor de especificação PBB interativo: diálogo gera problema, expectativa, arquivos alvos e critérios de aceite.
* PBB Gate formal: confirmação explícita do operador sobre a especificação antes de despachar a Etapa 1/2 de código.
* Flag de linha de comando `--verbose [low|medium|high]` e alias `-v [low|medium|high]` (com `medium` como padrão se a flag for passada sem valor).
* Detecção e exibição imediata da causa-raiz de erros e exceções (ex.: targets desalinhados, recusa de schema, crash de processo).
* Níveis de observabilidade bem delimitados:
  * `low`: sumário operacional + motivo explícito e sanitizado de recusa/erro.
  * `medium`: eventos de transição de fases + chamadas a ferramentas MCP intermediadas + badges do operador.
  * `high`: stream bruto JSONL do subprocesso runner + payloads de tokens + tracebacks completos.

#### Out of Scope
* Ignorar o PBB Gate para execuções em modo Alpha (decisão do operador de manter 100% de cobertura).
* Classificadores neurais remotos para orquestração de logs.
* Interfaces gráficas web ou janelas ConPTY full-screen.

### Quatro Riscos do Produto

| Risco | Evidência / Premissa | Status | Consequência se Errado | Validação Mínima | Decisão Afetada |
|---|---|---|---|---|---|
| **Valor** | Operadores preferem alinhar o plano via chat antes de disparar código cego. | `Supported` | O usuário sente que a ferramenta "pensa antes de agir", evitando retrabalho. | Ciclo completo guiado validando "updates docs folder" com sucesso. | Instituir PBB Gate como padrão no Alpha. |
| **Usabilidade** | A obrigação de dialogar antes de cada comando pode cansar em tarefas triviais. | `Unresolved` | Operador abandona a CLI por excesso de burocracia para tarefas de 1 linha. | Medir turnos médios até a convergência do PBB. | Decisão do operador mantida para versão Alpha. |
| **Viabilidade** | Manter o estado da sessão viva no Codex CLI via subprocesso no Windows. `[Likely]` | `Supported` | Perda de contexto entre perguntas do operador ou vazamento de subprocessos. | Testes de integração de turnos encadeados no runner. | Uso de `thread_id` ou histórico cumulativo no runner. |
| **Viabilidade do Negócio** | Diálogos longos acumulam tokens no contexto do modelo (`gpt-6.1-sol`). `[Likely]` | `Supported` | Custo por objetivo aumenta substancialmente se a conversa for prolixa. | Exibição de custo parcial acumulado a cada turno da conversa. | Telemetria de orçamento ativa no chat. |

### Decisões
* Decisão: **Toda** interação na versão Alpha requer a convergência e aprovação de um PBB antes do código ser executado.
* Decisão: Implementar a flag `--verbose <low|medium|high>` (e `-v`) para calibrar o nível de telemetria emitido no console.
* Decisão: Falhas em qualquer fase do ciclo devem expor sua causa-raiz exata no sumário, sem depender de inspeção manual de arquivos no SQLite/evidence.

---

## 2. Problemas

* **P1 — Execução Cega Sem Alinhamento:** O CBM despacha o Builder diretamente a partir de um comando texto bruto; se o objetivo não condizer com os alvos pré-configurados (ex: `knowledge.targets`), o modelo gera payload incompatível ou vazio, culminando em `❌ RECUSADO`.
* **P2 — Opacidade e Falhas Silenciosas:** Quando uma execução falha ou é recusada, a CLI emite apenas status genérico sem detalhar a exceção subjacente (ex.: targets não contemplados, erro de validação de schema), forçando o usuário a garimpar arquivos JSON de evidência.
* **P3 — Perda de Continuidade Conversacional:** O runner executa comandos de forma efêmera e pontual, descartando o raciocínio anterior e impedindo que o operador explique ou refine o que deseja em múltiplos turnos.

---

## 3. Expectativas

* **E1 — Diálogo de Descoberta Pré-Execução:** O operador conversa com o CBM no terminal para clarificar objetivos, escopo e arquivos afetados até a formalização de um mini-PBB.
* **E2 — Transparência Operacional Configurável:** O operador escolhe o grau de detalhe dos logs (`--verbose low|medium|high`) e recebe diagnósticos claros em caso de erro.
* **E3 — Sessão Contínua de Raciocínio:** O CBM preserva o histórico de diálogo durante a sessão, permitindo refinar instruções iterativamente antes da aprovação final.

---

## 4. Personas

* **PER1 — Engenheiro de Software / Operador CBM:** Utiliza a CLI no terminal para planejar e delegar alterações complexas no repositório. Deseja visibilidade total do que o modelo compreendeu e controle estrito sobre quais arquivos serão alterados antes de qualquer mutação.

---

## 5. Funcionalidades

* **F1 — Conversational Discovery Engine (PBB Gate):** Conduz o diálogo socrático no Alpha Chat para extrair problema, alvos e critérios de aceite, compilando uma proposta estruturada de PBB para aprovação do operador.
* **F2 — Observabilidade Multinível (`--verbose`):** Roteia e filtra mensagens de log, chamadas MCP e stream bruto de acordo com o nível solicitado (`low`, `medium`, `high`).
* **F3 — Diagnóstico Ativo de Erros:** Captura e sintetiza exceções e recusas em qualquer etapa do ciclo, exibindo o motivo real de falha no console.

---

## 6. Product Backlog

### PBI-01: Suporte a flags `--verbose` e `-v` com níveis `low`, `medium` e `high`
- **Path:** F2 → PER1 → P2 → E2
- **Outcome:** Como operador, posso passar `--verbose [low|medium|high]` (ou `-v`) na CLI para visualizar o nível desejado de telemetria durante a sessão.
- **Acceptance Criteria:**
  - `cbm --verbose low` exibe sumário operacional e causa-raiz de erros.
  - `cbm --verbose medium` (ou `--verbose` sem valor) adiciona eventos de transição de fase, badges de subsistema e chamadas MCP filtradas.
  - `cbm --verbose high` adiciona o stream bruto JSONL do runner, payloads e tracebacks.
  - Se `--verbose` não for fornecido, a CLI mantém o modo silencioso/limpo padrão.
- **Dependencies:** Nenhuma.
- **Readiness:** `Ready for refinement`
- **Assumptions & Questions:** A1, A2.

### PBI-02: Diagnóstico e exposição explícita da causa-raiz de recusa no resumo
- **Path:** F3 → PER1 → P2 → E2
- **Outcome:** Como operador, quando um ciclo for recusado ou falhar, quero ver a mensagem explicativa da causa-raiz no sumário de conclusão.
- **Acceptance Criteria:**
  - Resumo de conclusão exibe `Motivo da Recusa:` com a mensagem clara do erro (ex.: "Builder retornou artefato vazio" ou "Arquivos fora do escopo alocado").
  - Em modo `--verbose low` ou superior, exibe o arquivo de evidência correspondente para auditoria.
  - Nunca encerra com `❌ RECUSADO` sem indicar a causa.
- **Dependencies:** PBI-01.
- **Readiness:** `Ready for refinement`
- **Assumptions & Questions:** A2.

### PBI-03: Sessão multi-turno contínua no Alpha Chat com o modelo
- **Path:** F1 → PER1 → P3 → E3
- **Outcome:** Como operador, quero conversar com o modelo em turnos sucessivos no prompt `CBM>`, preservando o contexto da conversa.
- **Acceptance Criteria:**
  - O prompt `CBM>` mantém histórico acumulado de mensagens ou identificador de thread ativo entre turnos.
  - Respostas do modelo são exibidas como diálogo conversacional e não como código cru em JSON.
  - O operador pode enviar correções sucessivas até convergir no escopo desejado.
- **Dependencies:** PBI-01.
- **Readiness:** `Ready for refinement`
- **Assumptions & Questions:** A1, V1.

### PBI-04: Portão formal de aprovação do PBB (PBB Gate) pré-execução
- **Path:** F1 → PER1 → P1 → E1
- **Outcome:** Como operador, quero que a conversa convirja em uma especificação PBB consolidada (objetivo, alvos e critérios) que exige minha aprovação explícita antes de tocar código.
- **Acceptance Criteria:**
  - Toda interação gera um resumo de PBB: Problema, Arquivos Alvos (`targets`) e Critérios de Aceite.
  - O sistema pergunta explicitamente: `Confirmar e executar especificação PBB? [S/n/c]`.
  - Somente após `S`, os alvos são configurados dinamicamente e o ciclo autônomo (Builder ➔ Sealer ➔ Verifier) é acionado.
  - Opção `c` permite continuar dialogando para ajustar o PBB; opção `n` cancela sem efeitos no disco.
- **Dependencies:** PBI-02, PBI-03.
- **Readiness:** `Ready for refinement`
- **Assumptions & Questions:** A1, V1.

---

## 7. Matriz de Rastreabilidade

| Problema | Expectativa | Persona | Funcionalidade | PBI |
|---|---|---|---|---|
| **P2** (Opacidade e Falhas Silenciosas) | **E2** (Transparência Operacional) | **PER1** (Operador) | **F2** (Observabilidade) | `PBI-01` |
| **P2** (Opacidade e Falhas Silenciosas) | **E2** (Transparência Operacional) | **PER1** (Operador) | **F3** (Diagnóstico) | `PBI-02` |
| **P3** (Perda de Continuidade) | **E3** (Sessão Contínua) | **PER1** (Operador) | **F1** (Discovery Engine) | `PBI-03` |
| **P1** (Execução Cega) | **E1** (Diálogo de Descoberta) | **PER1** (Operador) | **F1** (Discovery Engine) | `PBI-04` |

---

## 8. Premissas e Evidências

* **A1 `[Certain]`:** O Codex CLI suporta encadeamento de mensagens e sessões via opções nativas ou preservação de histórico em memória do processo Node.js.
  * *Proveniência:* Documentação do Codex CLI e testes de subprocesso já validados.
  * *Verificação:* Validar que passar turnos preserva a memória do diálogo no runner.
* **A2 `[Certain]`:** O orquestrador já possui todas as informações de erro e telemetria capturadas no objeto `runResult` e nos arquivos de evidência SQLite/JSON.
  * *Proveniência:* Inspeção em [`orchestrator.ts`](file:///c:/Users/User/Documents/codebase-memory-multigraph-mcp/packages/cbm-orchestrator/src/orchestrator.ts) e [`run-1793239314631-703.json`](file:///c:/Users/User/Documents/codebase-memory-multigraph-mcp/.cbm-orchestrator/run-1793239314631-703.json).
  * *Verificação:* Expor a propriedade `terminationDetail` / `failureReason` diretamente no sumário de `interactive.ts`.
* **V1 `[VERIFY]`:** Consumo de tokens em diálogos multi-turno preparatórios não excederá a política de reserva do ciclo configurada no `cbm.config.json`.
  * *Proveniência:* Hipótese de uso pelo operador na versão Alpha.
  * *Verificação:* Medir tokens médios por turno na Etapa 1 e emitir alerta quando atingir 50% da reserva de planejamento.

---

## 9. Questões em Aberto

* **Q1:** Como persistir o resumo do mini-PBB gerado na conversa?
  * *Decisão afetada:* `PBI-04`.
  * *Resposta sugerida:* Salvar o mini-PBB aprovado em `.cbm-orchestrator/runs/<runId>-pbb.json` e registrar o digest no Journal SQLite.
  * *Status:* `Provisional`.
* **Q2:** A flag `--verbose` deve ser reconfigurável dinamicamente durante a sessão interativa?
  * *Decisão afetada:* `PBI-01`.
  * *Resposta sugerida:* Sim, aceitar comandos como `:verbose low|medium|high` diretamente no prompt `CBM>`.
  * *Status:* `Provisional`.

---
