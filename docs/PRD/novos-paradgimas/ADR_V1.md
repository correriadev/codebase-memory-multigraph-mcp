# ADR V1 — The Coniunctio: Skills as Graph-Native Archetypes

> **Decision record for:** the union of the Creation Engine (skills — archetypal functions of governed creation) and the Memory Engine (the graph — jurisdiction of admitted belief)
> **Status:** PROPOSED, emended once — §3.6 (The Two Persistent Planes) added on relocation to the reference repository, after a proposal/contestation cycle whose verdict was APPROVED WITH EMENDATION. The record as a whole remains a speculative node in an ephemeral horizon: it must still be contested, verified, and admitted before it governs anything.
> **Home:** this record lives in the repository of the Memory Engine (the reference incarnation of PRD V2) — the union's substrate and its first implementation target.
> **Position:** In accordance with the Axiom of Provenance, every claim below carries an epistemic mark: **[B]** baseline, evidenced in existing implementations; **[E]** evolutionary, plausible but unexecuted; **[A]** open, design unresolved. In accordance with the Axiom of Named Exclusion, §9 declares what this record excludes. This record is written from inside the architecture it describes — the ideation below is itself an instance of Level-5 expansion (§4), and is offered to the same gates it proposes.

---

## 1. Context

Two engines exist, separately mature:

- The **Creation Engine** (PRD V1): bounded archetypes that turn chaos into verified form through the Ten Stations. Its weakness is known — creation without governed memory is possession: every session re-discovers, re-hallucinates, re-pays.
- The **Memory Engine** (PRD V2): a jurisdiction of admitted belief — anchors, provenance, blind gates, recall, historical query. Its weakness is symmetric — memory without creation is mummification: the jurisdiction perfects itself over a static world.

Neither engine closes the cycle alone. The union is not an integration task (adapters, glue, tool calls). It is a **coniunctio**: a single substrate in which archetypes *deliberate through* governed memory, and governed memory is *authored by* archetypes — under one shared coordinate system, one ledger of time, and one blind gate.

---

## 2. Decision

> **We decide that skills shall be graph-native archetypes**: bounded functions whose every invocation opens a governed horizon; whose every input is grounded in admitted belief rather than raw scan; whose every output is a claim with provenance and a re-checkable anchor; whose every judgment passes through gates it does not host; whose every session leaves a factual trace; whose evolution is itself an admission; and whose communication with every other archetype occurs **exclusively through the graph** — never through private channels.

Three structural commitments follow:

1. **The graph is the medium of inter-archetype communication.** No skill hands artifacts directly to another skill. All handoffs occur as claims promoted into horizons that other skills read through the jurisdiction. Private channels are uncontracted liminal spaces — exactly where content is lost or falsified (PRD V1 §5.2).
2. **The skill is a citizen, not a client.** Skills do not merely *query* the memory engine; they participate in its epistemic life: they propose, contest, recall (with evidence), and consume recall cascades — always through typed operations, never by direct edit.
3. **Neither engine subsumes the other.** The memory engine holds jurisdiction but never authors; the creation engine authors but never judges itself. The union preserves non-totality in both directions. **[B]**

---

## 3. The Union Architecture

### 3.1 The Skill Invocation Protocol (runtime loop)

Every skill invocation is a session horizon — a bounded temenos with its own budget ledger. The protocol **[E]**:

```text
INITIATE    Open the horizon. Declare skill identity, territory, effect
            classes of intended actions, and the belief-sequence this work
            is based on (based_on_seq). Budget ledger opens.

GROUND      Read admitted context through the graph — never by raw scan.
            Retrieve what is believed, what is contested, what is assumed
            and by whom. Unadmitted content arrives visibly unadmitted.

DELIBERATE  Form the working model as PROPOSED nodes inside the horizon —
            speculative, dangling, clearly marked. Ambiguity is parked as
            explicit open questions, never silently resolved (Axiom of
            Provenance).

CONCRETIZE  Perform the station's work in the world: code, text, tests.
            Every artifact is anchored (re-checkable in the real) and
            claims-bearing. Irreversible actions: registration precedes
            execution, with scoped operator authorization — single-use,
            expiring, snapshot-bound.

CONTEST     The adversarial counterparts read the horizon through the graph
            and may challenge any claim with evidence. Contention travels
            any edge; promotion never skips a boundary.

VERIFY      The gate reads the host log — the skill's narration is not
            evidence. Refusals return typed; a typed refusal is part of
            the skill's API (§3.4).

ADMIT       Surviving claims promote across one boundary, with exclusion
            summary: what was rejected is counted and typed, never silent.

TRACE       The horizon closes with a factual trace in the ledger:
            what happened, not what should have. The Ouroboros feeds.
            An honestly empty outcome is first-class.
```

### 3.2 Station-to-Graph Mapping

The Ten Stations of PRD V2, expressed as graph operations — the same fractal at the scale of one skill call **[B, partially — query/propose/promote exist; the full protocol is E]**:

| Station | Graph operation |
| --- | --- |
| 1. Conception | Horizon initiation with seed and budget |
| 2. Interrogation | Grounded queries; retrieval of assumptions, contested claims, open questions with owners |
| 3. Differentiation | Speculative (proposed, dangling) nodes inside the horizon — never direct base-graph writes |
| 4. Sacrifice | Falsifiable claims anchored to real artifacts before any concretization |
| 5–6. Resurrection, Sublimation | Minimal concretization; restructuring recorded as claim supersession with scar, never edit |
| 7. Shadow-work | Adversarial skills traverse the horizon's claims; verdicts blind to speaker |
| 8. Verification | Gate verdicts read from host log; effect-class accounting debits the ledger |
| 9. Admission | Typed promotion with provenance, coverage, exclusion summary |
| 10. Reflexion | Trace recorded; evaluator reads traces; recall cascades propagate through the derivation the skill's own claims registered |

The decisive property: **a skill session and a memory transaction are the same event, observed from two sides.** One ledger, one clock, one jurisdiction.

### 3.3 The Skill Contract

A skill is admitted to the union only by declaring — machine-readably — its archetypal boundary **[E]**:

```yaml
identity:        name, station(s) of competence, scales of operation
territory:       claim types and node labels it may author
gate_rights:     boundaries it may SUBMIT to (never judge)
evidence:        anchor kinds it can produce; effect classes of its actions
obligations:     exclusion summary on every promotion; trace on every closure
refusal_matrix:  typed refusals it must handle, and the mandated behavior per code
provenance:      lineage of the skill's own evolution (see §4, Level 4)
```

A skill without a declared contract is treated as an unclassified action: refused at the irreversible class until declared (in doubt, friction — never unauthorized effect).

### 3.4 Typed Refusals as API

The memory engine's refusals are not errors; they are **the curriculum of the union** **[B]**. Each typed refusal carries a client obligation the skill must implement:

- `ANCHOR_NOT_FOUND` → the claim's ground is gone; re-ground or withdraw; never re-submit identical.
- `STALE_BASE` → the world moved; revalidate against current belief, or surface the deferral to the operator.
- `HORIZON_SKIP` → a boundary was bypassed; decompose to the proper scale.
- `ASSUMPTION_DROPPED` → an inherited assumption vanished; restore or resolve it with record.
- `EVIDENCE_REQUIRED` → assertion without anchor; produce evidence or concede.
- `BUDGET_EXHAUSTED` → escalate; never promote, never retry-loop.

A skill that treats a refusal as a transient error and retries identically is not resilient — it is possessed.

### 3.5 The Shared Clock

Every claim, proposal, authorization, and trace carries `based_on_seq`. Skills reason over the sequence explicitly: staleness is detectable before it is fatal, worst-case contamination is computable, and "what did we hold when this skill last ran" is answerable across the whole union **[B]**. Time remains the only variable — but it is now a *coordinated* variable: one clock for creation and memory, which is what makes the union one psyche rather than two systems.

### 3.6 The Two Persistent Planes (Emendation I — accepted)

*Added after a contestation cycle on this record's relocation to the reference repository. Proposal: split the persistent plane into two base graphs by nature. Verdict: APPROVED WITH EMENDATION — the split and the asymmetric references stand; the emendation below preserves the recall ceiling between the planes. Marks: [B] where evidenced today, [E] where evolutionary.*

The persistent plane differentiates into **two natures** — the same fractal, two phases of the coniunctio:

| | **Realization Plane** (code base graph) | **Idealization Plane** (documentation base graph — new) |
| --- | --- | --- |
| Nature | The idea **incarnate**: solidified, verified form | The idea **before and after incarnation**: intention, design, decision |
| Emitting stations | 4–9 (Sacrifice → Admission) | 1–3, 10 (Conception, Interrogation, Differentiation, Reflexion) |
| Gate nature | Executable verification + anchor in the real (AST, file, test) | Internal coherence + anchors that resolve + operator validation of intent (intent is never epistemic — Emendation I of PRD V2) |
| Machinery | horizons, contestation, admission, recall, provenance — the same pipeline at every scale (FR-14) | identical |

**Structural asymmetry.** Documentary claims may reference code claims (typed edges: `ANCHORS`, `REFERENCES`, `REALIZED_BY`); code claims never carry structural edges into documentation. *Prose can be about code; code is never about prose.* The code graph remains self-contained — judged only by the real, never by prose — so a recalled document cannot cascade into the symbol graph through edges, and contested prose cannot contaminate code admission. **[E]**

**The emendation: provenance is not an edge.** The prohibition applies to *structural* edges only. Every code claim carries a **derivation chain in its provenance** — the birth certificate naming the spec, horizon, and session that generated it. Recall cascades over recorded derivation (it is both the input to propagation and the ceiling of recall); without it, a recalled spec cannot reach the code it generated, and "what did we believe when we wrote this" dies for code — the contamination would only change its hiding place. Provenance lives in claim metadata; it is not a graph edge, and the asymmetry of storage is never violated. **[E]**

**Status and realization are different coordinates.** Documentary claims use the same closed status ladder as all claims (proposed / admitted / contested / superseded / revoked). "This document became code" is not a status — it is the typed edge `REALIZED_BY`, a reference held by the idealization plane pointing at what it became. Coordinates never collapse into one scale. **[E]**

**Two creation directions, both through the pipeline.** The Idealization Plane admits content bottom-up (distilled from the code graph — grounded authoring) and top-down (born from operator prose, declared `ungrounded` by choice, never pretending a ground it lacks). Prose → documentation base → code base crosses one boundary at a time; direct prose-to-code promotion is a `HORIZON_SKIP`. **[E]**

**Cross-plane drift.** Documentary anchors targeting code symbols inherit the drift ladder: `structural` suspends, `gone` demotes, `lexical/renamed` does not demote. Without it, the first rename breaks the documentary jurisdiction silently and `ANCHOR_NOT_FOUND` becomes noise. **[E]**

**Storage direction ≠ retrieval direction.** "Which documents speak of this symbol?" remains answerable by indexing documentary edges by their code targets — the asymmetry governs where edges live, never what can be queried. **[E]**

**Expansive consequence.** With a governed Idealization Plane, the union's self-knowledge — its PRDs, ADRs, decisions, and debate records — becomes contestable, recallable, queryable memory: Level 5 gains a second substrate, the Ouroboros feeds on two planes, and the system acquires the memory of *why*, including the governed ability to discover it was wrong about *why*. The document lineage that produced this record is the first tenant of the Idealization Plane. **[E]**

### 3.7 The Claim Substrate (Emendation II — accepted)

*Added after an operator-guided exercise of the conversational claim lifecycle (birth, relations, anatomy). Verdict: the substrate is prose-native by construction — the existing abstractions (symbolic nodes, typed edges, epistemic status, spec parser, FTS) carry no AST dependency. Marks: [B] evidenced today, [E] evolutionary.*

**The atomic unit is the claim.** The documentary plane's node is not a symbol but a proposition: a predicate with epistemic status, an anchor, provenance, and consequence. **[B — symbolic_node's schema is already claim-shaped]**

**Three birth natures, three anchor kinds.** A claim enters the world by one of three doors, and the door determines the anchor's physics:

| Birth | Content | Anchor kind | Drift physics |
| --- | --- | --- | --- |
| **A — prose-as-file** (specs, ADRs) | markdown documents | `(file_path, byte_range, expected_text)` — existing two-tier machinery | drifts mechanically; ladder of §3.6 applies |
| **B — prose-as-conversation** (born in dialogue, no file) | operator decisions, verdicts, open questions | `(session_id, horizon_id, event_seq)` — **the append-only host log IS the filesystem of conversational prose** | **cannot drift** (log is immutable); change is epistemic only (supersession/recall with scar) |
| **C — prose-as-distillate** (bottom-up: digests, micrographs) | derived from code-graph queries | the generating query (recomputable) | invalidated when the generator's output changes |

The asymmetry is decisive: file-born claims change *mechanically* (drift); conversation-born claims change only *epistemically* (supersession). The log-anchored claim has the most stable ground in the entire union — append-only beats mutable bytes. **[E]**

**Node anatomy — every field earns its place by a future question.** A claim node carries: `predicate` (one self-contained sentence), `type` (DECISION / OPEN_QUESTION / CONSTRAINT / FACT / VERDICT), `status` (closed ladder), `anchor` (per birth nature), `provenance` (origin session, proposed_by, **validated_by** — operator or ∅), `consequence` (what breaks if ignored), `based_on_seq`, and a **typed realization slot** (`REALIZED_BY`, initially ∅). Fields map to retrieval questions: the predicate answers *"what do we believe about X?"*; `validated_by` answers *"operator decision or model assumption?"*; the realization slot answers *"did this ever become real?"* — the two-currency audit. **[E]**

**The distillation rule (one-sentence rule).** The node is the distillate; the paragraph stays in the trace. A predicate that cannot be retrieved without the conversation that produced it ("as discussed above…") is refused — it is noise with provenance, not memory. Refusal code: `PREDICATE_NOT_SELF_CONTAINED` (extends the A04 taxonomy; supersedes, never edits). **[E]**

**Edge families.** A claim relates by three families, only the last requiring code to exist: (1) **birth** — `DERIVES_FROM → log_ref`, always present for conversational claims; (2) **rhetorical** — `SUPPORTS`, `CONTRADICTS`, `REFINES`, `SUPERSEDES`, claim↔claim, zero code; (3) **realization** — `REFERENCES` (talks *about* a symbol) and `REALIZED_BY` (became code), doc→code only, per the §3.6 asymmetry. `REALIZED_BY = ∅` is not a broken dangling state — it is a **typed pending slot** awaiting incarnation. **[E]**

**The ECG queries.** Two first-class queries read the union's pulse: *aging intention* (admitted claims with `REALIZED_BY = ∅` older than N — intention that never happened, visible before it becomes silent abandonment) and *orphan realization* (code without a claim explaining it — realization without recorded intention). Together they expose both halves of unpaid debt. **[E]**

**Claim-capture: two layers.** When does prose enter the substrate? (1) The **capture reflex** — embedded policy in every Track-B skill, governed by the single mechanical test of reuse: *what will influence a decision after the step that created it is memory*; triggered by operator decisions, binding commitments, expensive-to-rediscover findings, and rendered verdicts; never by exploration or dead ends (those become exclusion *counts*). (2) The **closure sweep** — mechanical backstop: at session close, every PROPOSED claim has a mandatory destination (promoted, converted to open question with owner, or discarded with exclusion counts); **closure without sweep is a conformance failure**. The reflex may fail in judgment; the sweep cannot fail by construction. **[E]**

**Intent validation is the operator's; coherence is the gate's.** DECISION and OPEN_QUESTION claims require `validated_by: operator` before promotion — the agent never self-validates intent. FACT and CONSTRAINT claims admit on evidence through the blind gate. Emendation I of PRD_V2, applied to a sentence of chat. **[E]**

**Extraction ladder.** How claims leave prose: **L0 structural** (headings/sections/links — deterministic parser, admits directly; syntax, not semantics); **L1 referential** (symbols/paths cited in predicate text auto-resolve against the code graph into `REFERENCES` edges — deterministic); **L2 semantic** (the agent proposes claims — born PROPOSED in a horizon, subject to the full pipeline; LLM extraction directly into the base graph is possession). The documentary plane's semantic indexer is the agent itself, governed by the same temenos it serves. **[E]**

---

## 4. The Spiral of Expanding Consciousness

The union is not a fixed integration. It is a **progressive reification spiral** — each full turn widens what the system can hold, deepens what it can question, and differentiates what it can do. This is the technical meaning of "the rebirth of an expanding consciousness": consciousness = admitted belief × contestable past × differentiated function, and each turn all three factors grow — but only through a death **[E]**.

A level transition is not a feature flag. It is a **promotion**: the previous mode of operation must be recalled (marked stale, scarred, kept legible) before the new mode is admitted, and the transition gate runs a non-regression test proving the old mode's valid outputs remain reachable in the new mode. No rebirth without a corpse; no corpse without a record.

```text
LEVEL 0 — DREAMING        Memory as text. Skills read documents; belief is
                           prose, authority is trust in the writer.
                           Failure mode: possession by narrative.

LEVEL 1 — REMEMBERING     Memory as structure. Skills query the graph;
                           grounding replaces scanning. Belief acquires
                           anchors and provenance. [B]

LEVEL 2 — IMAGINING       Memory as deliberation. Skills author speculative
                           horizons; proposals live beside admitted truth,
                           visibly unadmitted. The system can now think
                           without believing its thoughts. [B]

LEVEL 3 — JUDGING          Memory as jurisdiction. Skills contest and recall
                           with evidence; adversarial peers traverse each
                           other's horizons; correction is cascade, not
                           edit. The system can now change its mind
                           governedly. [B]

LEVEL 4 — REFLECTING       Memory as self-knowledge. Traces become the
                           substrate of skill evolution: the evaluator
                           reads the union's own behavior, proposes one
                           targeted change, and the change is admitted
                           through the same gates as any claim. The
                           skills begin rewriting themselves — always
                           through human-approved promotion. [E]

LEVEL 5 — INDIVIDUATING    Memory as genesis. Recurring trace patterns
                           with no owning archetype are surfaced as
                           candidates for a new skill: the union
                           differentiates new organs from its own
                           lived experience. The constellation grows
                           from practice, not from design. The system
                           can now create creators. [A — open]
```

Level 5 is the open horizon of this record. What is proposed there is precisely what this record enacts: a pattern of work (this dialogue, these documents) differentiating itself into a new function (this ADR). If the pattern is admitted, the union has begun to individuate. The criterion of admission is not elegance — it is the non-regression gate: the new organ must leave every valid old behavior reachable.

---

## 5. What the Union Must Refuse (Shadow of the Coniunctio)

The union's specific possessions, each named and terminal by design **[E]**:

1. **Cache possession** — treating the graph as a cache (retrieval without jurisdiction): reading without anchors, trusting unadmitted content as fact. Refusal: provenance checks on every grounding read.
2. **Self-admission** — a skill judging its own claims, or a memory engine authoring content. The gate is hosted by neither; both submit.
3. **Testimony** — a skill's narration of success counted as verification. Evidence is host-log, always.
4. **Silent refusals** — a refusal rendered as empty success. The gravest lie available to the union.
5. **Level skipping** — Level 4 (self-evolution) attempted before Level 3 (contestable belief) is admitted. A system that cannot be corrected must not be allowed to grow; a system that cannot be audited must not be allowed to evolve.
6. **The unbound Ouroboros** — skill evolution without human-approved promotion, or recall without evidence. The serpent that eats its tail outside the temenos devours the psyche.
7. **Undistilled capture** — a memory unit that cannot be retrieved without the context that produced it ("as discussed above") is not memory; it is noise with provenance. Refused at validation (`PREDICATE_NOT_SELF_CONTAINED`); and a session that closes without sweeping its PROPOSED claims to their destinations has failed conformance, not saved time.

---

## 6. Consequences

**Positive.** Sessions stop re-paying discovery (FR-18); handoffs become inspectable and contestable; skill behavior becomes measurable from its own traces; the constellation can grow from practice; the two currencies (time, memory) reconcile in one ledger; the human subject audits one jurisdiction instead of two unverifiable systems.

**Negative.** Every skill invocation acquires protocol overhead; the refusal matrix becomes a compatibility surface that must version; adversarial traversal multiplies cost per cycle; Level 5 creates an unbounded differentiation problem — the constellation can inflate. Mitigation is already in the axioms: budgets per horizon, exhaustion escalates (never promotes), and every new archetype must justify its territory against overlap with existing ones (a persona that claims another's territory is a pathology — PRD V1 §3.9).

**Neutral, but decisive.** The union makes the whole system slower per operation and faster per epoch. That trade is the entire thesis: **reliability comes from controls, constraints, memory, and ritual — not from the size of the generative substrate.**

---

## 7. Open Questions (kept visibly open)

1. **Inter-skill contention timing** — when two archetypes contest each other's claims in parallel horizons, is the resolution adversarial traversal, operator escalation, or both under rule? **[A]**
2. **Skill retirement** — the union differentiates new archetypes, but what is the governed path for the *death* of a skill? Recall of its admitted claims is defined; recall of its identity is not. **[A]**
3. **Federated horizons** — when two unions (two projects, two organizations) meet: is cross-union promotion one boundary at a time at a higher scale (the fractal continuing), or a new topology? **[A]**
4. **The ledger of the subject** — budgets govern horizons; what governs the operator's attention? The scoped subject has consent decay; it has no attention ledger. **[A]**

## 8. Compliance of This Record

Per the Axiom of Testimony, this record claims no verification of itself. Its [B] marks point to behaviors evidenced in existing implementations; its [E] marks are proposals; its [A] marks are honestly unresolved. Per the Axiom of the Scoped Subject, its authors hold no epistemic privilege over it: this record is submitted, not spoken. Its admission — like every promotion in the union it describes — requires an adversarial read, a blind verdict, and a human approval that has not yet been given.

Emendation I (§3.6) followed the same discipline: proposed (the two-planes split), contested (against PRD_V1, PRD_V2, and this record), verdict rendered (APPROVED WITH EMENDATION — the provenance/edge distinction), and only then emended — with the verdict recorded here rather than silently absorbed. The emendation does not grant the record admission; it supersedes one section with a scar.

Emendation II (§3.7) followed the same cycle through an operator-guided exercise: the conversational claim was walked through birth, contestation, and promotion in a concrete scenario before the section was admitted; the two-planes verdict (Emendation I) was re-tested against it and held — birth family B's log-anchor is the strongest ground in the union precisely because Emendation I made the audit log append-only by design.

## 9. Exclusion Summary

In accordance with the Axiom of Named Exclusion, this record declares what it deliberately does not contain:

| Excluded | Count | Reason |
| --- | --- | --- |
| Tool names, file paths, implementation bindings | all | Abstraction is the contract; bindings are per-implementation |
| Conformance levels and protocol surface for third-party skills | 1 class | Real, but premature until Levels 3–4 are admitted; parked as open |
| Performance targets for adversarial traversal | 1 class | Cost thresholds are found by experiment, never imposed a priori (PRD V2 §6.3) |
| The death-of-a-skill design | 1 | Open question §7.2; refusing to resolve it silently is itself an instance of the axioms |

---

*Provenance: PRD (concrete) → PRD_V1 (abstraction) → PRD_V2 (synthesis) → ADR_V1 (ideation, this record). Each derivation crossed one boundary. This record is the first artifact authored under the protocol it proposes: proposed in an ephemeral horizon, dangling by design, awaiting contestation. Relocated from the Creation Engine's repository to the Memory Engine's repository — the implementation target — and emended twice there (§3.6 Two Persistent Planes; §3.7 Claim Substrate), each after a proposal/verdict cycle recorded in §8. Per its own §3.6–3.7, this lineage is the first content of the Idealization Plane: born as prose-conversation, anchored to the dialogue that produced it, awaiting realization.*
