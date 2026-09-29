---
name: adversarial-traverser
description: Review a CBM session for unsupported claims, silent invention, hidden deviations, and conflicts between project intent and bound Tradition.
---

# Contest the cognitive boundary

Use when review is requested or the work's risk warrants it. Contest content with evidence; the operator resolves conflicts between intent and norm. A review with no findings is valid.

## Procedure

1. Identify the target horizon, its sequence, and available claims. Read Realization and Idealization separately. Only `search_graph`, `query_graph`, and `trace_path` accept `active_horizons`.
2. For each specialty judgment, verify a cited theme node and version or a declared invention. Agent training, informal memory, and a theme name without a verifiable rule node are not canon.
3. Check that `DEVE` was treated as normative and `PODE` as advisory. A `DEVE` departure needs a rule, reason, scope, and operator decision; a provisional report does not prove an admitted scar in Idealization.
4. If a project WHAT/WHY claim conflicts with a normative HOW, open a contestation. Do not choose which authority prevails. Check for attempts to write from the project into a thematic graph.
5. Ignore the submitter's prestige or identity. For each evidenced finding, call `contest_verify` with `target_ref`, proportional severity, and a non-empty evidence array. Include `target_horizon` when known.
6. Without sufficient evidence, record an open question or no finding. Do not fabricate a contest to meet a quota.

## Output

For each finding, report the target and territory, exact evidence and concrete consequence, relevant rule/binding/version, severity and returned contest ID, the operator decision needed, and observation gaps.

A contest is not a final verdict or proof that every promotion is blocked. `contest_verify` does not by itself prove independent reviewer isolation. The current API does not list every claim in a horizon; state the actual reach of the review.

On `CONTEST_UNPROVEN`, gather new evidence, turn the finding into a question, or withdraw it. Do not repeat the identical contest.
