# SCOPE A02 — Skill Contract Registry

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** archetype boundary declaration
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01
> **Provenance:** ADR_V1 §3.3 (Skill Contract); PRD_V2 Axiom of Prohibition; PRD_V1 FR-1

## Problem

A skill enters the union only by declaring its archetypal boundary machine-readably. Without a registry, territories are prose, overlap is undetectable (persona inflation), and effect classes are unclaimed. The registry is what makes "every function is a bounded archetype" enforceable instead of aspirational.

## Scope

**In:**
- Machine-readable contract schema: `identity`, `territory` (claim types / node labels it may author), `gate_rights` (boundaries it may SUBMIT to — never judge), `evidence` (anchor kinds, effect classes of its actions), `obligations` (exclusion summary on promotion, trace on closure), `refusal_matrix` (acknowledged codes + mandated behaviors), `provenance` (lineage of the skill's own evolution).
- Validation on submission: missing/invalid field → typed refusal naming the field.
- Registry query: given a skill identity, return its contract or `CONTRACT_UNKNOWN`.
- Territory-overlap report: given a new contract, list overlapping territories with existing registered contracts (used by B06 and B07).

**Out:**
- Contract *authoring* guidance (B06, harness-kit side).
- Enforcement of behavior inside a skill (B04).
- Revocation/retirement of contracts — open question (see below), out of this scope.

## Acceptance criteria

1. **Given** a contract missing any mandatory field, **When** submitted, **Then** refusal `CONTRACT_INVALID` is emitted naming the exact missing field, logged as refusal.
2. **Given** a valid contract, **When** submitted, **Then** it is admitted and referenced by the horizon protocol (A01 opens without restricted mode).
3. **Given** an unregistered identity, **When** the registry is queried, **Then** `CONTRACT_UNKNOWN` returns — and A01's restricted mode is the operative consequence.
4. **Given** a new contract whose territory overlaps an existing one, **When** submitted, **Then** the overlap report is generated and attached to the admission record; overlap is flagged, not silently merged.
5. **Given** an unclassified action declared in `evidence` (an effect class left empty), **When** validated, **Then** refusal `TOOL_UNCLASSIFIED` per A05 semantics — the contract must classify everything it declares.

## Open questions

- Contract versioning: does evolving a skill (Level 4) mean superseding its contract with a scar, or a new identity? Proposal: supersession with scar, same identity — retirement stays open (ADR_V1 §7.2).
- Who admits contracts into the registry — blind gate only, or scoped operator approval for new identities? Proposal: blind gate for schema validity; operator approval only for the *admission of a new identity* (an archetype is born through the subject, per Level-5).
