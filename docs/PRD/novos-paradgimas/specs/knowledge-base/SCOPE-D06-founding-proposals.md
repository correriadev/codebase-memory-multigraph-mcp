# SCOPE D06 — Closure Sweep Founding Proposals

> **Track:** D — Thematic Knowledge Bases (`knowledge-base/`) · **Domain:** closure sweep extension & theme founding
> **Status:** REVIEW · **Mark:** [E] · **Deps:** C03, D01, D03
> **Provenance:** PRD_V3 §5, §7 (FR-27); C03 (Conversational birth & closure sweep)

## Problem

When a project incarnation develops an innovative craft solution — marked during execution as a `declared_invention` (D03) — that pattern typically remains trapped within that single project's boundaries. Conversely, allowing an agent to unilaterally convert local discoveries into institutional themes leads to uncontrolled canon inflation. The session closure sweep (C03) must provide a governed mechanism to harvest trans-project craft patterns and present them exclusively to the operator as founding proposals.

## Scope

**In:**
- **Closure Sweep Destination Extension (extends C03)**:
  - Adds `FOUNDING_PROPOSAL` to the mandatory closure sweep destinations: candidate seeds representing craft patterns of trans-incarnational utility.
- **Founding Proposal Structure**:
  - Properties: `suggested_theme_id`, `namespace`, `rationale`, `seed_nodes` (distilled canonical rule drafts extracted from session artifacts/claims), `origin_session` reference.
- **Strict Operator Adjudication**:
  - The proposal is strictly offered to the operator; autonomous theme instantiation is impossible.
  - If approved by the operator: a new theme catalog entry is created in KnowledgeBase (D01) with `status=ABSENT` or `DRAFT` and `founding_provenance` referencing the originating session.
  - If declined by the operator: the proposal is discarded and recorded as a typed exclusion count (`FOUNDING_PROPOSAL_DECLINED`) in the session closure log.
- **Refusal Code Extension (extends A04)**:
  - `AUTO_FOUNDING_FORBIDDEN`: emitted if an agent attempts to bypass operator validation and directly found a theme graph.

**Out (named exclusions):**
- Intra-project claim promotion (covered by C03).
- Direct curation or continuous editing of established theme graphs.

## Acceptance criteria

1. **Given** a session containing validated `declared_invention` craft patterns, **When** the closure sweep runs, **Then** the sweep may compile and present a `FOUNDING_PROPOSAL` payload to the operator.
2. **Given** a presented `FOUNDING_PROPOSAL`, **When** the operator accepts the proposal, **Then** an entry is registered in the KnowledgeBase registry with `status=DRAFT` and full origin provenance.
3. **Given** a presented `FOUNDING_PROPOSAL`, **When** the operator declines the proposal, **Then** the proposal is purged from active memory and incremented in the session's typed exclusion counter.
4. **Given** an adversarial test where an agent attempts to execute a theme creation command without operator acceptance, **When** intercepted, **Then** refusal `AUTO_FOUNDING_FORBIDDEN` is logged.
5. **Given** a session closure attempt with an unadjudicated `FOUNDING_PROPOSAL`, **When** sweep validation occurs, **Then** closure is held in `SWEEP_INCOMPLETE` until the operator provides a verdict.

## Open questions

- Minimum threshold for proposing a theme: how does the agent decide if a pattern has trans-project merit? Proposal: heuristic based on repeated patterns in declared inventions or explicit operator tagging during dialogue.
- Multi-project aggregation: can founding proposals combine findings from multiple separate sessions before prompting the operator? Proposal: deferred to curator-level tooling; session sweep remains strictly bounded to its own session horizon.
