# SCOPE C06 — Prose Drift Ladder

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** mutation semantics for documentary anchors
> **Status:** REVIEW · **Mark:** [E] (the ladder itself mirrors the existing code-anchor drift policy [B]) · **Deps:** C02, C04
> **Provenance:** ADR_V1 §3.6 (drift ladder), §3.7 (Emendation II, drift physics per birth nature)

## Problem

Documents mutate: headings renamed, sections deleted, bytes edited. Without a classified ladder, the first rename either breaks the documentary jurisdiction silently (anchors rot) or floods it with `ANCHOR_NOT_FOUND` noise. And the ladder must know what it cannot touch: log-anchored claims do not drift — ever.

## Scope

**In:**
- Drift classification for FILE_BYTES anchors on documentary nodes:
  - **lexical** (heading renamed / anchor moved, text preserved or renamed) → scar recorded, **no demotion**, URI resolves through the rename chain;
  - **structural** (bytes within the anchored range changed, section still exists) → node suspended until re-verified; suspended claims surface with status in grounding reads;
  - **gone** (section/file deleted) → node demotes to `source`/ungrounded — visible, never silently removed.
- **Immutability declaration for LOG_REF anchors**: no drift classification applies; the ladder refuses to process them (`LOG_REF_IMMUTABLE` information event if attempted) — change for conversation-born claims is epistemic only (supersession/recall via C05/C03).
- Drift events logged with before/after refs; the scar chain is queryable ("this claim was anchored under X, renamed to Y at seq N").

**Out (named exclusions):**
- Anchor verification mechanics (C02 — emits the verification result; this scope classifies drift from it). Emission of parse events (C04). Recall/supersession mechanics (existing machinery; the ladder never recalls — it only demotes/suspends structure).

## Acceptance criteria

1. **Given** a heading renamed with section content intact, **When** drift runs, **Then** classification `lexical`, scar recorded, no demotion, and the URI resolves through the rename chain.
2. **Given** byte edits inside an anchored section, **When** drift runs, **Then** classification `structural`, the node is suspended, and a grounding read surfaces it with its suspended status (A03 quarantine).
3. **Given** a section deleted, **When** drift runs, **Then** classification `gone`, demotion recorded — the node remains queryable as ungrounded, with its scar chain intact.
4. **Given** a LOG_REF-anchored claim passed to the ladder, **When** processed, **Then** `LOG_REF_IMMUTABLE` — no classification, no demotion; the claim's only change paths are epistemic (adversarial test: attempt structural classification on a log anchor, assert the refusal).
5. **Given** any drift event, **When** logged, **Then** before/after refs and the seq are present — the scar chain reconstructs the anchor's history by replay alone.
6. **Given** a renamed-then-deleted section, **When** the chain is queried, **Then** the full history (original → renamed → gone) is legible in order.

## Open questions

- Re-verification policy for `structural` suspensions: manual, on-next-grounding, or scheduled? Proposal: on-next-grounding (the reader re-grounds lazily) + a periodic sweep as backstop; suspension is visible either way.
- Rename chains: is there a maximum depth after which the chain compacts? Proposal: no cap — chains are the scar memory; compaction is history-editing by another name.
