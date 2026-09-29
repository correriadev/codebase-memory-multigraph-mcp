# Tactical Design — cognitive_respiration_mutation_seam

**Domain:** cognitive_respiration_mutation_seam | **Project:** codebase-memory-mcp

This design follows the existing C11 MCP, Union, CLI, and SQLite layers. The host contracts must be verified against supported Antigravity and Codex releases before claiming complete interception. The gate is enabled only when that verification establishes a pre-execution denial path for every in-scope write route.

## Section 0 — Refinement Questions and Answers

| ID | Category | Question | Recommendation | Final Answer | Answered By |
|---|---|---|---|---|---|
| Q01 | failure | When the CBM session check is unavailable, should repository writes be blocked? | Block writes while the gate is enabled. | Block writes (Recommended) | human |
| Q02 | boundary | Which writes does E01 govern? | All repository writes including docs/config; host scratch outside repository exempt. | All repository writes (Recommended) | human |
| Q03 | security | How are shell commands with unknown repository write effects handled before grounding? | Block until grounded session active. | Block ambiguous commands (Recommended) | human |
| Q04 | boundary | What does passive consultation guarantee? | No Union session or mutation-gate action; incidental index/cache writes permitted. | No Union side effects (Recommended) | human |

## Section 1 — Main Structure

| Element | Layer / Type | Invariants / Tech Rules | 4-line Snippet |
|---|---|---|---|
| Mutation attempt classifier | Shared policy, `src/union/` | Canonicalize repository root and affected paths; include code, docs, configuration, deletes, renames, and creations. Any uncertain shell or tool effect is treated as a possible repository write. Read-only consultation has no Union side effects. | `attempt: host_context + operation + canonical_root + effects` |
| Mutation decision | Shared policy, `src/union/` | Permit only with timely grounding, authoritative live session bound to the host work context, and durable write intent. Unavailable authority refuses. A marker file is never proof. | `decision: permit(intent_id) | refuse(reason)` |
| Session authority | Union lifecycle, `src/union/` and `src/mcp/` | Validate open horizon, current context binding, and session status in the authoritative service; bind at open and reject replay across contexts. Existing counter-only registry and horizon ID are insufficient. | `session: horizon_id + host + context_id + status` |
| Provenance journal | Union persistence, `src/union/` / SQLite | Store a durable intent before allowing the host to execute, then record an observed result with the same write ID. An unconfirmed intent remains `outcome_unknown`; do not label it applied. Use managed persistence and recovery rules. | `write: id + intent + grounding + outcome` |
| Antigravity adapter | CLI installed host hook | Decode the actual supported payload and emit its documented denial response; verify every direct file, patch, shell, and other mutation-capable route available in the host. Installation must be idempotent and ownership-aware. | `antigravity_hook(payload) -> decision_response` |
| Codex adapter | CLI installed `PreToolUse` hook | Decode verified Codex payloads, including `apply_patch`, shell, and MCP calls where interceptable; emit the documented denial response. Existing Codex installer handles lifecycle hooks only. | `codex_hook(payload) -> decision_response` |
| Outcome reporter | Host adapter + Union action API | Correlate post-execution observation with pre-write intent when the host provides a trustworthy result hook. A host without one retains `outcome_unknown`; never assert atomic host file write and journal commit. | `observe(write_id, host_result) -> outcome` |

The write protocol is: classify attempt → verify grounding and session → durably record intent → return permit → host attempts write → record observed outcome when available → close session after outstanding intents are reconciled or marked unknown. A failed intent commit refuses the write. A crash after permit can leave an unknown outcome, which is visible in the audit trace; it cannot be silently recorded as success. Every further write repeats the protocol. Session closure must preserve unresolved intents and closure reason in durable history, even if horizon content is removed.

## Section 2 — Value Objects / Types / Interfaces

| Name | Context / Layer | Validation & Typing Rules | 4-line Snippet |
|---|---|---|---|
| HostWorkContext | Host adapter / Union | Host kind and stable host-issued context identity; reject absent or mismatched identity, never infer identity from a mutable marker. | `HostWorkContext { host, context_id }` |
| RepositoryEffect | Classifier | Canonical root plus normalized paths and effect kind; resolve traversal, symlink, case, and rename source/destination according to platform semantics; uncertainty remains explicit. | `RepositoryEffect { root, paths, certainty }` |
| ChangeGrounding | Grounding | Either a valid canon citation or declared invention with rationale, tied to intended change and freshness boundary. | `ChangeGrounding { kind, reference, rationale, intent_key }` |
| WriteIntent | Authorization / journal | Unique write ID, session ID, work context, operation digest, grounding link, time, and `pending` status; durable before permit. | `WriteIntent { id, session_id, context, grounding_id }` |
| WriteOutcome | Journal | `observed_applied`, `observed_failed`, or `outcome_unknown`; do not infer success from preflight permit alone. | `WriteOutcome { write_id, status, observed_at }` |
| MutationRefusal | Shared policy | Stable code and human-readable remediation for no grounding, stale/unbound session, unavailable authority, unknown effect, and journal failure. | `MutationRefusal { code, reason }` |

## Section 3 — Domain Services / Use Cases / Actions

| Operation / Hook | Responsibility | Coordinates / Subscriptions | 4-line Snippet |
|---|---|---|---|
| ClassifyRepositoryAttempt | Normalize host invocation into read, known write, or possible write. | Host adapters, root/path resolver | `classify(payload, root) -> RepositoryEffect` |
| EstablishChangeGrounding | Validate canon citation or declared invention for a specific change intent. | Graph evidence, grounding record | `ground(change, citation_or_invention) -> grounding_id` |
| OpenBoundUnionSession | Open a horizon tied to a verified host work context. | Existing Union open path, session authority | `open(context, grounding) -> session_id` |
| AuthorizeRepositoryWrite | Check effect, grounding, live bound session, then commit durable intent before permit. | Classifier, session authority, journal | `authorize(attempt) -> permit_or_refusal` |
| ObserveRepositoryWrite | Correlate trustworthy post-tool outcome to a prior write ID; preserve unknown if no observation arrives. | Host adapter, journal | `observe(write_id, result) -> outcome` |
| CloseGovernedSession | Close the session and retain action/unknown-outcome audit history. | Union close path, journal | `close(session_id) -> closure_trace` |

Consultation reads use existing code discovery paths and do not call these Union mutation operations. The gate's own authoritative check must be read-only; incidental index or cache refresh by consultation is outside the Union-side-effect guarantee.

## Section 4 — Events / Messages / Async Flows

| Event / Action Name | Trigger | Minimum Payload | Consumers |
|---|---|---|---|
| ChangeGroundingEstablished | Grounding accepted for an intended change | grounding ID, intent key, kind, reference/rationale | Authorization, audit view |
| UnionSessionOpened | Context-bound session becomes live | session ID, host, context ID | Session authority, adapters |
| RepositoryWriteRefused | Preflight denies a write | attempt ID, reason code | Host denial response, audit view |
| RepositoryWriteAuthorized | Durable intent commit succeeds | write ID, session ID | Host adapter, outcome reporter |
| RepositoryWriteOutcomeObserved | Host supplies correlated execution result | write ID, outcome | Journal, audit view |
| RepositoryWriteOutcomeUnknown | Host result cannot be proven after permit | write ID, reason | Journal, closure trace |
| UnionSessionClosed | Governed work ends | session ID, closure reason, unresolved count | Audit view |

These are logical records and triggers within the current MCP/Union service. No message broker is required. Denied attempts may be recorded as refusals, but consultation must not create a mutation-gate action.

## Section 5 — Persistence / Repository / Data Access Interfaces

| Resource / Adapter | Methods / Actions | Return Types / Expected State |
|---|---|---|
| SessionAuthority | openBound, getLiveBound, closeBound | Authoritative session or typed refusal; never trust a marker or stale cache |
| GroundingStore | record, getForIntent | Valid citation or declared invention linked to intended change |
| MutationJournal | commitIntent, observeOutcome, listUnresolved, loadHistory | Durable write ID before permit; idempotent correlated outcome updates; unknown retained after crash |
| HostMutationAdapter | decodeAttempt, deny, permit, observeIfSupported | Verified per-host payload and response contracts; no guessed format |
| RepositoryRootResolver | canonicalizeRoot, classifyEffects | Normalized inside/outside/uncertain result under platform path semantics |

Illustrative interface shape only:

```text
interface MutationJournal:
  commitIntent(session, effect, grounding) -> write_id
  observeOutcome(write_id, result) -> recorded_status
```

The journal's storage design must account for the current in-memory session registry and horizon teardown. Intent and outcome history must survive process failure and session closure through existing approved SQLite lifecycle facilities or a dedicated durable ledger; the implementation must establish that choice with recovery tests. Host execution and CBM persistence cannot be one transaction, so exact-once application is not promised. Idempotency protects repeated hook delivery and observation, while unresolved intents surface as unknown outcomes.

## Section 6 — Ordered Development Tasks
```json
[
  {
    "id": "01",
    "title": "Define mutation attempt contracts",
    "description": "Introduce C11 types for host context, repository effects, grounding, and decisions so both hosts share one policy input.",
    "scope": ["src/union/mutation_gate.h", "src/union/mutation_gate.c", "tests/test_mutation_gate.c"],
    "acceptance": ["Types distinguish read, known write, and uncertain effect", "Direct and ambiguous repository writes are classified without file-extension exemptions"],
    "depends_on": null
  },
  {
    "id": "02",
    "title": "Bind Union sessions to host contexts",
    "description": "Extend authoritative session state and MCP lifecycle contracts so a session can only authorize its own host work context.",
    "scope": ["src/union/union_session.h", "src/union/union_session.c", "src/mcp/union_handler.c", "tests/test_union_workflow_e2e.c"],
    "acceptance": ["Live same-context session validates and stale, closed, or cross-context session refuses", "Unavailable session authority refuses a repository write"],
    "depends_on": "01"
  },
  {
    "id": "03",
    "title": "Validate change grounding",
    "description": "Link a timely canon citation or declared invention to each intended change before authorization.",
    "scope": ["src/union/mutation_gate.h", "src/union/mutation_gate.c", "tests/test_mutation_gate.c"],
    "acceptance": ["Missing or unrelated grounding refuses authorization", "Declared invention requires a rationale and canon grounding requires a citation"],
    "depends_on": "02"
  },
  {
    "id": "04",
    "title": "Persist write intents and outcomes",
    "description": "Add durable, idempotent per-write provenance records that survive failure and session closure.",
    "scope": ["src/union/mutation_journal.h", "src/union/mutation_journal.c", "src/union/union_session.c", "tests/test_union_workflow_e2e.c"],
    "acceptance": ["Intent is durable before permit and failed commit refuses execution", "Crash recovery retains pending intents as outcome_unknown", "Repeated outcome delivery does not duplicate action history"],
    "depends_on": "03"
  },
  {
    "id": "05",
    "title": "Expose mutation authorization through MCP",
    "description": "Wire shared policy, session authority, and journal into request and outcome operations with typed refusals.",
    "scope": ["src/mcp/union_handler.c", "src/mcp/union_handler.h", "src/mcp/mcp.c", "tests/test_union_workflow_e2e.c"],
    "acceptance": ["Each write request receives a pre-execution permit or refusal and per-write ID", "No session or mutation action is created by consultation"],
    "depends_on": "04"
  },
  {
    "id": "06",
    "title": "Verify Antigravity interception contract",
    "description": "Establish supported pre-tool payloads, denial responses, and write route coverage before installing an enforcing adapter.",
    "scope": ["docs/specs/cognitive_respiration_mutation_seam/antigravity-hook-contract.md", "tests/test_cli.c"],
    "acceptance": ["Contract records tested direct-file, patch, shell, and other available write routes", "Uncovered route is reported as unsupported and prevents a complete-coverage claim"],
    "depends_on": "05"
  },
  {
    "id": "07",
    "title": "Install Antigravity mutation adapter",
    "description": "Translate verified Antigravity hook invocations to the shared authorization policy and enforce refusals.",
    "scope": ["src/cli/cli.c", "src/cli/hook_augment.c", "tests/test_cli.c"],
    "acceptance": ["Verified denied writes do not reach disk and approved writes carry write IDs", "Repeated installation preserves user configuration and does not duplicate managed hooks"],
    "depends_on": "06"
  },
  {
    "id": "08",
    "title": "Verify Codex interception contract",
    "description": "Establish Codex PreToolUse payload, denial, and observation semantics for all supported mutation-capable tool routes.",
    "scope": ["docs/specs/cognitive_respiration_mutation_seam/codex-hook-contract.md", "tests/test_cli.c"],
    "acceptance": ["Contract verifies apply_patch, shell, and MCP calls where the host exposes them", "Any unsupported write route is identified before gate enablement"],
    "depends_on": "05"
  },
  {
    "id": "09",
    "title": "Install Codex mutation adapter",
    "description": "Extend managed Codex hooks with verified pre-tool denial while preserving existing lifecycle augmentation.",
    "scope": ["src/cli/cli.c", "src/cli/config_toml_edit.c", "src/cli/hook_augment.c", "tests/test_cli.c"],
    "acceptance": ["Denied apply_patch, shell, and supported MCP writes do not reach disk", "Installer and removal are idempotent, preserve unrelated hooks, and reject ambiguous ownership"],
    "depends_on": "08"
  },
  {
    "id": "10",
    "title": "Verify cross-host mutation lifecycle",
    "description": "Run end-to-end host fixtures for authorization, observation, unknown outcome, consultation, and closure.",
    "scope": ["tests/test_union_workflow_e2e.c", "tests/test_cli.c", "docs/specs/cognitive_respiration_mutation_seam/003-codebase-memory-mcp-tactical-design.md"],
    "acceptance": ["Both supported hosts refuse ungrounded, unavailable-authority, and ambiguous writes before execution", "Authorized writes have durable intent and observed or explicitly unknown outcome", "Closure retains provenance and passive consultation has no Union side effects"],
    "depends_on": "09"
  }
]
```
