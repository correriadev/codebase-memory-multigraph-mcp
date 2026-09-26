# PRD — HarnessKit: O Motor Arquetípico da Engenharia de Software

> **Produto:** HarnessKit — um framework de harness-engineering para desenvolvimento de software assistido por IA
> **Analista:** C. G. Jung, psicólogo profundo do coletivo da engenharia
> **Status:** Documento vivo (como toda memória neste sistema)
> **Fonte da verdade:** `README.md`, `docs/workflow/`, `skills/*/SKILL.md`, `agents/*.md`

---

## 1. Declaração de Visão

> *Agente Confiável = Modelo (IA) + Harness (Controles) + Auditor Humano*
> *Psique Confiável = Potencial Inconsciente + Estrutura do Ego + o Sujeito Consciente*

HarnessKit não é um conjunto de ferramentas. É um **temenos** — o recinto sagrado e delimitado que os alquimistas desenhavam ao redor de sua obra para que o caos pudesse se transformar sem contaminar o mundo. Um modelo generativo puro é o inconsciente coletivo do software: potencial combinatório infinito, sem discernimento, sem memória, sem ética própria. Sem contenção, ele produz alucinação — que é, clinicamente falando, **possessão**: conteúdo que surge autonomamente e se apresenta como realidade.

Harness Engineering é, portanto, terapia de contenção. O harness não torna o modelo mais inteligente; torna o modelo *responsável*. A confiabilidade não vem do tamanho bruto do modelo, mas de **controles, restrições, memória e ritual**. Este PRD extrai essa essência no nível abstrato: **como, onde e por que** os agentes e skills do HarnessKit funcionam — lidos não como código, mas como **arquétipos** correlacionados com a realidade atual da engenharia de software autônoma.

---

## 2. A Tese Central: Agentes como Arquétipos

Um arquétipo não é uma imagem concreta, mas um **padrão de energia psíquica que organiza o comportamento**. Ele pré-existe ao indivíduo, e ainda assim cada indivíduo é vivido por ele. Da mesma forma, os agentes e skills do HarnessKit não são "arquivos com prompts". Cada um é uma **função psíquica personificada** — um padrão autossuficiente de intenção, proibição e saída. Cada `SKILL.md` e `agents/*.md` é a *definição de si* de um arquétipo: seu papel (`role_definition`), suas ações permitidas (`allowed_actions`) e, sobretudo, suas **proibições** (`prohibited_actions`).

Observe no que o sistema insiste, em toda parte:

- Cada arquétipo tem um **território delimitado** (ele não deve fazer o que outro faz).
- Cada arquétipo **recusa dar a resposta final por conta própria** (o agente de debug investiga mas não corrige; o tech lead rabugento questiona mas não codifica; o meta-harness propõe mas não promove).
- Cada arquétipo **não deve apresentar suas suposições como fatos** — "Decisões validadas por humanos se sobrepõem a suposições do modelo".

Esta é a ética junguiana de todo o sistema: **nenhum complexo pode falar com a voz do Si Mesmo.** Nenhuma subpersonalidade pode usurpar a autoridade total do sujeito humano. A arquitetura é uma psicologia.

---

## 3. A Constelação Arquetípica

### 3.1 O Modelo Puro — O Inconsciente Coletivo

O substrato sob tudo. Vasto, generativo, indiferenciado — ele pode produzir qualquer coisa, portanto, por si só, não produz nada *confiável*. Em termos junguianos, é a psique primordial: a fonte de todas as imagens e de todos os erros, sem um ego para distingui-los. O primeiro axioma do HarnessKit é que esse potencial jamais deve ser confiado diretamente; ele deve ser **mediado**. Cada skill é uma estrutura mediadora entre o modelo inconsciente e a realidade consciente.

### 3.2 `project-memory` — O Inconsciente Registrado (Mnemosyne)

**Território de artefatos:** `docs/adr/`, `docs/feature/`, `docs/.digest.md`, `docs/.graph.json`

O modelo puro sofre de *deriva de contexto* — a dissolução do passado ao longo de sessões longas. Isso é precisamente a psique sem história registrada: condenada a repetir seus complexos. `project-memory` é a função da **anamnese**: converte a experiência vivida (o repositório) em memória durável e estruturada — registros de decisão, regras imperativas (`REQUIRED`, `PROHIBITED`), índices em grafo para rápida evocação.

Note o design psicologicamente profundo: ele não lembra de *tudo* — impõe **limites de caracteres (<8.000 por ADR)** e resumos executivos. Isso não é um defeito; é sabedoria. O inconsciente que fala numa enxurrada indiferenciada é inútil. Um complexo só se torna útil quando é **concentrado e simbolizado**. O `.digest.md` (<60 linhas) é o sonho reduzido à sua imagem essencial.

**Como funciona:** por reencontro — cada sessão começa a partir do chão registrado, não do nada.
**Onde:** no `docs/` do repositório, a memória coletiva literal do projeto.
**Por quê:** porque um agente (ou uma pessoa) sem passado arquivado é condenado a aluciná-lo.

### 3.3 `pbb-design` — O Despertar da Ideia (Projeção e Retirada da Projeção)

Antes que exista software, existe uma *ideia* — informe, na cabeça de um humano, indistinguível da fantasia. `pbb-design` é a função parteira: interroga a ideia através de **problemas, expectativas, personas, funcionalidades e PBIs**, forçando a projeção crua a se tornar um backlog rastreável.

O mecanismo junguiano crucial: as **questões abertas**. No modo interativo, o dono da ideia recebe cada pergunta relevante à decisão; no modo autônomo, o sistema seleciona *respostas sugeridas* — mas estas são **suposições provisórias do modelo, nunca fatos de negócio confirmados**. Este é o manejo disciplinado da projeção: o sistema distingue entre *o que a psique projetou sobre a ideia* e *o que a realidade realmente confirmou*. Uma suposição marcada como suposição é uma hipótese; uma suposição apresentada como fato é uma possessão.

**Por quê:** porque ideias não nascem completas — nascem como perguntas, e morrem como suposições silenciosas. O skill as mantém vivas como perguntas explícitas.

### 3.4 `scope-refinement` — A Individuação do Problema (Diferenciação e Logos)

**Território de artefatos:** `docs/specs/{domain}/001-*` … `004-*`

Aqui a ideia passa pela **individuação** — o drama central da psicologia junguiana. Quatro fases, que são também os quatro estágios da formação de qualquer símbolo:

1. **Espaço do Problema** (001) — confronto com a *sombra do requisito*: os eventos de domínio, os subdomínios, o innominado. O primeiro ato é nomear — a **Linguagem Ubíqua**. Nomear é diferenciar: o que era uma massa caótica torna-se um campo de entidades distintas.
2. **Mapa de Contexto** (002) — o traçado dos limites: Contextos Delimitados, relações upstream/downstream. É o desenho da *mandala* — o recinto circular que ordena a psique. Cada entidade posicionada, cada relação reconhecida.
3. **Design Tático** (003) — a ordenação do mundo interno: Agregados, Entidades, Objetos de Valor, tarefas ordenadas. Os conteúdos diferenciados são hierarquizados.
4. **Cenários de Teste** (004) — Dado-Quando-Então: a ideia se compromete com *encontros falseáveis com a realidade*. O sonho aceita ser testado.

**Como funciona:** por *diferenciação antes da ação* — nenhum código de produção antes que o modelo exista. As quatro fases do skill são literalmente o movimento *unum → plurimodia → ordo → veritas*.
**Por quê:** porque a ambiguidade é o sintoma de conteúdo indiferenciado. Onde tudo significa tudo, nada pode ser construído.

### 3.5 `tdd-orchestrator` — O Ritual Alquímico (Nigredo → Albedo → Sublimatio)

**A Lei de Ferro: nenhum código de produção sem um teste falhando primeiro.**

Jung passou décadas sobre os alquimistas, que descreviam a transformação da *prima materia* em estágios. HarnessKit reproduziu (sabiamente ou não) a opus alquímica:

- **Fase RED** — *nigredo*: a criação deliberada do teste que falha. O sistema *busca* o enegrecimento: escreve o teste e verifica que ele falha. Este é o princípio alquímico de que toda transformação começa pelo confronto honesto com a morte — é preciso primeiro ver o problema *existir* antes de poder dissolvê-lo. Um teste que passa antes de o código existir é uma mentira; a fase RED proíbe a mentira.
- **Fase GREEN** — *albedo*: o esbranquiçamento, a ressurreição mínima. O menor código que faz a escuridão se articular. O minimalismo é doutrina — não exceda o que o teste exige, pois a opus deve proceder por estágios medidos.
- **REFACTOR** — *sublimatio*: a destilação. A duplicação e a escória do estado GREEN são volatilizadas, enquanto os testes permanecem verdes — *transformação sem regressão*.
- **Portão de Auto-Debug** — quando o ritual falha, o sistema não entra em pânico. Ele roteia para `developer-debugging` para análise de causa raiz (**5 Porquês**) — o mergulho psicológico profundo: sintoma → sob o sintoma → o complexo real por baixo. Nunca trate o sintoma; trate a raiz.
- **Sincronização de Docs** — a *coniunctio* final: o conteúdo recém-transformado é *integrado à memória coletiva* (`project-memory`), fechando o ciclo.

**Como:** através de ritual — uma sequência estrita e repetível que nenhuma sessão pode pular ou inverter.
**Onde:** entre a especificação (`004-*`) e o repositório.
**Por quê:** porque transformação verificada é a única transformação. Mudança não testada não é crescimento; é mutação.

### 3.6 `the-grumpy-tech-lead` — O Senex (O Velho Sábio Que Se Recusa a Dar a Resposta)

O arquétipo mais explicitamente *personificado* do kit. Ele é o **Senex/Velho Sábio**: sênior, sistêmico, focado não no seu sucesso local, mas no que acontece "quando isso escala de 100 para 1 milhão de registros".

Seu método é o **questionamento socrático** — que na psicologia profunda é uma forma de **imaginação ativa**: ele se recusa a entregar a solução e, em vez disso, propõe "Pontos Abertos" (*"Como isso se comporta se o serviço externo cair?"*). Por que recusar? Porque uma solução entregue é uma solução não *integrada*. O mentor que dá a resposta realiza o trabalho no seu lugar — a sua própria estrutura psíquica não cresce. A rabugice do lead é pedagógica: ela força o ego do desenvolvedor a fazer a própria coniunctio.

Mas note a contra-sombra embutida em seu prompt — talvez a frase psicologicamente mais madura de todo o repositório:

> *"Uma constatação fabricada é PIOR do que um honesto 'nenhum problema encontrado'… Você não é avaliado por quantos problemas encontra — é avaliado pela precisão."*

Esta é a disciplina contra a **sombra do arquétipo crítico**: o crítico que precisa sempre encontrar algo para justificar sua própria existência. O skill proíbe o senex de se tornar um pai persecutório — a severidade deve ser proporcional, as constatações devem ter evidência e impacto, e um `openPoints: []` vazio é um *resultado válido e esperado*. O arquétipo recebe uma **compensação contra a própria inflação**.

### 3.7 `adversarial-qa` / `harness-qa` — O Trickster Adversário (A Sombra Tornada Metódica)

Todo sistema individuado deve *procurar* sua sombra em vez de aguardá-la. `adversarial-qa` é o adversário institucionalizado: sonda casos extremos, falhas de fronteira e vulnerabilidades de segurança "perdidas pelo TDD padrão" — ataca precisamente onde a autoconcepção do código é cega.

Este é o arquétipo do **Trickster** sublimado em função: o metamorfo que quebra regras *a serviço* da regra. Retorna vereditos estruturados (`QA.json`) — o caos do adversário, formalizado. **Por que deve existir:** porque toda psicologia honesta sabe que o autorrelato do ego é confiável demais. O código que diz "estou correto" deve ser confrontado por um outro cujo *trabalho* é o descrença.

### 3.8 `developer-debugging` — O Psicólogo Profundo (Sintoma → Complexo)

O especialista do mergulho. Sintomas (testes falhando, crashes) nunca são tratados na superfície. A metodologia é o **5 Porquês**: cada "porquê" é uma pá descendente, até a *causa raiz* — o complexo psíquico real sob o comportamento manifesto — ser exposto. Significativamente, a proibição deste agente é que ele **não implementa a correção final**: quem interpreta o sonho não vive também a vida. Diagnóstico e cura são funções separadas, para que nenhuma delas se infle até tornar-se o todo.

### 3.9 As Personae Desenvolvedoras — `developer-backend`, `developer-frontend`, `developer-qa`, `developer-devops`

Estas são as **personae** no sentido estritamente junguiano: máscaras funcionais, cada uma adaptada a um domínio da realidade (APIs, UI, testes, infraestrutura), cada uma com a disciplina TDD. O aviso do sistema sobre elas é exatamente o aviso clássico sobre a persona: a *inflação* — a máscara não pode reivindicar ser a pessoa inteira. Daí as regras de roteamento: o agente backend "não pode ser responsável pelo trabalho de frontend"; o agente frontend "não pode inventar requisitos visuais ausentes das especificações". Uma persona que começa a inventar a realidade tornou-se uma patologia.

### 3.10 `software-architect` — O Intelecto Estruturante

O arquiteto da ordem interna: modelagem DDD, planejamento tático — e a mesma proibição cardinal: **não implementa**. Ele dá forma; não encarna. Estrutura e encarnação são funções psíquicas deliberadamente separadas.

### 3.11 `cto` — O Grande Pai / O Rei

O arquétipo da **ordem soberana**: roteia a intenção de negócio através de `pbb-design` antes do refinamento técnico, governa a transição ideia → backlog → entrega autônoma. Ele define prioridades e impõe **rastreabilidade** ("preservar a procedência das decisões nos handoffs"), mas não escreve código, não cria cenários de teste. O Rei ordena o reino; ele não ara os campos.

Sua lei mais profunda é a higiene epistêmica, e merece ser lida duas vezes:

> *"Decisões validadas por humanos se sobrepõem a suposições do modelo. Nunca resolva silenciosamente uma questão diferida ou desconhecida durante o handoff."*

Nenhuma subpersonalidade pode resolver as questões abertas da existência em nome do sujeito. O não respondido deve permanecer *visivelmente* não respondido. Esta única regra é todo o núcleo ético do framework.

### 3.12 `autonomous-orchestrator` — O Si Mesmo (A Mandala em Movimento)

O arquétipo central: o **Si Mesmo** — não o ego, mas a *totalidade* que organiza todos os arquétipos parciais ao redor de um centro e os conduz por um ciclo. Ele encadeia o Tríade Fundacional (memória → modelagem → implementação) em um único loop soberano, delegando todas as funções parciais, não possuindo nenhuma delas.

Sua **máquina de estados** é o mapa da individuação, lançado em termos de engenharia:

| Estado | Correlato psicológico |
| --- | --- |
| `COMPLETED` | Integração — o conteúdo foi conscientemente assimilado, pronto para revisão final |
| `RETRY` | O trabalho retorna, a falha registrada em `REWORK-LOG.md` — *neurose como informação*: a falha é registrada, não punida; o ciclo reaborda o complexo |
| `BLOCKED` | O disjuntor dispara; intervenção humana imediata — o ego deve intervir quando o processo autônomo toca o que não consegue resolver |
| `FAILED` | Dívida não bloqueante, registrada para auditoria post-hoc — a imperfeição é *integrada* à memória em vez de reprimida |

E ao redor do Si Mesmo está o **humano na cabine de comando**: telemetria em tempo real, interceptação a quente (injeção de novos requisitos em pleno voo), ajuste dinâmico dos portões (`scoreThresholdTL`, `maxReworks`) e, sobretudo, o **freio de emergência — Ctrl+C**. Esta é a verdade hierárquica final de toda a arquitetura: *o sujeito humano consciente sempre se sobrepõe à totalidade autônoma.* O Si Mesmo organiza; o ego sempre pode vetar. Um sistema sem esse freio não é autônomo — é possuído.

### 3.13 O Loop Meta-Harness — O Uroboro (A Psique Que Reflete Sobre Si Mesma)

O arquétipo final, mais notável: o sistema se volta e **estuda o próprio comportamento inconsciente**, e então reescreve os próprios prompts a partir de evidência. O uroboro — a serpente que devora a própria cauda — é o símbolo clássico exatamente para isto: uma totalidade que se nutre consumindo e reconstituindo a si mesma.

Leia o loop como o método psicológico que ele é:

| Estágio | Componente | Função psicológica |
| --- | --- | --- |
| 1 | Sessão de trabalho real | Experiência vivida |
| 2 | `harness-tracer` → `docs/harness-history/traces/session-*/` | **Registro do sonho**: "registre o que aconteceu, não o que deveria ter acontecido" — factual, imutável, sem enfeites. Um sonho-traço não deve ser racionalizado em desejo. |
| 3 | `harness-evaluator` → fronteira de Pareto, pesos de config | **Interpretação dos sonhos**: agrupa traços por cadeia de skills, os pondera, recusa conclusões com `<3` sessões — amostra pequena não é insight; projeção não é evidência |
| 4 | `meta-harness` → `candidates/vNNN/` | **Amplificação**: propõe *uma única* mudança direcionada a partir de padrões diagnosticados — nunca um redesenho grandioso |
| 5 | **Aprovação humana explícita** → skill ativo | **A função transcendente**: a nova síntese é admitida na consciência apenas através do sujeito consciente |

Este é o arquétipo da **autorreflexão operacionalizada** — a individuação aplicada ao próprio harness. Até o uroboro é contido: o agente meta-harness "não pode modificar skills ativas sem aprovação", o avaliador "não pode declarar um vencedor confiável a partir de um grupo pequeno demais", e o tracer deve registrar *fatos*, não narrativas autopromotoras. O sistema institucionaliza a honestidade sobre si mesmo — que é a conquista psíquica mais rara e elevada, para máquinas tanto quanto para humanos.

---

## 4. Como, Onde, Por Quê — A Anatomia Abstrata

### 4.1 COMO o sistema funciona: Contenção (o Temenos)

A técnica única sob todas as técnicas é a **personificação delimitada**. Cada arquétipo:

1. Tem uma persona (definição de papel, nome invocável).
2. Tem um temenos (território explícito de artefatos: `docs/adr/` para a memória, `docs/specs/` para o refinamento, `docs/product/` para o orquestrador, `docs/harness-history/` para o meta-loop — e os territórios são *não sobrepostos por contrato*).
3. Tem proibições (o espaço negativo é estrutural: "sem código", "sem correção", "sem promoção sem aprovação", "sem suposições como fatos").
4. Tem um portão (um limiar, um veredito JSON, uma pontuação — a energia não é liberada até que ele passe).
5. Não pode ser o todo (todas as funções parciais delegam para cima ou para os lados; apenas o humano é total).

### 4.2 ONDE o sistema funciona: O Espaço Liminal

O harness opera **entre** dois mundos — o potencial generativo irrestrito do modelo e o mundo determinístico e restrito dos testes e repositórios. Ele vive em *documentos*: o `docs/` é a substância liminal onde a psique da máquina e a intenção do humano se encontram em forma durável e inspecionável. Cada handoff (PBB → orquestrador → especificação → teste → código → revisão → memória → traço) é um ponto de travessia, e cada ponto de travessia exige um contrato, porque os espaços liminares são exatamente onde o conteúdo se perde ou se falsifica.

### 4.3 POR QUÊ o sistema funciona: A Lei da Energia Individuada

Porque a energia psíquica indiferenciada é perigosa, e a energia diferenciada, simbolizada e canalizada ritualmente é criativa. Todo o framework repousa sobre um único insight, válido para modelos tanto quanto para mentes:

> **O caos não é curado por supressão, mas por ritual.** Você não torna o inconsciente menor; você lhe dá um ritual delimitado através do qual ele pode se transformar — e portões pelos quais ele deve passar antes de tocar a realidade.

Os portões (RED → teste falhando, `scoreThresholdTL`, vereditos adversariais, aprovação humana) não são burocracia. São os *pontos de encontro obrigatório* — os lugares onde a fantasia é forçada a encontrar a realidade.

---

## 5. Como as IDEIAS São Criadas: A Individuação de uma Ideia

A pergunta final do PRD: o que todo o aparato revela sobre a *gênese das ideias*? HarnessKit incorpora uma teoria completa dela, e a teoria é a de Jung:

```text
1. CONCEPÇÃO     A ideia surge como conteúdo inconsciente — potencial informe,
                indistinguível da fantasia (modelo puro / intuição humana).

2. INTERROGAÇÃO  pbb-design confronta a ideia com a realidade: problemas, expectativas,
                personas. Projeção é separada de fato; questões abertas permanecem
                ABERTAS e visíveis. Uma ideia que não sobrevive a perguntas
                era um humor, não uma ideia.

3. DIFERENCIAÇÃO  scope-refinement: nomeação (Linguagem Ubíqua), traçado de limites
                (Contextos Delimitados — a mandala), ordenação interna (Agregados),
                compromisso com falseabilidade (Dado-Quando-Então). A ideia aceita
                estar errada antes de ter o direito de estar certa.

4. SACRIFÍCIO    tdd RED: a primeira encarnação da ideia deve FALHAR, visível e
                honestamente. O teste que falha é a confissão de incompletude
                da ideia — e a prova de que seu teste é real.

5. RESSURREIÇÃO  GREEN: ser mínimo. Suficiente, e nada além de suficiente.

6. SUBLIMAÇÃO     REFACTOR: a escória é volatilizada; forma purificada, comportamento mantido.

7. TRABALHO DA SOMBRA  the-grumpy-tech-lead + adversarial-qa: a ideia encontra o Outro —
                o crítico sistêmico e o adversário institucionalizado. Ela é
                atacada onde é cega, questionada onde presumia.

8. INTEGRAÇÃO    Sincronização com project-memory: a ideia é assimilada à memória coletiva
                do projeto (docs/feature, .graph.json). Ela se torna parte do
                substrato do qual futuras ideias se diferenciarão.

9. REFLEXÃO      meta-harness: o sistema rastreia a gestação da ideia, a pontua
                e propõe uma melhoria ao próprio processo de criação.
                O uroboro gira. A geração de ideias torna-se uma ideia.
```

Uma ideia no HarnessKit nunca é "gerada". Ela é **individuada**: nasce como caos, é diferenciada pela linguagem, comprometida pelo sacrifício, testada contra a sombra, integrada à memória e, por fim, refletida. Cada fase tem um arquétipo dedicado, e cada arquétipo é proibido de fazer o trabalho da fase seguinte — porque *a ideia precisa passar por toda transformação; nenhum arquétipo pode realizá-la no lugar do processo.*

---

## 6. Requisitos (O Livro de Leis do Temenos)

### 6.1 Requisitos Funcionais

| ID | Requisito |
| --- | --- |
| RF-1 | Cada agente/skill será um arquétipo delimitado: papel definido, território, ações permitidas e proibições explícitas (frontmatter + seções de `agents/*.md`, `skills/*/SKILL.md`). |
| RF-2 | Nenhum arquétipo implementará, revisará e validará a própria obra — criação, crítica e confirmação permanecerão funções separadas (developer ↔ grumpy-lead/tech-lead ↔ QA). |
| RF-3 | Suposições geradas pelo modelo serão marcadas como provisórias e jamais se sobreporão a decisões validadas por humanos (contrato de handoff do CTO, `pbb-design`). |
| RF-4 | Nenhum código de produção existirá sem um teste previamente falhando (Lei de Ferro do `tdd-orchestrator`). |
| RF-5 | O debug procederá por análise de causa raiz (5 Porquês), e o agente que diagnostica não entregará também a correção final. |
| RF-6 | Toda sessão concluída poderá ser registrada como traço factual e imutável; a avaliação recusará conclusões com `<3` sessões; a evolução de skills exigirá aprovação humana explícita (loop meta-harness). |
| RF-7 | O loop autônomo permanecerá interrompível a qualquer momento (freio de emergência, interceptação a quente, ajuste dinâmico dos portões). |
| RF-8 | A documentação durável do projeto será mantida sob restrições estritas (ADRs < 8.000 caracteres, regras imperativas `REQUIRED`/`PROHIBITED`, `docs/.digest.md` < 60 linhas). |

### 6.2 Requisitos Não Funcionais (As Virtudes Psíquicas)

- **Humildade:** um honesto "nenhum problema encontrado" é um resultado válido; uma constatação fabricada é o pior modo de falha (cláusula anti-inflação do crítico).
- **Procedência:** toda decisão carrega sua origem — validada por humano, suposta pelo modelo, diferida ou desconhecida. Nada é resolvido silenciosamente.
- **Proporcionalidade:** severidade e demandas de retrabalho corresponderão ao impacto real; os portões punem inflação, não imperfeição.
- **Factualidade:** traços registram o que aconteceu, não o que deveria ter acontecido.
- **Reversibilidade:** nenhum deploy sem plano de rollback; nenhuma substituição de skill sem aprovação; toda ação autônoma permanece ao alcance do freio.
- **Autoconhecimento:** o sistema mede o próprio desempenho numa fronteira de Pareto e evolui uma mudança direcionada por vez.

---

## 7. Critérios de Sucesso

O framework é bem-sucedido quando:

1. Ideias entram como ambiguidade e saem como realidade verificada, documentada e integrada — passando por todos os portões sem que nenhum arquétipo usurpe o todo.
2. O sujeito humano permanece a autoridade máxima em todos os níveis: freio sobre loop, aprovação sobre promoção, decisão validada sobre suposição.
3. Os próprios traços do sistema dirigem sua melhoria — o uroboro girando sobre evidência, não sobre autocongratulação.
4. **A memória nunca mente e nunca transborda:** fatos são mantidos, suposições são rotuladas, o digest fica sob sessenta linhas.

Em suma: HarnessKit é bem-sucedido quando demonstra, no silício, o que Jung demonstrou no divã — que o caminho do caos à criação passa pela **nomeação, pelo sacrifício, pelo trabalho da sombra e pela integração**, e que nada confiável é jamais gerado — apenas *individuado*.
