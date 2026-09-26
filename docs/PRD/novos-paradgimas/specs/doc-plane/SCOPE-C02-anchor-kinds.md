# SCOPE C02 — Anchor Kinds (Three Birth Natures)

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** anchoring prose to the real
> **Status:** REVIEW · **Mark:** [E] (file-bytes anchors and two-tier verification are [B] today) · **Deps:** A09 (the append-only trace/log is birth-B's ground)
> **Provenance:** ADR_V1 §3.7 (Emendation II); PRD_V2 Emendation II (anchor re-checkable in the real)

## Problem

Admission demands "an anchor re-checkable in the real." For code and file-born prose, the real is physical bytes — machinery exists [B]. For prose born in conversation there is no file; without a defined anchor kind, conversational claims are either unadmittable or (worse) anchored by narration. The append-only host log is the missing ground.

## Scope

**In:**
- Three anchor kinds, per the birth natures of ADR_V1 §3.7:
  - **FILE_BYTES** `(file_path, byte_start, byte_len, expected_text)` — existing two-tier machinery, reused [B];
  - **LOG_REF** `(session_id, horizon_id, event_seq)` — resolvable against the append-only trace/log store; verification = event lookup + verbatim text match of the recorded message;
  - **DERIVATION_REF** `(generator_query_ref)` — the code-graph query that generated a distillate; verification = regeneration matches.
- Verification dispatch by kind; `ANCHOR_NOT_FOUND` when the ground fails for any kind.
- Drift physics per kind declared as data: FILE_BYTES drifts (ladder in C06); LOG_REF **cannot drift** (immutable ground — change is epistemic only, supersession/recall); DERIVATION_REF invalidates when the generator's output changes.

**Out (named exclusions):**
- The drift ladder mechanics (C06). Claim anatomy (C01). The promotion gate (C03). Log retention policy beyond verifiability (A09 open question stands).

## Acceptance criteria

1. **Given** a LOG_REF anchor whose `(session, event_seq)` does not exist in the log store, **When** verified, **Then** refusal `ANCHOR_NOT_FOUND` — by log lookup, never by narrator.
2. **Given** a valid LOG_REF, **When** verified at any later time, **Then** the event resolves and the verbatim text matches — append-only means the same anchor verifies identically forever (replay test across a later `seq`).
3. **Given** a FILE_BYTES anchor over a drifted file, **When** verified, **Then** the drift ladder (C06) classifies — never a silent pass.
4. **Given** a DERIVATION_REF whose generator query now yields different output, **When** verified, **Then** the claim is flagged invalid-by-derivation with the delta referenced — derived prose may never contradict its generator.
5. **Given** a promotion proposal whose anchor kind is absent or malformed, **When** submitted, **Then** refusal `CLAIM_INVALID` naming `anchor`.
6. **Given** any admitted claim, **When** its ground is re-checked, **Then** the verification reads only from the store (host-log or filesystem) — zero narrator input (Axiom of Testimony extended to anchors).

## Open questions

- Should LOG_REF verification pin a content hash of the event text (belt against log implementation bugs)? Proposal: yes — hash recorded at admission, re-checked on verification; cost found by experiment.
- DERIVATION_REF regeneration cost: full re-run vs. sampled check? Proposal: sampled with full on suspicion — the ladder mirrors the two-tier philosophy.
