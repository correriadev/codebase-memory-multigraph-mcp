# SCOPE B01 — `graph-grounding` (GROUND station)

> **Track:** B — New harness-kit skills · **Station:** GROUND (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [B/E] · **Deps:** A03, D01, D02, D03 · **Codes in:** harness-kit
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 2 (Interrogation, grounded form); PRD_V3 §5 (The routing moment, D03)

## Compatibility with CBM (2026-09-28)

The Realization Plane already exposes `search_graph`, `search_code`, `query_graph`, `trace_path`, `get_code_snippet`, `get_file_outline`, `get_graph_schema`, and `get_architecture`. Union exposes `classify_activity`, `theme_lookup`, `binding_claim`, and `validate_provenance`. The skill's remaining work is orchestration and an evidence report, not new graph APIs. `binding_claim` creates a binding; it is not a read API for enumerating existing bindings. A specialty preflight must use only queryable evidence and declare an unknown binding state when it cannot inspect one. Do not create a binding just to discover it. Check `index_status`/`check_index_coverage` before treating a missing symbol as evidence of absence. Use `active_horizons` for speculative code reads.

**Scope decision:** keep B01 as a reusable grounding procedure; defer any criterion requiring an unimplemented binding-list API or per-read trace references until the CBM exposes them. A raw source read remains valid to verify exact bytes after graph localization.

## Problem

The first act of a graph-native skill is to ground: read admitted context through the graph, never by raw scan. Today harness-kit skills instruct agents to scan repositories and read docs linearly; every session re-pays the discovery cost (PRD_V2 FR-18 violation, daily). Furthermore, unrouted grounding conflates project facts with external craft standards, hiding statistical bias as authority (PRD_V3 §1, §3).

## Scope

**In:**
- Skill definition (SKILL.md + contract, A02) implementing the GROUND step of the invocation protocol.
- **Epistemic Activity Routing (`D03`)**:
  - `CONSULTATIVE` activity: grounds strictly within the project's local planes (Realization for code structure, Idealization for claims/decisions). Zero theme queries demanded.
  - `SPECIALTY` activity: two-phase ordered grounding:
    1. *Phase 1 (WHAT / WHY)*: grounds in project planes (symbols, architectural claims, requirements).
    2. *Phase 2 (HOW)*: queries KnowledgeBase registry (`D01`) and project bindings (`D02`, `DEVE` / `PODE`). Retrieves active canonical rule nodes.
- **Handling Theme Absence & Unbound Craft**:
  - When querying themes with `status=ABSENT` (`D01`), surfaces the absence as an open question without failing the session.
  - When entering a specialty task with no bound theme, emits pre-flight transparency event: *"No theme governs craft [craft_name]; proceeding under declared invention."*
- **Read-Only Cross-Territory Boundary (`D04`)**: All queries into external theme graphs are strictly read-only.
- **Refusal discipline**: `ANCHOR_NOT_FOUND` / `STALE_BASE` / `THEME_UNKNOWN` during grounding → re-ground, fall back to declared invention, or defer; never re-submit identically (B04 matrix).

**Out:**
- The query and registry APIs themselves (A03, D01 — Track A & D).
- Any judgment or modification of grounded content (that is the invoking station's work).
- Cache management (the graph is the jurisdiction; the skill holds no private cache — cache possession, ADR_V1 §5.1).

## Acceptance criteria (testable in isolation)

**Staging:** AC 1-2 are executable with current tools when the theme and project are known. AC 3-5 and 7 require binding discovery or richer trace fields; retain them as engine integration criteria. AC 6 is testable only for refusals actually emitted by the called tool.

1. **Given** a consultative request ("explain flow X in module Y"), **When** the skill grounds, **Then** the host log shows queries confined strictly to project planes and zero external theme lookups.
2. **Given** a specialty request ("scaffold domain aggregate under clean-arch"), **When** the skill grounds, **Then** the host log shows Phase 1 project grounding followed by Phase 2 theme grounding via KnowledgeBase registry (`D01`).
3. **Given** a specialty request with no bound theme, **When** grounding runs, **Then** a pre-flight notice is logged and the session is marked for `declared_invention` without throwing exceptions.
4. **Given** a grounding result containing `PROVENANCE_MISSING` or `status=ABSENT` theme entries, **When** the context report is emitted, **Then** those items are visibly quarantined from being cited as established fact.
5. **Given** a contested item or active deviation (`DEVIATION` claim, D02) surfaced by grounding, **When** the report is emitted, **Then** the contest ref and deviation scar are attached.
6. **Given** a `STALE_BASE` or `THEME_UNKNOWN` refusal during grounding, **When** handled, **Then** the skill revalidates against current `seq` or falls back to declared invention without identical re-query.
7. **Given** any completed grounding, **When** the session trace (B05) is written, **Then** every grounding read (project and theme) is recorded by reference and count (A09/D03).

## Open questions

- Should grounding be a standalone invocable skill or a mandatory preamble embedded in every Track-B skill? Proposal: embedded preamble + standalone invocable for interactive use — the same content, two surfaces.
- Interactive-mode surfacing: how much of the epistemic status do we show a human user by default? Proposal: full marks in autonomous mode; collapsed badges in interactive mode, expandable.
