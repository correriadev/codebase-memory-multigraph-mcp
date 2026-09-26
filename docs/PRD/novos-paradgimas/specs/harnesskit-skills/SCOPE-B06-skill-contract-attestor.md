# SCOPE B06 — `skill-contract-attestor`

> **Track:** B — New harness-kit skills · **Station:** cross-cutting (the union's physician)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A02 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.3 (Skill Contract); PRD_V1 §3.9 (persona inflation warning); PRD_V2 Axiom of Prohibition

## Problem

Skills must author and validate their contracts against the registry (A02), and new skills must be checked for territory overlap before admission — a persona that claims another's territory is a pathology (PRD_V1 §3.9 lineage).

## Scope

**In:**
- Skill definition implementing contract **authoring** (guiding a new skill's declaration of identity, territory, gate rights, evidence, obligations, refusal matrix) and **doctor-style validation**: submits the contract to the registry (A02), consumes the overlap report, and renders a per-field diagnostic report.
- Territory-overlap analysis: given the overlap report (A02 rule 4), the attestor must either negotiate the boundary (adjust its territory) or declare the overlap explicit with justification — overlap flagged, never silently merged.
- The attestor's own anti-totality: it validates and diagnoses; it **never admits** — admission of a contract is the registry's blind gate plus operator approval for new identities (A02 open question 2).

**Out:**
- The registry, schema, and validation refusals (A02 — Track A).
- The actual evolution/supersession of an existing contract (B07/B08 territory).
- Skill *implementation* quality — the attestor reads declarations, not behavior.

## Acceptance criteria (testable in isolation)

1. **Given** a draft contract missing a field, **When** the attestor validates, **Then** the diagnostic names the exact field and the registry refusal `CONTRACT_INVALID` is consumed (not bypassed — log shows the attestor re-submitting only after correction).
2. **Given** a valid contract, **When** validated, **Then** it is admitted to the registry and the report cites its admission ref.
3. **Given** an overlapping territory with an existing contract, **When** the overlap report returns, **Then** the attestor either adjusts and re-validates, or declares the overlap with justification that survives into the admission record — silent merge impossible by schema.
4. **Given** an unregistered skill discovered operating (restricted mode event, A01 rule 2), **When** the attestor is invoked, **Then** it can bootstrap a contract from that skill's observed behavior — with every inferred field marked provisional (Axiom of Provenance: inference declared as inference).
5. **Given** the attestor itself, **When** its own contract is validated, **Then** it holds one too — the physician is not exempt from the medicine (non-totality applies to the attestor itself).

## Open questions

- Bootstrap-from-behavior (AC 4): is inference from observed behavior admissible as `provenance` for a contract, or must the skill's author declare? Proposal: admissible as `PROPOSED` lineage only; the contract admits with the provisional mark until the author confirms (operator approval per A02).
- Version negotiation between overlapping skills: mediated by the attestor, the operator, or a contest (A10)? Proposal: operator mediation — territory is an intentional decision (Emendation I), not an epistemic one.
