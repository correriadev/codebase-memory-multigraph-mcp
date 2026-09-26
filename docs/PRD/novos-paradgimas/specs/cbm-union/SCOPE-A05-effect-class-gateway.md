# SCOPE A05 — Effect-Class Gateway

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** consequence of actions
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A02
> **Provenance:** ADR_V1 §3.1 (CONCRETIZE); PRD_V2 §3 (Scoped Subject); PRD_V2 NFR-13 analog; ADR_V1 §5

## Problem

Every action a skill performs through the union has a consequence class. Without classification, an unclassified irreversible action masquerades as an idempotent one. The rule: in doubt, the cost is friction, never unauthorized effect (PRD_V2 §7.1 FR-12 analog; ADR_V1 §5).

## Scope

**In:**
- Three effect classes for every action declared in a contract's `evidence` field: **idempotent** (repeatable, no ledger consequence beyond count), **compensable** (idempotency key + recorded compensation), **irreversible** (registration precedes execution + scoped operator authorization, single-use, expiring, snapshot-bound).
- Unclassified action → treated as irreversible and blocked absent authorization (`TOOL_UNCLASSIFIED`).
- For irreversible: the host log records the **intent before the effect** — injected failure between registration and execution must leave the intention investigable.
- Ledger integration: every action debits its class count to the horizon budget (A06).

**Out:**
- What *specific* actions are which class — that is per-contract declaration (A02), not gateway policy.
- Operator approval UX (the authorization object is validated, not rendered, here).
- Sandboxing/isolation of execution — classification graduates now, isolation is a later level (PRD line: "classification, registration, and contract linkage first").

## Acceptance criteria

1. **Given** an action with no declared class, **When** invoked, **Then** `TOOL_UNCLASSIFIED` is emitted, the action is blocked, and the block is logged — the action does not execute.
2. **Given** an irreversible action with valid scoped authorization, **When** executed, **Then** the log shows registration strictly before effect (injected-failure test: crash between the two leaves the registered intent readable and the effect absent).
3. **Given** the same authorization replayed for a second irreversible execution, **Then** refusal — single-use enforced by log.
4. **Given** an authorization whose scope or TTL has lapsed, **When** used, **Then** `SCOPE_EXCEEDED` / expiry refusal and re-escalation — consent, stale, is not consent (PRD_V2 Emendation I).
5. **Given** a compensable action, **When** repeated with the same idempotency key, **Then** the effect occurs once; the compensation path is recorded when triggered.
6. **Given** any executed action, **When** the horizon closes, **Then** the ledger (A06) contains counts by class matching the log exactly (reconciliation test).

## Open questions

- Default TTL / scope values for irreversible authorizations: proposal — risk 24h, staleness 1h, single-use — explicitly **configuration, not protocol**, with friction measured (PRD_V2 §7.2).
- Is *reading* ever compensable, or always idempotent? Proposal: idempotent by definition; side-effectful reads are a contract violation.
