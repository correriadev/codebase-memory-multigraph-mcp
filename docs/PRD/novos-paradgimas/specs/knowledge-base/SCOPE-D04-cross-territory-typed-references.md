# SCOPE D04 — Cross-Territory Typed References & Non-Write Barrier

> **Track:** D — Thematic Knowledge Bases (`knowledge-base/`) · **Domain:** cross-territory edges & access boundaries
> **Status:** REVIEW · **Mark:** [E] · **Deps:** C05, D01, D02
> **Provenance:** PRD_V3 §2, §4, §7 (FR-25); A10 (Inter-Skill Contestation)

## Problem

Thematic knowledge bases represent the third territory (Tradition) — shared across incarnations and external to any single project. If a project were allowed to write into a theme graph, or if references were untyped and bidirectional, the project would contaminate shared tradition with local assumptions. Furthermore, when a project's intent collides with a bound theme's canon, an agent must never silently suppress either side.

## Scope

**In:**
- **Cross-Territory Typed Edge Family (extends C05)**:
  - Allowed edges are strictly asymmetric and unidirectional (`Project → Theme`):
    - `CONFORMS_TO`: asserts that an implementation artifact or claim strictly matches a canonical theme node.
    - `DEVIATES_FROM`: points to a violated theme standard node, referencing the active `type=DEVIATION` claim (D02).
    - `CONSULTS`: indicates advisory, non-normative grounding in an external theme.
- **Strict Non-Write Barrier**:
  - The storage/memory engine enforces an immutable write barrier: any graph mutation command (node creation, edge creation, property modification, deletion) whose target context resolves to a thematic graph is blocked if initiated from a project session context.
  - Emits refusal `TERRITORY_WRITE_FORBIDDEN` upon any write attempt.
- **Contestation Surfacing (extends A10)**:
  - When an operational contradiction is detected between a project claim (WHAT/WHY) and a bound normative theme rule (HOW), the engine prohibits silent reconciliation.
  - An open contestation event is generated (`CONTESTATION_OPEN`), escalating to the operator with both conflicting nodes cited.

**Out (named exclusions):**
- Internal project edges (covered by C05).
- Dedicated curator workflows for authoring themes outside project sessions.

## Acceptance criteria

1. **Given** an edge created between a project node and an external theme node, **When** validated, **Then** the edge type must strictly belong to `{CONFORMS_TO, DEVIATES_FROM, CONSULTS}`, else refusal `EDGE_TYPE_INVALID` is emitted.
2. **Given** any write attempt (create/update/delete) originating from a project session context targeting a theme graph URI, **When** evaluated at the engine gateway, **Then** the mutation is blocked and refusal `TERRITORY_WRITE_FORBIDDEN` is logged.
3. **Given** a direct contradiction between a project claim and a bound theme standard, **When** discovered during verification, **Then** a contestation record is opened (A10) and logged, and the agent is blocked from auto-resolving the conflict.
4. **Given** a read query across the territory boundary (`Project → Theme`), **When** executed, **Then** canonical nodes are returned cleanly without creating reverse edges in the theme graph.

## Open questions

- Read performance across remote theme graphs: should theme graphs be locally cached/mirrored with cryptographic checksums? Proposal: yes, read-through cache keyed by `theme_id` and content hash.
- Bidirectional queries: can a curator query "which projects conform to Theme X?" Proposal: yes, via registry indexer aggregating external back-links, without mutating the theme graph itself.
