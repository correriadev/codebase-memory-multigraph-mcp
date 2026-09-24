# REWORK-LOG — Feature F001 (multi_graph_federation)

## Attempt #1 Validation Findings

### Grumpy Tech Lead Findings (Score: 0.55)
1. **MCP Handler Federation Bypassed & Discarded:**
   - Location: `src/mcp/handlers.c:52-98` and `src/mcp/mcp.c:17273-17290`
   - Issue: `cbm_mcp_handle_federated_search_graph`, `cbm_mcp_handle_federated_query_graph`, and `cbm_mcp_handle_federated_trace_path` parse `active_horizons` but immediately return the base graph result without invoking K-Way merge or overlaying horizon nodes, while calling static functions from `mcp.c`. Speculative nodes in active overlays are discarded.
2. **MergeRecord Key Truncation & Shadowing Collisions:**
   - Location: `src/query/kway_merge.h:20-25` and `src/query/kway_merge.c:80-88`
   - Issue: `MergeRecord.key` is capped at 256 bytes while `CbmUri` allows up to 1024 bytes (`CBM_URI_MAX_LEN`). This risks key truncation collisions and false shadowing for deeply nested files. Payloads > 1024 bytes are also silently truncated.
3. **AST Signature Match Missing Tree-sitter & Using strstr/Byte Hash:**
   - Location: `src/admission/anchor_checker.c:78-98`
   - Issue: `cbm_ast_signature_match` relies on raw `strstr()` and FNV-1a byte hashing rather than Tree-sitter AST parsing. Identifiers matching earlier comments/imports cause false matches, and byte hashing fails benign formatting changes.
4. **Stack Overflow Risk & Silent Truncation in BFS Queue:**
   - Location: `src/core/symbolic_node.c:183-238` and `src/admission/recall_engine.c:12-58`
   - Issue: BFS traversals allocate a 128 KB queue array directly on the stack (`char queue[128][CBM_URI_MAX_LEN]`), cap queue size at 128 items, and limit edge fan-out to 16. This risks stack exhaustion on worker threads and silently truncates dependency closures when a symbol has >128 dependents.
5. **Autocommit & Repeated Statement Compilation in Recall Loop:**
   - Location: `src/admission/recall_engine.c:84-98`
   - Issue: `cbm_trigger_recall` repeatedly prepares and finalizes single-row UPDATE statements inside a loop without reusing statement handles or wrapping in an explicit SQLite transaction.
6. **Admission Gate Promote Bypasses Two-Tier Checks & Base Graph Consolidation:**
   - Location: `src/mcp/promote_handler.c:21-23` and `src/admission/admission_gate.c:46-54`
   - Issue: `handle_promote_horizon` passes null anchors and zero count to `cbm_promote_horizon`, bypassing all two-tier anchor checks. `cbm_promote_horizon` merely toggles the status column in `horizon_metadata` without transferring promoted nodes or edges to the base graph.

### Adversarial QA Findings (Score: 0.05, High/Critical Vulns: true)
1. **[HIGH] Heap Buffer Over-Read in `cbm_ast_signature_match`:**
   - Location: `src/admission/anchor_checker.c:89-91`
   - Issue: When anchor text is found near the end of the file buffer via `strstr()`, `match_len` is set to `anchor->byte_len` without verifying `(content + sz - found) >= match_len`. If `match_len` exceeds remaining buffer space, `cbm_fnv1a_64` reads past heap buffer bounds.
2. **[HIGH] Out-of-Bounds Struct Read in `cbm_fast_offset_match`:**
   - Location: `src/admission/anchor_checker.c:44` and `src/admission/anchor_checker.h:14`
   - Issue: `memcmp` compares `anchor->byte_len` bytes against `anchor->expected_text` (fixed capacity 1024). If `anchor->byte_len > 1024`, `memcmp` reads past `expected_text` field in `TwoTierAnchor` struct.
3. **[HIGH] Path Traversal & Arbitrary File Deletion in `cbm_discard_horizon` / `cbm_create_horizon`:**
   - Location: `src/core/horizon_pool.c:54, 107-109, 251-262`
   - Issue: `make_horizon_path` concatenates `horizon_id` without sanitizing directory traversal sequences (`..` or path separators `/`, `\`). An attacker supplying a crafted `horizon_id` can trigger `cbm_unlink` on arbitrary SQLite files.
4. **[HIGH] Admission Gate Integrity Bypass in `mcp.c` / `promote_handler.c`:**
   - Location: `src/mcp/mcp.c:17318` and `src/mcp/promote_handler.c:23`
   - Issue: Tool dispatcher calls `handle_promote_horizon` with NULL pool and NULL gate. `handle_promote_horizon` invokes `cbm_promote_horizon` with `anchors=NULL` and `anchor_count=0` without parsing anchors from `args_json`.
5. **[HIGH] Broken Core Overlay Workflow:**
   - Location: `src/mcp/mcp.c:17273-17290` and `src/mcp/handlers.c:52-98`
   - Issue: Tools `search_graph`, `query_graph`, and `trace_path` bypass federated handlers and call legacy handlers directly. Federated handlers in `handlers.c` return base results without reading horizon databases or streaming overlays via K-Way merge.
6. **[TIER 3 | -0.05] Reverse Dependency Drop on Queue Overflow:**
   - Location: `src/admission/recall_engine.c:12-14`
   - Issue: Non-circular queue of 128 elements stops enqueuing when `q_tail` reaches 128, silently dropping dependents beyond the 128th node.
7. **[TIER 3 | -0.05] Inbound Virtual Edge Limit (Fan-In Truncation):**
   - Location: `src/core/symbolic_node.c:212`
   - Issue: Static array of 16 edges ignores inbound virtual edges beyond 16, prematurely truncating BFS traversal.
8. **[TIER 3 | -0.05] KWayMergeContext `limit=0` Premature Termination:**
   - Location: `src/query/kway_merge.c:108`
   - Issue: Loop condition `ctx->emitted_count < ctx->limit` without sentinel for unlimited (`limit == 0` or negative) causes 0 rows to be emitted.
9. **[TIER 3 | -0.05] Makefile.cbm Missing Federation Sources in Prod & Test Targets:**
   - Location: `Makefile.cbm:507, 743`
   - Issue: Federation sources omitted from `PROD_SRCS` and `ALL_TEST_SRCS`, causing build failure or omitted test execution.

## Attempt #2 Validation Findings

### Grumpy Tech Lead Findings (Score: 0.65)
1. **Unattached `base_db` in Admission Gate (`src/mcp/mcp.c:1719, 17323` and `src/admission/admission_gate.c:63-126`):**
   - `cbm_admission_gate_set_base_db` is never called, leaving `srv->admission_gate.base_db` perpetually NULL. `promote_horizon` fails to consolidate admitted symbols into the Base Graph.
2. **Overlay Payload Integration in MCP Content Array (`src/mcp/handlers.c:118, 209-210, 281`):**
   - Overlays are attached as sibling properties to the top-level response rather than properly integrated/documented inside `content[0].text` for standard MCP clients/LLMs.
3. **Hardcoded Pagination in Federated Query Handlers (`src/mcp/handlers.c:80, 153, 246`):**
   - `cbm_kway_merge_init(&ctx, 0, 100)` hardcodes `skip = 0` and `limit = 100`, ignoring caller pagination parameters in `args_json`.
4. **Active Horizons Parsing with `strstr` on Raw JSON (`src/mcp/handlers.c:17-56`):**
   - Manual substring search with `strstr` on raw JSON instead of using `yyjson` risks false positives when query text or symbols contain `"active_horizons"`.
5. **AST Signature Match Robustness (`src/admission/anchor_checker.c:80-108`):**
   - `cbm_ast_signature_match` should handle whitespace/formatting variations cleanly to avoid false drift.

### Adversarial QA Findings (Score: 0.45, CRITICAL Vulns: true, isCrashing: true)
1. **[CRITICAL / CRASH] Infinite Loop, Uninitialized Stack Memory Leak, and OOM Crash in KWay Merge Loop:**
   - Location: `src/mcp/handlers.c:101-110, 173-182, 265-274` and `src/query/kway_merge.c:137-139`
   - Issue: In `cbm_mcp_handle_federated_search_graph`, `cbm_mcp_handle_federated_query_graph`, and `cbm_mcp_handle_federated_trace_path`, `MergeRecord rec` is uninitialized on stack. The loop condition `while (cbm_kway_merge_step(&ctx, &rec, &has_more) == 0 && rec.key[0])` causes an infinite loop because `cbm_kway_merge_step` returns 0 when no rows are emitted (heap exhausted or limit reached) without zeroing `out_record`. If a row was previously emitted, `rec.key[0]` stays non-zero, continuously appending objects until OOM crash. Also, if heap was empty, `rec.key[0]` reads uninitialized stack memory.
   - Fix: Initialize `MergeRecord rec = {0};`, and check `while (cbm_kway_merge_step(&ctx, &rec, &has_more) == 0 && has_more)` (or ensure step sets `out_record->key[0] = '\0'` when exhausted and check return value properly).
2. **[HIGH] Admission Gate Base Graph Consolidation Bypass:**
   - Location: `src/mcp/mcp.c:1719, 17323` and `src/admission/admission_gate.c:63-125`
   - Issue: `srv->admission_gate.base_db` is NULL because `cbm_admission_gate_set_base_db` is never called. Wire the active database handle from `srv->store` (or session DB) into `srv->admission_gate.base_db` during MCP server initialization / project load, and attach it to the gate before promoting.
3. **[TIER 3 | -0.05] Reverse Dependency Cap at 64 (`src/admission/recall_engine.h:8`, `src/admission/recall_engine.c:39-42`):**
   - Issue: `CBM_RECALL_MAX_AFFECTED` is hardcoded to 64. Increase or dynamically grow so symbols with >64 reverse dependents are not silently dropped.
4. **[TIER 3 | -0.05] Client Pagination Parameter Extraction (`src/mcp/handlers.c:80, 152, 245`):**
   - Issue: Parse `limit` and `skip`/`offset` from `args_json` using `yyjson` rather than hardcoding `(0, 100)`.
