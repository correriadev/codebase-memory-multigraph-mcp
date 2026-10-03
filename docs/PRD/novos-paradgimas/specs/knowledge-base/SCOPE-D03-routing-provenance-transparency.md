# SCOPE D03 — Epistemic Routing & Provenance Transparency

> **Track:** D — Thematic Knowledge Bases (`knowledge-base/`) · **Domain:** activity routing & provenance enforcement
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A03, A04, D02
> **Provenance:** PRD_V3 §3, §5, §7 (FR-23, FR-24); ADR_V1 §5.4

## Problem

When an agent performs engineering or craft tasks, it frequently draws upon its pretraining distribution silently, conflating statistical bias with institutional canon. Silent invention is the primary failure mode of ungrounded agents: the operator cannot inspect, contest, or version the standard being applied. The system must structurally classify incoming requests and mandate that every specialty judgment explicitly cite a canonical theme node or openly declare invention.

## Scope

**In:**
- **Activity Classification (`ActivityClass`)**:
  - `CONSULTATIVE`: queries concerning project facts, flow tracing, claim history, and existing architecture. Grounding is strictly bounded to the project's own planes (Realization + Idealization).
  - `SPECIALTY`: artifact production or modification (code generation, backlog authoring, architecture structuring, UI design, test stanzas). Grounding requires project planes for WHAT/WHY and theme graphs for HOW.
  - `classify_activity` is heuristic routing metadata. It must not suppress thematic discovery when an action's method or craft judgment can materially change its result.
- **Two-Fork Provenance Mandate**:
  - Every specialty judgment must carry exactly one valid provenance payload:
    1. `canon_citation`: `{ theme_id, node_uri, pinned_version }` resolving to a materialized node in the exact pinned theme version; OR
    2. `declared_invention`: `{ declared: true, rationale: string }` explicitly logging that no theme governs the decision.
  - A canon citation resolves an exact immutable catalog version and verifies the exact node URI through that version's backing graph before reporting `anchor_verified=true`. Citation validity does not create or imply authority; a `binding_claim` is a separate declaration, and catalog lifecycle state is returned by `theme_lookup`.
- **Refusal Code Extension (extends A04)**:
  - `PROVENANCE_UNDECLARED`: emitted whenever a specialty judgment carries neither a valid citation nor a declared invention flag.
- **Pre-Flight Transparency Event**:
  - When entering a specialty task with no bound theme covering the target craft, the engine emits a visible notice to the host log and operator: *"No theme governs craft [craft_name]; proceeding under declared invention."*

**Out (named exclusions):**
- Prohibiting invention itself (declared invention is fully allowed; only silent invention is refused).
- The prompt engineering of individual skills (Track B).
- Storage of theme graphs (D01).

## Acceptance criteria

1. **Given** a request classified as `CONSULTATIVE` ("explain flow X in module Y"), **When** executed, **Then** grounding queries only project planes, and zero theme citation is required.
2. **Given** a request classified as `SPECIALTY` ("scaffold new domain aggregate"), **When** executed, **Then** grounding queries project claims for WHAT/WHY followed by applicable thematic graphs for HOW; any binding authority is checked separately.
3. **Given** a specialty judgment produced without a valid `canon_citation` and without `declared_invention=true`, **When** validated at the gate, **Then** refusal `PROVENANCE_UNDECLARED` is emitted and logged.
4. **Given** a specialty judgment carrying `declared_invention=true` with a non-empty rationale, **When** validated, **Then** the judgment is admitted and tagged with epistemic status `DECLARED_INVENTION`.
5. **Given** a specialty judgment citing a theme node that does not exist in the referenced theme graph, **When** validated, **Then** refusal `ANCHOR_NOT_FOUND` is emitted.

## Open questions

- Activity classification ambiguity: if a prompt blends consultative and specialty requests, how is it handled? Proposal: split into dual sub-steps or classify conservatively as `SPECIALTY`.
- Granularity of citations: must every single function or AST node carry a citation, or per architectural decision/file block? Proposal: per coherent craft decision / file generation boundary.
