# SCOPE A10 — Inter-Skill Contestation

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** the Other speaks
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01, A03
> **Provenance:** PRD_V2 Station 7 (Shadow-work); PRD_V2 Axiom of Governed Belief; ADR_V1 §3.2 (CONTEST); PRD_V2 FR-8

## Problem

Any admitted belief must be contestable by any archetype, with evidence, from any direction, at any scale — contestation travels, promotion does not skip. Today contestation exists as a horizon epistemic status; it does not exist as a **typed operation any skill can submit against another's claims**, with severity-dependent effects.

## Scope

**In:**
- Typed contestation object: `source_horizon`, `target_ref` (claim or node), `evidence[]`, `severity` ∈ {informative, blocking, invalidating}.
- Evidence is mandatory: contest without evidence → refusal `CONTEST_UNPROVEN`.
- Severity effects, distinct and logged:
  - **informative** — registers the question; no transition; target remains admitted.
  - **blocking** — target claim marked `CONTESTED` with contest ref; the claim cannot participate in promotion (A07 checks contests before admitting) until resolved.
  - **invalidating** — triggers re-opening at the claiming horizon's scale; against base-graph claims, the claim becomes a **recall candidate** (feeds the recall cascade; recall itself is existing machinery — reused, not redesigned here).
- Contest resolution: revalidation of the target by its normal proof path (anchor, coverage, evidence), or withdrawal by the contesting party with record. Resolution is recorded; contests never silently expire — an expired contest is a logged event.
- Blindness applies (A08): contest verdicts are invariant to the contesting identity.

**Out:**
- The recall cascade mechanics themselves (existing recall machinery).
- The skill-side traversal behavior (B03 — the adversarial traverser).
- Severity *escalation policy* (who decides a contest is blocking) — that is the contesting skill's judgment, exercised within its own territory and subject to A08 invariance.

## Acceptance criteria

1. **Given** a contestation without evidence, **When** submitted, **Then** refusal `CONTEST_UNPROVEN` and no state change in the target.
2. **Given** an informative contest, **When** admitted, **Then** the question is registered on the target's record, the target remains `admitted`, and the contest ref is queryable (A03 surfaces it).
3. **Given** a blocking contest on a claim inside a horizon, **When** a promotion containing that claim is submitted, **Then** the promotion is refused with the contest ref in the reason — by log.
4. **Given** an invalidating contest with evidence, **When** admitted, **Then** the re-opening/recall-candidate event fires at the correct scale, and the prior belief remains legible with its scar.
5. **Given** identical contest content under N identities, **When** processed, **Then** identical verdicts (A08 harness extended to contests).
6. **Given** a resolved contest, **When** the resolution is recorded, **Then** the target's status transition (contested → admitted-with-scar or superseded) is replayable from the log alone.

## Open questions

- Contest on *assumptions* (not claims): do assumptions contest at the same severity ladder? Proposal: yes — an assumption dropped by contest is `ASSUMPTION_DROPPED`-shaped and must be resolved per A07 rule 5.
- Maximum concurrent contests per target before operator escalation? Proposal: no fixed cap; budget-ledger of the contesting horizon (A06) is the natural dam — exhaustion escalates, never blocks silently.
