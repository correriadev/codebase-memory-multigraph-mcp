# Specs — Refined Atomic Scopes (Review Pending)

Breakdown of `docs/PRD/PRD.md` → `PRD_V1.md` → `PRD_V2.md` → `ADR_V1.md` into atomic, per-context scopes. **Each scope is independently testable and independently reviewable. Nothing here is approved for implementation until reviewed.**

## Reading a scope

Every scope file carries:

- **Status** — `REVIEW` (awaiting your review) · `APPROVED` · `REJECTED` · `DEFERRED`
- **Epistemic mark** — `[B]` evidenced in existing implementations · `[E]` evolutionary, unexecuted · `[A]` open, design unresolved (per ADR_V1 §0)
- **Deps** — other scopes that must be admitted first
- **In / Out** — the atomic boundary: what the scope owns, and what it explicitly refuses to contain (named exclusion, per PRD_V2)
- **AC** — acceptance criteria in Given/When/Then, each verifiable **by host log, never by agent self-report** (PRD_V2, Axiom of Testimony)

## Track A — Memory Engine capabilities (`cbm-union/`)

Capabilities the codebase-memory-mcp side must expose or consume so skills can operate as graph-native archetypes (ADR_V1 §2–3).

| Scope | Capability | Mark | Deps | Status |
| --- | --- | --- | --- | --- |
| [A01](cbm-union/SCOPE-A01-session-horizon-protocol.md) | Session Horizon Protocol | [E] | — | REVIEW |
| [A02](cbm-union/SCOPE-A02-skill-contract-registry.md) | Skill Contract Registry | [E] | A01 | REVIEW |
| [A03](cbm-union/SCOPE-A03-epistemic-grounding-reads.md) | Epistemic-Status Grounding Reads | [B/E] | — | REVIEW |
| [A04](cbm-union/SCOPE-A04-typed-refusal-taxonomy.md) | Typed Refusal Taxonomy + Client Obligations | [E] | — | REVIEW |
| [A05](cbm-union/SCOPE-A05-effect-class-gateway.md) | Effect-Class Gateway | [E] | A02 | REVIEW |
| [A06](cbm-union/SCOPE-A06-horizon-budget-ledger.md) | Horizon Budget Ledger | [E] | A01 | REVIEW |
| [A07](cbm-union/SCOPE-A07-promotion-named-exclusion.md) | Promotion with Named Exclusion | [E] | A01 | REVIEW |
| [A08](cbm-union/SCOPE-A08-caller-blind-verdicts.md) | Caller-Blind Verdicts | [B/E] | A07 | REVIEW |
| [A09](cbm-union/SCOPE-A09-factual-session-traces.md) | Factual Session Traces | [E] | A01 | REVIEW |
| [A10](cbm-union/SCOPE-A10-inter-skill-contestation.md) | Inter-Skill Contestation | [E] | A01, A03 | REVIEW |

## Track B — New graph-native skills (`harnesskit-skills/`)

New archetypes for harness-kit, per ADR_V1 §3–4. None of these exist as skills today; all are Level 2–5 artifacts of the spiral.

| Scope | Skill / capability | Mark | Deps | Status |
| --- | --- | --- | --- | --- |
| [B01](harnesskit-skills/SCOPE-B01-graph-grounding.md) | `graph-grounding` (GROUND station) | [E] | A03 | REVIEW |
| [B02](harnesskit-skills/SCOPE-B02-horizon-deliberator.md) | `horizon-deliberator` (DELIBERATE station) | [E] | A01, A03 | REVIEW |
| [B03](harnesskit-skills/SCOPE-B03-adversarial-traverser.md) | `adversarial-traverser` (CONTEST station) | [E] | A03, A10 | REVIEW |
| [B04](harnesskit-skills/SCOPE-B04-refusal-discipline.md) | Refusal-Handling Discipline (embedded in every skill) | [E] | A04 | REVIEW |
| [B05](harnesskit-skills/SCOPE-B05-union-tracer.md) | `union-tracer` (TRACE station) | [E] | A09 | REVIEW |
| [B06](harnesskit-skills/SCOPE-B06-skill-contract-attestor.md) | `skill-contract-attestor` | [E] | A02 | REVIEW |
| [B07](harnesskit-skills/SCOPE-B07-constellation-individuator.md) | `constellation-individuator` (Level 5) | [A/E] | A09, B05 | REVIEW |
| [B08](harnesskit-skills/SCOPE-B08-level-graduator.md) | `level-graduator` (rebirth / non-regression gate) | [A/E] | B07 | REVIEW |

## Track C — Idealization Plane substrate (`doc-plane/`)

The documentary base graph of ADR_V1 §3.6–3.7 (Emendations I–II): the claim substrate, its anchors, its birth pipeline, and its drift and pulse queries. Previously sketched in conversation as A11–A13; broken here into atomic, testable scopes.

| Scope | Capability | Mark | Deps | Status |
| --- | --- | --- | --- | --- |
| [C01](doc-plane/SCOPE-C01-claim-node-anatomy.md) | Claim Node Anatomy (model + distillation validation) | [B/E] | A04 | REVIEW |
| [C02](doc-plane/SCOPE-C02-anchor-kinds.md) | Anchor Kinds (file-bytes / log-ref / derivation) | [B/E] | A09 | REVIEW |
| [C03](doc-plane/SCOPE-C03-conversational-birth-sweep.md) | Conversational Birth, Intent Validation & Closure Sweep | [E] | A01, A07, A09, C01, C02 | REVIEW |
| [C04](doc-plane/SCOPE-C04-structural-extraction-l0.md) | Structural Extraction L0 (markdown → section nodes) | [B/E] | C02 | REVIEW |
| [C05](doc-plane/SCOPE-C05-referential-resolution-edges.md) | Referential Resolution L1 & Edge Families (REALIZED_BY slot) | [E] | C01, A03 | REVIEW |
| [C06](doc-plane/SCOPE-C06-prose-drift-ladder.md) | Prose Drift Ladder (lexical / structural / gone; log-immutable) | [E] | C02, C04 | REVIEW |
| [C07](doc-plane/SCOPE-C07-aging-orphan-queries.md) | Aging & Orphan Queries (the ECG) | [E] | C05 | REVIEW |

## Track D — Thematic Knowledge Bases (`knowledge-base/`)

The third territory (Tradition) of PRD_V3: external craft themes, indirect binding claims, epistemic routing, non-write barriers, cross-graph drift propagation, and closure founding proposals.

| Scope | Capability | Mark | Deps | Status |
| --- | --- | --- | --- | --- |
| [D01](knowledge-base/SCOPE-D01-registry-graph.md) | KnowledgeBase Registry Graph & Absence Querying | [E] | A01, A04 | REVIEW |
| [D02](knowledge-base/SCOPE-D02-binding-claims.md) | Binding Claims (DEVE/PODE) & Deviation Ledger | [E] | C01, C03, D01 | REVIEW |
| [D03](knowledge-base/SCOPE-D03-routing-provenance-transparency.md) | Epistemic Routing & Provenance Transparency | [E] | A03, A04, D02 | REVIEW |
| [D04](knowledge-base/SCOPE-D04-cross-territory-typed-references.md) | Cross-Territory Typed Edges & Non-Write Barrier | [E] | C05, D01, D02 | REVIEW |
| [D05](knowledge-base/SCOPE-D05-theme-drift-propagation.md) | Theme Drift Propagation & Orphan Binding ECG Queries | [E] | C06, C07, D01, D02 | REVIEW |
| [D06](knowledge-base/SCOPE-D06-founding-proposals.md) | Closure Sweep Founding Proposals | [E] | C03, D01, D03 | REVIEW |

## Review order (suggested)

```text
A01 → A02 → A04 ──┐          (substrate: horizon, contract, refusals)
A03 ──────────────┼→ A05..A10 (gates, ledgers, verdicts, traces)
                  └→ B01..B06 (skills that consume the substrate)
                        B07 → B08  (individuation — admit last, if at all)

C01 → C02 → C04 ──┐
        C03 ←─────┤           (claim substrate: anatomy, anchors, birth)
C05 ──────────────┤           (edges + REALIZED_BY slot — after C01)
C06 ← C04, C02    │
C07 ← C05         (ECG — last: pulse queries need the slot filled)

D01 (registry) ──────┐
D02 (bindings) ←─────┼─ C01, C03
D03 (routing/prov) ←─┼─ A03, A04, D02
D04 (typed edges) ←──┼─ C05, D02
D05 (drift/orphan) ←─┼─ C06, C07, D02
D06 (founding seed) ←┘─ C03, D03
```

Additional rules for Track C: the doc plane must not be implemented before the judgment machinery it depends on (A07 admission, A08 blindness, A10 contestation) — an Idealization Plane without gates is a cache with ambitions, the exact possession ADR_V1 §5.1 refuses. Conversational birth (C03) must not be approved before A09: a LOG_REF anchor without factual traces is an anchor into narration.

Additional rules for Track D: Thematic Knowledge Bases (Tradition) must never receive direct writes from a project session (D04); binding claims must be operator-validated (D02); specialty judgments must never be silent (`PROVENANCE_UNDECLARED` refusal in D03); and founding proposals can only be offered to the operator, never auto-founded (D06).

Rules carried over from the PRD line: no scope may implement, review, and validate itself (A08 + B03 separate creation from judgment); every scope's acceptance evidence is host-log-based; exhaustion never promotes (A06); Level-5 scopes (B07/B08) must not be approved while Track A judgment scopes (A07, A08, A10) are unapproved — a system that cannot be corrected must not be allowed to grow (ADR_V1 §5.5).
