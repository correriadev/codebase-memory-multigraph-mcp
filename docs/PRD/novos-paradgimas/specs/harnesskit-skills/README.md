# Track B against the current CBM (2026-09-28)

These are reviewable harness-kit skill specifications, not an assertion that the skills have been implemented. The original PRD and ADR remain the design source; each B scope now names what the current CBM tool surface can support and what still needs host work. `docs/PRD/novos-paradgimas/AUDIT-WORKFLOW.md` describes an earlier state before Union MCP tools were added; use `docs/feature/union_workflow.md`, `docs/feature/code_discovery.md`, and the registered handlers in `src/mcp/mcp.c` for the current surface.

| Scope | Current decision | Main remaining host dependency |
| --- | --- | --- |
| B01 Grounding | Keep; orchestrate existing discovery and routing tools | Binding read/list query and per-read trace references |
| B02 Deliberation | Keep; distinguish claims from arbitrary proposed graph nodes | Typed speculative authoring and question schema |
| B03 Contestation | Keep as risk-triggered review | Complete target traversal and reviewer budget evidence |
| B04 Refusals | Keep as shared skill policy | Public contract behavior mapping and complete runtime code coverage |
| B05 Tracing | Narrow to verification/enrichment of host closure trace | Per-event references and provenance metrics from the host |
| B06 Attestation | Keep as offline lint until registry API exists | Public contract registration, overlap, and admission |
| B07 Individuation | Defer executable skill | Rich factual trace corpus and admission path |
| B08 Graduation | Defer executable skill | Atomic supersession, recall, operator authorization, and rollback |

The human's core scope still makes sense for B01-B06 as procedural skills. B07-B08 are premature as executable skills with the current CBM surface. The original acceptance criteria remain as target conditions, with staging notes in the individual files where the present API cannot yet prove them. A skill-side report is never substituted for host evidence.
