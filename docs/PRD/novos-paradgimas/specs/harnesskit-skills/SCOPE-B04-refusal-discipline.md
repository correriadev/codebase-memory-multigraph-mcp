# SCOPE B04 — Refusal-Handling Discipline (embedded in every skill)

> **Track:** B — New harness-kit skills · **Station:** cross-cutting (no single station owns it)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A04 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.4 (Typed Refusals as API); PRD_V2 §7.2 (Typing of Refusal)

## Problem

A refusal is curriculum, not error. A skill that treats a typed refusal as a transient failure and retries identically is not resilient — it is possessed (ADR_V1 §3.4). The discipline must be embedded in every Track-B skill and testable per refusal code.

## Scope

**In:**
- A shared, distributed reference (not an invocable skill): the refusal matrix as a mandated behavior table, embedded in every Track-B skill definition and in every skill contract's `refusal_matrix` field (A02).
- Behavior matrix (initial, aligned with A04 codes):

| Code | Mandated behavior |
| --- | --- |
| `ANCHOR_NOT_FOUND` | Re-ground or withdraw; never re-submit identical |
| `STALE_BASE` | Revalidate against current `seq`; or defer with operator visibility |
| `HORIZON_SKIP` | Decompose to the proper scale; never widen the target |
| `ASSUMPTION_DROPPED` | Restore or resolve the assumption with record |
| `EVIDENCE_REQUIRED` | Produce evidence or concede — never argue |
| `BUDGET_EXHAUSTED` | Escalate; never promote, never retry-loop |
| `RETRY_IDENTICAL` | Treat as contract violation of this discipline: halt and log |
| `CONTEST_UNPROVEN` | Withdraw or evidence; the traverser never reaches it (B03) |

- Scripted scenario suite: one scenario per code asserting the mandated behavior by host log.

**Out:**
- The taxonomy itself and its host-side emission (A04 — Track A).
- Conformance certification of third-party clients (out of both cycles).

## Acceptance criteria (testable in isolation)

1. **Given** each refusal code in the matrix, **When** the scripted scenario triggers it against a Track-B skill, **Then** the host log shows the mandated behavior and no identical re-submission (per code, one AC).
2. **Given** a `RETRY_IDENTICAL` event, **When** logged against a skill, **Then** the skill halts that line of work and records the violation — the discipline treats its own violation as a finding, not a retry.
3. **Given** a skill contract (A02) for any Track-B skill, **When** validated, **Then** its `refusal_matrix` acknowledges every code it can encounter with a behavior — else `CONTRACT_INVALID`.
4. **Given** the full suite, **When** executed against a skill that *does* naively retry, **Then** the suite fails — the suite is adversarial to its subject.

## Open questions

- Matrix evolution: when A04 versions a new code, do existing skills become non-conformant immediately or at next contract supersession? Proposal: at next supersession, with the gap logged — grace with visibility, not silent drift.
- Is `RETRY_IDENTICAL` really a *discipline* violation if a human explicitly instructs the retry? Proposal: the skill may re-submit only with an explicit new-evidence claim attached; the identical path stays closed even to the subject (the gate is blind, PRD_V2).
