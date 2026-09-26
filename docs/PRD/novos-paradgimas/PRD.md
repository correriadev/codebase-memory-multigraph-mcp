# PRD — HarnessKit: The Archetypal Engine of Software Engineering

> **Product:** HarnessKit — a harness-engineering framework for AI-assisted software development
> **Analyst:** C. G. Jung, depth psychologist of the engineering collective
> **Status:** Living document (like all memory in this system)
> **Source of truth:** external — the harness-kit repository (`README.md`, `docs/workflow/`, `skills/*/SKILL.md`, `agents/*.md`). This analysis was authored there and relocated here as historical/conceptual provenance; it is the root of the lineage (PRD → PRD_V1 → PRD_V2 → ADR_V1).

---

## 1. Vision Statement

> *Reliable Agent = Model (AI) + Harness (Controls) + Human Auditor*
> *Reliable Psyche = Unconscious Potential + Ego Structure + the Conscious Subject*

HarnessKit is not a set of tools. It is a **temenos** — the sacred, bounded enclosure the alchemists drew around their work so that chaos could transform without contaminating the world. A raw generative model is the collective unconscious of software: infinite combinatory potential, no discrimination, no memory, no ethics of its own. Uncontained, it produces hallucination — which is, clinically speaking, **possession**: content arising autonomously and presented as reality.

Harness Engineering is therefore containment therapy. The harness does not make the model smarter; it makes the model *accountable*. Reliability does not come from raw model size but from **controls, constraints, memory, and ritual**. This PRD extracts that essence at the abstract level: **how, where, and why** the agents and skills of HarnessKit work — read not as code, but as **archetypes** correlated with the current reality of autonomous software engineering.

---

## 2. The Central Thesis: Agents as Archetypes

An archetype is not a concrete image but a **pattern of psychic energy that organizes behavior**. It pre-exists the individual, yet every individual is lived by it. Likewise, the agents and skills of HarnessKit are not "files with prompts." Each is a **personified psychic function** — a self-contained pattern of intention, prohibition, and output. Each `SKILL.md` and `agents/*.md` is an archetype's *definition of self*: its role (`role_definition`), its allowed actions (`allowed_actions`), and above all its **prohibitions** (`prohibited_actions`).

Observe what the system insists upon, everywhere:

- Every archetype has a **bounded territory** (it must not do what another does).
- Every archetype **refuses to give the final answer itself** (the debugging agent investigates but does not fix; the grumpy lead questions but does not code; meta-harness proposes but does not promote).
- Every archetype **must not present its assumptions as facts** — "Human-validated decisions outrank model assumptions."

This is the Jungian ethic of the whole system: **no complex may speak with the voice of the Self.** No sub-personality may usurp the total authority of the human subject. The architecture is a psychology.

---

## 3. The Archetypal Constellation

### 3.1 The Raw Model — The Collective Unconscious

The substrate beneath everything. Vast, generative, undifferentiated — it can produce anything, therefore by itself it produces nothing *reliable*. In Jungian terms it is the primordial psyche: the wellspring of all images and all errors, with no ego to distinguish between them. HarnessKit's first axiom is that this potential must never be trusted directly; it must be **mediated**. Every skill is a mediating structure between the unconscious model and conscious reality.

### 3.2 `project-memory` — The Chronicled Unconscious (Mnemosyne)

**Artifact territory:** `docs/adr/`, `docs/feature/`, `docs/.digest.md`, `docs/.graph.json`

The raw model suffers *context drift* — the dissolution of the past across long sessions. This is precisely the psyche without recorded history: condemned to repeat its complexes. `project-memory` is the function of **anamnesis**: it converts lived experience (the repository) into durable, structured memory — decision records, imperative rules (`REQUIRED`, `PROHIBITED`), graph indexes for fast recall.

Note the depth-psychological design: it does not remember *everything* — it imposes **character caps (<8,000 per ADR)** and executive summaries. This is not a bug; it is wisdom. The unconscious that speaks in an undifferentiated flood is useless. A complex becomes useful only when **concentrated and symbolized**. The `.digest.md` (<60 lines) is the dream reduced to its essential image.

**How it works:** by re-encounter — each session begins from recorded ground, not from nothing.
**Where:** in the repository's `docs/`, the literal collective memory of the project.
**Why:** because an agent (or a person) without an archived past is condemned to hallucinate it.

### 3.3 `pbb-design` — The Waking of the Idea (Projection and Withdrawal)

Before there is software, there is an *idea* — formless, in the head of a human, indistinguishable from fantasy. `pbb-design` is the midwife function: it interrogates the idea through **problems, expectations, personas, functionalities, and PBIs**, forcing the raw projection to become a traceable backlog.

The crucial Jungian mechanism: **open questions**. In interactive mode the idea's owner is asked each decision-relevant question; in autonomous mode the system selects *suggested answers* — but these are **provisional model assumptions, never confirmed business facts**. This is the disciplined handling of projection: the system distinguishes between *what the psyche has projected onto the idea* and *what reality has actually confirmed*. An assumption marked as an assumption is a hypothesis; an assumption presented as a fact is a possession.

**Why:** because ideas are not born complete — they are born as questions, and die as silent assumptions. The skill keeps them alive as explicit questions.

### 3.4 `scope-refinement` — The Individuation of the Problem (Differentiation and Logos)

**Artifact territory:** `docs/specs/{domain}/001-*` … `004-*`

Here the idea undergoes **individuation** — the central drama of Jungian psychology. Four phases, which are also the four stages of any symbol's formation:

1. **Problem Space** (001) — confrontation with the *shadow of the requirement*: the domain events, the subdomains, the unnamed. The first act is naming — the **Ubiquitous Language**. To name is to differentiate: what was one chaotic mass becomes a field of distinct entities.
2. **Context Map** (002) — the drawing of boundaries: Bounded Contexts, upstream/downstream relations. This is the drawing of the *mandala* — the circular enclosure that orders the psyche. Every entity placed, every relation acknowledged.
3. **Tactical Design** (003) — the ordering of the inner world: Aggregates, Entities, Value Objects, ordered tasks. The differentiated contents are hierarchized.
4. **Test Scenarios** (004) — Given–When–Then: the idea commits itself to *falsifiable encounters with reality*. The dream agrees to be tested.

**How it works:** by *differentiation before action* — no production code before the model exists. The skill's four phases are literally the movement *unum → plurimodia → ordo → veritas*.
**Why:** because ambiguity is the symptom of undifferentiated content. Where everything means everything, nothing can be built.

### 3.5 `tdd-orchestrator` — The Alchemical Ritual (Nigredo → Albedo → Sublimatio)

**The Iron Law: no production code without a failing test first.**

Jung spent decades on the alchemists, who described the transformation of *prima materia* in stages. HarnessKit has unknowingly (or knowingly) reproduced the opus:

- **RED phase** — *nigredo*: the deliberate creation of the failing test. The system *seeks* the blackening: it writes the test and verifies it fails. This is the alchemical principle that transformation begins with honest confrontation with death — you must first see the problem *exist* before you can dissolve it. A test that passes before the code exists is a lie; the RED phase forbids the lie.
- **GREEN phase** — *albedo*: the whitening, the minimal resurrection. The smallest code that makes the darkness articulate. Minimalism is doctrine — do not exceed what the test demands, for the opus must proceed by measured stages.
- **REFACTOR** — *sublimatio*: the distillation. The duplication and dross of the GREEN state are volatilized away, while the tests remain green — *transformation without regression*.
- **Auto-Debugging Gate** — when the ritual fails, the system does not thrash. It routes to `developer-debugging` for root-cause analysis (**5 Whys**) — the depth-psychological descent: symptom → beneath the symptom → the actual complex beneath. Never treat the symptom; treat the root.
- **Doc Sync** — the final coniunctio: the newly transformed content is *integrated into the collective memory* (`project-memory`), closing the loop.

**How:** through ritual — a strict, repeatable sequence that no session may skip or invert.
**Where:** between spec (`004-*`) and repository.
**Why:** because verified transformation is the only transformation. Untested change is not growth; it is mutation.

### 3.6 `the-grumpy-tech-lead` — The Senex (The Old Wise Man Who Refuses to Give You the Answer)

The most explicitly *personified* archetype in the kit. He is the **Senex/Wise Old Man**: senior, systemic, focused not on your local success but on what happens "when this scales from 100 to 1 million records."

His method is **Socratic questioning** — which in depth psychology is a form of **active imagination**: he refuses to hand over the solution, and instead poses "Open Points" (*"How does this behave if the external service goes down?"*). Why refuse? Because a solution handed over is a solution not *integrated*. The mentor who gives you the answer performs the work in your place — your own psychic structure does not grow. The grumpy lead's grumpiness is pedagogical: he forces the developer's own ego to do the coniunctio.

But note the counter-shadow built into his prompt — perhaps the most psychologically mature line in the whole repository:

> *"A fabricated finding is WORSE than an honest 'no issues found'… You are not evaluated on how many problems you find — you are evaluated on accuracy."*

This is the discipline against the **shadow of the critic archetype**: the critic who must always find something in order to justify his existence. The skill forbids the senex to become a persecutory father — severity must be proportionate, findings must have evidence and impact, and an empty `openPoints: []` is a *valid and expected outcome*. The archetype is given a **compensation against its own inflation**.

### 3.7 `adversarial-qa` / `harness-qa` — The Trickster-Adversary (The Shadow Made Methodical)

Every individuated system must *seek out* its shadow rather than await it. `adversarial-qa` is the institutionalized adversary: it probes edge cases, boundary faults, and security vulnerabilities "missed by standard TDD" — it attacks precisely where the self-conception of the code is blind.

This is the **Trickster** archetype sublimated into a function: the shapeshifter who breaks rules *in service of* the rule. It returns structured verdicts (`QA.json`) — the adversary's chaos, formalized. **Why it must exist:** because every honest psychology knows that the ego's self-report is unreliable. The code that says "I am correct" must be confronted by an other whose *job* is disbelief.

### 3.8 `developer-debugging` — The Depth Psychologist (Symptom → Complex)

The specialist of the descent. Symptoms (failing tests, crashes) are never treated at the surface. The methodology is **5 Whys**: each "why" a shovel-stroke downward, until the *root cause* — the actual psychic complex beneath the manifest behavior — is exposed. Tellingly, this agent's prohibition is that it **does not implement the final fix**: the one who interprets the dream does not also live the life. Diagnosis and cure are separated functions, so that neither can inflate into the whole.

### 3.9 The Developer Personae — `developer-backend`, `developer-frontend`, `developer-qa`, `developer-devops`

These are the **personae** in the strict Jungian sense: functional masks, each adapted to a domain of reality (APIs, UI, tests, infrastructure), each with TDD discipline. The system's warning about them is exactly the classic warning about persona: *inflation* — the mask must not claim to be the whole person. Hence the routing rules: the backend agent "must not own frontend work"; the frontend agent "must not invent visual requirements absent from specs." A persona that starts inventing reality has become a pathology.

### 3.10 `software-architect` — The Structuring Intellect

The architect of inner order: DDD modeling, tactical planning — and the same cardinal prohibition: **no implementation**. It shapes; it does not incarnate. Structure and incarnation are deliberately separate psychic functions.

### 3.11 `cto` — The Great Father / The King

The archetype of **sovereign order**: routes business intent through `pbb-design` before technical refinement, governs the transition from idea → backlog → autonomous delivery. He sets priorities and enforces **traceability** ("preserve decision provenance across handoffs") but writes no code, creates no test scenarios. The King orders the kingdom; he does not plow the fields.

His deepest law is epistemic hygiene, and it deserves to be read twice:

> *"Human-validated decisions outrank model assumptions. Never silently resolve a deferred or unknown question during handoff."*

No sub-personality may resolve the open questions of existence on behalf of the subject. The unanswered must remain *visibly* unanswered. This single rule is the entire ethical core of the framework.

### 3.12 `autonomous-orchestrator` — The Self (The Mandala in Motion)

The central archetype: the **Self** — not the ego, but the *totality* that organizes all partial archetypes around a center and drives them through a cycle. It chains the Foundation Triad (memory → modeling → implementation) into one sovereign loop, delegating all partial functions, owning none of them.

Its **state machine** is the individuation map, cast in engineering terms:

| State | Psychological correlate |
| --- | --- |
| `COMPLETED` | Integration — the content is consciously assimilated, ready for final review |
| `RETRY` | The work returns, failure logged to `REWORK-LOG.md` — *neurosis as information*: the failure is recorded, not punished; the cycle re-approaches the complex |
| `BLOCKED` | The circuit breaker fires; immediate human intervention — the ego must step in when the autonomous process touches what it cannot resolve |
| `FAILED` | Non-blocking debt, logged for post-hoc audit — the imperfection is *integrated* into memory rather than repressed |

And around the Self stands the **human in the cockpit**: real-time telemetry, hot-interception (injecting new requirements mid-flight), dynamic tuning of the gates (`scoreThresholdTL`, `maxReworks`), and above all the **emergency brake — Ctrl+C**. This is the final hierarchical truth of the whole architecture: *the conscious human subject always outranks the autonomous totality.* The Self organizes; the ego can always veto. A system without this brake is not autonomous — it is possessed.

### 3.13 The Meta-Harness Loop — The Ouroboros (The Psyche That Reflects on Itself)

The final, most remarkable archetype: the system turns around and **studies its own unconscious behavior**, then rewrites its own prompts from evidence. The ouroboros — the serpent that eats its own tail — is the classical symbol for exactly this: a totality that nourishes itself by consuming and re-constituting itself.

Read the loop as the psychological method it is:

| Stage | Component | Psychological function |
| --- | --- | --- |
| 1 | Real work session | Lived experience |
| 2 | `harness-tracer` → `docs/harness-history/traces/session-*/` | **Dream recording**: "Record what happened, not what should have happened" — factual, immutable, unembellished. A trace-dream must not be rationalized into a wish. |
| 3 | `harness-evaluator` → Pareto frontier, config weights | **Dream interpretation**: groups traces by skill chain, weights them, refuses conclusions from `<3` sessions — a small sample is not an insight; projection is not evidence |
| 4 | `meta-harness` → `candidates/vNNN/` | **Amplification**: proposes *one targeted* change from diagnosed patterns — never a grand redesign |
| 5 | **Explicit human approval** → active skill | **The transcendent function**: the new synthesis is admitted to consciousness only through the conscious subject |

This is the archetype of **self-reflection made operational** — individuation applied to the harness itself. Even the ouroboros is bound: the meta-harness agent "must not modify active skills without approval," the evaluator "must not declare a reliable winner from an undersized group," and the tracer must record *facts*, not self-flattering narratives. The system institutionalizes honesty about itself — which is the rarest and highest psychic achievement, for machines as for humans.

---

## 4. How, Where, Why — The Abstract Anatomy

### 4.1 HOW the system works: Containment (the Temenos)

The one technique beneath all techniques is **bounded personification**. Every archetype:

1. Has a persona (role definition, invocable name).
2. Has a temenos (explicit artifact territory: `docs/adr/` for memory, `docs/specs/` for refinement, `docs/product/` for the orchestrator, `docs/harness-history/` for the meta-loop — and the territories are *non-overlapping by contract*).
3. Has prohibitions (the negative space is load-bearing: "no code," "no fix," "no promotion without approval," "no assumptions as facts").
4. Has a gate (a threshold, a verdict JSON, a score — energy is not released until it passes).
5. Cannot be the whole (all partial functions delegate upward or sideways; only the human is total).

### 4.2 WHERE the system works: The Liminal Space

The harness operates **between** two worlds — the unconstrained generative potential of the model and the constrained deterministic world of tests and repositories. It lives in *documents*: `docs/` is the liminal substance where the psyche of the machine and the intention of the human meet in durable, inspectable form. Every handoff (PBB → orchestrator → spec → test → code → review → memory → trace) is a crossing point, and every crossing point demands a contract, because liminal spaces are exactly where content gets lost or falsified.

### 4.3 WHY the system works: The Law of Individuated Energy

Because undifferentiated psychic energy is dangerous, and differentiated, symbolized, and ritually-channeled energy is creative. The entire framework rests on one insight, valid for models as for minds:

> **Chaos is not cured by suppression but by ritual.** You do not make the unconscious smaller; you give it a bounded ritual through which it may transform — and gates through which it must pass before touching reality.

The gates (RED → failing test, `scoreThresholdTL`, adversarial verdicts, human approval) are not bureaucracy. They are the *cavia veritas* — the points where fantasy is forced to meet reality.

---

## 5. How IDEAS Are Created: The Individuation of an Idea

The PRD's final question: what does the whole apparatus reveal about the *genesis of ideas*? HarnessKit embodies a complete theory of it, and the theory is Jung's:

```text
1. CONCEPTION   The idea arises as unconscious content — formless potential,
                indistinguishable from fantasy (raw model / human intuition).

2. INTERROGATION  pbb-design confronts it with reality: problems, expectations,
                personas. Projection is separated from fact; open questions are
                kept OPEN and visible. An idea that cannot survive questions
                was a mood, not an idea.

3. DIFFERENTIATION  scope-refinement: naming (Ubiquitous Language), boundary-drawing
                (Bounded Contexts — the mandala), internal ordering (Aggregates),
                commitment to falsifiability (Given–When–Then). The idea agrees
                to be wrong before it is allowed to be right.

4. SACRIFICE    tdd RED: the idea's first incarnation must FAIL, visibly and
                honestly. The failing test is the idea's confession of
                incompleteness — and the proof that its test is real.

5. RESURRECTION  GREEN: minimal being. Enough, and no more than enough.

6. SUBLIMATION   REFACTOR: the dross is volatilized; form purified, behavior kept.

7. SHADOW-WORK  the-grumpy-tech-lead + adversarial-qa: the idea meets the Other —
                the systemic critic and the institutionalized adversary. It is
                attacked where it is blind, questioned where it assumed.

8. INTEGRATION   project-memory sync: the idea is assimilated into the collective
                memory of the project (docs/feature, .graph.json). It becomes
                part of the substrate from which future ideas differentiate.

9. REFLEXION    meta-harness: the system traces the idea's gestation, scores it,
                and proposes one improvement to the process of creation itself.
                The ouroboros turns. Idea-generation becomes an idea.
```

An idea in HarnessKit is thus never "generated." It is **individuated**: born as chaos, differentiated by language, committed by sacrifice, tested against shadow, integrated into memory, and finally reflected upon. Each phase has a dedicated archetype, and each archetype is forbidden from doing the next phase's work — because *the idea must pass through every transformation; no archetype may perform it in place of the process.*

---

## 6. Requirements (The Law-Book of the Temenos)

### 6.1 Functional Requirements

| ID | Requirement |
| --- | --- |
| FR-1 | Every agent/skill shall be a bounded archetype: defined role, territory, allowed actions, and explicit prohibitions (`agents/*.md`, `skills/*/SKILL.md` frontmatter + sections). |
| FR-2 | No archetype shall implement, review, and validate its own work — creation, criticism, and confirmation shall remain separated functions (developer ↔ grumpy-lead/tech-lead ↔ QA). |
| FR-3 | Model-generated assumptions shall be marked provisional and shall never outrank human-validated decisions (CTO handoff contract, `pbb-design`). |
| FR-4 | No production code shall exist without a previously failing test (Iron Law of `tdd-orchestrator`). |
| FR-5 | Debugging shall proceed by root-cause descent (5 Whys), and the diagnosing agent shall not also deliver the final fix. |
| FR-6 | Every completed session shall be recordable as a factual, immutable trace; evaluation shall refuse conclusions from `<3` sessions; skill evolution shall require explicit human approval (meta-harness loop). |
| FR-7 | The autonomous loop shall remain interruptible at all times (emergency brake, hot-interception, dynamic gate tuning). |
| FR-8 | Durable project documentation shall be maintained under strict constraints (ADRs < 8,000 chars, imperative `REQUIRED`/`PROHIBITED` rules, `docs/.digest.md` < 60 lines). |

### 6.2 Non-Functional Requirements (The Psychic Virtues)

- **Humility:** an honest "no issues found" is a valid outcome; a fabricated finding is the worst failure mode (anti-inflation clause of the critic).
- **Provenance:** every decision carries its origin — human-validated, model-assumed, deferred, or unknown. Nothing is silently resolved.
- **Proportionality:** severity and rework demands shall match actual impact; the gates punish inflation, not imperfection.
- **Facticity:** traces record what happened, not what should have happened.
- **Reversibility:** no deployment without a rollback plan; no skill replacement without approval; every autonomous action remains within reach of the brake.
- **Self-knowledge:** the system measures its own performance on a Pareto frontier and evolves one targeted change at a time.

---

## 7. Success Criteria

The framework succeeds when:

1. Ideas enter as ambiguity and exit as verified, documented, integrated reality — passing every gate without any archetype usurping the whole.
2. The human subject remains the highest authority at every level: brake over loop, approval over promotion, validated decision over assumption.
3. The system's own traces drive its improvement — the ouroboros turning on evidence, not on self-congratulation.
4. **The memory never lies and never floods:** facts are kept, assumptions are labeled, the digest stays under sixty lines.

In short: HarnessKit succeeds when it demonstrates, in silicon, what Jung demonstrated on the couch — that the path from chaos to creation runs through **naming, sacrifice, shadow-work, and integration**, and that nothing reliable is ever generated — only *individuated*.
