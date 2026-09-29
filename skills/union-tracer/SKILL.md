---
name: union-tracer
description: At CBM session closure, check whether the factual trace supports project claims, craft judgments, and their declared sources.
---

# Factual Union trace

Use after a successful `union_session_close`. The trace should help distinguish what the project did, what it intended, and where its HOW came from. If closure fails, preserve the unresolved state; do not invent a partial trace.

## Procedure

1. Preserve `horizon_id`, the closure response, and references to observed calls.
2. Check the returned `trace`. If host logs are accessible, confirm `union.session.trace`; otherwise mark log confirmation unavailable.
3. Reconcile identity, contract, sequence, outcome, action counts, refusals, and exclusions against available evidence.
4. Separate session facts from agent commentary. An explanation does not prove that a claim was admitted, a binding exists, or a theme was consulted.
5. Report canon citations and declared inventions only when real references support them. Do not derive their ratio from generic trace counts.
6. If closure yielded reusable craft seeds, describe a founding proposal to the operator, never a completed founding.

## Output

Return a report keyed by `horizon_id` with checked fields and sources, contradictions and refusals, proven craft provenance and deviations, open questions, and facts the host does not record.

The current trace contains counts, not per-read references, cited-theme sets, provenance ratios, or every deviation. Mark those fields unknown. `SWEEP_INCOMPLETE` is not a completed closure.
