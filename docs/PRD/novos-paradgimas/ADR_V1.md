# ADR V1 — The Coniunctio: Skills as Graph-Native Archetypes

> **Decision record for:** the union of the Creation Engine (skills — archetypal functions of governed creation) and the Memory Engine (the graph — jurisdiction of admitted belief)
> **Status:** PROPOSED — this document is itself a speculative node in an ephemeral horizon. It must be contested, verified, and admitted before it governs anything.
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

## 9. Exclusion Summary

In accordance with the Axiom of Named Exclusion, this record declares what it deliberately does not contain:

| Excluded | Count | Reason |
| --- | --- | --- |
| Tool names, file paths, implementation bindings | all | Abstraction is the contract; bindings are per-implementation |
| Conformance levels and protocol surface for third-party skills | 1 class | Real, but premature until Levels 3–4 are admitted; parked as open |
| Performance targets for adversarial traversal | 1 class | Cost thresholds are found by experiment, never imposed a priori (PRD V2 §6.3) |
| The death-of-a-skill design | 1 | Open question §7.2; refusing to resolve it silently is itself an instance of the axioms |

---

*Provenance: PRD (concrete) → PRD_V1 (abstraction) → PRD_V2 (synthesis) → ADR_V1 (ideation, this record). Each derivation crossed one boundary. This record is the first artifact authored under the protocol it proposes: proposed in an ephemeral horizon, dangling by design, awaiting contestation.*
