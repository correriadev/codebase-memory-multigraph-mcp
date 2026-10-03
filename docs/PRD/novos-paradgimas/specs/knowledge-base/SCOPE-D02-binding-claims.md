# SCOPE D02 — Binding Claims & Deviation Ledger

> **Track:** D — Thematic Knowledge Bases (`knowledge-base/`) · **Domain:** binding claims & deviation governance
> **Status:** REVIEW · **Mark:** [E] · **Deps:** C01, C03, D01
> **Provenance:** PRD_V3 §4, §7 (FR-22, FR-25); C03 (Intent validation rule)

## Problem

An incarnation's relation to an external tradition must never be an implicit assumption or an agent-invented binding. If an agent could bind an incarnation to a theme without operator mandate, it would project its own training bias as institutional law. Furthermore, adherence to bound traditions cannot be enforced by binary prohibitions without stifling project-specific differentiation; instead, strictness must be governed by cost — making every deviation an explicitly recorded, visible scar on the Idealization plane.

## Scope

**In:**
- **Binding Claim Anatomy (`type=BINDING`)**:
  - Properties: `theme_id` (refs D01), `pinned_version` (exact semver), `mode` ∈ {`NORMATIVE` (DEVE), `CONSULTED` (PODE)}, `binding_scope` (workspace, module, or global), `validated_by` (host-authenticated operator identity for `NORMATIVE`). The MCP request uses `operator_id` and `operator_token`; caller-supplied `validated_by` is ignored. `CONSULTED` validation remains optional.
- **Jurisdiction & Intent Validation**:
  - The agent is structurally barred from self-validating a binding claim. A `NORMATIVE` binding without host-verified operator credentials triggers refusal `BINDING_SELF_VALIDATED`; a claimed identity alone is not evidence of authority.
- **Deviation Claim Anatomy (`type=DEVIATION`)**:
  - Properties: `theme_id`, `theme_rule_node_ref` (canonical rule violated), `reason` (mandatory rationale), `affected_scope` (symbol/file path), `validated_by` (operator), `status` ∈ {`ACTIVE`, `RECONCILED`}.
- **Strictness by Cost (The Visible Scar)**:
  - Deviations are stored as permanent claim nodes in the Idealization plane, queryable by audits and linters.

**Out (named exclusions):**
- Physical edge resolution into the external graph (D04).
- Automatic notification when the pinned theme version moves (D05).
- Altering the blind admission gate (A08) — conformance remains a grounding and review concern, never an admission gate blocker.

## Acceptance criteria

1. **Given** a `type=BINDING` claim submitted with `validated_by=agent`, **When** evaluated by the admission pipeline, **Then** refusal `BINDING_SELF_VALIDATED` is emitted and logged.
2. **Given** an operator-validated `type=BINDING` claim with `mode=NORMATIVE`, **When** promoted, **Then** it persists in the project's Idealization plane with immutable `pinned_version`.
3. **Given** an implementation that knowingly deviates from a bound theme rule, **When** recorded, **Then** a `type=DEVIATION` claim is admitted containing `theme_rule_node_ref`, `reason`, and `affected_scope`.
4. **Given** an active deviation, **When** Idealization plane queries are performed, **Then** the deviation is returned with its reason, preventing silent divergence.
5. **Given** a `type=BINDING` claim with `mode=CONSULTED`, **When** admitted, **Then** operator validation is optional, but provenance citations are strictly advisory.

## Open questions

- Transitive bindings: if Theme A depends on Theme B, does binding Theme A auto-bind Theme B? Proposal: no auto-binding; transitive dependencies surface as consulted unless explicitly bound by the operator.
- Scope granularity: can a binding be restricted to a single sub-package (e.g. `src/frontend`)? Proposal: yes, via `binding_scope` glob pattern.
