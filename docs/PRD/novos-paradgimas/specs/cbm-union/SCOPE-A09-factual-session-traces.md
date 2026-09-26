# SCOPE A09 — Factual Session Traces

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** the Ouroboros substrate
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01
> **Provenance:** PRD_V2 Station 10; ADR_V1 §3.1 (TRACE); PRD_V2 NFR Facticity; PRD_V2 Axiom of Testimony

## Problem

The Ouroboros feeds on facts. A trace that records what *should* have happened is a rationalized dream — worthless for evaluation and poisonous for skill evolution (B07). Traces must be factual, immutable-in-spirit (append-only), and separated from the graph (testimony is never belief).

## Scope

**In:**
- Per-session trace record, emitted at horizon closure (A01): identity, contract ref, task summary, inputs, context reads (what was grounded, A03), steps as executed, refusals received (codes, A04), debits (A06), verdicts, exclusions (A07), outcome.
- Trace schema enforces facticity: event-typed fields with host-log references; free-text narrative fields are structurally absent or clearly quarantined as `narrative` and never consumed by evaluators.
- Append-only storage, separated from the knowledge graph; derived indices may be rebuilt from traces (replay reconstructs verdicts and ledger).
- Trace completeness rule: a horizon cannot close without a trace (closure without trace = conformance failure, not an optimization).

**Out:**
- Evaluation/scoring of traces (consumer: B05/B07 and future evaluator work — out of this scope).
- Retention policy beyond closure (how long traces live) — open question.
- Cross-horizon aggregation.

## Acceptance criteria

1. **Given** a completed horizon, **When** it closes, **Then** a trace exists containing every refusal code received, every debit, the verdict, the exclusion summary, and the outcome — each field carrying a reference to its host-log event.
2. **Given** a trace, **When** a verifier diffs it against the host log for that horizon, **Then** zero events appear in the log that contradict the trace and zero trace claims lack a log reference (diff-clean).
3. **Given** the narrative-quarantine rule, **When** any evaluator-facing schema is generated, **Then** `narrative` fields are excluded by construction.
4. **Given** an abnormal closure (crash), **When** the horizon is reconstructed, **Then** a partial trace exists up to the last completed event — crash does not destroy what already happened.
5. **Given** the trace store, **When** indices are destroyed and replay is executed, **Then** verdicts and ledgers are reconstructible — the trace is the durable layer, the index is derived and losable (PRD_V2 NFR: log separate from graph).

## Open questions

- Trace granularity: per-protocol-step events vs. per-action events? Proposal: per-action with step grouping — evaluation needs step shape, debugging needs action detail.
- Do grounding reads (A03) enter the trace in full (what was returned) or by reference (what was queried + count)? Proposal: by reference + count; full returns bloat the two-currencies ledger on the trace side. Full capture is an [A] for recall-forensics later.
