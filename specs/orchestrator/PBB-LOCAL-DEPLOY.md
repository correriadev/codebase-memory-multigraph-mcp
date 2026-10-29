# PBB — Build e Deploy Local do CBM Orchestrator

Documento canônico de Product Backlog Building (PBB) definindo a estratégia de compilação/validação e deploy local para tornar o CBM Orchestrator acessível globalmente no terminal (`cbm`) sem fricção operacional.

- **Data:** 2026-10-28
- **Versão:** 1.0 (Deploy Local Alpha)
- **Domínio:** `orchestrator`
- **Contexto:** Facilitação de uso e distribuição local no ambiente do operador
- **Referência:** Práticas de CLI UNIX/Windows, launchers em PATH de usuário, Node.js 24 runtime

---

### Avaliação Inicial do Produto (Advisory Posture)

Discordo de criar um processo pesado de empacotamento em binário estático (compilador nativo, `pkg` ou instalador MSI) neste momento. `[Certain]`
Motivo: O CBM Orchestrator está em fase Alpha de iteração intensiva. Binários estáticos tornam o ciclo de feedback lento, adicionam dependências de compilação desnecessárias e dificultam inspeção e hotfixes. `[Certain]`
Minha proposta: Deploy local com shim executável (`cbm.cmd` e `cbm` shell) instalado diretamente no diretório já presente no PATH do usuário (`$env:LOCALAPPDATA\Programs\codebase-memory-mcp`), com suporte a invocação global `cbm` com fallback inteligente de configuração. `[Likely]`
Risco na sua abordagem: Criar pipeline de release prematuro antes do orquestrador estabilizar seu núcleo de execução, desviando foco de produto para empacotamento. `[Certain]`

---

## 1. Produto

* **Nome:** `cbm-local-deploy` — Build e Deploy Local do CBM Orchestrator.
* **Intenção:** Tornar o CBM Orchestrator executável a partir de qualquer terminal e diretório de trabalho através do comando `cbm`, sem que o operador precise digitar caminhos longos de scripts internos ou navegar até a pasta do pacote.
* **Restrições:**
  * Não exigir privilégios de Administrador no Windows para instalação/deploy. `[Certain]`
  * Manter compatibilidade com PowerShell, CMD e Bash no Windows. `[Certain]`
  * Depender estritamente de Node.js >= 24 já instalado na máquina. `[Certain]`
  * Instalar shims no diretório de programas do usuário que já consta no PATH. `[Certain]`
* **Resultado Pretendido:** Operador digita `cbm` em qualquer diretório de projeto e entra imediatamente no chat Alpha ou dispara comandos diretos (`cbm probe`, `cbm run`).
* **Sinal de Sucesso:** Comando único `npm run deploy:local` instala os wrappers e a execução de `cbm` em novo terminal responde em < 500ms com prompt `CBM>`.
* **Recomendação:** Seguir com a implementação do menor slice defensável: launcher em PATH de usuário + validação de configuração.

### Limites do Escopo

#### In Scope
* Definição de entrada de binário (`bin`) no `package.json` de `packages/cbm-orchestrator`.
* Script de build/validação (`npm run build`) para checagem estática de tipos e integridade do entrypoint.
* Script de deploy local idempotente (`scripts/deploy-local.ps1` ou comando `npm run deploy:local`) que cria shims `cbm.cmd` e `cbm` apontando para o entrypoint.
* Resolução hierárquica de configuração (`cbm.config.json` no cwd -> `~/.cbm/config.json` -> defaults seguros com aviso).
* Diagnóstico amigável de pré-requisitos (Node.js >= 24 e presença do daemon `codebase-memory-mcp` no PATH).
* Suporte a execução padrão sem subcomando iniciando o modo interativo (`cbm` = `cbm interactive`).

#### Out of Scope
* Publicação do pacote no registro público npm (`registry.npmjs.org`).
* Empacotamento em executável único autônomo (ex: `pkg`, `bun compile` ou `nexe`).
* Gerador de instalador gráfico Windows (MSI, InnoSetup ou WiX Toolset).
* Instalação global via privilégios de Administrador (`C:\Program Files`).
* Configuração de serviço em segundo plano (Windows Service / systemd).

### Quatro Riscos do Produto

| Risco | Evidência / Premissa | Status | Consequência se Errado | Validação Mínima | Decisão Afetada |
|---|---|---|---|---|---|
| **Value** | Operador reportou atrito e abandono de teste exploratório devido à complexidade de linha de comando. `[Certain]` | Supported | Ferramenta desenvolvida não é utilizada na rotina diária de desenvolvimento. | Teste de uso real executando `cbm` de um diretório externo. | PBI-01, PBI-02 |
| **Usability** | Terminal de usuário pode não enxergar novos shims se o PATH não for atualizado ou se houver conflito de nomes. `[Likely]` | Supported | Operador recebe erro de `'cbm' não é reconhecido como um comando interno`. | Script de deploy valida presença da pasta no PATH do processo e do usuário. | PBI-02, PBI-04 |
| **Feasibility** | Node.js 24 suporta execução de TypeScript; shims `.cmd` no Windows executam `node` com argumentos preservados (`%*`). `[Certain]` | Supported | Falha de execução de scripts TypeScript sem transpilação prévia. | Teste de fumaça chamando `cbm.cmd` em CMD puro. | PBI-01, PBI-02 |
| **Business Viability** | Deploy local via scripts internos tem custo zero de infraestrutura e manutenção mínima. `[Certain]` | Supported | Desperdício de tempo mantendo infraestrutura de CI/CD para binários descartáveis. | Manter abordagem de shims leves sem dependências externas. | Todo o escopo |

### Decisões Explícitas
* **D1:** O comando global será denominado `cbm`. `[Certain]`
* **D2:** A instalação de usuário utilizará o diretório já existente `$env:LOCALAPPDATA\Programs\codebase-memory-mcp`, que já está garantidamente no PATH do operador. `[Certain]`
* **D3:** A invocação de `cbm` sem argumentos despachará automaticamente para o modo `interactive` (Alpha Chat). `[Certain]`

---

## 2. Problemas

* **P1 (Invocação Longa e Frágil):** O operador precisa digitar `node packages/cbm-orchestrator/src/cli.ts interactive ...`, comando com mais de 70 caracteres, sujeito a erros de digitação e esquecimento de flags. `[Certain]`
* **P2 (Dependência de Diretório Fixo):** Executar fora da raiz do repositório falha porque os caminhos relativos de configuração e módulos quebram. `[Certain]`
* **P3 (Barreira de Adoção para Teste Exploratório):** Usuário não consegue realizar testes rápidos de uso real em outros projetos locais sem criar scripts manuais ad-hoc. `[Likely]`

---

## 3. Expectativas

* **E1 (Comando Curto Ubíquo):** O operador digita `cbm` no terminal e inicia a sessão interativa Alpha imediatamente em qualquer diretório. (Ref: `P1`, `P3`) `[Certain]`
* **E2 (Instalação em Comando Único):** O desenvolvedor executa `npm run deploy:local` na raiz e o ambiente fica pronto para uso sem reiniciar o sistema. (Ref: `P1`, `P2`) `[Certain]`
* **E3 (Resolução Autônoma de Contexto):** O orquestrador detecta o `cbm.config.json` do diretório de trabalho atual ou utiliza uma configuração mínima padrão sem abortar por erro fatal. (Ref: `P2`) `[Likely]`
* **E4 (Feedback de Pré-requisitos Claro):** Se o Node.js for incompatível ou o daemon do CBM estiver ausente, a CLI exibe mensagem corretiva com instruções claras. (Ref: `P3`) `[Certain]`

---

## 4. Personas

* **PER1 (Desenvolvedor / Operador Local):**
  * **Papel:** Engenheiro de software utilizando o CBM para navegar, auditar e alterar bases de código locais.
  * **Objetivo:** Iniciar e interagir com o agente rapidamente a partir de qualquer pasta de projeto com o mínimo de digitação.
  * **Interações:** Executa `cbm` no terminal, responde a propostas no chat Alpha e monitora logs.
  * **Problemas Vinculados:** `P1`, `P2`, `P3`.
  * **Expectativas Vinculadas:** `E1`, `E2`, `E3`, `E4`.

---

## 5. Funcionalidades

* **F1 (Entrypoint e Launcher Global):** Manifesto `bin` e shims multiplataforma (`cbm.cmd` e `cbm`) com resolução de caminhos absolutos para o pacote orquestrador. (Serve: `PER1`, Ref: `E1`)
* **F2 (Automação de Deploy Local):** Script de instalação/link local que valida o PATH e instala os shims no diretório do usuário de forma idempotente. (Serve: `PER1`, Ref: `E2`)
* **F3 (Auto-descoberta e Fallback de Configuração):** Mecanismo de busca de configuração com fallback contextual para permitir execução em qualquer pasta. (Serve: `PER1`, Ref: `E3`)
* **F4 (Guarda de Integridade de Ambiente):** Verificador de ambiente de execução (versão Node.js >= 24 e integridade do binário CBM no PATH). (Serve: `PER1`, Ref: `E4`)

---

## 6. Product Backlog

| ID | Título | Funcionalidade | Persona | Resultado Entregue | Critérios de Aceite | Dependências | Prontidão |
|---|---|---|---|---|---|---|---|
| **PBI-01** | Entrypoint Executável Global (`cbm`) | F1 | PER1 | Permite executar a CLI via alias curto com despacho padrão para modo interativo. | 1. Invocação de `cbm` sem argumentos equivale a `cbm interactive`.<br>2. Invocação de `cbm --help` ou `cbm probe` encaminha argumentos corretamente.<br>3. Shebang e flags de execução TypeScript respeitadas no Node 24. | Nenhuma | Ready for refinement |
| **PBI-02** | Script Idempotente de Deploy Local | F2 | PER1 | Instala ou atualiza o comando `cbm` no ambiente do usuário sem precisar de admin. | 1. Script `deploy-local.ps1` cria shims `cbm.cmd` e `cbm` em `$env:LOCALAPPDATA\Programs\codebase-memory-mcp`.<br>2. Não requer elevação de privilégios de Administrador.<br>3. Execuções repetidas sobrescrevem com segurança de forma atômica. | PBI-01 | Ready for refinement |
| **PBI-03** | Auto-descoberta Hierárquica de Configuração | F3 | PER1 | Executa com sucesso a partir de qualquer pasta de trabalho sem forçar argumento de config. | 1. Procura `cbm.config.json` no diretório atual de trabalho (`process.cwd()`).<br>2. Se não encontrar, busca em `$HOME/.cbm/cbm.config.json`.<br>3. Se ausente, usa template padrão de sandbox/exploratório alertando o operador. | PBI-01 | Ready for refinement |
| **PBI-04** | Validador e Diagnóstico de Ambiente | F4 | PER1 | Emite diagnósticos acionáveis quando faltam requisitos básicos de runtime. | 1. Se `node -v` for inferior a 24, emite erro explicativo e sai com código 1.<br>2. Se `codebase-memory-mcp` não estiver no PATH, exibe aviso indicando execução do instalador do daemon. | PBI-01 | Ready for refinement |

---

## 7. Matriz de Rastreabilidade

| Problema | Expectativa | Persona | Funcionalidade | PBI |
|---|---|---|---|---|
| `P1` | `E1` | `PER1` | `F1` | `PBI-01` |
| `P2` | `E2` | `PER1` | `F2` | `PBI-02` |
| `P2` | `E3` | `PER1` | `F3` | `PBI-03` |
| `P3` | `E4` | `PER1` | `F4` | `PBI-04` |
| `P3` | `E1` | `PER1` | `F1` | `PBI-01` |

---

## 8. Premissas e Evidências

* **A1:** `[Certain]` Node.js v24.0.0+ está instalado e disponível no ambiente local do operador.
* **A2:** `[Certain]` O diretório `C:\Users\User\AppData\Local\Programs\codebase-memory-mcp` existe e já está configurado na variável de ambiente `PATH` do usuário.
* **A3:** `[Likely]` Criar um arquivo `.cmd` executável no diretório de programas do usuário não é bloqueado pelo Windows Defender em modo de desenvolvimento.
* **A4:** `[VERIFY]` Comportamento do `process.cwd()` ao executar `cbm` em repositório externo com múltiplos projetos no grafo CBM:
  * *Proveniência:* Hipótese de uso fora do repositório `codebase-memory-multigraph-mcp`.
  * *Status:* Não verificado em ambiente externo ao monorepo.
  * *Confirmação:* Executar teste em diretório temporário fora do workspace CBM.
  * *Decisão afetada:* `PBI-03`.

---

## 9. Questões Abertas

* **Q1:** Devemos fornecer também um atalho para bash (`cbm` sem extensão para WSL / Git Bash)?
  * *Sugestão:* Sim, gerar tanto `cbm.cmd` quanto `cbm` (shell script) no mesmo diretório de deploy.
  * *Status:* Resolvido (incorporado ao PBI-02).
* **Q2:** Deve haver comando de desinstalação (`cbm-uninstall` ou `npm run undeploy:local`)?
  * *Sugestão:* Não incluir agora para manter o slice mínimo. Bastará apagar os dois arquivos criados se desejado.
  * *Status:* Resolvido.
