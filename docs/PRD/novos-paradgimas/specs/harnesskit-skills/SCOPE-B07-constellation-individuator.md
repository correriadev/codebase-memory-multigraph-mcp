# SCOPE B07 — `constellation-individuator` (Level 5)

> **Track:** B — New harness-kit skills · **Station:** REFLEXION, genesis form (ADR_V1 §4, Level 5)
> **Status:** DEFERRED · **Mark:** [A/E] · **Deps:** A07, A08, A09, A10, B05, B06, D06 · **Codes in:** harness-kit (future)
> **Provenance:** ADR_V1 §4 (Individuating); PRD_V1 §3.13; PRD_V2 §5 (Level-5 gate rule); PRD_V3 §1, §5, §10 (D06: Craft Theme vs Procedural Skill)

## Compatibility with CBM (2026-09-28)

The current closure trace is too coarse to mine recurring procedural patterns with reliable evidence. `founding_propose` already exists for candidate themes, but it must not be used as a skill-admission channel. Contract admission and supersession are not public workflow operations. A trace miner would therefore turn incomplete counts into unjustified new skill personas.

**Scope decision:** defer executable B07. Its next deliverable is an offline, human-reviewed candidate format and an evidence sufficiency test over several factual traces. No candidate proceeds to B08 from the current trace schema alone.

## Problem

Recurring trace patterns with no owning archetype are the system differentiating a new organ from its own lived experience (ADR_V1 Level 5). This is the union creating creators — the most powerful and most inflation-prone capability in the entire line. Under PRD_V3, treating every recurring pattern as a new skill persona would cause persona explosion; the individuator must **strictly distinguish recurring craft conventions (which belong in Theme Graphs via D06) from genuinely novel universal reasoning organs (B07)**.

## Scope

**In:**
- Skill definition implementing pattern mining over factual traces (A09/B05).
- **Pattern Classification Boundary (D06 vs B07)**:
  - *Domain Craft Conventions*: If recurring shapes represent technical implementation styles, technology idioms, or architectural patterns, B07 delegates them to the closure sweep as a **Founding Proposal (`D06`)** for a Thematic Knowledge Base.
  - *Universal Procedural Stations*: ONLY if the recurring shape represents a novel, content-neutral reasoning station (an operational mechanism of reality) does B07 author a **Candidate Skill Proposal**.
- Each candidate skill proposal is a typed proposal containing: the recurring procedural pattern (with trace refs), the proposed contract draft (B06 format), territory-overlap analysis against existing contracts (A02 report), non-regression plan (B08 feed), and budget forecast.
- **One targeted candidate per run** — individuation is one organ at a time.
- Promotion is human-only: candidate parks in `WAITING_HUMAN`; no path exists from mining to admission without the operator (ADR_V1 §5.6).

**Out:**
- Admission of new skills (B08 gates it).
- Theme founding proposals (delegated to D06).
- Any modification of existing skills (Level-4 evolution).
- Automated deprecation/death of skills.

## Acceptance criteria (testable in isolation)

1. **Given** recurring trace patterns representing domain implementation conventions (e.g. repetitive API scaffolding styles), **When** mined, **Then** the individuator routes them to closure sweep Founding Proposals (`D06`) and does NOT author a candidate skill.
2. **Given** a recurring pattern demonstrating a genuine novel universal procedural station, **When** mined, **Then** it surfaces at most one candidate skill proposal with all typed fields.
3. **Given** a candidate skill whose territory overlaps an existing procedural contract, **When** emitted, **Then** the overlap analysis is attached and does not proceed to B08 until resolved or justified.
4. **Given** any candidate, **When** promotion is attempted without operator approval, **Then** refusal — the log shows no automated admission path.
5. **Given** the Level gate (ADR_V1 §5.5), **When** Track A judgment scopes (A07, A08, A10) are not admitted, **Then** this skill refuses to run at all.

## Open questions (this scope is [A] — these are the reason)

- What is the recurrence threshold? Found by experiment, never imposed a priori.
- Does a candidate organ get a *probation horizon* (operate in restricted mode, A01 rule 2, for K sessions) before full admission? Proposal: yes — restricted mode is the natural amniotic fluid for a new archetype.
- Is the individuator allowed to propose its own retirement (the miner mining the pattern of its own obsolescence)? Genuinely open — kept visibly open per the Axiom of Provenance.
