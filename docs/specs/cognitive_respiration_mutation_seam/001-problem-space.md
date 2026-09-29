# Cognitive Respiration Mutation Seam — Problem Space

## Scope and business outcome

E01 governs the transition from repository consultation to repository mutation in Antigravity and Codex. A developer may inspect graph evidence without a Union session or mutation-gate action. Before any repository write, the agent must ground the intended change, establish an authoritative live Union session bound to its current host work context, state a canon citation or declared invention, and record the action. The session must be closed when the governed work ends. All repository writes, including documentation and configuration, are in scope. Host scratch files outside the repository are outside this gate. An unavailable session check blocks writes. A shell command whose repository write effects cannot be determined is blocked until a grounded session is active. Consultation may incidentally refresh an index or cache; the no-side-effect promise concerns Union sessions and mutation-gate actions.

This is a product model, not a claim that the current host hooks enforce it. The sample Antigravity script and marker are unverified. Existing Union APIs do not yet establish host work-context binding or carry per-write provenance.

## 1. Event Storming

| # | Domain Event (past tense) | Command (trigger) | Aggregate | External Systems | Read Models |
|---|---|---|---|---|---|
| 1 | Repository Evidence Consulted | Inspect indexed code and relationships | Consultation | Code graph service | Evidence and coverage view |
| 2 | Repository Change Intended | Request a change to repository content | Mutation Attempt | Antigravity or Codex host | Pending change view |
| 3 | Change Grounding Established | Cite canon evidence or declare an invention for the intended change | Grounding | Code graph service | Grounding explanation |
| 4 | Union Session Opened | Begin governed work for the current host work context | Union Session | Antigravity or Codex host; Union service | Session status |
| 5 | Repository Write Requested | Invoke a host tool or command that may change repository content | Mutation Attempt | Antigravity or Codex host | Pending write view |
| 6 | Repository Write Refused | Check found missing grounding, absent/expired/unbound session, unavailable session authority, or ambiguous shell effects before grounding | Mutation Decision | Antigravity or Codex host; Union service | Refusal reason |
| 7 | Repository Write Authorized | Check confirmed grounding, a live session bound to current work context, and provenance for the write | Mutation Decision | Antigravity or Codex host; Union service | Authorization result |
| 8 | Repository Write Applied | Execute an authorized repository change | Mutation Attempt | Antigravity or Codex host; repository filesystem | Change result |
| 9 | Mutation Action Recorded | Record the governed write and its provenance in the live session | Union Session | Union service | Session action history |
| 10 | Union Session Closed | End governed work and complete required closure | Union Session | Union service | Closure trace |

Rows 6 and 7 are alternative outcomes for a write request. A refused write does not reach the repository filesystem. Each further write repeats the request, decision, application, and recording cycle. The required ordering between authorization, application, and durable recording must prevent an applied write from losing its provenance under partial failure; the tactical design must make that ordering explicit.

## 2. Subdomain Classification

| Subdomain | Type | Justification |
|---|---|---|
| Mutation authorization | Core | The just-in-time seam between free consultation and governed repository changes is the defining product rule. |
| Grounding and provenance | Core | Canon evidence or declared invention makes each change explainable and is essential to the authorization decision. |
| Union session lifecycle | Supporting | Live, context-bound sessions and closure provide authority and audit continuity for governed work. |
| Repository consultation | Supporting | Graph evidence informs change decisions while remaining outside the Union mutation workflow. |
| Host tool interception | Supporting | Antigravity and Codex adapters expose write attempts to the shared business rule. |
| Repository filesystem operations | Generic | File writes and path handling use host and operating-system capabilities, subject to the mutation decision. |

## 3. Ubiquitous Language Glossary

| Term | Definition | Notes |
|---|---|---|
| Consultation | Inspecting the repository and its evidence to understand a possible change. | Does not open a Union session or record a mutation-gate action; index/cache refresh may occur. |
| Repository Write | Any operation that changes content inside the repository, including code, docs, and configuration. | Tool names and file extensions do not determine exemption. |
| Mutation Seam | The moment an intended repository write must receive a governance decision before execution. | Applies in both Antigravity and Codex. |
| Grounding | Evidence supporting a change before it is made. | Must be timely and tied to the intended change. |
| Canon Citation | A reference to accepted knowledge that supports the change. | Alternative to a declared invention, not a generic statement of confidence. |
| Declared Invention | An explicit statement that the proposed change introduces a new claim or design. | Requires a rationale; must not silently masquerade as canon. |
| Union Session | A bounded period in which governed actions are authorized and recorded. | Existing API does not yet prove host context binding. |
| Host Work Context | The current Antigravity work context or Codex turn whose writes the session governs. | A session from another context cannot authorize this one's writes. |
| Live Session | A Union session that the authoritative service confirms is open and valid for the current work context. | A local marker alone is insufficient proof. |
| Mutation Attempt | A requested operation that may write inside the repository. | Includes direct file tools, patches, and shell commands. |
| Ambiguous Shell Command | A command whose possible repository write effects cannot be determined before it runs. | Refused until a grounded session is active. |
| Mutation Decision | The authorization or refusal issued before a mutation attempt executes. | An unavailable authority yields refusal. |
| Mutation Action | The recorded account of a governed repository write and its provenance. | Recorded per write; existing action API lacks this provenance. |
| Closure Trace | The account produced when governed work and its Union session end. | Supports later review of actions and completion. |
