# SCOPE B03 — `adversarial-traverser` (CONTEST station)

> **Track:** B — New harness-kit skills · **Station:** CONTEST (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A03, A10 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 7 (Shadow-work); PRD_V1 §3.6 (the Critic's anti-inflation clause)

## Problem

The adversarial peer must traverse a sibling horizon's claims and speak through typed contestation (A10) — not through prose review. This is the institutionalized shadow of the union: it exists to attack where self-conception is blind, and it is forbidden from inflating.

## Scope

**In:**
- Skill definition implementing the CONTEST step: traverses a target horizon's claims (A03 scoped reads), submits typed contests (A10) with evidence, severity, and impact.
- The anti-inflation clause, inherited verbatim from the lineage: every contest requires **evidence (exact ref), impact (concrete consequence), proportional severity**; "no findings" is a valid and expected outcome; a fabricated finding is the worst failure mode.
- Blindness discipline: the traverser does not read the submitting identity's reputation, seniority, or friendliness (A08) — it contests content.
- Severity honesty: the traverser's own budget (A06) is debited per contest; contests are not free (exhaustion escalates rather than flooding).

**Out:**
- The contestation API (A10 — Track A).
- Any fixing/implementation of what it contests (the Other names the wound; it does not heal it — PRD_V1 §3.8 lineage).
- Verdict computation (the host judges; the traverser only submits).

## Acceptance criteria (testable in isolation)

1. **Given** a target horizon with claims, **When** the traverser runs, **Then** every submitted contest carries an exact ref, a stated concrete consequence, and a severity from the closed ladder — contests failing the triple are not submitted (self-gate, log-verified by absence + trace).
2. **Given** a horizon with genuinely solid claims, **When** the traverser completes, **Then** a "no findings" outcome is recorded as a valid terminal event — and no fabricated contest appears.
3. **Given** identical claim content submitted under two identities, **When** traversed in separate runs, **Then** the contest sets are identical (blindness, N=2 minimum).
4. **Given** a contest the traverser cannot evidence, **When** it would be submitted, **Then** it is withheld or downgraded to an informative open question — `CONTEST_UNPROVEN` never reached by the traverser's own hand.
5. **Given** the traverser's budget, **When** exhausted mid-traversal, **Then** escalation with partial-contest record — never a truncated sweep presented as complete.

## Open questions

- Is the traverser invoked automatically on every CONCRETIZE (union-wide shadow) or on demand (contract-declared adversarial review)? Proposal: automatic for irreversible-class concretizations; on-demand otherwise (friction where the risk is).
- Should the traverser traverse *traces* (A09) as well as claims — contesting the process, not just the product? Defer to B07's territory; overlap flagged (A02 overlap report).
