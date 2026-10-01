# Real-World Workflows — Codebase Memory MCP

This document presents four detailed, end-to-end operational scenarios demonstrating how human software architects and autonomous AI agents utilize `codebase-memory-mcp` across all project phases.

---

## Scenario 1: Pure Epistemic Exploration (Diástole)

### Objective
An engineer joins an unfamiliar codebase and asks an autonomous agent: *"How does payment processing interact with the fraud detection service, and what breaks if we modify `validate_transaction`?"*

### Mindset & Phase
- **Cognitive Phase**: **Diástole (Expansão Contemplativa)**.
- **Rules**: Zero session creation, zero locks, zero ledger debits. The knowledge graph operates as an immutable oracle.

### Execution Trace

#### Step 1: Discover the candidate symbol
The agent queries the graph for transaction validation functions:
```json
// Tool Call: search_graph
{
  "project": "ecommerce-core",
  "name_pattern": ".*validate_transaction.*",
  "format": "tree"
}
```
**Response**:
```text
[Function] validate_transaction
  file: src/billing/payment_processor.c:84
  qn: billing::payment_processor::validate_transaction
  degree: 14 (in: 9, out: 5)
```

#### Step 2: Trace Inbound Callers (Blast Radius)
To discover what breaks if `validate_transaction` changes, the agent traces incoming call chains:
```json
// Tool Call: trace_path
{
  "project": "ecommerce-core",
  "function_name": "validate_transaction",
  "direction": "inbound",
  "depth": 3,
  "mode": "calls"
}
```
**Response**:
```text
validate_transaction (src/billing/payment_processor.c:84)
  ▲ CALLS (inbound)
  ├── checkout_cart (src/api/routes/checkout.c:112)
  │     ▲ CALLS
  │     └── handle_http_post (src/api/server.c:340)
  ├── retry_failed_payment (src/billing/scheduler.c:45)
  └── webhook_receive_pix (src/integrations/pix_gateway.c:67)
```

#### Step 3: Inspect Exact Code Implementation
The agent inspects the function logic:
```json
// Tool Call: get_code_snippet
{
  "project": "ecommerce-core",
  "qualified_name": "billing::payment_processor::validate_transaction",
  "source_mode": "full"
}
```

#### Step 4: Verify Index Coverage (Epistemic Discipline)
Before claiming that no other services call this function, the agent verifies coverage:
```json
// Tool Call: check_index_coverage
{
  "project": "ecommerce-core",
  "paths": [
    "src/billing/payment_processor.c",
    "src/api/routes/checkout.c",
    "src/integrations/pix_gateway.c"
  ]
}
```
**Response**: All paths `INDEXED`, 0 missed ranges.

#### Outcome
The agent responds to the developer with exact file locations, call hierarchy diagrams, and risk assessments. **Cost to the system**: 4 read-only queries, 0 seconds of lock contention, 0 database writes.

---

## Scenario 2: Speculative Refactoring in Ephemeral Horizons (The Temenos)

### Objective
An architect wants to experiment with decoupling the monolithic `payment_processor.c` into a modular strategy pattern without modifying the working tree or altering the production graph.

### Mindset & Phase
- **Cognitive Phase**: **Speculative Deliberation in the Temenos**.
- **Rules**: Allocate an ephemeral horizon database. Merge virtual nodes over the base graph. Admit changes only after Two-Tier AST Anchor verification.

### Execution Trace

#### Step 1: Create an Ephemeral Cognitive Horizon
The agent creates horizon `h_payment_decouple`:
```json
// Tool Call: create_horizon
{
  "horizon_id": "h_payment_decouple",
  "project": "ecommerce-core",
  "nodes": [
    {
      "cbm_uri": "cbm://billing/strategy.c#PaymentStrategy",
      "label": "Interface",
      "epistemic_status": "PROPOSED",
      "code_snippet": "typedef struct PaymentStrategy { int (*execute)(void *payload); } PaymentStrategy;"
    },
    {
      "cbm_uri": "cbm://billing/strategy_card.c#CreditCardStrategy",
      "label": "Class",
      "epistemic_status": "PROPOSED"
    }
  ],
  "edges": [
    {
      "source_uri": "cbm://billing/strategy_card.c#CreditCardStrategy",
      "target_uri": "cbm://billing/strategy.c#PaymentStrategy",
      "edge_type": "IMPLEMENTS"
    },
    {
      "source_uri": "cbm://billing/payment_processor.c#validate_transaction",
      "target_uri": "cbm://billing/strategy.c#PaymentStrategy",
      "edge_type": "DELEGATES_TO"
    }
  ]
}
```

#### Step 2: Query the Federated Graph
The agent runs Cypher queries overlaying the speculative horizon on top of the base graph:
```json
// Tool Call: query_graph
{
  "project": "ecommerce-core",
  "query": "MATCH (p:Function {name: 'validate_transaction'})-[:DELEGATES_TO]->(s) RETURN p.name, s.name",
  "active_horizons": ["h_payment_decouple"]
}
```
**Result**: Returns the virtual delegation edge seamlessly through the streaming K-Way merge iterator.

#### Step 3: Validate Scope Connectivity
Before promoting, the agent tests for orphan or disconnected nodes:
```json
// Tool Call: validate_scope_horizon
{
  "horizon_id": "h_payment_decouple",
  "strict_connectivity": true
}
```
**Verdict**: `VALID`. All speculative nodes connect cleanly to base graph anchors.

#### Step 4: Promote via Two-Tier AST Anchors
Once production code is written and tests pass, the horizon is promoted:
```json
// Tool Call: promote_horizon
{
  "horizon_id": "h_payment_decouple",
  "anchors": [
    {
      "file_path": "src/billing/strategy.c",
      "symbol_name": "PaymentStrategy",
      "byte_start": 0,
      "byte_len": 120,
      "ast_signature_hash": "a4f891b2c"
    }
  ]
}
```
**Result**: The Admission Gate verifies byte offsets and AST hashes, committing the new nodes to the Base Graph.

---

## Scenario 3: Governed Code Mutation & Cognitive Respiration (Sístole / The Seam)

### Objective
An agent receives an instruction: *"Add currency conversion logic to `src/billing/currency.c`"*.

### Mindset & Phase
- **Cognitive Phase**: **The Seam (O Limiar) & Sístole**.
- **Rules**: Any attempt to write without an active, bounded session is intercepted by the PreToolUse hook. TDD Nigredo requires failing tests before implementation.

### Execution Trace

#### Step 1: The Seam Intercepts
The agent directly attempts:
```json
// Client calls: write_to_file("src/billing/currency.c", content="...")
```
The **Antigravity Lifecycle Hook** intercepts the action before execution:
```json
// Hook Output:
{
  "decision": "deny",
  "reason": "VETO: Operation 'create/modify' on 'src/billing/currency.c' rejected. No active Union session horizon with valid intent_scope whitelist."
}
```

#### Step 2: Sístole — Session Opening with Strict Intent Scope
The agent must open an authenticated session with an exact path whitelist (1–4 files):
```json
// Tool Call: union_session_open
{
  "identity": "senior-developer",
  "contract_id": "c_strict_tdd",
  "based_on_seq": "gen_2026_09_30",
  "client_id": "session_86bef9aa",
  "intent_scope": [
    {"path": "c:/Users/corre/Documents/codebase-memory-mcp/tests/test_currency.c", "operation": "create"},
    {"path": "c:/Users/corre/Documents/codebase-memory-mcp/src/billing/currency.c", "operation": "create"}
  ]
}
```
**Verdict**: Session opened (`h_sess_curr_01`).

#### Step 3: Validate Provenance of Craft
The agent must declare how it will build the conversion algorithm:
```json
// Tool Call: validate_provenance
{
  "theme_id": "theme_clean_arch",
  "node_uri": "cbm://theme_clean_arch/rules#money_value_object",
  "pinned_version": "1.4.0",
  "declared_invention": false,
  "rationale": "Money must be represented as an immutable struct with integer cents to prevent floating point drift."
}
```
**Verdict**: Provenance accepted (`CITED_CANON`).

#### Step 4: TDD Nigredo (Sacrifício Material)
1. Agent writes the test in `tests/test_currency.c`.
2. Hook validates that path matches `intent_scope[0]`. **PERMITTED**.
3. Agent runs the test suite: **TEST FAILS (RED PHASE)**.

#### Step 5: TDD Albedo (Ressurreição & Sublimação)
1. Agent writes the implementation in `src/billing/currency.c`.
2. Hook validates that path matches `intent_scope[1]`. **PERMITTED**.
3. Agent executes test runner: **TEST PASSES (GREEN PHASE)**.
4. Agent debits action in ledger:
```json
// Tool Call: union_record_action
{
  "horizon_id": "h_sess_curr_01",
  "action_name": "implement_currency_conversion",
  "effect_class": "COMPENSABLE"
}
```

#### Step 6: Session Sweep & Closure
The agent concludes the session:
```json
// Tool Call: union_session_close
{
  "horizon_id": "h_sess_curr_01",
  "reason": "NORMAL"
}
```
**Verdict**: Factual trace emitted to disk, session horizon unlinked, Base Graph ready for incremental re-indexing.

---

## Scenario 4: Thematic Tradition & Cross-Project Governance (The Third Territory)

### Objective
A platform lead registers a company-wide standard for POSIX C11 error handling, binds it to the current project, and later harvests an innovative locking pattern discovered by an agent into a new theme.

### Mindset & Phase
- **Cognitive Phase**: **The Third Territory (A Tradição)**.
- **Rules**: Themes are read-only to projects. Projects reference themes via `DEVE` (normative) or `PODE` (consulted). Founding proposals require sovereign operator authorization.

### Execution Trace

#### Step 1: Register Institutional Craft Theme
```json
// Tool Call: theme_register
{
  "theme_id": "theme_posix_c11_errors",
  "namespace": "engineering/c/errors",
  "curator": "systems_architecture_board",
  "version": "1.0.0",
  "status": "ACTIVE"
}
```

#### Step 2: Establish Normative Binding
The tech lead binds the repository to this theme:
```json
// Tool Call: binding_claim
{
  "claim_id": "claim_bind_c11_err",
  "theme_id": "theme_posix_c11_errors",
  "pinned_version": "1.0.0",
  "mode": "NORMATIVE",
  "binding_scope": "repo",
  "validated_by": "tech_lead_user"
}
```
Now, any code modification that introduces raw `exit(-1)` or ignores return codes violates the normative binding, triggering `OUT_OF_SCOPE` or a blocking contestation.

#### Step 3: Peer Skill Contestation (Caller-Blind)
An adversarial QA skill inspects a proposed horizon:
```json
// Tool Call: contest_verify
{
  "target_ref": "cbm://core/connection.c#init_socket",
  "target_horizon": "h_socket_refactor",
  "severity": "BLOCKING",
  "evidence": "Violates bound theme 'theme_posix_c11_errors': socket error ignores errno translation."
}
```
**Result**: The Admission Gate blocks promotion of `h_socket_refactor` until the contestation is addressed.

#### Step 4: Harvesting Innovation via Founding Proposal
During implementation, the agent develops a high-performance lock-free ring buffer. During the session sweep, it proposes this pattern to the collective Tradition:
```json
// Tool Call: founding_propose
{
  "suggested_theme_id": "theme_lockfree_ringbuffer",
  "namespace": "engineering/c/concurrency",
  "rationale": "High-performance ring buffer with zero mallocs in critical path. Generic and applicable across all C microservices.",
  "origin_session": "h_sess_concurrency_42"
}
```

#### Step 5: Sovereign Operator Decision
The human curator reviews the proposal and accepts:
```json
// Tool Call: founding_decide
{
  "suggested_theme_id": "theme_lockfree_ringbuffer",
  "operator_accepted": true,
  "decided_by": "systems_architecture_board",
  "operator_token": "operator_secret_bearer_token"
}
```
**Outcome**: A new theme graph is founded in the catalog, ready to be bound and consulted by all future projects across the organization.
