# PBB — Descoberta Dinâmica de Provedores, Modelos e Esforços de Raciocínio (CBM Orchestrator)

Documento canônico de Product Backlog Building (PBB) revisado e refinado após auditoria crítica de arquitetura (Codex `gpt-6.1-sol` e validação cruzada).

- **Data:** 2026-10-26
- **Versão:** 2.0 (Pós-auditoria)
- **Contexto:** CBM System Intelligence Engine / Interactive CLI v4
- **Referência Arquitetural:** Padrão de Provedores do `open-design` (`apps/daemon/src/runtimes/defs/`) e Contrato Polimórfico do CBM Orchestrator

---

## 1. Product

**Nome:** `cbm-provider-discovery` — Módulo de Introspecção, Descoberta Dinâmica e Calibração de Provedores, Modelos e Raciocínio.

**Intenção:** Prover ao CBM Orchestrator um mecanismo dinâmico, resiliente e transparente de introspecção dos provedores locais (**OpenAI Codex**, **Google Antigravity**, **OpenCode** e **Claude Code**). O módulo detecta a disponibilidade dos executáveis por máquina de estados, descobre os catálogos reais de modelos em tempo de execução via adaptadores específicos com degradação graciosa, normaliza os parâmetros de esforço de raciocínio e persiste o estado em cache particionado com TTL dentro do `stateDirectory` do projeto, fornecendo uma tupla atômica e validada `(provider, model, effort)` para o CLI interativo e para os runners do Outer Loop.

**Hierarquia de Degradação Graciosa:**
1. *Descoberta Viva:* Probe nativo do CLI com timeout unificado (5s para Codex, Antigravity e Claude; 10s para OpenCode).
2. *Cache Válido Anterior:* Leitura do cache particionado em disco caso o probe falhe ou esteja offline.
3. *Catálogo de Emergência:* Lista estática mínima curada por provedor para não interromper a navegação.
4. *Entrada Customizada Livre:* Digitação direta sanitizada com regex rigorosa contra injeção de shell.

**Máquina de Estados de Prontidão do Provedor:**
- `NOT_INSTALLED`: Executável não localizado no PATH nem nas pastas conhecidas do sistema.
- `INCOMPATIBLE`: Executável localizado, mas versão do CLI é inferior aos requisitos mínimos.
- `UNAUTHENTICATED`: Executável funcional, porém sessão de login expirada ou ausente.
- `CATALOG_AVAILABLE`: Modelos descobertos com sucesso via CLI ou recuperados de cache válido.
- `ACCESS_UNVERIFIED`: Catálogo visível, mas permissão da conta e quota de tokens só são validadas na execução efetiva.

**Limites do Produto:**
- **Dentro do escopo:**
  - Descoberta e classificação do estado dos executáveis locais (`codex`, `agy`, `opencode`, `claude`).
  - Adaptadores dedicados de catálogo e variantes:
    - **OpenAI Codex:** `codex debug models` (JSON estruturado com `models`, `supported_reasoning_levels`, `default_reasoning_level`) e fallback para leitura não bloqueante de `~/.codex/config.toml`.
    - **Google Antigravity:** `agy models` (TSV com slug e label amigável) e normalização de flag `--effort`.
    - **OpenCode:** `opencode models --verbose` (parser multiline para blocos JSON intercalados com `variants`) e fallback tabular simples.
    - **Claude Code:** Catálogo curado de modelos/aliases (`sonnet`, `opus`, `haiku`, `claude-sonnet-4-5`, etc.) com checagem de versão e suporte nativo a `--model` e `--effort`.
  - Normalização sem escalonamento forçado de esforço de raciocínio (suporte a omitido/default do CLI, `none` explícito e níveis validados).
  - Seleção atômica de tupla `(provider, model, effort)` no menu interativo (trocar provedor redefine modelo e esforço para valores válidos).
  - Cache particionado por provedor salvo em `<stateDirectory>/models-cache.json` com escrita atômica e TTL de 24 horas.
  - Refresh assíncrono e sob demanda `[R]` com cancelamento e feedback visual no terminal.
  - Sanitização de entrada livre (`sanitizeCustomModel`) com regex `/^[A-Za-z0-9][A-Za-z0-9._/:@-]*$/`.
- **Fora do escopo:**
  - Gerenciamento de credenciais e fluxo interativo de login (delegado a `codex login`, `agy auth`, `opencode auth`, `claude auth login`).
  - Implementação completa dos novos executores de processo filho para Antigravity, OpenCode e Claude (o PBB entrega a tupla configurada e a interface do catálogo; a refatoração polimórfica de `AgentRunner` segue em ADR/PBI técnico dedicado).
  - Bypass de segurança ou permissões não autorizadas (`bypassPermissions` / `--dangerously-skip-permissions` não pertencem à descoberta).

---

## 2. Problems

| ID | Problema (situação indesejável comprovada) |
| --- | --- |
| P01 | O menu interativo exibe uma lista fixa e desatualizada de modelos; modelos novos disponibilizados nas contas dos provedores (ex: `gpt-6.1-sol`, `gemini-4-argon`, `claude-opus-5-5`, `qwen3.8-flash`) não aparecem para seleção sem alteração manual no código-fonte. |
| P02 | O operador pode selecionar provedores não instalados ou modelos indisponíveis, pois a interface atual não possui máquina de estados para verificar prontidão (`installed`, `authenticated`, `catalog_available`), gerando falhas tardias no início do ciclo. |
| P03 | Cada um dos quatro provedores possui convenções e interfaces de listagem totalmente distintas (`codex debug models` em JSON, `agy models` em TSV, `opencode models --verbose` em JSON multiline intercalado, `claude` sem subcomando de listagem), impedindo o consumo uniforme pelo orquestrador. |
| P04 | A execução síncrona e repetida de comandos CLI externos na abertura do menu de terminal gera latência de rede perceptível, bloqueando o operador caso a conexão esteja lenta ou instável. |
| P05 | A configuração de esforço de raciocínio varia entre provedores; tentar mapear valores arbitrários pode forçar um modelo de baixa latência ao nível máximo, inflando custos financeiros e tempos de resposta, ou quebrar a execução por envio de flags rejeitadas pelo CLI. |
| P06 | No menu interativo atual, alternar apenas o provedor mantém o modelo e o esforço do provedor anterior, gerando configurações inválidas (ex: provedor Antigravity com modelo do Codex). |

---

## 3. Expectations

| ID | Expectação de Resultado | Endereça |
| --- | --- | --- |
| E01 | O orquestrador descobre dinamicamente os modelos reais disponíveis no ambiente do usuário para cada provedor através de adaptadores isolados. | P01 |
| E02 | O sistema classifica o estado do provedor (`NOT_INSTALLED`, `INCOMPATIBLE`, `UNAUTHENTICATED`, `CATALOG_AVAILABLE`), alertando o operador com antecedência sobre passos necessários de configuração. | P02 |
| E03 | As particularidades de protocolo e sintaxe de cada CLI são encapsuladas em adaptadores dedicados que implementam uma interface unificada `ProviderModelCatalog`. | P03 |
| E04 | O menu interativo abre de forma instantânea (<100ms) lendo o cache particionado local em disco; probes externos ocorrem em segundo plano ou sob demanda manual com timeout rígido e unificado. | P04 |
| E05 | O esforço de raciocínio é normalizado de acordo com as capacidades conhecidas do modelo selecionado, preservando a opção de omitir a flag para respeitar o padrão nativo do provedor e nunca escalonando esforço para cima sem autorização. | P05 |
| E06 | A seleção no menu opera como uma tupla atômica `(provider, model, effort)`: alterar o provedor recalibra imediatamente o modelo e o esforço para os padrões recomendados daquele provedor. | P06 |

---

## 4. Personas

### 4.1 Operador / Desenvolvedor (Humano)
- **Meta principal:** Alternar rapidamente entre provedores (Codex, Antigravity, OpenCode, Claude Code), selecionar modelos disponíveis e calibrar o nível de esforço de raciocínio sem bloqueios de terminal e sem memorizar sintaxes CLI.
- **Interações:** Acessa a opção `[6]` no menu interativo; visualiza o catálogo e o estado de prontidão; força atualização manual via `[R]`; insere modelos experimentais via digitação livre.
- **Problemas e expectações relevantes:** P01/E01, P02/E02, P04/E04, P05/E05, P06/E06.

### 4.2 Runner Orchestrator (Sistema / Agente)
- **Meta principal:** Instanciar processos de agentes (`Builder`, `Sealer`, `Adversarial`) recebendo parâmetros de execução formalmente validados e coerentes com a ferramenta de destino.
- **Interações:** Consome a tupla `(provider, model, effort)` e a definição do catálogo para compor argumentos limpos de execução de subprocessos.
- **Problemas e expectações relevantes:** P03/E03, P05/E05, P06/E06.

---

## 5. Functionalities

| ID | Funcionalidade | Persona principal | Valor reconhecível |
| --- | --- | --- | --- |
| F01 | Avaliar prontidão de provedores por máquina de estados | Operador | Identifica claramente executáveis ausentes, incompatíveis ou sem autenticação antes do início do ciclo. |
| F02 | Descobrir catálogos de modelos com degradação graciosa | Operador | Exibe modelos reais da conta e degrada suavemente para cache ou catálogo de emergência se o CLI falhar. |
| F03 | Calibrar tupla atômica e normalizar esforço de raciocínio | Runner Orchestrator | Garante que modelo e esforço sejam coerentes com o provedor ativo, evitando escalonamentos indevidos. |
| F04 | Gerenciar cache particionado no diretório de estado | Operador | Permite inicialização instantânea do terminal e persistência segura em `<stateDirectory>/models-cache.json`. |

---

## 6. Product Backlog

### F01 — Avaliar prontidão de provedores por máquina de estados
- **PBI-01 — Inspeção de Executáveis e Versões**
  - *Como* Operador, *quero* que o orquestrador localize os executáveis (`codex`, `agy`, `opencode`, `claude`) no PATH e em diretórios de instalação de usuário, verificando a versão com timeout de 3s, *para que* eu veja no menu quais ferramentas estão realmente instaladas e compatíveis.
  - *Critérios de Aceite:*
    1. Retorna `NOT_INSTALLED` se o executável não existir.
    2. Retorna `INCOMPATIBLE` se a versão for inferior aos requisitos mínimos (ex: Claude < 2.1).
    3. Registra o caminho absoluto resolvido para evitar buscas repetidas no PATH.
- **PBI-02 — Diagnóstico de Autenticação e Guia de Reparo**
  - *Como* Operador, *quero* que um provedor detectado como `UNAUTHENTICATED` exiba no menu o comando exato de login (`codex login`, `agy auth`, `opencode auth`, `claude auth login`), *para que* eu saiba como restabelecer o acesso sem adivinhar a flag.
  - *Critérios de Aceite:*
    1. Prova de status rápida e não bloqueante para provedores que suportam probe (`claude auth status`, `codex login status`).
    2. Provedor com autenticação não verificável é marcado como `ACCESS_UNVERIFIED` em vez de gerar falso positivo de erro.

### F02 — Descobrir catálogos de modelos com degradação graciosa
- **PBI-03 — Adaptador de Catálogo OpenAI Codex**
  - *Como* Operador, *quero* que o catálogo do Codex seja obtido via `codex debug models` com timeout de 5s, extraindo display name e níveis de raciocínio suportados, com fallback para `~/.codex/config.toml` e lista de emergência, *para que* modelos recém-lançados apareçam automaticamente.
  - *Critérios de Aceite:*
    1. Parser trata saída JSON contendo a chave `models`.
    2. Em caso de falha de parsing ou timeout, lê o modelo ativo em `~/.codex/config.toml`.
    3. Se o TOML for inacessível, carrega a lista estática (`gpt-6.1-sol`, `gpt-6-luna`, `gpt-5.6-sol`).
- **PBI-04 — Adaptador de Catálogo Google Antigravity**
  - *Como* Operador, *quero* que o catálogo do Antigravity execute `agy models` com timeout de 5s, parseando a saída TSV (`slug\tlabel`), *para que* modelos como `gemini-4-argon` e `claude-opus-5-5` fiquem visíveis.
  - *Critérios de Aceite:*
    1. Parser ignora linhas informativas (ex: `"Fetching available models..."`) e extrai pares tabulados.
    2. Preserva os slugs contendo variantes (ex: `gemini-4-argon-medium`).
    3. Fallback carrega lista estática mínima caso o comando falhe.
- **PBI-05 — Adaptador de Catálogo OpenCode**
  - *Como* Operador, *quero* que o catálogo do OpenCode execute `opencode models --verbose` com timeout de 10s, processando a saída multiline com blocos JSON e variantes, *para que* eu selecione modelos locais ou remotos com seus identificadores completos `provider/model`.
  - *Critérios de Aceite:*
    1. Parser multiline extrai o ID na primeira linha e o JSON correspondente na linha seguinte.
    2. Lê as chaves do objeto `variants` para identificar níveis de esforço disponíveis.
    3. Fallback suporta `opencode models` (tabular) e lista estática de emergência.
- **PBI-06 — Adaptador de Catálogo Claude Code**
  - *Como* Operador, *quero* que o adaptador do Claude Code forneça o catálogo canônico de modelos/aliases (`sonnet`, `opus`, `haiku`, `fable`, `claude-sonnet-4-5`, `claude-opus-4-5`, etc.) com estado `ACCESS_UNVERIFIED`, *para que* eu escolha modelos Claude válidos sem disparar chamadas inexistentes de listagem.
  - *Critérios de Aceite:*
    1. Nunca executa `claude models` (evitando envio de prompts acidentais ao CLI).
    2. Disponibiliza os aliases canônicos e modelos versionados homologados.
- **PBI-07 — Entrada Customizada Sanitizada**
  - *Como* Operador, *quero* informar manualmente identificadores de modelos experimentais via opção `[Outro / Custom]`, *para que* eu teste modelos novos antes de serem incorporados aos catálogos oficiais.
  - *Critérios de Aceite:*
    1. Valida a string de entrada com regex `/^[A-Za-z0-9][A-Za-z0-9._/:@-]*$/`.
    2. Rejeita caracteres de controle, espaços ou sequências de injeção de shell (`&`, `|`, `;`, `$`, `` ` ``).

### F03 — Calibrar tupla atômica e normalizar esforço de raciocínio
- **PBI-08 — Política de Normalização de Esforço sem Escalonamento**
  - *Como* Runner Orchestrator, *quero* validar o esforço solicitado contra os níveis suportados pelo modelo, permitindo omitir a flag ou passar `none` explícito, e **nunca** escalonando para um nível mais alto automaticamente, *para que* custos e latência não sejam inflados sem consentimento.
  - *Critérios de Aceite:*
    1. Mapeia os níveis canônicos (`none`, `minimal`, `low`, `medium`, `high`, `xhigh`, `max`) para as flags de cada CLI.
    2. Se o modelo não suportar raciocínio ou a opção for `default`, omite as flags de esforço na chamada do processo.
    3. Se o esforço solicitado for incompatível, emite aviso ao operador e rejeita a seleção em vez de aumentar o esforço.
- **PBI-09 — Seleção Atômica de Tupla no Menu Interativo**
  - *Como* Operador, *quero* que ao alterar o provedor ativo no menu `[6]`, o orquestrador redefina imediatamente o modelo e o esforço para os padrões daquele provedor, *para que* não ocorram combinações cruzadas inválidas.
  - *Critérios de Aceite:*
    1. A troca de provedor redefine `model` para o modelo padrão daquele provedor.
    2. A troca de provedor redefine `effort` para o esforço padrão suportado pelo novo modelo.
    3. Persiste a tupla íntegra na configuração ativa do ciclo.

### F04 — Gerenciar cache particionado no diretório de estado
- **PBI-10 — Persistência e Recuperação Particionada em Disco**
  - *Como* Operador, *quero* que os catálogos sejam armazenados em `<stateDirectory>/models-cache.json` particionados por provedor com timestamp, versão do CLI e TTL de 24h, *para que* a abertura do menu ocorra em <100ms sem chamadas de rede.
  - *Critérios de Aceite:*
    1. Escrita atômica em arquivo temporário com rename seguro para evitar arquivos corrompidos.
    2. Cada provedor possui sua própria entrada e data de expiração independente.
    3. O arquivo é armazenado no `stateDirectory` existente do projeto (evitando pastas órfãs).
- **PBI-11 — Refresh Sob Demanda e Observabilidade de Cache**
  - *Como* Operador, *quero* visualizar a idade do cache no menu e poder pressionar `[R]` para forçar a reatualização de todos os provedores em segundo plano, *para que* eu tenha controle sobre o frescor dos catálogos.
  - *Critérios de Aceite:*
    1. Menu exibe origem dos dados: `[LIVE]`, `[CACHE - há 2h]` ou `[FALLBACK]`.
    2. Tecla `[R]` dispara probe assíncrono exibindo spinner no terminal.
    3. Falhas no refresh preservam o cache válido anterior sem apagá-lo.
- **PBI-12 — Contrato de Integração com Runner Polimórfico**
  - *Como* Runner Orchestrator, *quero* que o catálogo forneça argumentos limpos e compatíveis com a interface `AgentRunner`, *para que* futuras implementações de executores (Antigravity/OpenCode/Claude) consumam a tupla sem alterações na interface pública.
  - *Critérios de Aceite:*
    1. Define a interface TypeScript canônica `ProviderModelCatalog` e `ProviderModelOption`.
    2. Não embute flags de bypass de permissão (`--dangerously-skip-permissions`) no contrato do catálogo.

---

## 7. Traceability

| PBI | Funcionalidade | Persona | Problema / Expectação | Critério de Teste Principal |
| --- | --- | --- | --- | --- |
| PBI-01 | F01 | Operador | P02 / E02 | Detecção de executável ausente e versões incompatíveis via probe com timeout. |
| PBI-02 | F01 | Operador | P02 / E02 | Exibição de comando de login correto para provedor em estado `UNAUTHENTICATED`. |
| PBI-03 | F02 | Operador | P01, P03 / E01, E03 | Parsing de JSON do `codex debug models` e fallback em cascata para TOML e lista estática. |
| PBI-04 | F02 | Operador | P01, P03 / E01, E03 | Parsing de TSV do `agy models` preservando slugs e labels reais. |
| PBI-05 | F02 | Operador | P01, P03 / E01, E03 | Parser multiline do `opencode models --verbose` extraindo variantes e suporte a raciocínio. |
| PBI-06 | F02 | Operador | P01, P03 / E01, E03 | Entrega de catálogo curado para Claude Code sem invocar `claude models`. |
| PBI-07 | F02 | Operador | P01 / E01 | Validação de regex em entrada livre, rejeitando metacaracteres de injeção de shell. |
| PBI-08 | F03 | Runner Orchestrator | P05 / E05 | Validação estrita de esforço: omissão de flag para default e rejeição de escalonamento. |
| PBI-09 | F03 | Operador | P06 / E06 | Atualização atômica conjunta de `(provider, model, effort)` ao trocar provedor no menu. |
| PBI-10 | F04 | Operador | P04 / E04 | Leitura e escrita atômica do cache particionado no `stateDirectory` com respeito ao TTL de 24h. |
| PBI-11 | F04 | Operador | P01, P04 / E01, E04 | Disparo de refresh manual `[R]` em segundo plano e preservação de cache anterior em falhas. |
| PBI-12 | F04 | Runner Orchestrator | P03 / E03 | Interface tipada `ProviderModelCatalog` sem flags de segurança indevidas. |

---

## 8. Assumptions

1. **Versões Locais Homologadas na Pesquisa:**
   - OpenAI Codex: `OpenAI Codex v0.164.0` em `AppData\Local\Programs\OpenAI\Codex\bin\codex.exe`.
   - Google Antigravity: `agy` v1.1.15+ em `AppData\Local\agy\bin\agy.exe`.
   - OpenCode: `opencode` v1.1.48+ em `AppData\Roaming\npm\opencode.ps1`.
   - Claude Code: `claude` v2.1.234 em `.local\bin\claude.exe`.
2. **Ambiente de Execução e Segurança:**
   - Todas as invocações de CLIs de probe são disparadas via argumentos separados (`execFile` ou spawn direto sem `shell: true`), eliminando risco de shell injection no Windows.
   - Timeouts de probe unificados: 5 segundos para Codex, Antigravity e Claude; 10 segundos para OpenCode (devido a consultas a registros remotos).
3. **Localização do Cache:**
   - O arquivo de cache deve residir em `<workspace>/<stateDirectory>/models-cache.json` (padrão `.cbm-orchestrator/models-cache.json`), compartilhando o ciclo de vida do projeto.

---

## 9. Open Questions (Resolvidas no Refinamento)

### Q01 — Qual é a política padrão de latência e carregamento ao abrir o menu interativo?

**Suggested answer:** Carregamento instantâneo via cache local (`models-cache.json`). Se o cache existir e for válido (<24h), é renderizado imediatamente (<50ms). Se estiver expirado ou ausente, exibe o catálogo de emergência imediatamente com marcador `[DEGRADADO]` e agenda a atualização dos catálogos em background, garantindo que o menu nunca congele aguardando rede.

**Rationale:** O terminal precisa ser ágil e responsivo para o operador; processos de probe de rede não podem bloquear a navegação interativa.

**Resolved answer:** Carregamento instantâneo por cache com fallback síncrono para catálogo estático se ausente, atualizando em background.

**Answered by:** human (validado em conjunto na sessão de auditoria)

**Status:** Human validated

---

### Q02 — Como tratar incompatibilidade entre o nível de esforço escolhido e o modelo selecionado?

**Suggested answer:** Se o modelo selecionado não suportar o esforço escolhido (ou for um modelo de inferência fixa sem raciocínio), o orquestrador emite aviso visual no console e reverte o esforço para o padrão recomendado daquele modelo (`default` / omitido). Nunca aumentar o esforço automaticamente (evita custo excessivo e latência inesperada).

**Rationale:** Aumentar esforço silenciosamente viola o princípio da menor surpresa e gera cobranças desnecessárias de tokens de raciocínio.

**Resolved answer:** Reverter para o esforço padrão/omitido do modelo selecionado com aviso explícito; jamais escalar para cima automaticamente.

**Answered by:** human (validado em conjunto na sessão de auditoria)

**Status:** Human validated

---

### Q03 — Como o PBB de Descoberta se articula com a execução dos novos provedores no Outer Loop?

**Suggested answer:** O PBB de Descoberta entrega a especificação da tupla atômica `(provider, model, effort)` e a interface unificada `ProviderModelCatalog`. A implementação do runner polimórfico (criação de executores dedicados para Antigravity, OpenCode e Claude no lugar do atual `CodexAgentRunner` estático) é uma dependência arquitetural registrada e tratada em um ADR/PBI técnico complementar de execução.

**Rationale:** Mantém o Princípio da Responsabilidade Única (SRP): descoberta e calibração de catálogo não devem se misturar com o gerenciamento de sessões, MCP e parsing de streaming de cada agente.

**Resolved answer:** Descoberta e catálogo delimitados no PBB atual; runners polimórficos como dependência formal de execução.

**Answered by:** human (validado em conjunto na sessão de auditoria)

**Status:** Human validated
