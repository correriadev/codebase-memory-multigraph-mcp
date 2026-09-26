# SCOPE A06 — Horizon Budget Ledger

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** the two currencies, accounted
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01
> **Provenance:** PRD_V2 §6.2–6.3 (Two Currencies); PRD_V2 FR-14; PRD_V1 §6 (Fractal Law); ADR_V1 §3.1 (INITIATE)

## Problem

Time is the only variable — and it must be accountable per scale. Without a per-horizon budget ledger, a skill can burn unbounded tokens, attempts, and actions before anyone notices; exhaustion must escalate, never convert into belief (PRD_V2 Emendation III; FR-14).

## Scope

**In:**
- Ledger opens with the horizon (A01): declared budgets per dimension — cost (tokens/credits), time, attempts, and action counts **by effect class** (A05).
- Debit per action: every operation of the protocol debits its dimension(s); debits are visible mid-horizon (queryable, not just post-hoc).
- Exhaustion semantics: any exhausted dimension emits `BUDGET_EXHAUSTED` and escalates (suspends the horizon for operator decision). **No path from exhaustion to admission or promotion** — verified by reachability test, not by hope.
- Ledger closes into the closure event (A01) and feeds the trace (A09) — the Ouroboros consumes the numbers.
- Exclusion: the ledger never *judges* whether a budget was "worth it" — cost thresholds are found by experiment, never imposed a priori (PRD_V2 §7.2 NFR).

**Out:**
- Cross-horizon aggregation and analysis (consumer: B07 individuator, later cycles).
- Rate limiting of grounding reads (A03 open question — throttle, not budget).
- Any notion of "prohibited cost" — the threshold is a *finding*, not a gate.

## Acceptance criteria

1. **Given** an open horizon, **When** any protocol action executes, **Then** the ledger shows the debit for its dimension(s), and a mid-horizon query reflects it exactly.
2. **Given** a horizon whose attempt budget is 3, **When** the 4th attempt is requested, **Then** `BUDGET_EXHAUSTED` is emitted, the horizon is suspended/escalated, and no further action executes — by log.
3. **Given** exhaustion of any dimension, **When** a promotion is attempted, **Then** refusal — verified by a reachability analysis over the protocol state machine showing zero exhaustion→admission paths (PRD_V2 NFR-14 analog).
4. **Given** a horizon that completes without exhaustion, **When** it closes, **Then** the closure event carries the full ledger (per-dimension totals, per-class action counts), reconciling with the log to the last debit.
5. **Given** a crashing horizon (abnormal closure), **When** reconstructed, **Then** the ledger is replayable to the last recorded debit — debits precede effects for irreversible actions (A05 order).
6. **Given** any exhausted horizon, **When** an operator raises the budget explicitly, **Then** the horizon resumes with the escalation recorded as operator decision with provenance — raising a budget is a scoped, logged act, never a silent default.

## Open questions

- Per-skill default budgets: declared in the contract (A02 `obligations`) or per-domain configuration? Proposal: contract declares defaults; operator may tighten per-run; loosening is always an explicit operator act.
- Should *thinking* (model-side deliberation) be debitable separately from action cost? Proposal: yes, as a distinct dimension — the ledger should not be blind to the largest cost center.
