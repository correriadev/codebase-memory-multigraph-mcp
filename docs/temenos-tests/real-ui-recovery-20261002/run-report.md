# Codex UI Conversation Recovery Run Report

## R1 fidelity correction: FAILED

The R1 artifact is not source-faithful. Comparison with the original UTF-8 cbm-log.md and registro.md found character substitutions during transcription. R1 hash equality proves only that the corrupted R1 bytes were stored and retrieved consistently; it does not verify fidelity to the original conversation. The R1 file and horizon remain unchanged as failed-attempt evidence. Use corrected R2 below for source-faithful retrieval.

Corrected artifact: C:/Users/corre/Documents/harness-kit/docs/temenos/recovered-ui-20261002-r2/actual-ui-conversation.md  
Corrected horizon: h_ui_conversation_recovery_20261002_codex_r2  
Corrected run report: run-report-r2.md



Date: 2026-10-02  
Temenos contract: fractal 0.5  
Project: `C-Users-corre-Documents-harness-kit`  
Horizon: `h_ui_conversation_recovery_20261002_codex`  
Purpose: recover and round-trip the historical Jira-like backlog conversation using Codex only.

## Catalog and scope

The live project catalog returned 5 projects with no further page; the exact harness-kit project was present. `list_horizons` for that exact project, `status=ALL`, returned 8 records, `has_more=false`, `partial=false`, and `unreadable_entries=0`. The seven existing `h_fractal_real_*` horizons are the narrow D-03 probes described in the task brief; `h_ui_visual_ideation_e2e_20261002_a1` is synthetic. The actual-conversation recovery scope was not represented, so this new horizon was created independently, without parent/child links.

The historical conversation host was Antigravity. This capture and recovery test ran in Codex. Source facts were supplied from `docs/temenos/t-20261001-1754/cbm-log.md` and `registro.md`; the artifact is a concise reconstruction, not a full transcript. It retains the three supplied user turns, the broad scope of `essa abordagem esta boa`, D-03/D-04/D-05 as recorded/accepted design choices, the not-verified implementation status, Q-03, and explicitly unknown visual details. No visual values were invented.

## Artifact and initial create

Human file: `C:\Users\corre\Documents\harness-kit\docs\temenos\recovered-ui-20261002\actual-ui-conversation.md`  
Artifact ID: `ART-UI-CODEX-R1`  
UTF-8 byte count: 2,464  
SHA-256: `0c3a773a619f823313421626fc9145529d67ddd06f5d66275d95931b92423546`

The SHA-256 and byte count were computed from the exact file bytes with Python before the first `create_horizon`. Its root node initially stored that digest, size, join policy, and all ordered part IDs/byte counts/digests. `create_horizon` returned success, `ACTIVE`, `based_on_seq=0`, `client_pid=4932`, 16 nodes, and 20 edges. The 16 initial node payloads range up to 1,472 UTF-8 bytes, below the 3,000-byte limit.

| Order | Part ID | Bytes | SHA-256 | MCP content equality |
| ---: | --- | ---: | --- | --- |
| 1 | `ART-UI-CODEX-R1-P01` | 448 | `356d7ae8860dc062ec1d07ae1b45d0833a6ae277148ca8e8db4993457c7ceca6` | yes |
| 2 | `ART-UI-CODEX-R1-P02` | 438 | `eb657f36b3bca3d15e94addd60281f13ecbacf62c67042255e84c22c004dd5f4` | yes |
| 3 | `ART-UI-CODEX-R1-P03` | 341 | `5496c6df0c427147c24e6c0bce41f5852d5526a1eac5abd2e8ac07bbf13e5bfa` | yes |
| 4 | `ART-UI-CODEX-R1-P04` | 349 | `d21cc743fc7a1280bfad3ffef351361b70273458a60f578e9e41801e6bba03f2` | yes |
| 5 | `ART-UI-CODEX-R1-P05` | 448 | `36dfba249d075216b4b525ddd169b9eda3079dbeb1771ea94a6c746aeed1d52a` | yes |
| 6 | `ART-UI-CODEX-R1-P06` | 440 | `24475b39f859b0a758bc7c5dc3a573c07c909ef59218faceda61711a7a8a3b19` | yes |

## Sync and MCP verification

`sync_horizon_spec` received the same full Markdown content and path as the human file. It returned `success=true`, `nodes_compiled=0`, `edges_compiled=0`, and `sections_indexed=6`; the 16 explicit nodes/20 edges from initial create remained present.

- Strict `validate_scope_horizon`: `VALID`; 0 isolated nodes; 0 unresolved dependencies.
- `search_graph` with the new horizon active and `qn_pattern=".*"` retrieved 22 overlays: all 16 explicit nodes and all 6 Markdown sections. Each of the six part payloads parsed successfully, matched its source segment exactly, and matched its stored byte count and part digest.
- Reconstructed the file by concatenating MCP `exact_content` in part order, with no separators or normalization. Python SHA-256 over the reconstructed MCP UTF-8 bytes was `0c3a773a619f823313421626fc9145529d67ddd06f5d66275d95931b92423546`, 2,464 bytes. The physical file hash, MCP root-node hash, and reconstructed MCP hash are equal.
- `query_graph` with the same active horizon returned all 22 horizon nodes and 20 horizon edges, including all `CONTAINS_PART` and ordered `NEXT_PART` links. The strict validator also passed after sync.

Persisted state: human Markdown present; CBM horizon active and synchronously verified. Implementation status in the recovered record remains `NOT_VERIFIED`.
