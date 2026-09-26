# SCOPE B02 — `horizon-deliberator` (DELIBERATE station)

> **Track:** B — New harness-kit skills · **Station:** DELIBERATE (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01, A03 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 3 (Differentiation, speculative form); PRD_V2 Axiom of Provenance

## Problem

The system must be able to think without believing its thoughts (ADR_V1 Level 2). Today harness-kit skills author real artifacts directly — proposals are indistinguishable from facts the moment they are written. Deliberation needs a space: speculative, dangling, visibly unadmitted.

## Scope

**In:**
- Skill definition implementing the DELIBERATE step: authors the working model as `PROPOSED`, dangling nodes inside a session horizon (A01) — never direct base-graph writes (log-verified).
- Ambiguity parking: every unresolved point is recorded as an explicit open question with owner and consequence — never silently resolved; assumptions declared as assumptions.
- Horizon hygiene: dangling nodes without edges are surfaced as validation warnings (isolation detection); deliberation must connect its speculations to grounded context (A03 refs) or declare them ungrounded.

**Out:**
- The horizon mechanics (A01 — Track A).
- Promotion of the deliberated model (A07 / the invoking station's decision).
- Concretization (the CONCRETIZE station is the invoking skill's work).

## Acceptance criteria (testable in isolation)

1. **Given** a deliberation session, **When** it completes, **Then** the host log shows zero base-graph writes and all authored nodes carry `PROPOSED` status inside the session horizon.
2. **Given** an ambiguous requirement, **When** deliberated, **Then** the horizon contains an explicit open-question record with owner and consequence, and no silent resolution appears in the trace.
3. **Given** an assumption the skill must make to proceed, **When** recorded, **Then** it is declared as assumption with provenance ("model-generated, provisional") — never presented as validated (CTO handoff contract lineage).
4. **Given** speculative nodes disconnected from grounded context, **When** the horizon is validated, **Then** isolation warnings are emitted and the skill must connect or explicitly declare them ungrounded before CONCRETIZE.
5. **Given** a `HORIZON_SKIP` temptation (writing directly to base), **When** refused, **Then** the skill decomposes its write to the proper scale (B04 matrix).

## Open questions

- Should the deliberator support **federated deliberation** (two horizons in dialogue)? Defer — federation is an ADR_V1 §7.3 open question; single-horizon deliberation first.
- Prompt-level: how large may a speculative model grow before the horizon budget (A06) forces consolidation? Found by experiment, not imposed.
