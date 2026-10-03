# Corrected Codex UI Conversation Recovery Run Report

Date: 2026-10-02
Contract: Temenos fractal 0.5
Project: C-Users-corre-Documents-harness-kit
Horizon: h_ui_conversation_recovery_20261002_codex_r2
Status: source-fidelity correction verified

## Source and artifact

R2 was assembled by Python Path.read_text(encoding='utf-8') from:
- docs/temenos/t-20261001-1754/cbm-log.md (SHA-256 b0cd866b81fcaa73fb71c3e0da99e851ad22fc64f024308f25e8b6f92d7e6bec)
- docs/temenos/t-20261001-1754/registro.md (SHA-256 b07933f4f5f47b33efefa34e59d97c5b22c0f45de2dba50ea0b3f5ae2ebf3e86)

Human artifact: C:/Users/corre/Documents/harness-kit/docs/temenos/recovered-ui-20261002-r2/actual-ui-conversation.md
Artifact ID: ART-UI-CODEX-R2; 3,479 UTF-8 bytes; SHA-256 53cd5a65c2aa46cba792c0242662b8788c0f941eef50c9a8aa7c151be6ba359b.

The root stored the digest, byte count, seven ordered part IDs/digests, and correction text corrects_horizon=h_ui_conversation_recovery_20261002_codex; reason=R1 replaced UTF-8 characters during transcription before initial create. create_horizon returned ACTIVE, based_on_seq 0, 17 nodes and 22 edges. Maximum node code_snippet was 1,998 bytes.

## Fidelity and retrieval

Python checks confirmed the exact spellings pareça, usuário, porém, próprio, até, and the transition "CRIADO" > "PRONTO". The original source spells the user parenthetical no proprio without an accent; R2 preserves this verbatim and separately identifies the checklist spelling próprio. No question-mark substitutions occur in the source quote. Visual values remain explicitly UNKNOWN.

sync_horizon_spec returned success with 7 sections indexed. Strict validate_scope_horizon returned VALID with 0 isolated nodes and 0 unresolved dependencies. Fresh search_graph retrieval returned 24 overlays (17 explicit nodes plus 7 sections); all seven part contents exactly matched their source segments. query_graph returned 24 horizon nodes and all 22 edges, including ordered part links.

MCP-reconstructed bytes: 3,479; SHA-256 53cd5a65c2aa46cba792c0242662b8788c0f941eef50c9a8aa7c151be6ba359b. Physical file hash, root hash, and MCP reconstruction hash are equal.

R1 remains marked failed for source fidelity. Its artifact and horizon were not modified.

## Independent Session B: automatic discovery and recovery

Session B received only the project name and topic. It did not receive a horizon ID, artifact path, expected hash, source quotation, or answer, and it read no files. MCP catalog discovery returned 10 of 10 horizons (`has_more=false`, `partial=false`, no unreadable entries). It found three plausible UI candidates: the R1 recovery, this corrected recovery, and the synthetic visual fixture. It selected R2 from the retrieved provenance and contents: R2 identifies itself as a source-fidelity correction; the synthetic horizon explicitly describes itself as a fixture with no product approval.

Via MCP only, Session B retrieved the seven ordered parts and independently reconstructed 3,479 bytes with the root SHA-256 above. The per-part hashes and lengths matched. `validate_scope_horizon(strict_connectivity=true)` returned `VALID`, with zero isolated nodes and zero unresolved dependencies; the selected overlay contained 24 nodes and 22 edges. Search and query results marked the base-graph result sets truncated, but returned all 24 selected-horizon overlay nodes and all 22 overlay edges. No physical file was used in this recovery.

This proves source-checked capture, byte-integral horizon storage, and automatic topic discovery/recovery in another Codex MCP session. It does not prove retention after TTL expiration or daemon restart, and it does not test Antigravity skill discovery. The historical conversation itself originated in Antigravity; this verification ran only in Codex.

## Fresh MCP client recovery (owner process alive)

After the writer agent had exited, a new `codebase-memory-mcp.exe` stdio client received only the project and topic. It rediscovered the corrected horizon from the then-complete catalog, read all 24 overlay nodes, reconstructed the seven artifact parts, and independently matched every part hash/byte count and the root digest (3,479 bytes; SHA-256 `53cd5a65c2aa46cba792c0242662b8788c0f941eef50c9a8aa7c151be6ba359b`). The relation query returned 24 horizon nodes and 22 edges; strict validation remained `VALID` with zero isolated nodes or unresolved dependencies. No physical HarnessKit file was read and no MCP write occurred. Evidence: [after-writer-exit-smoke.json](after-writer-exit-smoke.json).

This was a new reader client after the writer agent exited; it was **not** a test after the horizon-owning MCP process exited. Follow-up catalog evidence identified owner PID 4932 as still alive for these horizons. The smoke therefore proves cross-client retrieval while the owner process remained alive, not retention after owner-process exit or daemon restart.

The continuation request supplied no new UI fact or design choice, so this test correctly made no new UI revision rather than inventing a delta. TTL cleanup was not exercised: the reaper function is present in `src/daemon/horizon_reaper.c`, but a scoped search found no production caller in `src`; expiration behavior therefore cannot be claimed from the current MCP path. No daemon restart was attempted.

The repository's existing `python -m unittest tests.e2e.test_crash_recovery_reaper -v` passed both tests. This test-only Python sweep uses `last_heartbeat` and a configurable `ttl_seconds=0`; the C implementation reads `created_at` and uses the fixed `CBM_HORIZON_TTL_SECONDS` (3,600), so it is not an equivalent implementation test. I corrected the Python harness docstring to state that boundary. The passing tests verify only the live/dead client branches of that isolated simulation, not the compiled C reaper, 3,600-second expiry, or a production scheduled sweep. The current production tool catalog exposes no reaper operation. Thus the extra checks prove immediate survival across writer/client exit and the harness's simulated orphan policy, but not durable retention or actual TTL cleanup. No new product UI data was supplied, so a material same-horizon UI progression remains correctly untested rather than fabricated.
