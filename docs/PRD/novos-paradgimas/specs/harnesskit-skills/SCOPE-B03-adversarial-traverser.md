# SCOPE B03 — `adversarial-traverser` (CONTEST station)

> **Track:** B — New harness-kit skills · **Station:** CONTEST (ADR_V1 §3.1)
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A03, A10, D02, D03, D04 · **Codes in:** harness-kit (not in this implementation cycle)
> **Provenance:** ADR_V1 §3.1; PRD_V2 Station 7 (Shadow-work); PRD_V1 §3.6 (the Critic's anti-inflation clause); PRD_V3 §3, §4 (Tradition auditing & collision contestation)

## Problem

The adversarial peer must traverse a sibling horizon's claims and speak through typed contestation (A10) — not through prose review. This is the institutionalized shadow of the union: it exists to attack where self-conception is blind, and it is forbidden from inflating. With the admission of the Third Territory (Tradition), the shadow must additionally guard against **silent invention, unrecorded deviations, and cross-territory contamination** (PRD_V3 §3, §4).

## Scope

**In:**
- Skill definition implementing the CONTEST step: traverses a target horizon's claims (A03 scoped reads), submits typed contests (A10) with evidence, severity, and impact.
- **Tradition & Provenance Audit (`D02`, `D03`, `D04`)**:
  - *Anti-Silent Invention*: Contests any specialty artifact or decision produced without either a valid `canon_citation` or a `declared_invention` rationale (`PROVENANCE_UNDECLARED`).
  - *Anti-Silent Deviation*: Contests any implementation departure from a bound normative theme (`DEVE`) lacking an accompanying `type=DEVIATION` claim (D02).
  - *Fictitious Canon Detection*: Audits whether cited theme nodes actually exist in the pinned theme version in KnowledgeBase (`D01`).
  - *Territory Boundary Enforcement*: Contests any attempted write operation directed at an external theme graph (`TERRITORY_WRITE_FORBIDDEN`, D04).
  - *Tradition-Project Collision Contestation*: Elevates direct contradictions between project claims (WHAT) and bound theme standards (HOW) to `CONTESTATION_OPEN` for operator resolution, preventing silent auto-resolution in either direction.
- The anti-inflation clause: every contest requires **evidence (exact ref), impact (concrete consequence), proportional severity**; "no findings" is a valid and expected outcome; a fabricated finding is the worst failure mode.
- Blindness discipline: the traverser does not read the submitting identity's reputation, seniority, or friendliness (A08) — it contests content.
- Severity honesty: the traverser's own budget (A06) is debited per contest; contests are not free (exhaustion escalates rather than flooding).

**Out:**
- The contestation API (A10 — Track A).
- Any fixing/implementation of what it contests (the Other names the wound; it does not heal it — PRD_V1 §3.8 lineage).
- Verdict computation (the host judges; the traverser only submits).

## Acceptance criteria (testable in isolation)

1. **Given** a target horizon containing a specialty decision without canon citation and without declared invention, **When** the traverser runs, **Then** a contest naming `PROVENANCE_UNDECLARED` with exact artifact ref and concrete impact is submitted.
2. **Given** code departing from a bound normative theme rule without an explicit `type=DEVIATION` claim, **When** traversed, **Then** a contest for unrecorded deviation is submitted citing the violated theme rule.
3. **Given** a collision between a project requirement and a bound theme standard, **When** identified, **Then** event `CONTESTATION_OPEN` is emitted to the operator without auto-resolving.
4. **Given** a horizon with genuinely solid claims and full provenance adherence, **When** the traverser completes, **Then** a "no findings" outcome is recorded as a valid terminal event — and no fabricated contest appears.
5. **Given** identical claim content submitted under two identities, **When** traversed in separate runs, **Then** the contest sets are identical (blindness, N=2 minimum).
6. **Given** a contest the traverser cannot evidence, **When** it would be submitted, **Then** it is withheld or downgraded to an informative open question — `CONTEST_UNPROVEN` never reached by the traverser's own hand.
7. **Given** the traverser's budget, **When** exhausted mid-traversal, **Then** escalation with partial-contest record — never a truncated sweep presented as complete.

## Open questions

- Is the traverser invoked automatically on every CONCRETIZE (union-wide shadow) or on demand (contract-declared adversarial review)? Proposal: automatic for irreversible-class concretizations; on-demand otherwise (friction where the risk is).
- Should the traverser traverse *traces* (A09) as well as claims — contesting the process, not just the product? Defer to B07's territory; overlap flagged (A02 overlap report).
