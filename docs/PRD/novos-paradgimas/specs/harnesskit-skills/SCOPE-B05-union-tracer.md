# SCOPE B05 — `union-tracer` (TRACE station)

> **Track:** B — New harness-kit skills · **Station:** TRACE (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A09, D03, D06 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 10; PRD_V1 §3.13 (Ouroboros); PRD_V3 §7 (Success criterion 7 & D06)

## Problem

At horizon closure, the session must leave a factual trace that a verifier can diff against the host log — the Ouroboros feeds on facts, and the existing harness-kit tracer lineage ("record what happened, not what should have happened") must now operate on union sessions. Furthermore, to fulfill the Locatability of Taste (PRD_V3 §7), the trace must capture the **provenance ratio and craft adherence metrics** to substantiate future theme founding proposals (D06).

## Scope

**In:**
- Skill definition implementing the TRACE step: at horizon closure (A01 event), emits the per-session trace per the A09 schema — every refusal, debit, verdict, exclusion, and outcome carrying host-log references.
- **Tradition & Provenance Metrics (`D03`, `D06`)**:
  - `ActivityClass` classification (`CONSULTATIVE` vs `SPECIALTY`).
  - Provenance ratio: count of `CITED_CANON` vs `DECLARED_INVENTION` specialty judgments.
  - Set of theme rule nodes cited and version IDs.
  - Admitted deviation claims (`type=DEVIATION`, D02) with recorded reasons.
  - Provenance refusals encountered (`PROVENANCE_UNDECLARED`, `TERRITORY_WRITE_FORBIDDEN`, etc.).
  - Factual candidate seeds under declared invention for the closure sweep's founding proposal generator (`D06`).
- Diff-verification procedure: the tracer reconciles its trace against the host log and reports discrepancies as findings about *itself*.
- Narrative quarantine: any human-readable commentary is emitted into quarantined `narrative` fields, excluded from evaluator-facing schemas by construction (A09 rule 3).

**Out:**
- The trace storage and replay (A09 — Track A).
- Evaluation and scoring of traces (B07 consumers; scoring is a later evaluator scope).
- Trace collection for non-union (legacy) sessions.

## Acceptance criteria (testable in isolation)

1. **Given** a closed session horizon, **When** the tracer runs, **Then** the trace matches the A09 schema and every field carries a resolvable host-log reference.
2. **Given** a specialty session, **When** traced, **Then** the trace explicitly records the counts of `CITED_CANON` vs `DECLARED_INVENTION` judgments and any admitted `DEVIATION` claims.
3. **Given** a trace and the host log, **When** diff-verification runs, **Then** the result is clean (zero contradictions, zero unbacked claims) or discrepancies are reported as findings with refs.
4. **Given** declared inventions utilized during the session, **When** closure sweep runs, **Then** the trace supplies the exact empirical seed references for founding proposals (`D06`).
5. **Given** any narrative commentary, **When** the trace is consumed by an evaluator-facing schema, **Then** the narrative is excluded by construction.
6. **Given** an abnormal closure, **When** the tracer runs on reconstruction, **Then** the partial trace covers events up to the last completed action — no gap, no invention.

## Open questions

- Does the tracer run *inside* the session horizon (as its own budgeted action) or as a post-closure observer? Proposal: post-closure observer with its own small budget — the trace of the tracer must not perturb the traced.
- Should diff-verification be a CI gate per union run? Proposal: yes — clean diff is part of "closure without trace = failure" (A09 rule 5).
