# SCOPE D01 — KnowledgeBase Registry Graph & Absence Querying

> **Track:** D — Thematic Knowledge Bases (`knowledge-base/`) · **Domain:** registry graph model & catalog tenancy
> **Status:** REVIEW · **Mark:** [E] (named project substrate is [B]) · **Deps:** A01, A04
> **Provenance:** PRD_V3 §2, §5, §7 (FR-21, FR-28)

## Problem

Today, project memory spaces exist either as isolated single-tenant indexes or through rudimentary cross-repo linking, with no governed registry of external craft themes. Furthermore, the absence of an institutional standard is treated as an unstructured error (e.g. 404 / file missing) rather than an explicit, queryable epistemic state. Under the Fractal Law (FR-28), the collective dimension requires a dedicated registry graph where absence is a first-class truth, not an exception.

## Scope

**In:**
- **KnowledgeBase Registry Graph Model**:
  - Theme entry node anatomy: `theme_id` (namespaced identifier, e.g., `@inst/frontend-clean-arch`), `name`, `namespace` (personal / institutional / public), `target_uri` (URI of the backing graph), `version` (semver), `curator` (identity), `status` ∈ {`ACTIVE`, `DEPRECATED`, `ABSENT`}, `founding_provenance` (origin session/horizon or historical anchor), `created_at`, `updated_at`.
- **Queryable Absence Semantics**:
  - Querying an unregistered or not-yet-materialized theme returns an `ABSENT` status node containing catalog metadata, allowing downstream agents to reason explicitly about missing canon without throwing host exceptions.
- **Validation & Refusals (extends A04)**:
  - `THEME_SCHEMA_INVALID`: emitted when registration lacks mandatory fields (`theme_id`, `namespace`, `curator`, `version`).
  - `THEME_UNKNOWN`: emitted when a queried namespace or theme ID does not exist in the catalog.
- **Scale-Invariance (Fractal Law)**:
  - Each material theme graph pointed to by `target_uri` is a full union tenant (possessing its own admission, recall, provenance, and refutation mechanics).

**Out (named exclusions):**
- Project binding claims (D02).
- Epistemic routing and craft provenance checks (D03).
- Modification of theme graph content from project sessions (strictly prohibited; D04).

## Acceptance criteria

1. **Given** a theme registration request missing any mandatory catalog field (`theme_id`, `namespace`, `curator`, `version`), **When** submitted to the registry, **Then** refusal `THEME_SCHEMA_INVALID` is emitted naming the missing attribute.
2. **Given** a registered theme with `status=ABSENT`, **When** queried by a client or routing station, **Then** the host returns a valid record containing `status=ABSENT` and zero host-level runtime exceptions occur.
3. **Given** a query for a completely unregistered theme identifier, **When** evaluated, **Then** refusal `THEME_UNKNOWN` is returned with the queried namespace/identifier logged.
4. **Given** an existing theme graph, **When** inspected via the registry, **Then** its tenant root satisfies the same fractal admission and query protocol as any standard union base graph.

## Open questions

- Namespace hierarchy and collisions: proposal — follow standard scoped namespaces (`@scope/theme-name`), rejecting collisions at registration unless submitted as a version increment by the designated curator.
- Offline/unreachable `target_uri` behavior: proposal — resolve to `status=UNREACHABLE` (distinct from `ABSENT`, indicating network or path partition).
