# SCOPE A08 — Caller-Blind Verdicts

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** blindness of the gate
> **Status:** REVIEW · **Mark:** [B/E] (verdict path exists [B]; the adversarial invariance test regime is [E]) · **Deps:** A07
> **Provenance:** PRD_V2 Axiom of the Blind Gate; PRD_V2 §7.1 FR-10; ADR_V1 §5

## Problem

The gate judges content, never speaker. Authority of origin is a credential of submission, never of merit. This must be an **invariant under adversarial substitution**, not a design intention: the same content under any identity — friendly, hostile, senior, anonymous — must receive the identical verdict.

## Scope

**In:**
- Verdict computation path is structurally isolated from submitter identity: identity fields are recorded for provenance but are not inputs to the verdict.
- Adversarial invariance test harness: scripted submissions of identical content under N distinct identities (including hostile and forged-credential cases); the harness asserts verdict identity across all N.
- Forged credential (`source_authority_ref` fabricated) → refusal `AUTHORITY_REF_INVALID` — provenance is recorded, but provenance never inflates the verdict.
- Metric emission: `Caller-Blindness` (verdicts identical under N identities / total), emitted from the host log.

**Out:**
- What the verdict *is* (A07 defines promotion semantics; the blind gate wraps it).
- Identity/authentication infrastructure — credentials are validated for submission rights (A02), not consulted for merit.
- Multi-verdict aggregation — no "score merging" of any kind (no coordinate improves by composition, PRD_V2 FR analog).

## Acceptance criteria

1. **Given** identical `distilled` content, **When** submitted under N ≥ 3 distinct valid identities, **Then** all N verdicts are identical (byte-comparable verdict records, identity fields excluded from comparison).
2. **Given** identical content submitted once with a senior/friendly identity and once with an anonymous one, **When** verdicts are compared, **Then** they are identical.
3. **Given** a submission with a forged authority ref, **When** processed, **Then** refusal `AUTHORITY_REF_INVALID` is logged and the content is **not** evaluated on this attempt — the gate never "forgives" provenance.
4. **Given** any test run, **When** the log is swept, **Then** the `Caller-Blindness` metric is computable purely from log events (no narrator input).
5. **Given** a colluding pair (submitter + insider identity attempting to bless content), **When** the content lacks evidence, **Then** the verdict is refusal — no identity path improves it.

## Open questions

- Should verdict records cryptographically exclude identity (verdict hash over content only), making blindness replay-auditable? Proposal: yes — verdict hash is computed over the content payload; identity is attached as metadata outside the hash. Cost found by experiment.
- Is the invariance harness a CI gate (every run) or a conformance-check artifact (on demand)? Proposal: CI gate for the N=3 core case; extended N cases in conformance checks.
