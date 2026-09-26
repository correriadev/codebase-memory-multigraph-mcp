# SCOPE C05 — Referential Resolution L1 & Edge Families

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** the claim's relations, with and without code
> **Status:** REVIEW · **Mark:** [E] · **Deps:** C01, A03
> **Provenance:** ADR_V1 §3.6 (asymmetry), §3.7 (Emendation II, edge families + extraction ladder L1)

## Problem

A claim's value grows by its relations: to other claims (rhetorical), to its birth (provenance), and to code (realization). Symbols cited in predicate text must resolve deterministically into typed `REFERENCES` edges — no LLM in this loop. The `REALIZED_BY` pending slot must exist as a first-class typed state, not an absence.

## Scope

**In:**
- **L1 referential resolution**: symbols, file paths, and `cbm://` URIs cited in claim predicates and section content resolve against the code base graph into `REFERENCES` edges (deterministic; unresolved citations are flagged `UNRESOLVED_REFERENCE`, declared — never dropped).
- **Edge families** as typed relations in the documentary plane:
  - birth: `DERIVES_FROM → log_ref/trace_ref` (mandatory for conversational claims);
  - rhetorical: `SUPPORTS`, `CONTRADICTS`, `REFINES`, `SUPERSEDES` (claim↔claim; supersession carries the scar and the successor ref);
  - realization: `REFERENCES → code`, `REALIZED_BY → code` — **doc→code only**; the code graph's structural edges are never touched.
- **REALIZED_BY pending slot**: admitted claims carry `REALIZED_BY = ∅` as a typed pending state; a realization event fills the slot (code claim refs recorded), and the claim's *status does not change* — realization is provenance, never a status transition.
- **Reverse index**: code symbol → claims referencing it; code claim → doc claim that realized into it — the storage direction is doc→code, the retrieval direction is both.

**Out (named exclusions):**
- The ECG query surface over aging/orphans (C07 — this scope stores the slot, that scope queries it). Anchor verification (C02). L0 link recording (C04 records; this scope resolves). Any code-graph write — structurally impossible by scope.

## Acceptance criteria

1. **Given** a predicate citing a resolvable symbol, **When** resolved, **Then** a `REFERENCES` edge exists to the code claim, drift-tracked per C06.
2. **Given** a predicate citing a nonexistent symbol, **When** resolved, **Then** `UNRESOLVED_REFERENCE` flag on the claim — visible, never silently dropped (an unresolved citation is information).
3. **Given** an admitted conversational claim, **When** inspected, **Then** `DERIVES_FROM` resolves to its birth event and `REALIZED_BY` shows the typed pending state `∅` — distinguishable in queries from "no slot field."
4. **Given** a realization event for a claim, **When** applied, **Then** the slot fills with the code claim refs, the claim's status is unchanged, and the fill event is logged with both refs.
5. **Given** claim A superseding claim B, **When** recorded, **Then** the `SUPERSEDES` edge exists with scar, and B remains queryable with its status transition legible in history.
6. **Given** any edge created by this scope, **When** the code base graph is inspected, **Then** zero edges point from code nodes to doc nodes — the asymmetry is invariant (adversarial: attempt a code→doc write, assert refusal).
7. **Given** a code symbol, **When** the reverse index is queried, **Then** all referencing claims return with status and anchor refs — "which documents speak of this symbol" is answered without code→doc edges.

## Open questions

- Citation detection granularity: backtick-quoted symbols only, or free-text heuristic? Proposal: explicit markup first (backticks/URIs), free-text heuristics deferred until traces show value — deterministic before probabilistic, always.
- Should `CONTRADICTS` auto-open a contestation (A10)? Proposal: no — contradiction is recorded; contestation is a separate deliberate act with evidence. Recording is free; contesting is budgeted.
