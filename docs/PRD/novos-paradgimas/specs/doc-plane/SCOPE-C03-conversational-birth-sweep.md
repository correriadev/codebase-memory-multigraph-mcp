# SCOPE C03 — Conversational Birth, Intent Validation & Closure Sweep

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** the claim lifecycle for prose born in dialogue
> **Status:** REVIEW · **Mark:** [E] · **Deps:** A01, A07, A09, C01, C02
> **Provenance:** ADR_V1 §3.7 (Emendation II); PRD_V2 FR-19; Emendation I (intent validation is the operator's, never the agent's)

## Problem

Conversational prose is not indexed by scanning — it is *authored into the session horizon as PROPOSED* and *promoted through the pipeline*. Two failure modes must be designed out: (a) the agent self-validating its own proposals as operator decisions; (b) a session closing with PROPOSED claims silently evaporating — the loss of intent memory by omission.

## Scope

**In:**
- **Capture surface**: claims enter the session horizon via C01 validation during the session (the *reflex* is the skill-side policy, Track B dependency — this scope provides the host-side surface it calls).
- **Intent validation rule**: `type ∈ {DECISION, OPEN_QUESTION}` requires `validated_by = operator` before promotion — the agent never fills this itself; `type ∈ {FACT, CONSTRAINT, VERDICT}` admits on evidence through the blind gate (validator slot may remain ∅ for these).
- **Closure sweep**: at session close, every PROPOSED claim must reach a mandatory destination — promoted / converted to OPEN_QUESTION with owner / discarded with exclusion counts. Closure with unresolved PROPOSED claims emits `SWEEP_INCOMPLETE` and blocks closure until destinations are assigned.
- **Promotion gate, doc-nature**: coherence + anchor resolves (C02) + intent validation (per type). Dead ends and exploration are counted in the exclusion summary — never stored as content.
- Transparency event: capture is announced to the operator (the claim's existence and its pending status are visible before validation).

**Out (named exclusions):**
- The reflex *policy text* embedded in skills (Track B — B01/B04 dependency, noted). The ECG queries (C07). Anchor verification mechanics (C02). The gate's blindness (A08, reused).

## Acceptance criteria

1. **Given** an operator decision captured mid-session, **When** the reflex fires, **Then** a PROPOSED claim exists in the horizon with `provenance.proposed_by=agent, validated_by=∅` and a LOG_REF anchor to the exchange.
2. **Given** a DECISION claim promoted with `validated_by=∅`, **When** submitted, **Then** refusal — intent validation is the operator's; the blind gate cannot substitute for the subject.
3. **Given** a FACT claim with evidence refs and no operator validation, **When** submitted, **Then** promotion proceeds through the blind gate on evidence alone (facts are epistemic, not intentional).
4. **Given** session close with 2 PROPOSED claims unresolved, **When** close is attempted, **Then** `SWEEP_INCOMPLETE` is logged, closure is blocked, and the 2 claims are listed with refs.
5. **Given** a completed sweep, **When** the closure event is written, **Then** every claim's destination (promoted/converted/discarded) is recorded, and discarded claims appear as typed exclusion counts — never content.
6. **Given** any captured claim, **When** the operator inspects the session, **Then** the claim and its pending status are visible before any validation — capture is never silent.
7. **Given** an adversarial test where the agent attempts to fill `validated_by` with its own identity, **When** validated, **Then** refusal `CLAIM_INVALID` naming `validated_by` — self-validation is structurally impossible.

## Open questions

- Sweep destination defaults: may the sweep auto-convert unresolved DECISION claims to OPEN_QUESTION with `owner=operator`, or must it always block? Proposal: block — an unnoticed auto-conversion is a silent resolution of an open question (the gravest form, per PRD_V2 FR-4).
- Micro-promotion during session (eager, at validation moment) vs. batch at close: proposal — eager for validated DECISIONs (the validation is at hand), batch for the rest; both paths through the same gate.
