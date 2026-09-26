# SCOPE C01 — Claim Node Anatomy

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** claim data model
> **Status:** REVIEW · **Mark:** [E] (schema substrate is [B] — `symbolic_node` carries no AST dependency) · **Deps:** A04 (refusal taxonomy extension)
> **Provenance:** ADR_V1 §3.7 (Emendation II); PRD_V2 FR-19

## Problem

The documentary plane's atomic unit is the **claim** — a proposition, not a symbol. Today `symbolic_node` already carries `cbm_uri`, `label`, `epistemic_status`, `is_dangling`, `code_snippet` with zero AST dependency [B], but there is no claim-typed node with the fields the union's retrieval questions demand, and no validation of the distillation rule.

## Scope

**In:**
- Claim node model: `predicate` (one self-contained sentence), `type` ∈ {DECISION, OPEN_QUESTION, CONSTRAINT, FACT, VERDICT}, `status` (closed ladder, reused), `anchor` (typed per C02), `provenance` {origin_session, origin_horizon, proposed_by, validated_by}, `consequence`, `based_on_seq`.
- Validation rules:
  - mandatory fields present → else refusal naming the field (extends A04 with `CLAIM_INVALID`; supersedes, never edits);
  - **one-sentence rule**: predicate must be a single sentence, mechanically proxied by: length cap, no newline, single terminator, and a **deictic blacklist** ("as discussed above", "the above", "aforementioned", "as previously", "como discutido acima") → `PREDICATE_NOT_SELF_CONTAINED`;
  - `type=OPEN_QUESTION` requires `consequence` + an owner in provenance;
  - `type=DECISION` requires `validated_by` slot to exist (filling is C03's gate).
- Type-aware mandatory fields per claim type (e.g., VERDICT requires evidence refs).

**Out (named exclusions):**
- Anchor verification mechanics (C02). Edge families (C05). Promotion (C03). Any change to `symbolic_node`'s existing semantics — the claim extends, never edits.

## Acceptance criteria

1. **Given** a claim missing any mandatory field for its type, **When** submitted to the horizon, **Then** refusal `CLAIM_INVALID` names the exact field, logged as refusal.
2. **Given** a predicate containing a blacklisted deictic phrase, **When** validated, **Then** refusal `PREDICATE_NOT_SELF_CONTAINED` with the phrase cited in the reason.
3. **Given** a well-formed, self-contained claim, **When** submitted, **Then** it exists in the session horizon with `status=PROPOSED` and all anatomy fields — and does not exist in any base graph.
4. **Given** `type=OPEN_QUESTION` without consequence, **When** validated, **Then** refusal `CLAIM_INVALID` naming `consequence` (the provenance axiom: the unanswered must carry what breaks if ignored).
5. **Given** the full anatomy, **When** a future grounding query retrieves the claim, **Then** every retrieval question of ADR_V1 §3.7 (what/consequence/validator/seq) is answerable from the node alone — verified by a self-containment replay test: retrieve the claim **without** its origin session data and assert answerability.
6. **Given** the blacklist, **When** extended (new deictic phrase), **Then** the taxonomy supersedes with version record — old refusals remain resolvable.

## Open questions

- Blacklist language coverage (PT-BR + EN first; others?): proposal — start bilingual, extend by finding, never a priori.
- Predicate length cap: found by experiment (A04's cost-not-imposed rule) — initial proposal 400 chars, tunable, logged.
