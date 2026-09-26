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

## Review order (suggested)

```text
A01 → A02 → A04 ──┐          (substrate: horizon, contract, refusals)
A03 ──────────────┼→ A05..A10 (gates, ledgers, verdicts, traces)
                  └→ B01..B06 (skills that consume the substrate)
                        B07 → B08  (individuation — admit last, if at all)
```

Rules carried over from the PRD line: no scope may implement, review, and validate itself (A08 + B03 separate creation from judgment); every scope's acceptance evidence is host-log-based; exhaustion never promotes (A06); Level-5 scopes (B07/B08) must not be approved while Track A judgment scopes (A07, A08, A10) are unapproved — a system that cannot be corrected must not be allowed to grow (ADR_V1 §5.5).
