---
name: horizon-deliberator
description: Deliberate on a CBM change in a provisional horizon while separating project intent, thematic rules, and declared invention.
---

# Horizon deliberation

Use after grounding when a task needs a proposal or decision. A horizon holds project possibilities; it neither changes Tradition nor grants authority to its own hypotheses.

## Open a session

1. Call `union_session_open` with an identity, a contract when available, and the observed `based_on_seq`. Inspect `restricted` and `open_refusal`; an unknown contract opens in restricted mode.
2. Keep proposals provisional and tie each one to the project's WHAT/WHY. `union_claim_capture` records conversational claims; it does not create arbitrary Idealization nodes.
3. Check the target and authorization of each action separately. The session does not automatically confine file edits or other tools. A project session must never write to a thematic graph.

## Provenance of craft judgments

For each specialty judgment, record the applicable source and its role:

- Cited canon: a verifiable `theme_id`, `node_uri`, and pinned rule version. State whether the project follows it through `DEVE` or consults it through `PODE`.
- Declared invention: `declared_invention: true` with a reason when proposing a HOW without verifiable governing canon. Do not portray the agent's training as institutional authority.

Use `validate_provenance` when available. If neither source is present, stop the judgment and surface `PROVENANCE_UNDECLARED`. If binding status is unknown, keep that uncertainty visible before acting; do not turn it into confirmed absence.

Use `union_claim_capture` only for host-supported types (`DECISION`, `OPEN_QUESTION`, `CONSTRAINT`, `FACT`, `VERDICT`), with a self-contained predicate, consequence, and sequence. Keep details the host cannot represent in a clearly provisional review artifact; this is not admission to Idealization.

If project needs conflict with a `DEVE` rule, surface the collision to the operator. Record the rule reference, reason, and affected scope, and await an authorized decision. The specification requires a visible `DEVIATES_FROM` scar in Idealization, but the current claim tool does not accept `DEVIATION` as a type. A report or conversational `DECISION` is not that admitted scar.

Keep open questions with an owner and consequence. Agent assumptions remain provisional, never operator-validated intent.

## Actions and closure

For a mutation governed by the Union workflow, call `union_record_action` with an explicit effect class and inspect the result before acting. This call does not execute or automatically guard separate tools. Do not promote the horizon from this skill. Give each captured claim a valid destination before closure; `PROMOTED` for a `DECISION` or `OPEN_QUESTION` requires host-proven operator validation. `SWEEP_INCOMPLETE` leaves the session open.

Return the horizon, proposals, project evidence, craft provenance, collisions, questions, authorizations, and unresolved items. Opening a session alone does not prove that no base-graph write occurred.

On `HORIZON_SKIP`, reduce the scale. On `PROVENANCE_UNDECLARED`, cite a real node or declare invention with a reason. Do not repeat an identical submission.
