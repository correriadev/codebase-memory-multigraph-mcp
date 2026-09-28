# AUDIT-WORKFLOW — Auditoria do Workflow das Skills sobre o CBM (v1)

> **Derivação:** PRD → PRD_V1 → PRD_V2 → ADR_V1 → PRD_V3 → specs A/B/C/D → **AUDIT-WORKFLOW (auditoria operacional)**
> **Objeto:** o workflow que sustenta a nova forma de trabalho — as skills Track B operando sobre a Track A/C/D — não as capabilities isoladas.
> **Método:** verificação dirigida (Tier 2): specs lidos lado a lado com o código (`src/union/*`, `src/mcp/*`, `src/core/horizon_pool.c`, `Makefile.cbm`), grep de inclusões em toda a árvore `src/`, cobertura do índice confirmada para os caminhos citados. Evidência por arquivo:linha; ACs sempre por host log (Axiom of Testimony).
> **Status:** REVIEW — auditoria; nada aqui é aprovado para implementação sem revisão.

---

## 0. Veredito executivo

A percepção do operador está correta e agora tem endereço: **o workflow existe como especificação e como biblioteca, mas não como caminho de execução.** Os três commits de feature (967d9815, 6da66c17, fa502194) entregaram 22 módulos em `src/union/` (~4.3k linhas) compilados dentro do binário de produção (`Makefile.cbm:549`, `UNION_SRCS` em `Makefile.cbm:348-371`) — e **nenhum handler MCP, daemon ou CLI inclui um único header `union_*`** (grep de `#include "union*` em toda `src/`: ocorrências apenas dentro de `src/union/`). A Track A/C/D é hoje uma biblioteca sem chamador: tudo que o ciclo narrativo (GROUND → DELIBERATE → CONTEST → CONCRETIZE → TRACE → founding) exige está implementado e testado **em unidade**, e **inacessível em runtime** — nenhuma das 19 ferramentas MCP expostas (`src/mcp/mcp.c`, handlers em `src/mcp/horizon_handler.c` etc.) toca em sessão, contrato, gateway, roteamento, provenance, sweep, founding ou trace.

O exemplo ponta a ponta da auditoria anterior (a história do Split de Pagamento) é, portanto, **verificável como intenção e falsificável como operação**: cada passo cita uma função que existe e que nenhum agente consegue invocar.

---

## 1. O estado real, estação por estação

| Estação | Scope | Código existe | Tool MCP existe | Gap operacional |
| --- | --- | --- | --- | --- |
| Roteamento (CONSULTATIVE/SPECIALTY) | D03 | `cbm_classify_activity` (union_routing.c:27) | **não** | veredito de roteamento nunca é emitido nem logado |
| Grounding 2 fases | B01, D01/D02 | `union_grounding`, `union_theme_registry`, `union_binding` | **não** (só search/query genéricos) | sem leitura roteada; bindings não são consultáveis por tool |
| Deliberate (horizonte + provenance) | A01, B02 | `cbm_session_open`, `union_claim` | **parcial** (`create_horizon` cria horizonte **sem sessão**) | horizonte sem identidade, sem contrato, sem `based_on_seq`, sem contadores |
| Gateway de efeitos | A05 | `cbm_gateway_authorize_action` (union_gateway.h:66) | **não** | nenhuma ação de agente passa pelo gateway |
| Contest (adversarial) | A10, B03 | `union_contest`, caller-blind A08 | **não** | subagent revisor não tem tool de verificação cega |
| Promote | A07 | `cbm_promotion_*` + admission gate existente | `promote_horizon` existe, **desligado** da sessão | promove sem checar contest pendente, gateway ou sessão |
| Closure sweep + founding | C03, D06 | `cbm_sweep_close_session`, `union_founding` | **não** | nada dispara o sweep; proposta de fundação não tem como chegar ao operador |
| Trace factual | A09, B05 | `union_trace` | **não** | rastro nunca é emitido; stats de provenance ficam em memória de teste |
| Drift/orphan (D05) | D05 | `union_theme_drift` | **não** | notices de re-verificação nunca propagam |

**Conclusão estrutural:** o gap não é de qualidade de código — as 21 suítes de teste union passam e os ACs unitários estão honestos. O gap é de **costura**: falta a camada que transforma capabilities em workflow. Isso confirma a regra da própria specs/README.md:96 ("the doc plane must not be implemented before the judgment machinery it depends on") — só que agora invertida: a maquinaria de julgamento foi implementada antes de qualquer porta de entrada.

---

## 2. Gaps críticos (com evidência)

### G1 — Duas portas de horizonte desconectadas (bloqueador máximo)
`create_horizon` (src/mcp/horizon_handler.c:28) cria horizonte via `horizon_pool` sem abrir sessão Track A. `cbm_session_open` (src/union/union_session.h:81) usa o mesmo `horizon_pool` mas não tem chamador. Resultado: **todo agente que usa as tools atuais hoje trabalha fora do protocolo A01 por completo** — sem identidade, sem restricted mode, sem closure event. O workflow tem duas entradas paralelas e nenhuma delas é a do protocolo.

### G2 — Estado trans-sessão sem persistência e sem dono
`CbmSessionRegistry` (cap 32, union_session.h:26), `CbmGateway` (caps 64, union_gateway.h:25-27), `CbmThemeRegistry`, `CbmContractRegistry`: structs in-memory sem armazenamento. Sessões morrendo com o processo é decisão correta e documentada (SCOPE-A01 open question: "die with it — content destroyed, events survive"). Mas **contratos (A02), temas (D01), bindings (D02) e traces (A09) são trans-sessão por natureza** e não têm home: nem arquivo, nem tool de registro, nem carga no boot. O daemon reinicia e a instituição esquece tudo.

### G3 — Classificador de atividade heurístico, isolado e com default perigoso
`cbm_classify_activity` (union_routing.c:27-60) é keyword-matching bilingue hardcoded em C, com default `CONSULTATIVE` (union_routing.c:59). Três problemas: (a) não é tool — o "routing moment" do PRD_V3 §5 nunca é registrado no host log, logo não é auditável; (b) default conservador: specialty não detectado ⇒ provenance nunca exigida ⇒ `PROVENANCE_UNDECLARED` (o coração do PRD_V3) **não pode disparar na prática**; (c) listas de triggers são código, não dado — o próprio PRD_V3 §1 diz que especialidade deve ser dado, não código.

### G4 — B03 condicional tem a ponte no papel, não no fluxo
A auditoria anterior pediu B03 condicionado à classe de risco. A ponte natural existe e está órfã: o Effect-Class Gateway (A05, `cbm_gateway_authorize_action`). Hoje: (a) nenhuma ação declara effect_class; (b) o gateway não é chamado por nada; (c) `promote_horizon` (a única tool de concretização) valida âncoras da federação mas **não consulta sessão, contest ou gateway**. Sem o wiring G4, ou B03 roda sempre (fricção que a auditoria anterior rejeitou) ou nunca roda (a sombra que o ADR_V1 exige).

### G5 — Ciclo de fechamento sem trigger e sem caminho de volta ao operador
`cbm_sweep_close_session` (union_sweep.h:97) bloqueia fechamento com claims sem destino (`SWEEP_INCOMPLETE`) e `union_founding` produz a proposta — mas não existe tool `close_session`, não existe o par accept/decline do operador (PBI-06 cenários 2–4 do PRD_V3 §9.3), e o `FOUNDING_PROPOSAL` não tem como atravessar a fronteira engine→operador. O fecho do ciclo (a "colheita") é a parte mais especificada e a menos alcançável.

### G6 — Trace factual sem emissão
`union_trace` computa as métricas que o PRD_V3 §7 exige (ratio canon:invention, deviations admitted) a partir do ledger — em memória de teste. Sem tool de emissão no closure e sem sink no host log, **nenhum AC de B05 é verificável** e o critério de sucesso nº 7 do PRD_V3 ("no specialty judgment is silent — ratio measurable, trending") é uma variável sem instrumento.

### G7 — As skills Track B não existem em território nenhum
Todos os oito scopes B declaram "Codes in: harness-kit (not in this implementation cycle)" (e.g. SCOPE-B02:4) — repositório externo. Isso é uma decisão de fronteira correta, mas significa que **o workflow inteiro atualmente não tem nenhum dos dois lados**: a engine não expõe e as skills não existem. Falta o contrato entre os lados: qual sequência de estações, qual tool por estação, qual refusal routing, qual artifact de skill contract com `refusal_matrix` (B04 exige `CONTRACT_INVALID` para contrato incompleto — o código tem o refusal, não tem o validador acessível).

### G8 — Subagent revisor sem protocolo de encarnação
A divisão de territórios da auditoria anterior (dev diário / revisor no PR / fechamento / plataforma) precisa de encarnação técnica: o revisor B03 deve ser um subagent com **contexto isolado do autor** (caller-blind, A08) e uma tool de verificação que aceite evidência sem identidade de quem chama (`union_contest` existe, sem tool). Sem isso, "subagente revisor em segundo plano" é narrativa, não mecanismo — e o próprio A08 vira inalcançável, quebrando a regra "no scope may implement, review, and validate itself" (specs/README.md:100).

### G9 — Drift e órfãos sem propagação
`union_theme_drift` computa notices e orphan bindings (PBI-05). Nada consulta, nada emite: não há hook no refresh de índice de tema, nem query ECG exposta. Um tema evolui e os projetos vinculados nunca sabem.

---

## 3. Pontos de melhoria (não bloqueantes, mas reais)

1. **Caps fixos sem recusa tipada própria:** `CBM_SESSION_REGISTRY_CAP 32` vira `CBM_SESSION_ERR_FULL` (union_session.h:33) — que refusal da taxonomia A04 é esse? O mapeamento `ERR_FULL → ?` precisa existir ou o gateway/disciplina B04 não sabe o que fazer com ele.
2. **Heurística de roteamento:** externalizar as listas de triggers para dado (arquivo/tema), com teste de regressão; manter um único ponto bilingue EN/PT.
3. **Idempotency key:** quando o gateway entrar, a key de ações compensáveis precisa de ponto de nascimento definido (contrato de skill? chamada da tool?) — hoje `CbmCompensableRecord` existe sem provedor.
4. **Binary size/custo:** 22 módulos mortos linkados no PROD inflam o binário; enquanto não há wiring, avaliar compilar union só no test-runner (decisão explícita, com comentário de prazo — não deixar morrer por acidente).
5. **Specs B04 matriz × código:** a matriz de 16 recusas do B04 e o enum `CbmRefusalCode` precisam de um teste de paridade mecânica (um gerado do outro, ou um teste que falha quando um lado cresce sozinho) — hoje a sincronização é manual e silenciosa.

---

## 4. Features a criar (candidatos a scopes — Track W)

Proposta: **Track W (Workflow)** no specs/README, entre a Track A e a Track B. Nenhum destes substitui A/B/C/D; todos são costura declarada, cada um com named exclusions. Rascunhos:

### P0 — viabilizam o ciclo mínimo end-to-end

**W01 — `session-lifecycle-tools`** (Deps: A01, A02, A05)
- In: tools MCP `union_session_open` (identity, contract_id, based_on_seq, territory; abre horizonte + sessão; restricted mode se contrato ausente), `union_session_get`, `union_session_close` (closure event tipado + trace emit + sweep). Integração: `create_horizon` sem sessão vira **deprecated no contrato da tool** (named exclusion: não reescreve horizon_pool).
- Named exclusion explícita em W01 (P0): `union_session_close` nesta fase emite o evento de encerramento (`CbmSessionClosure`) e contadores de duração/ações/recusas; a emissão do trace factual completo em schema A09 (G6) e a validação de sweep para claims órfãs (`cbm_sweep_close_session` - G5) ficam diferidas para W06/W09 (P1) como fechamento persistido.
- AC: dado um agente invocando `union_session_open` com identidade sem contrato, o host log registra restricted mode e a primeira ação irreversible-class é bloqueada (herda AC2 de A01 — agora pela superfície real).

**W02 — `action-gateway-tools`** (Deps: W01, A05, A06)
- In: tool `union_record_action(action_name, effect_class, idempotency_key?)` → `cbm_gateway_authorize_action` + contadores + ledger; mapeamento `ERR_FULL`/cap overflow → refusal tipado (resolve melhoria nº1).
- AC: ação IRREVERSIBLE sem autorização scoped → block no host log; COMPENSABLE repetida com mesma key → detectada.

**W03 — `promote-session-gate`** (Deps: W01, W02, A07, A08, A10)
- In: `promote_horizon` consulta a sessão: contest pendente → recusa; gateway block não compensado → recusa; tudo ok → promove e registra named exclusions. Named exclusion explícita: **não duplica o admission gate de âncoras da federação** — compõe.
- AC: horizonte promovido com sessão aberta em contest → refusal no host log; sem sessão (fluxo legado) → funciona como hoje, com o evento de bypass logado.

**W04 — `routing-provenance-tools`** (Deps: D03, D01)
- In: tools `classify_activity(intent)` (veredito logado no host — o routing moment torna-se auditável) e `validate_provenance(judgment)` (canon → lookup no registry; invention → exige rationale). Listas de triggers externalizadas como dado (resolve melhoria nº2).
- AC: specialty com judgment sem citação nem invenção → `PROVENANCE_UNDECLARED` via tool, logado — PBI-03 cenário 4 verificável.

**W05 — `territory-tools`** (Deps: D01, D02, D06, W04)
- In: `theme_register`/`theme_lookup` (absence como estado, PBI-01), `binding_claim(validated_by)` — enforced operator-only (`BINDING_SELF_VALIDATED`), `founding_propose` (só no closure sweep) + `founding_decide` (operator-only; aceita → tema DRAFT; recusa → `FOUNDING_PROPOSAL_DECLINED`).
- AC: binding com `validated_by=agent` → recusa (PBI-02 cenário 2); decisão de fundação aceita → entrada DRAFT no registry com provenance de sessão (PBI-06 cenário 2).

### P1 — sustentação e governança

**W06 — `union-persistence`** (Deps: A02, A09, D01, D02): contracts, theme registry, bindings e traces persistidos em `.codebase-memory/` (extensão do artifact.json ou arquivo próprio); carga no boot; sessões permanecem efêmeras por A01. Resolve G2.

**W07 — `contest-blind-tool`** (Deps: A08, A10): tool `contest_verify(evidence)` sem identidade de caller no payload — a encarnação técnica do revisor B03 como subagent isolado. Resolve G8.

**W08 — `drift-propagation-hook`** (Deps: D05, W05, W06): no refresh/index de tema com bump de versão, emitir notices para projetos com bindings; `query_orphan_bindings` exposto como query. Resolve G9.

**W09 — `workflow-e2e-suite`** (Deps: W01–W05): `tests/test_union_workflow_e2e.c` — a história do Split de Pagamento como fixture executável pela superfície de tools, ACs por host log. O exemplo da auditoria anterior deixa de ser narrativa e vira teste. (O repositório já tem o precedente: `tests/test_mcp_federation.c` e a suíte e2e de federação.)

### P2 — documentação e contrato de fronteira

**W10 — `workflow-contract-doc`** (Deps: W01–W05): `docs/workflow/` (ou PRD_V4 operacional) definindo: sequência de estações com pré/pós-condições; mapa tool→estação→scope; refusal routing table (qual código pode aparecer em qual estação e o comportamento mandado — a matriz B04 **encarnada no fluxo**); divisão de territórios operacional (abaixo, §5); protocolo do subagent revisor; formato do artifact de skill contract com `refusal_matrix`. É o documento que os autores das skills em harness-kit consumirão. Resolve G7.

**W11 — `contract-attest-tool`** (Deps: A02, B04): tool/CLI que registra e valida skill contracts (refusal_matrix completa → senão `CONTRACT_INVALID`). A ponte engine↔B06.

---

## 5. Divisão de territórios operacional (refinada, agora com encarnação)

A auditoria anterior acertou a divisão; esta acrescenta **onde cada linha vive tecnicamente**:

| Território | Skills | Encarnação | Ferramentas exigidas (ops desta auditoria) |
| --- | --- | --- | --- |
| **Diário** (cada prompt/feature) | B01, B02, B04, B05 | skill principal do agente de código | W01, W02, W04; sweep no fechamento (W01) |
| **Controle** (PR / pré-merge, risco) | B03 | subagent revisor, contexto isolado | W07 + gate W03 (B03 dispara condicional ao effect-class: ações IRREVERSIBLE ou deviação DEVE não auditada) |
| **Fechamento de ciclo** | D06 (sweep/founding) | fechamento de sessão + interação com operador | W05 (`founding_propose/decide`) |
| **Curadoria** (raro, batch) | D01/D05, B07, B08 | offline: curador/Tech Lead; cron ou fim de sprint | W05 registro de temas, W08 drift; B07/B08 **continuam fora do ciclo de implementação** (nível 5, admitidos por último, por specs/README.md:100) |

Reforço dos achados da auditoria anterior, agora com respaldo estrutural: B06 não pertence à esteira diária (é tool de plataforma — W11); B07/B08 permanecem deliberadamente por último; B03 condicional é solucionável **sem nova filosofia** — o gateway A05 já carrega a semântica de risco necessária (G4).

---

## 6. O workflow alvo (sequência canônica proposta)

```text
[skill B01] classify_activity ──► routing logado (W04)
      │ CONSULTATIVE ──► grounding só nos planos do projeto (tools atuais)
      │ SPECIALTY    ──► grounding planos + temas (bindings via W05)
      ▼
[skill B02] union_session_open (W01) ──► deliberar no horizonte
      │        cada decisão: validate_provenance (W04) → canon | invenção | DEVIATION
      │        cada ação:    record_action (W02) → gateway decide
      ▼
[revisor B03, se effect-class exige] contest_verify cego (W07)
      ▼
promote_horizon ──► gate de sessão (W03): contest pendente? gateway ok? → promove
      ▼
union_session_close (W01) ──► closure event + trace factual emitido (G6 resolvido)
      │
      └─► sweep: claims sem destino → SWEEP_INCOMPLETE
          declared_inventions de valor → FOUNDING_PROPOSAL → operador decide (W05)
```

Cada seta do diagrama é hoje uma chamada de função existente sem chamador; W01–W05 as transformam em tools. Nenhuma estação nova é inventada — esta auditoria **não propõe nenhuma capability que A/B/C/D já não especiquem**; propõe exclusivamente a costura.

---

## 7. Riscos

1. **Ordem de implementação:** W03 antes de W01/W02 é impossível (gate sem sessão). A ordem P0 é sequencial por dependência; W04/W05 podem paralelizar com W01.
2. **Compatibilidade:** `create_horizon` legado deve continuar funcionando (fluxo de federação puro) com o bypass logado — named exclusion de W03, nunca remoção silenciosa.
3. **Inflação teórica:** resistir à tentação de adicionar estações novas antes da costura (o próprio PRD_V3 §8: acréscimo sim, emenda não — aqui aplicado ao workflow: **nada novo até o existente ser alcançável**).
4. **A03/B/E marks:** W01–W05 referenciam scopes em REVIEW, não APPROVED. A specs/README diz "Nothing here is approved for implementation until reviewed" — os W-herdam essa condição e este documento deve ser revisado junto com os A/B/C/D que lhes servem de base.

---

## 8. Resumo em uma frase

A engine tem todos os órgãos e nenhum sistema circulatório: **falta expor a Track A/C/D como superfície de tools (W01–W05), dar persistência ao que é trans-sessão (W06), encarnar o revisor cego (W07) e testar o ciclo inteiro como um conto executável (W09)** — e documentar o contrato para que as skills Track B, quando nascerem em harness-kit, encontrem uma porta e não uma parede (W10).
