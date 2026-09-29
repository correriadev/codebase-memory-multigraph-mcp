---
name: refusal-discipline
description: When a CBM tool refuses an action, preserve the refusal and correct evidence, scope, provenance, or authority before proceeding.
---

# Refusal discipline

Use throughout CBM procedures. A refusal identifies a boundary: an unanchored fact, unvalidated intent, craft without provenance, a forbidden territory, or an exhausted budget.

Read the host's code and context. Record the refusal and what changed before another attempt. Keep unknown codes visible; do not narrate a failure as success.

| Refusal | Response |
| --- | --- |
| `ANCHOR_NOT_FOUND` | Re-ground or withdraw the reference. |
| `STALE_BASE` | Revalidate on the current sequence or defer. |
| `HORIZON_SKIP` | Reduce scope or split the proposal. |
| `ASSUMPTION_DROPPED` | Restore or resolve the assumption. |
| `EVIDENCE_REQUIRED` | Obtain evidence or withdraw the claim. |
| `EXCLUSION_UNDECLARED` | Name the excluded material and reason. |
| `PROVENANCE_MISSING` | Restore the source or mark the claim ungrounded. |
| `SCOPE_EXCEEDED` | Narrow the target to the authorized scope. |
| `BUDGET_EXHAUSTED` | Stop and escalate; do not loop or promote. |
| `RETRY_IDENTICAL` | Stop and record a contract violation. |
| `CONTEST_UNPROVEN` | Withdraw, turn it into a question, or add exact evidence. |
| `PROVENANCE_UNDECLARED` | Cite verifiable canon or declare invention with a reason. |
| `BINDING_SELF_VALIDATED` | Leave the binding for operator validation. |
| `TERRITORY_WRITE_FORBIDDEN` | Cancel the thematic-graph write. |
| `AUTO_FOUNDING_FORBIDDEN` | Prepare a founding proposal for the operator. |
| `THEME_UNKNOWN` | Check the registry; expose the gap or defer. |
| `THEME_SCHEMA_INVALID` | Correct the theme catalog metadata. |
| `CLAIM_INVALID` | Supply the required claim fields. |
| `PREDICATE_NOT_SELF_CONTAINED` | Rewrite the predicate as a self-contained proposition. |
| `SWEEP_INCOMPLETE` | Give every captured claim a valid destination before closure. |
| `CODE_DOC_ASYMMETRY` | Reconcile code and document references. |
| `LOG_REF_IMMUTABLE` | Preserve the log and create a new correction record. |
| `CONTRACT_INVALID` / `CONTRACT_UNKNOWN` | Correct the contract or honor the reported restricted mode. |
| `TOOL_UNCLASSIFIED` | Classify the action before requesting authorization. |

For `PROVENANCE_UNDECLARED`, a citation needs a verifiable theme, node, and version; inventing a reference does not resolve the refusal. `BINDING_SELF_VALIDATED` reserves binding authority for the operator. `TERRITORY_WRITE_FORBIDDEN` keeps Tradition outside project writes. `AUTO_FOUNDING_FORBIDDEN` permits a proposal, not autonomous founding. Apply the same visibility rule to session, gateway, and parameter errors.

`CbmSkillContract` stores an acknowledged-refusals bitmask, not this response table. Report contract gaps. A human instruction does not make an identical submission new evidence.
