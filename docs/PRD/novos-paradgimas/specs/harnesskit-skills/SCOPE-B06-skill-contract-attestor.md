# SCOPE B06 — `skill-contract-attestor`

> **Track:** B — New harness-kit skills · **Station:** cross-cutting (the union's physician)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A02, B04 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.3 (Skill Contract); PRD_V1 §3.9 (persona inflation warning); PRD_V3 §1, §8 (Decomposition of specialist skills into universal mechanics + externalized canon)

## Problem

Skills must author and validate their contracts against the registry (A02), and new skills must be checked for territory overlap before admission. Furthermore, under PRD_V3, skills must strictly represent **universal procedural stations** (mechanics of reality) — attempting to bake language rules, architectural conventions, or specialty craft into a skill contract pollutes the procedural organ with domain data that belongs in external Thematic Knowledge Bases (PRD_V3 §1, §8).

## Scope

**In:**
- Skill definition implementing contract **authoring** (guiding a new skill's declaration of identity, territory, gate rights, evidence, obligations, refusal matrix) and **doctor-style validation**: submits the contract to the registry (A02), consumes the overlap report, and renders a per-field diagnostic report.
- **De-Specialization Defense (Craft Pollution Prevention, PRD_V3 §1, §8)**:
  - Validates that the skill contract declares a universal procedural station (e.g. GROUND, DELIBERATE, CONTEST, TRACE) and general reasoning capabilities.
  - If a draft contract embeds domain craft rules (e.g. specific framework directives, styling patterns, database conventions), the attestor flags `CRAFT_POLLUTION_DIAGNOSTIC` instructing the author to externalize the canon into a Theme Graph (`D01`) rather than creating a specialist skill persona.
- **Refusal Matrix Completeness Check (`B04`)**:
  - Asserts that the contract's `refusal_matrix` acknowledges all applicable codes from the 14-code closed taxonomy (including Track D codes).
- Territory-overlap analysis: given the overlap report (A02 rule 4), the attestor must either negotiate the boundary (adjust its territory) or declare the overlap explicit with justification.
- The attestor's own anti-totality: it validates and diagnoses; it **never admits** — admission of a contract is the registry's blind gate plus operator approval for new identities.

**Out:**
- The registry, schema, and validation refusals (A02 — Track A).
- The actual evolution/supersession of an existing contract (B07/B08 territory).
- Skill *implementation* quality — the attestor reads declarations, not behavior.

## Acceptance criteria (testable in isolation)

1. **Given** a draft contract that embeds specific domain technology standards (e.g., "React Specialist Skill"), **When** the attestor validates, **Then** a diagnostic is returned pointing that craft standards belong in a Theme Graph (`D01`), guiding refactoring toward universal mechanics.
2. **Given** a draft contract whose `refusal_matrix` omits mandatory codes it can encounter (e.g. `PROVENANCE_UNDECLARED` or `STALE_BASE`), **When** validated, **Then** the diagnostic specifies the missing codes and blocks submission.
3. **Given** a draft contract missing a required field, **When** the attestor validates, **Then** the diagnostic names the exact field and the registry refusal `CONTRACT_INVALID` is consumed.
4. **Given** a valid, universal procedural contract, **When** validated, **Then** it is admitted to the registry and the report cites its admission ref.
5. **Given** an overlapping territory with an existing contract, **When** the overlap report returns, **Then** the attestor either adjusts and re-validates, or declares the overlap with justification that survives into the admission record.
6. **Given** the attestor itself, **When** its own contract is validated, **Then** it holds one too — the physician is not exempt from the medicine.

## Open questions

- Bootstrap-from-behavior: is inference from observed behavior admissible as `provenance` for a contract, or must the skill's author declare? Proposal: admissible as `PROPOSED` lineage only; the contract admits with the provisional mark until the author confirms (operator approval per A02).
- Version negotiation between overlapping skills: mediated by the attestor, the operator, or a contest (A10)? Proposal: operator mediation — territory is an intentional decision (Emendation I), not an epistemic one.
