# SCOPE A04 — Typed Refusal Taxonomy with Client Obligations

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** the curriculum of the union
> **Status:** REVIEW · **Mark:** [E] · **Deps:** none
> **Provenance:** ADR_V1 §3.4; PRD_V2 §7.2 (Typing of Refusal); PRD_V2 Axiom of Testimony

## Problem

Refusals are the union's API. Today refusals exist in scattered forms; they are not a closed, versioned taxonomy, they do not carry machine-readable client obligations, and identical re-submission after refusal is not detected. A refusal rendered as silence or empty success is the gravest lie available to the union (ADR_V1 §5.4).

## Scope

**In:**
- Closed, versioned refusal code set (initial): `CONTRACT_INVALID`, `CONTRACT_UNKNOWN`, `TOOL_UNCLASSIFIED`, `ANCHOR_NOT_FOUND`, `STALE_BASE`, `HORIZON_SKIP`, `ASSUMPTION_DROPPED`, `EVIDENCE_REQUIRED`, `BUDGET_EXHAUSTED`, `EXCLUSION_UNDECLARED`, `PROVENANCE_MISSING`, `CONTEST_UNPROVEN`, `SCOPE_EXCEEDED`, `RETRY_IDENTICAL`.
- Each code carries: trigger condition, machine-readable **client obligation** (the mandated behavior for the submitting skill), and log record format.
- `RETRY_IDENTICAL`: detection that a submission is byte-identical (or structurally identical) to one already refused → re-refusal with the original code attached; the re-refusal is itself logged.
- Every refusal is logged as a refusal, with code and reason — the log contains zero refusal-shaped events recorded as successes.

**Out:**
- The *skill-side* implementation of obligations (B04).
- Refusal codes' human-readable documentation UX.
- Network/transport-level error mapping — codes are protocol-level, transport is a binding.

## Acceptance criteria

1. **Given** any refusable condition in scope A01–A10, **When** triggered, **Then** the specific code (not free text) is emitted with reason and client obligation, and logged as refusal.
2. **Given** a refused submission, **When** an identical submission is re-submitted, **Then** `RETRY_IDENTICAL` is emitted referencing the original refusal — verified by log sequence.
3. **Given** the full taxonomy, **When** a log sweep is performed over any test run, **Then** zero refusals appear as silent successes and zero free-text refusals exist (100% coverage of emitted refusals by the taxonomy).
4. **Given** the taxonomy, **When** a client declares its `refusal_matrix` (A02), **Then** every code the client may encounter is either acknowledged with a mandated behavior or the contract is refused `CONTRACT_INVALID`.
5. **Given** taxonomy evolution (a new code), **When** the version advances, **Then** old codes remain resolvable — the taxonomy supersedes, it never edits (scar, per PRD_V2 FR-8).

## Open questions

- Is `RETRY_IDENTICAL` detection content-hash-based (cheap, byte-level) or structural (expensive, AST-level)? Proposal: hash-based first; structural detection deferred until traces show it paying (cost found by experiment, not imposed a priori).
- Should client obligations be advisory (log-only) or enforced (a client that violates its obligation is non-conformant)? Proposal: enforced at conformance-check level (checklist), not at runtime — the host cannot reach into the client, but it can refuse to certify it.
