# SCOPE B02 — `horizon-deliberator` (DELIBERATE station)

> **Track:** B — New harness-kit skills · **Station:** DELIBERATE (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [B/E] · **Deps:** A01, A03, D02, D03, D04 · **Codes in:** harness-kit
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 3 (Differentiation, speculative form); PRD_V3 §3, §4 (Two-fork provenance & Deviation governance)

## Compatibility with CBM (2026-09-28)

`union_session_open`, `union_session_get`, `union_claim_capture`, `union_claim_resolve`, `validate_provenance`, and `union_record_action` are public tools. `create_horizon` is a separate legacy entry point; B02 must open a Union session when it needs identity, contract, sweep, and trace semantics. An unknown contract opens in restricted mode, so the skill must inspect `restricted` and `open_refusal`. `union_claim_capture` captures a conversational claim as `PROPOSED`; it is not a generic API to author arbitrary nodes with `canon_citation` fields. Keep the full typed speculative model as a future engine requirement. For today's workflow, record claim IDs, provenance validation results, open questions, and unresolved assumptions in a reviewable artifact and resolve captured claims before normal closure.

**Scope decision:** retain deliberation, but split acceptance into current tool orchestration and future typed graph authorship. Do not claim zero base writes solely from a successful `union_session_open`.

## Problem

The system must be able to think without believing its thoughts (ADR_V1 Level 2). Today harness-kit skills author real artifacts directly — proposals are indistinguishable from facts the moment they are written. Deliberation needs a space: speculative, dangling, visibly unadmitted. Furthermore, deliberation that invents craft rules silently or bypasses institutional tradition produces unreviewable divergence (PRD_V3 §1, §3).

## Scope

**In:**
- Skill definition implementing the DELIBERATE step: authors the working model as `PROPOSED`, dangling nodes inside a session horizon (A01) — never direct base-graph writes (log-verified).
- **Two-Fork Provenance Discipline (`D03`)**:
  - Every proposed specialty decision or artifact node must carry explicit craft provenance:
    1. `canon_citation`: `{ theme_id, node_uri, pinned_version }` pointing to a bound or consulted theme rule; OR
    2. `declared_invention`: `{ declared: true, rationale: string }` documenting the rationale for novel craft.
  - Omission of both triggers refusal `PROVENANCE_UNDECLARED`.
- **Deviation Formulation (`D02`)**:
  - When deliberation identifies a necessary departure from a bound normative theme rule (`DEVE`), the deliberator authors a `type=DEVIATION` proposal containing `theme_rule_node_ref`, mandatory `reason`, and `affected_scope` (preserving the scar visibly on the Idealization plane).
- **Non-Write Barrier Confinement (`D04`)**:
  - Deliberation mutations are strictly isolated to the project session horizon. Mutations targeting external theme graphs trigger refusal `TERRITORY_WRITE_FORBIDDEN`.
- Ambiguity parking: every unresolved point is recorded as an explicit open question with owner and consequence — never silently resolved; assumptions declared as assumptions.
- Horizon hygiene: dangling nodes without edges are surfaced as validation warnings (isolation detection); deliberation must connect its speculations to grounded context (A03 refs) or declare them ungrounded.

**Out:**
- The horizon mechanics (A01 — Track A).
- Promotion of the deliberated model (A07 / the invoking station's decision).
- Concretization (the CONCRETIZE station is the invoking skill's work).

## Acceptance criteria (testable in isolation)

**Staging:** current tools support session opening, claim capture/resolution, and provenance checks. AC 1-6 require the proposed typed-node and question schema before they can be asserted as written. AC 7 applies only to refusals actually returned by the host.

1. **Given** a deliberation session, **When** it completes, **Then** the host log shows zero base-graph writes and all authored nodes carry `PROPOSED` status inside the session horizon.
2. **Given** a specialty decision generated during deliberation, **When** emitted, **Then** it carries either an exact `canon_citation` or `declared_invention: true` with rationale — and no decision enters the horizon without provenance.
3. **Given** an architectural solution that departs from a bound normative theme rule (`DEVE`), **When** deliberated, **Then** an explicit `type=DEVIATION` claim proposal is authored with rule reference, reason, and scope.
4. **Given** an attempted write targeting an external theme graph during deliberation, **When** intercepted, **Then** refusal `TERRITORY_WRITE_FORBIDDEN` is emitted and the mutation is confined to the project horizon.
5. **Given** an ambiguous requirement, **When** deliberated, **Then** the horizon contains an explicit open-question record with owner and consequence, and no silent resolution appears in the trace.
6. **Given** an assumption the skill must make to proceed, **When** recorded, **Then** it is declared as assumption with provenance ("model-generated, provisional") — never presented as validated.
7. **Given** a `HORIZON_SKIP` or `PROVENANCE_UNDECLARED` refusal, **When** handled, **Then** the skill adjusts its write scale or attaches explicit rationale without retrying identically (B04 matrix).

## Open questions

- Should the deliberator support **federated deliberation** (two horizons in dialogue)? Defer — federation is an ADR_V1 §7.3 open question; single-horizon deliberation first.
- Prompt-level: how large may a speculative model grow before the horizon budget (A06) forces consolidation? Found by experiment, not imposed.
