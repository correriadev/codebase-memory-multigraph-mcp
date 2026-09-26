# SCOPE B01 — `graph-grounding` (GROUND station)

> **Track:** B — New harness-kit skills · **Station:** GROUND (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A03 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 2 (Interrogation, grounded form)

## Problem

The first act of a graph-native skill is to ground: read admitted context through the graph, never by raw scan. Today harness-kit skills instruct agents to scan repositories and read docs linearly; every session re-pays the discovery cost (PRD_V2 FR-18 violation, daily).

## Scope

**In:**
- Skill definition (SKILL.md + contract, A02) implementing the GROUND step of the invocation protocol.
- Grounding procedure: status-aware queries (A03) replacing raw exploration; context report per read (believed / contested / assumed, with provenance refs); explicit handling of `PROVENANCE_MISSING` flags (park as open questions, never proceed as fact).
- Refusal discipline: `ANCHOR_NOT_FOUND` / `STALE_BASE` during grounding → re-ground or defer, never re-submit identically (B04 matrix embedded).

**Out:**
- The query API itself (A03 — Track A).
- Any judgment of grounded content (that is the invoking station's work).
- Cache management (the graph is the jurisdiction; the skill holds no private cache — cache possession, ADR_V1 §5.1).

## Acceptance criteria (testable in isolation)

1. **Given** a task on an indexed project, **When** the skill grounds, **Then** the host log shows zero full-repository raw scans — all context reads are status-aware graph queries.
2. **Given** a grounding result containing `PROVENANCE_MISSING` or `PROPOSED` items, **When** the context report is emitted, **Then** those items are visibly marked and quarantined from being cited as fact.
3. **Given** a contested item surfaced by grounding, **When** the report is emitted, **Then** the contest ref and severity are attached.
4. **Given** a `STALE_BASE` refusal during grounding, **When** handled, **Then** the skill revalidates against the current `seq` and the log shows no identical re-query.
5. **Given** any completed grounding, **When** the session trace (B05) is written, **Then** every grounding read is recorded by reference and count (A09 open-question contract).

## Open questions

- Should grounding be a standalone invocable skill or a mandatory preamble embedded in every Track-B skill? Proposal: embedded preamble + standalone invocable for interactive use — the same content, two surfaces.
- Interactive-mode surfacing: how much of the epistemic status do we show a human user by default? Proposal: full marks in autonomous mode; collapsed badges in interactive mode, expandable.
