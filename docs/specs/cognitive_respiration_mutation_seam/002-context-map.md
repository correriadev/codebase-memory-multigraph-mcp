# Cognitive Respiration Mutation Seam — Context Map

## 1. Bounded Context Identification

| Bounded Context | Responsibility | Boundary (excluded) | Team Ownership | Key Entities |
|---|---|---|---|---|
| Repository Consultation | Provides repository evidence and coverage for understanding a possible change without starting governed work. | Mutation decisions, Union session creation, and mutation-gate actions. Incidental index or cache refresh is permitted. | Undetermined | Consultation; Evidence and coverage view |
| Grounding and Provenance | Relates a timely canon citation or declared invention to an intended change and its recorded write. | Deciding whether a host tool may execute and managing session lifetime. | Undetermined | Grounding; Canon Citation; Declared Invention |
| Mutation Authorization | Decides each repository write request before execution using grounding, live session authority, work-context binding, and write provenance. | Executing filesystem writes, host-specific tool parsing, and owning Union session records. | Undetermined | Mutation Decision; Mutation Attempt |
| Union Session Lifecycle | Opens, validates, records actions in, and closes the context-bound Union session for governed work. | Interpreting host tool payloads and making repository write decisions. | Undetermined | Union Session; Mutation Action; Closure Trace |
| Host Tool Interception | Converts Antigravity and Codex tool or command invocations into repository mutation attempts and enforces the returned decision. | Defining authorization policy and treating a local marker as session authority. | Undetermined | Host Work Context; Mutation Attempt |
| Repository Filesystem Operations | Applies an authorized change to repository content through host and operating-system capabilities. | Grounding, session authority, provenance policy, and mutation decisions. | Undetermined | Repository Write; Change result |

## 2. Context Map

The arrows show the flow of information or execution. `Upstream` names the provider of the contract; `downstream` names its consumer. The Host Tool Interception context has separate Antigravity and Codex adapters, while Mutation Authorization owns one shared decision policy.

**Repository Consultation → Grounding and Provenance**  
Pattern: Customer-Supplier  
Direction: Repository Consultation upstream; Grounding and Provenance downstream.  
Justification: Grounding consumes graph evidence for canon citations; consultation remains usable without starting a Union session or recording a mutation-gate action.

**Grounding and Provenance → Mutation Authorization**  
Pattern: Customer-Supplier  
Direction: Grounding and Provenance upstream; Mutation Authorization downstream.  
Justification: Authorization requires timely grounding tied to the intended change, while grounding owns the distinction between canon citation and declared invention.

**Union Session Lifecycle → Mutation Authorization**  
Pattern: Open Host Service  
Direction: Union Session Lifecycle upstream; Mutation Authorization downstream.  
Justification: The authoritative session service must confirm a live session bound to the current host work context; an unavailable check yields refusal and a local marker cannot substitute.

**Mutation Authorization → Union Session Lifecycle**  
Pattern: Published Language  
Direction: Mutation Authorization upstream; Union Session Lifecycle downstream for the action-record contract.  
Justification: Each authorized write needs a per-write provenance record in its live session, with an explicit contract for write identity, grounding, and result. The ordering and partial-failure semantics remain to be specified.

**Host Tool Interception → Mutation Authorization**  
Pattern: Anti-Corruption Layer (ACL)  
Direction: Host Tool Interception upstream for normalized attempt input; Mutation Authorization upstream for the decision consumed by the adapter.  
Justification: Antigravity and Codex expose different tool payloads and deny responses; adapters translate them into the same repository-write and work-context concepts without changing the shared rule.

**Mutation Authorization → Repository Filesystem Operations**  
Pattern: Customer-Supplier  
Direction: Mutation Authorization upstream; Repository Filesystem Operations downstream.  
Justification: Only an authorized attempt may reach repository content; all repository writes, including docs and configuration, are covered, while host scratch files outside the repository are excluded.

## 3. Core Domain Highlight

Context: Mutation Authorization  
Reason: The defining product rule is a just-in-time decision for every repository write while consultation remains free of Union side effects.  
Investment: Define an explicit decision model for write scope, ambiguous shell effects, authoritative session availability, work-context binding, refusal reasons, and repeated per-write checks; verify the same outcomes for both hosts.

Context: Grounding and Provenance  
Reason: A canon citation or declared invention tied to each intended change makes authorized writes explainable.  
Investment: Define the grounding and per-write provenance contracts, including timeliness and identity links among intent, decision, applied write, and recorded action.

These match the Core subdomain classifications in `001-problem-space.md`. Union session lifecycle, repository consultation, and host tool interception remain Supporting; repository filesystem operations remain Generic.

## 4. Architectural Decisions

**Decision:** Keep a single Mutation Authorization policy with separate Antigravity and Codex interception adapters.  
**Context:** Both hosts must enforce the same seam, but their tools and hook contracts differ.  
**Consequences:** Policy and refusal reasons stay consistent; each adapter needs independent write-coverage and payload verification.

**Decision:** Make Union Session Lifecycle the authority for live session status and host work-context binding.  
**Context:** A marker file or a session belonging to another context cannot authorize a write.  
**Consequences:** The gate can reject stale or unbound sessions; unavailable authority blocks repository writes and session checking becomes a runtime dependency.

**Decision:** Require a mutation decision before every repository write and treat indeterminate shell effects as a write attempt requiring grounding.  
**Context:** Direct tools, patches, and commands can all mutate repository content, including docs and configuration.  
**Consequences:** Host adapters must identify repository effects and stop refused attempts before execution; some ambiguous commands wait for a grounded session.

**Decision:** Keep consultation outside Union mutation workflow.  
**Context:** Inspecting evidence must not open a Union session or record a mutation-gate action.  
**Consequences:** Read-only exploration remains fluid; incidental index or cache maintenance is allowed and should not be mistaken for a governed repository write.

**Decision:** Define a per-write action-record contract between authorization and session lifecycle before implementation.  
**Context:** The existing action API lacks per-write provenance, and an applied write must not lose its audit account under partial failure.  
**Consequences:** Write identity, grounding, application outcome, and ordering need an explicit durable protocol; this creates integration work across decision and session boundaries.
