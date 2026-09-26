# SCOPE D05 — Theme Drift Propagation & Orphan Binding ECG Queries

> **Track:** D — Thematic Knowledge Bases (`knowledge-base/`) · **Domain:** lifecycle drift & health diagnostics
> **Status:** REVIEW · **Mark:** [E] · **Deps:** C06, C07, D01, D02
> **Provenance:** PRD_V3 §4, §7 (FR-26); C06 (prose drift ladder); C07 (aging & orphan queries)

## Problem

Institutional traditions are not static; as craft practices mature, themes evolve into new versions, or deprecated themes are archived. If a project remains silently pinned to an outdated or vanished theme without visibility, architectural debt accumulates invisibly. The system requires cross-graph drift detection that alerts bound incarnations without auto-mutating their pinned versions, coupled with ECG diagnostic queries for orphan bindings and unreconciled deviations.

## Scope

**In:**
- **Theme Drift Propagation**:
  - When a new theme version is registered in the KnowledgeBase registry (D01), the engine computes the set of bound projects.
  - Generates an asynchronous `THEME_DRIFT_NOTICE` event ("your tradition moved; re-verify") associated with the project's binding claim.
  - The binding claim transitions its audit state to `DRIFT_PENDING` until reviewed by the operator.
- **Collective ECG Queries (extends C07)**:
  - `query_orphan_bindings`: scans project Idealization planes for bindings referencing theme IDs that are nonexistent, marked `ABSENT`, or marked `DEPRECATED`.
  - `query_unreconciled_deviations`: queries all `type=DEVIATION` claims where either the project code anchor drifted or the referenced canonical rule has been superseded.
- **Immutable Pinning Rule**:
  - Under no circumstances does the engine automatically bump `pinned_version` in a binding claim upon drift detection. Bumping requires explicit operator re-validation (D02).

**Out (named exclusions):**
- Automated code refactoring to match the updated theme.
- In-session markdown prose drift (C06).

## Acceptance criteria

1. **Given** a registered theme transitioning from version `1.0.0` to `2.0.0`, **When** the update is published in the KnowledgeBase registry, **Then** all incarnations carrying a normative binding to that theme have a `THEME_DRIFT_NOTICE` recorded in their audit logs.
2. **Given** a project bound to a theme whose registry status becomes `ABSENT` or `DEPRECATED`, **When** `query_orphan_bindings` is executed, **Then** the binding is returned with code `ORPHAN_BINDING` and full target provenance.
3. **Given** a project with an active deviation whose underlying code anchor has changed, **When** `query_unreconciled_deviations` is executed, **Then** the deviation is returned with status `UNRECONCILED` and its scarred file scope.
4. **Given** an agent attempting to resolve a drift notice by rewriting the project's `pinned_version` automatically, **When** submitted, **Then** refusal `BINDING_SELF_VALIDATED` is emitted, keeping the original version pinned.

## Open questions

- Drift notice delivery mechanism: should drift notices interrupt ongoing sessions or remain passive queue items checked at session start? Proposal: passive queue item checked during session horizon initialization (A01).
- Semantic versioning semantics: should drift notices only fire for major/breaking version bumps, or for all releases? Proposal: all version increments trigger notices, tagged with semver change class (`MAJOR`, `MINOR`, `PATCH`).
