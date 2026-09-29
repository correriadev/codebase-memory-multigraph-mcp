# SCOPE B08 — `level-graduator` (rebirth / non-regression gate)

> **Track:** B — New harness-kit skills · **Station:** cross-cutting (the temenos of the spiral)
> **Status:** DEFERRED · **Mark:** [A/E] · **Deps:** A02, A07, A08, A10, B06, B07, D05 · **Codes in:** harness-kit (future)
> **Provenance:** ADR_V1 §4 (level transition as promotion); PRD_V2 Emendation III (recall, scar); ADR_V1 §4 ("no rebirth without a corpse; no corpse without a record"); PRD_V3 §4 (Separation of procedural graduation from theme drift)

## Compatibility with CBM (2026-09-28)

The public CBM workflow has no atomic skill-contract supersession, recall, or admission ceremony. `promote_horizon` promotes graph state, not a new skill version. Theme drift is a separate data concern. A skill cannot safely mark its predecessor stale or claim a rollback merely by writing a trace or promoting a horizon.

**Scope decision:** defer executable B08 until contract versioning, operator authorization, non-regression evidence, and recall are exposed as host operations. For now, B08 can specify a review checklist and test fixture; it must not mutate the registry.

## Problem

Level transitions and skill admissions are promotions, not feature flags. The previous mode must be recalled (marked stale, scarred, kept legible) before the new mode is admitted, and a non-regression gate must prove the old mode's valid outputs remain reachable in the new mode. Without this gate, the spiral becomes inflation. Furthermore, procedural skill graduation must remain cleanly decoupled from **theme version evolution** (which is governed as data drift via D05).

## Scope

**In:**
- Skill definition implementing the graduation ceremony for any Level transition or new-skill admission (B07 output):
  1. Assemble the **non-regression suite**: for every valid outcome the previous mode/organ produced, a test that it remains reachable in the new configuration.
  2. Run the suite; failures block admission (suite is adversarial to the candidate — PRD_V2 FR-15 lineage).
  3. On pass: the previous mode is **recalled with scar** — marked stale/superseded, never deleted; the new mode is admitted with provenance linking to the candidate, the suite result, and the operator approval.
  4. On fail: the candidate returns to B07 with the failure as evidence; no partial admissions.
- Non-regression suite verification includes asserting that the candidate procedural organ preserves universal protocol adherence across all bound themes (`D02`) without regression in provenance tracking.
- Rollback semantics: admission carries the rollback plan (PRD_V2 Reversibility); a graduated organ that must be withdrawn is withdrawn through recall, not deletion.
- The graduator's own anti-totality: it gates and records; it never proposes candidates (B07's territory) and never admits without the operator.

**Out:**
- Candidate generation (B07).
- Theme version evolution & drift propagation (governed via D05 re-verification notices, not B08).
- Recall cascade mechanics (existing machinery; the graduator triggers it, does not implement it).
- Protocol-level version management of the taxonomy/contracts (A04/A02 supersede their own kind).

## Acceptance criteria (testable in isolation)

1. **Given** a candidate whose non-regression suite fails, **When** graduation is attempted, **Then** admission is blocked, the failure is recorded as evidence returned to the candidate's provenance, and no partial admission exists in the log.
2. **Given** a passing suite, **When** graduation executes, **Then** the previous mode is marked stale with scar (still queryable, still legible as the belief of its time) and the new mode's admission record links candidate → suite → approval.
3. **Given** any graduation, **When** replayed from the log alone, **Then** the transition (old state, suite result, approval, new state) is fully reconstructible.
4. **Given** graduation without operator approval, **When** attempted, **Then** refusal — `WAITING_HUMAN` is a terminal state here, not a pause.
5. **Given** a withdrawn graduated organ, **When** withdrawal executes, **Then** it goes through recall with cascade over its admitted claims — its history remains answerable ("what did we believe when this organ served us").

## Open questions (this scope is [A])

- Suite authoring: who writes the non-regression tests for a *new* organ with no predecessor (first-of-its-kind)? Proposal: the suite is then the candidate's own acceptance criteria from B07, run adversarially — the corpse is the *absence* it replaces (raw scans, silent drift), and the suite proves the absence's costs are gone while nothing else regressed.
- Can two candidates graduate in one ceremony? Proposal: never — one organ, one corpse, one record at a time.
