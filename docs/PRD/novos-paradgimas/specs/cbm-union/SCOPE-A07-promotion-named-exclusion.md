# SCOPE A07 — Promotion with Named Exclusion

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** admission
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01
> **Provenance:** PRD_V2 §2.2 (Emendation II), Station 9; ADR_V1 §3.1 (ADMIT); PRD_V2 Axiom of Named Exclusion

## Problem

Admission is the governed crossing from horizon into base graph. Today promotion validates anchors (two-tier verification); it does not demand a typed **exclusion summary** — what the promotion rejected. The integration that hides its exclusions is suppression, not admission (PRD_V2 §2.2). An honestly empty promotion is first-class and must not be treated as failure.

## Scope

**In:**
- Typed promotion object extended with `excluded_summary`: counts by type (rejected claims, failed anchors, abandoned paths), **never the content**.
- Promotion without `excluded_summary` → refusal `EXCLUSION_UNDECLARED`.
- Empty promotion (everything excluded, honestly counted) accepted as first-class with its own closure event.
- Promotion crosses exactly one horizon boundary (DAG parent); any other target → `HORIZON_SKIP`.
- Assumption conservation: assumptions present in the horizon are carried or explicitly resolved; omission → `ASSUMPTION_DROPPED` (structural comparison against the horizon's recorded assumptions).

**Out:**
- Blindness of the gate to the caller (A08).
- Anchor verification mechanics (existing two-tier anchor machinery — reused, not redesigned).
- Contested-content handling during promotion (A10 defines severity effects).

## Acceptance criteria

1. **Given** a promotion proposal lacking `excluded_summary`, **When** submitted, **Then** refusal `EXCLUSION_UNDECLARED` is logged and no base-graph state changes.
2. **Given** a promotion whose exclusions are declared (typed counts), **When** admitted, **Then** the closure/audit event carries the counts and the counts reconcile with the horizon's recorded rejections — no content preserved.
3. **Given** a horizon where every candidate was excluded, **When** an empty promotion is submitted, **Then** it is admitted as a valid terminal outcome with event `PROMOTION_EMPTY` — never recorded as failure.
4. **Given** a promotion targeting a horizon other than the DAG parent, **When** submitted, **Then** refusal `HORIZON_SKIP` and no state change in the target.
5. **Given** an assumption recorded in the horizon and absent from the proposal, **When** submitted, **Then** refusal `ASSUMPTION_DROPPED`, and the structural comparison is visible in the refusal reason.

## Open questions

- Are exclusion types a closed taxonomy (like refusals, A04) or free-form per contract? Proposal: closed initial set (`anchor_failed`, `coverage_open`, `contested`, `out_of_scope`, `budget_truncated`), versioned like A04.
- Does `budget_truncated` exclusion carry the exhaustion ref (A06) as mandatory provenance? Proposal: yes — an exclusion caused by exhaustion must name the exhaustion event.
