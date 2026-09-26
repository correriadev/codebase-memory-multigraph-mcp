# SCOPE C07 — Aging & Orphan Queries (The ECG)

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** the union's pulse queries
> **Status:** REVIEW · **Mark:** [E] · **Deps:** C05
> **Provenance:** ADR_V1 §3.7 (Emendation II, ECG queries); PRD_V2 FR-20

## Problem

Two debts are invisible without deliberate query surface: **intention debt** (admitted claims that never realized — ideas that quietly died) and **orphan realization** (code without a claim explaining it — form without recorded why). They are the two halves of the same unpaid bill, and both must age visibly before they become silent abandonment.

## Scope

**In:**
- Query 1 — **aging intention**: admitted claims with `REALIZED_BY = ∅`, filterable by age (against `based_on_seq`/admission time), type, and owner; results carry status, consequence, and validated_by (an operator-validated intention that died is a different finding than a model assumption that died).
- Query 2 — **orphan realization**: code claims with no reverse `REALIZED_BY` from any admitted doc claim — realization without recorded intention; filterable by recency and territory.
- Both queries emit metrics suitable for the evaluator/B07 consumption (counts by age bucket, by territory) — from log-computable events only.
- Query results are read-only; they never mutate, promote, or demote anything.

**Out (named exclusions):**
- Any automated action on the findings (escalation of aging claims is an operator/individuator decision — B07 territory; this scope only makes the debt visible). The realization slot mechanics (C05).

## Acceptance criteria

1. **Given** three admitted claims with `REALIZED_BY = ∅` of ages 5d, 40d, 90d, **When** aging is queried with threshold 30d, **Then** exactly the 40d and 90d claims return, each with consequence, status, and validated_by.
2. **Given** an operator-validated intention and a model-assumed intention, both unrealized, **When** results are compared, **Then** the distinction is visible in the result set (validated_by field) — the two findings are never flattened into one number.
3. **Given** a code claim realized by a doc claim, **When** orphan query runs, **Then** the code claim is absent from results.
4. **Given** code claims without any realizing doc claim, **When** orphan query runs, **Then** they return with refs, and the count is log-computable.
5. **Given** either query, **When** executed twice without world-change, **Then** identical results (determinism) — and the query itself reads only from the index/governed store, zero narrator input.
6. **Given** the metrics emission, **When** the evaluator consumes a session trace, **Then** intention-debt and orphan counts by bucket are computable from logged events alone.

## Open questions

- Age semantics: calendar time vs. seq-distance? Proposal: both available, seq-distance primary (the Fractal Law: time is the variable, seq is its coordinate — calendar rendering is presentation).
- Should orphan realization include tests? Proposal: initially yes (tests are code); refine by finding — the ECG's first reading will be noisy, and noise is a finding, not a failure.
