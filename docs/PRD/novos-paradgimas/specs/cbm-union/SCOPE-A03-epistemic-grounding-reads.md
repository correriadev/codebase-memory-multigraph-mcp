# SCOPE A03 — Epistemic-Status Grounding Reads

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** retrieval as jurisdiction
> **Status:** REVIEW · **Mark:** [B/E] (query infra exists [B]; epistemic-status surfacing on every grounding read is [E]) · **Deps:** none
> **Provenance:** PRD_V2 §6.1, Emendation II; ADR_V1 §3.1 (GROUND); PRD_V2 Axiom of Governed Belief

## Problem

Grounding is the act by which a skill reads admitted context *through the graph instead of scanning the world*. Today the graph returns symbols and relations; it does not uniformly return the **epistemic status** of what it returns (admitted / proposed / contested / superseded / revoked) or the provenance ref. A grounding read that returns unadmitted content as fact is a possession event (ADR_V1 §5.1: cache possession).

## Scope

**In:**
- Grounding read API: every returned item carries `epistemic_status`, `provenance_ref`, and `based_on_seq` of the belief it reflects.
- Federated visibility of speculative content: items proposed in ephemeral horizons are returned **only when explicitly requested** and always visibly marked `PROPOSED` — never mixed into admitted results unmarked.
- Contested surfacing: items under active contestation are returned with their contest ref and severity.
- Provenance hygiene: items lacking a provenance chain are flagged `PROVENANCE_MISSING` at read time — returned, but never as fact.

**Out:**
- How status is maintained (A07 promotion, A10 contestation define the transitions).
- Budget accounting for reads (A06).
- Any ranking/merit judgment — status is returned, never aggregated into a score (PRD_V2 FR-10; "no coordinate improves by composition").

## Acceptance criteria

1. **Given** a graph containing admitted, proposed (in-horizon), contested, and superseded claims, **When** a grounding query is issued, **Then** every returned item carries a distinguishable `epistemic_status` and a resolvable `provenance_ref`.
2. **Given** speculative content in an ephemeral horizon, **When** a default grounding query is issued (no horizon scope requested), **Then** the speculative content is absent from results; **When** requested with horizon scope, **Then** it appears marked `PROPOSED`.
3. **Given** an item under blocking contestation, **When** returned, **Then** the contest ref and severity are attached.
4. **Given** an item without a provenance chain, **When** returned, **Then** it is flagged `PROVENANCE_MISSING` — verified by a query whose result set includes such an item and shows the flag.
5. **Given** any grounding read, **When** replayed against a later `seq`, **Then** the earlier read remains reconstructible (items carry the `seq` they were believed at — historical query per PRD_V2 FR-8).

## Open questions

- Should grounding reads be free (no budget debit) to keep "consultation remains cheap" (PRD_V2 §5, FR analog), or debited at a low class? Proposal: free up to a rate cap; over-rate is throttling, not refusal.
- Alias/name drift: when a symbol is renamed, do historical reads resolve to the new identity with a scar? Proposal: yes — resolution follows the scar chain, `lexical/renamed` never demotes (PRD_V2 NFR-3 analog).
