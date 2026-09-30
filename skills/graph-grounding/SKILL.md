---
name: graph-grounding
description: For a CBM project question or artifact task, ground what exists and what is intended before consulting external craft themes.
---

# Grounding at the cognitive boundary

Use before explaining a project or producing an artifact. Realization answers what exists; Idealization answers what is intended; external Tradition answers how a craft is practiced. A theme is not part of the project's graph.

## Route the activity

Use `classify_activity` when available, then check its result against the user's actual intent. The classifier is heuristic and may label an unfamiliar specialty task `CONSULTATIVE`.

- `CONSULTATIVE`: answer questions about project state or intent from the two project planes. Theme consultation is not required.
- `SPECIALTY`: when creating or changing code, tests, screens, backlogs, writing, or another craft artifact, ground WHAT/WHY in the project first and HOW in Tradition second.

## Ground the work

1. Identify the project, sequence, and index coverage with `index_status` and, when absence matters, `check_index_coverage`.
2. Read Realization through relevant graph searches, source excerpts, relations, and architecture queries. Read Idealization claims and decisions when a queryable source exists; otherwise mark intent as unverified.
3. For specialty work, identify the relevant theme and version. `theme_lookup` queries the catalog; `ABSENT` is a recorded absence, not a tool failure. A catalog entry alone does not prove that a rule node exists or that the project is bound to it.
4. Distinguish normative `DEVE` (`NORMATIVE`) from advisory `PODE` (`CONSULTED`). Do not use `binding_claim` to discover or create a binding: it admits a binding, and the agent cannot bind a project on its own authority.
5. Before acting on specialty work, show a verifiable theme, rule node, and version, or ask whether to proceed with declared invention or propose binding/founding a theme. If binding status is unknown, say so; do not claim that no theme governs the craft. A consulted, unbound theme is advisory.
6. For speculative code context, pass `active_horizons` only to `search_graph`, `query_graph`, and `trace_path`. Other discovery tools may show only the base graph.

## Hand off repository changes

Consultation and read-only investigation do not require a Union session. When the authorized task will change repository files, hand the mutation workflow the selected canon citation or declared invention with its rationale, a specific `intent_key`, and the intended absolute paths and operations. `union_session_open` binds those pairs in `intent_scope`; grounding alone does not permit a write. Do not invent a host `context_id` or treat an unverified target as covered. A patch with multiple files or a command whose targets cannot be proven remains outside the currently supported gate path.

## Report

Record the activity class and routing uncertainty; separate Realization and Idealization evidence with references and sequence; the theme and version consulted; verified `DEVE`/`PODE` status or unknown binding status; the source of each HOW judgment or declared invention; and contestations, open questions, and evidence gaps.

Do not present the agent's learned conventions as institutional canon. Keep `ABSENT`, unverified absence, and contested claims distinct.

## Refusals and limits

On `STALE_BASE`, re-ground on the current sequence. On `THEME_UNKNOWN`, check the identifier and expose the gap. Tradition is read-only from a project session. The current API cannot list every binding or provide references for every read; report those limits instead of inventing proof.
