# Test Scenarios — codebase-memory-mcp

**Domain:** cognitive_respiration_mutation_seam  
**Project:** codebase-memory-mcp  
**Framework:** C11 custom test harness (`Makefile.cbm` and `test-runner`); unittest / pytest for Python E2E integration  
**Date:** 2026-09-29

These are acceptance specifications derived from `003-codebase-memory-mcp-tactical-design.md` and its confirmed refinement answers Q01–Q04. They describe required behavior, not current host support. A host gate may be enabled only after its supported hook contract proves a pre-execution denial path for every in-scope write route. The existing sample Antigravity hook and Codex lifecycle hooks are not proof of that coverage.

## 1. Unit tests

### 1.1 Aggregates and aggregate roots

N/A — the tactical design defines policy types, services, sessions, and a journal, but does not define a new aggregate root or aggregate creation contract.

### 1.2 Value objects and policy types

- **Should accept HostWorkContext when host and stable host-issued context identity are present**
  - Given a supported host kind and its stable work-context identity
  - When HostWorkContext is formed
  - Then the host and context identity are available for session binding and write authorization.

- **Should reject HostWorkContext when its identity is absent or differs from the current host work context**
  - Given an absent identity or an identity from another host work context
  - When HostWorkContext is validated for a mutation attempt
  - Then the attempt cannot use that identity to authorize a repository write.

- **Should classify repository effects when paths resolve inside the canonical repository root**
  - Given a creation, modification, deletion, or rename of code, documentation, or configuration inside the canonical repository root
  - When RepositoryEffect is classified
  - Then it identifies a repository write regardless of extension or directory name.

- **Should include both rename paths when a repository item is moved**
  - Given a rename with source and destination paths
  - When RepositoryEffect is classified
  - Then both paths are resolved under platform path semantics, and an inside-repository effect is governed.

- **Should resolve path aliases when repository membership is checked**
  - Given traversal, symbolic-link, or platform case variants that refer to repository content
  - When RepositoryEffect is classified
  - Then the canonical target is treated as an in-repository effect.

- **Should exclude host scratch writes when their canonical target is outside the repository**
  - Given a known write whose canonical target is outside the repository
  - When RepositoryEffect is classified
  - Then it is outside the E01 repository-write gate.

- **Should retain uncertainty when a shell or tool effect cannot be determined**
  - Given a command or host invocation with indeterminate repository effects
  - When RepositoryEffect is classified
  - Then it is a possible repository write rather than a proven read.

- **Should accept ChangeGrounding when a canon citation supports the intended change**
  - Given a timely canon citation tied to a specific intent key
  - When ChangeGrounding is validated
  - Then it is eligible for that intended change.

- **Should accept ChangeGrounding when a declared invention has a rationale**
  - Given a timely declared invention, its rationale, and the intended change key
  - When ChangeGrounding is validated
  - Then it is eligible for that intended change.

- **Should reject ChangeGrounding when its evidence is missing, stale, or unrelated**
  - Given a missing citation, a declared invention without rationale, or grounding outside the intent's freshness boundary
  - When ChangeGrounding is validated for the intended change
  - Then it cannot authorize that change.

- **Should retain one write identity when a WriteIntent and WriteOutcome refer to the same write**
  - Given a WriteIntent with its session, host work context, operation digest, grounding link, and pending status
  - When a WriteOutcome is associated with that write ID
  - Then its observed or unknown status remains linked to that intent.

- **Should refuse a repository write when MutationRefusal is returned**
  - Given missing grounding, a stale or unbound session, unavailable authority, an ungrounded uncertain effect, or journal failure
  - When the shared policy returns MutationRefusal
  - Then it contains a stable reason code and human-readable remediation for the host denial response.

**Equality, Set/Map behavior, and copy-on-update:** N/A — the tactical design gives no value equality or mutation API contract.

### 1.3 Domain services

- **Should classify consultation as a read when its effects are known to be read-only**
  - Given a repository evidence request with no repository mutation effect
  - When ClassifyRepositoryAttempt evaluates the host invocation
  - Then it returns a read classification and does not invoke Union mutation operations.

- **Should refuse authorization when grounding or a live bound Union Session is missing**
  - Given an in-repository Mutation Attempt without timely ChangeGrounding or without a Live Session for its Host Work Context
  - When AuthorizeRepositoryWrite runs
  - Then it returns MutationRefusal before a WriteIntent permit is issued.

- **Should authorize a repository write when grounding, session authority, and durable intent succeed**
  - Given a known Repository Write with timely ChangeGrounding and an authoritative Live Session bound to its Host Work Context
  - When AuthorizeRepositoryWrite commits its WriteIntent
  - Then it returns a permit containing that write ID.

- **Should refuse an ambiguous shell command when grounded session authority is absent**
  - Given an Ambiguous Shell Command and no grounded Live Session for its Host Work Context
  - When AuthorizeRepositoryWrite runs
  - Then it returns MutationRefusal before the command executes.

- **Should apply the same mutation decision for equivalent attempts from Antigravity and Codex**
  - Given equivalent Repository Effects, ChangeGrounding, and Live Session authority from each host adapter
  - When AuthorizeRepositoryWrite evaluates each attempt
  - Then the shared policy produces equivalent permit or refusal outcomes.

**Stateless service execution:** N/A — the tactical design requires persisted session and journal state and does not promise stateless service instances.

### 1.4 Domain events and logical records

- **Should link RepositoryWriteAuthorized to the durable WriteIntent when authorization succeeds**
  - Given a committed WriteIntent
  - When RepositoryWriteAuthorized is recorded
  - Then the logical record identifies the write and its Union Session.

- **Should record RepositoryWriteRefused when a mutation decision denies execution**
  - Given a refused Mutation Attempt
  - When RepositoryWriteRefused is recorded
  - Then it identifies the attempt and refusal reason without representing a filesystem write as applied.

- **Should link RepositoryWriteOutcomeObserved or RepositoryWriteOutcomeUnknown to the same write ID when an outcome is determined**
  - Given an authorized write ID and either a trustworthy host result or no provable result
  - When the outcome record is made
  - Then it identifies that write and uses `observed_applied`, `observed_failed`, or `outcome_unknown` accurately.

**Automatic timestamps and event immutability:** N/A — the tactical design specifies logical payloads and a time-bearing intent, but no event object lifecycle or immutable event class.

## 2. Integration tests

### 2.1 Session authority, grounding store, journal, and root resolver

- **Should validate a Live Session when its host work context matches**
  - Given an open Union Session bound to the current host and context identity
  - When SessionAuthority checks that session
  - Then it reports live and bound authority for the current write.

- **Should refuse a Union Session when it is stale, closed, or from another host work context**
  - Given a stale, closed, or cross-context Union Session
  - When SessionAuthority checks it for a repository write
  - Then the write receives a typed refusal.

- **Should refuse a repository write when session authority is unavailable**
  - Given an otherwise grounded Mutation Attempt and an unavailable authoritative session check
  - When authorization requests Live Session status
  - Then it refuses the write before host execution.

- **Should reject a local marker when it is presented as Union Session authority**
  - Given a local session marker without matching authoritative Live Session status and Host Work Context binding
  - When SessionAuthority checks the write
  - Then the marker does not authorize it.

- **Should retrieve ChangeGrounding only for its intended change when the grounding was recorded**
  - Given a recorded canon citation or declared invention linked to an intent key
  - When GroundingStore retrieves grounding for that key
  - Then it returns that grounding; a different intent key does not receive it.

- **Should persist a WriteIntent before returning a permit when the journal commit succeeds**
  - Given eligible ChangeGrounding, a Live Session, and a Repository Write
  - When MutationJournal commits the WriteIntent
  - Then the write ID and provenance survive a fresh journal load before the host receives permit.

- **Should refuse a repository write when the WriteIntent cannot be committed**
  - Given eligible authorization inputs and a failed durable journal commit
  - When AuthorizeRepositoryWrite runs
  - Then the host receives refusal and no permit.

- **Should keep an unresolved WriteIntent visible when the process fails after permit**
  - Given a committed WriteIntent and a process failure before trustworthy outcome observation
  - When MutationJournal recovers
  - Then the write remains visible as `outcome_unknown`, never as `observed_applied`.

- **Should update one Mutation Action when the same observed outcome is delivered repeatedly**
  - Given one committed WriteIntent and repeated delivery of its correlated host result
  - When MutationJournal observes the result each time
  - Then action history contains one consistent outcome for the write ID.

- **Should preserve unknown outcomes and provenance when a Union Session closes**
  - Given a session with a recorded Mutation Action and an unresolved WriteIntent
  - When CloseGovernedSession closes it
  - Then durable history retains the grounding, action, unknown outcome, closure reason, and unresolved count after horizon teardown.

- **Should distinguish inside, outside, and uncertain effects when repository paths are resolved**
  - Given canonical repository paths, outside scratch paths, and paths whose effects remain uncertain after resolution
  - When RepositoryRootResolver classifies them
  - Then the results preserve those three distinctions under platform path semantics.

**Delete, pagination, and optimistic locking:** N/A — the tactical design does not define deletion of journal history, paginated queries, or a versioned aggregate save contract.

### 2.2 Use cases and MCP boundary

- **Should open a bound Union Session when governed work begins in a verified Host Work Context**
  - Given a verified Host Work Context and accepted ChangeGrounding
  - When OpenBoundUnionSession executes through the Union lifecycle boundary
  - Then the authoritative session is live and bound to that context.

- **Should return one pre-execution decision and write ID when a grounded write is requested through MCP**
  - Given a supported host request, valid grounding, and a Live Session for that Host Work Context
  - When the MCP mutation authorization operation runs
  - Then it returns permit only after the WriteIntent is durable and includes the write ID.

- **Should return a typed refusal through MCP when authority or journal persistence fails**
  - Given an unavailable SessionAuthority or failed MutationJournal commit
  - When the MCP mutation authorization operation runs
  - Then it returns a typed refusal before the host tool executes.

- **Should keep consultation free of Union side effects when repository evidence is inspected**
  - Given no governed work in progress
  - When existing code discovery paths serve a Consultation request
  - Then they open no Union Session and record no mutation-gate action; incidental index or cache refresh remains permitted.

- **Should report an observed outcome when a trustworthy host result is available**
  - Given a committed WriteIntent and a correlated trustworthy post-tool result
  - When ObserveRepositoryWrite handles that result
  - Then the journal records `observed_applied` or `observed_failed` as the result warrants.

- **Should preserve an unknown outcome when the host cannot report a trustworthy result**
  - Given a permitted write and no trustworthy correlated post-tool observation
  - When the outcome is reconciled or the session closes
  - Then the journal and Closure Trace state `outcome_unknown`.

### 2.3 External integrations and host adapters

- **Should document Antigravity write-route coverage when its supported hook contract is verified**
  - Given a supported Antigravity release with direct-file, patch, shell, and other available mutation-capable routes
  - When its pre-execution payload and denial contracts are tested
  - Then the contract records each verified route and any route that remains unsupported.

- **Should document Codex write-route coverage when its supported hook contract is verified**
  - Given a supported Codex release with `apply_patch`, shell, and MCP calls where interceptable
  - When its `PreToolUse` payload, denial, and observation contracts are tested
  - Then the contract records each verified route and identifies unsupported write routes before gate enablement.

- **Should keep the host gate disabled when a repository write route lacks pre-execution denial**
  - Given either host has an in-scope repository write route without a verified pre-execution denial path
  - When adapter readiness is evaluated
  - Then complete gate coverage is not claimed or enabled for that host.

- **Should translate a verified host invocation into a shared Mutation Attempt when an adapter receives it**
  - Given a supported Antigravity or Codex hook payload with a repository effect and Host Work Context
  - When HostMutationAdapter decodes it
  - Then the shared policy receives the classified attempt and the adapter emits that host's documented permit or denial response.

- **Should refuse a malformed host invocation when its repository effects or Host Work Context cannot be trusted**
  - Given a malformed or unsupported host payload whose effect or context identity cannot be established
  - When HostMutationAdapter decodes it while the gate is enabled
  - Then it does not permit a possible repository write.

**Timeout retry:** N/A — the tactical design specifies fail-closed unavailable authority and does not define an automatic retry policy.

## 3. Functional tests

### 3.1 Happy path flows

- **Should allow consultation without Union side effects when a developer inspects repository evidence**
  - Given an Antigravity or Codex work context with no Union Session
  - When the developer performs Consultation through existing discovery tools
  - Then no Union Session opens and no mutation-gate action is recorded.

- **Should apply and record a grounded repository write when Antigravity uses a verified write route**
  - Given verified Antigravity interception, timely ChangeGrounding, and an authoritative Live Session bound to its Host Work Context
  - When Antigravity requests a Repository Write through each verified direct-file, patch, shell, or other mutation-capable route
  - Then a durable WriteIntent precedes permit, the host may execute, and a correlated observed result or explicit unknown outcome is retained.

- **Should apply and record a grounded repository write when Codex uses a verified write route**
  - Given verified Codex interception, timely ChangeGrounding, and an authoritative Live Session bound to its Host Work Context
  - When Codex requests a Repository Write through each verified `apply_patch`, shell, or interceptable MCP route
  - Then a durable WriteIntent precedes permit, the host may execute, and a correlated observed result or explicit unknown outcome is retained.

- **Should preserve a Closure Trace when governed work ends**
  - Given a Live Session containing grounded Mutation Actions and any unresolved WriteIntents
  - When the developer ends governed work and CloseGovernedSession runs
  - Then the session closes and durable history retains its actions, unresolved count, and closure reason.

### 3.2 Alternative and error flows

- **Should leave repository content unchanged when a verified host route is refused**
  - Given a verified Antigravity or Codex pre-execution denial route and a Mutation Attempt lacking eligible grounding or Live Session authority
  - When the host adapter receives MutationRefusal
  - Then the host does not execute that repository write and the target content remains unchanged.

- **Should refuse all repository content types when grounding is absent**
  - Given attempted writes to code, documentation, and configuration in the repository without ChangeGrounding
  - When either host requests authorization through verified routes
  - Then each request is refused before disk mutation.

- **Should refuse an ambiguous shell command when a grounded Live Session is absent**
  - Given a shell command whose repository write effects cannot be determined and no grounded Live Session
  - When Antigravity or Codex submits it through a verified interception route
  - Then the command is denied before execution.

- **Should refuse a repository write when a session belongs to another host work context**
  - Given a Live Session bound to a different Antigravity context or Codex turn
  - When the current context requests a repository write
  - Then the write is refused before disk mutation.

- **Should refuse a repository write when authority is unavailable or intent persistence fails**
  - Given a grounded attempt and either unavailable authoritative session status or a failed WriteIntent commit
  - When either verified host route requests authorization
  - Then it receives denial and does not execute the repository write.

- **Should retain outcome_unknown when execution cannot be confirmed after permit**
  - Given a durable WriteIntent followed by host or service failure before trustworthy outcome observation
  - When history is recovered and the session is closed
  - Then the write is visible as `outcome_unknown` and is not represented as applied.

- **Should preserve unrelated hooks when an adapter is installed or removed repeatedly**
  - Given existing user configuration and unrelated host hooks
  - When the Antigravity or Codex managed adapter is installed repeatedly or the Codex adapter is removed repeatedly
  - Then managed hooks are not duplicated, unrelated hooks remain, and ambiguous ownership is rejected where the installer cannot establish it.

### 3.3 Security scenarios

- **Should prevent repository-root escape from exempting an in-repository write when a path is aliased**
  - Given a traversal, symbolic-link, case, or rename path alias that resolves to repository content
  - When a verified host route requests the write
  - Then RepositoryRootResolver classifies the canonical effect as governed before authorization.

- **Should prevent replay of Union Session authority when host work context changes**
  - Given a valid session or local marker from another host work context
  - When a new Antigravity context or Codex turn requests a repository write
  - Then SessionAuthority refuses that authority and the write does not reach disk.

**SQL injection, XSS, numeric bounds, personal-data redaction, and cross-user resource access:** N/A — these are not specified by the E01 tactical model or a mapped host integration contract. Malformed hook payloads and path aliases are covered by the traceable adapter and resolver scenarios above.
