# SCOPE B05 — `union-tracer` (TRACE station)

> **Track:** B — New harness-kit skills · **Station:** TRACE (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A09 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 10; PRD_V1 §3.13 (Ouroboros)

## Problem

At horizon closure, the session must leave a factual trace that a verifier can diff against the host log — the Ouroboros feeds on facts, and the existing harness-kit tracer lineage ("record what happened, not what should have happened") must now operate on union sessions.

## Scope

**In:**
- Skill definition implementing the TRACE step: at horizon closure (A01 event), emits the per-session trace per the A09 schema — every refusal, debit, verdict, exclusion, and outcome carrying host-log references.
- Diff-verification procedure: the tracer (or a verifier mode of it) reconciles its trace against the host log and reports discrepancies as findings about *itself* — a tracer that cannot be diffed is testimony, not trace.
- Narrative quarantine: any human-readable commentary is emitted into quarantined `narrative` fields, excluded from evaluator-facing schemas by construction (A09 rule 3).

**Out:**
- The trace storage and replay (A09 — Track A).
- Evaluation and scoring of traces (B07 consumers; scoring is a later evaluator scope, deliberately excluded here — the tracer measures nothing).
- Trace collection for non-union (legacy) sessions — the existing harness-kit tracer lineage remains untouched for its own territory.

## Acceptance criteria (testable in isolation)

1. **Given** a closed session horizon, **When** the tracer runs, **Then** the trace matches the A09 schema and every field carries a resolvable host-log reference.
2. **Given** a trace and the host log, **When** the diff-verification runs, **Then** the result is clean (zero contradictions, zero unbacked claims) or the discrepancies are reported as findings with refs.
3. **Given** any narrative commentary, **When** the trace is consumed by an evaluator-facing schema, **Then** the narrative is excluded by construction.
4. **Given** an abnormal closure, **When** the tracer runs on reconstruction, **Then** the partial trace covers events up to the last completed action — no gap, no invention.
5. **Given** the honesty rule, **When** the session's outcome was failure/refusal-heavy, **Then** the trace records those codes in full — a flattering trace is a conformance failure.

## Open questions

- Does the tracer run *inside* the session horizon (as its own budgeted action) or as a post-closure observer? Proposal: post-closure observer with its own small budget — the trace of the tracer must not perturb the traced.
- Should diff-verification be a CI gate per union run? Proposal: yes — clean diff is part of "closure without trace = failure" (A09 rule 5).
