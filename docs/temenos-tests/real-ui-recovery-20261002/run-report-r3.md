# R3 UI Conversation Recovery Run Report

Date: 2026-10-02
Contract: Temenos fractal 0.5.2
Project: C-Users-corre-Documents-harness-kit
Horizon: h_ui_conversation_recovery_20261002_codex_r3
Status: verified; source-grounded editorial correction to R2
Artifact scope: concise, source-grounded recovery of relevant conversation content, not a verbatim full transcript.

## Source and artifact hashes

Original UTF-8 sources were read with Python Path.read_text(encoding='utf-8'); source files were not modified.
- docs/temenos/t-20261001-1754/cbm-log.md: SHA-256 b0cd866b81fcaa73fb71c3e0da99e851ad22fc64f024308f25e8b6f92d7e6bec
- docs/temenos/t-20261001-1754/registro.md: SHA-256 b07933f4f5f47b33efefa34e59d97c5b22c0f45de2dba50ea0b3f5ae2ebf3e86
- R2 artifact, read-only comparison source: SHA-256 53cd5a65c2aa46cba792c0242662b8788c0f941eef50c9a8aa7c151be6ba359b; 3,479 bytes

R3 human artifact: C:/Users/corre/Documents/harness-kit/docs/temenos/recovered-ui-20261002-r3/actual-ui-conversation.md
Artifact ID: ART-UI-CODEX-R3; 3,797 UTF-8 bytes; SHA-256 d701224de1587c13e37802c4da8c623e4535710a1ad390ab04ad5dcfee859567; UTF-8 without BOM.

The three labels now cite the source headings and speaker: Turno 2/3/4 with Humano. Python matched each quoted utterance exactly against the original source. A normalized comparison that reverts only the three labels, revision number, and R3 correction note equals the R2 file byte-for-text, confirming the user-design content did not change. The R3 correction/supersession applies to editorial turn/speaker metadata only. Source spelling caveat remains: the original quote says no proprio without an accent; the artifact preserves that source text and carries the prior R2 note.

## Ordered artifact parts

| Order | Part ID | Bytes | SHA-256 |
| ---: | --- | ---: | --- |
| 1 | ART-UI-CODEX-R3-P01 | 764 | ac40c9ded01eccd881bc7ee6e07304e5fcc9f220e4920ac4f058ec7c91666ad3 |
| 2 | ART-UI-CODEX-R3-P02 | 496 | da4f12f7fd792503ef3639673957cd4f65af99c803176e05059e3dd701699ce6 |
| 3 | ART-UI-CODEX-R3-P03 | 594 | 11c04a204f5f42b84c3d67ebf4d9ebef0737de3ff5f3f7da9ac5b9349a207f43 |
| 4 | ART-UI-CODEX-R3-P04 | 492 | 94667c6f34f8df638abba7fde8fded9f36746aa6c26922eb13e0d869e1b45489 |
| 5 | ART-UI-CODEX-R3-P05 | 753 | 1947b1ba12c33f0d1349f60ee486c7f863c8e08b49ee993515b8287d74190814 |
| 6 | ART-UI-CODEX-R3-P06 | 190 | a8faefa6268c70f7124e67a5616b07fa89ff19c37ee89eb3d6d3af64aacae7fc |
| 7 | ART-UI-CODEX-R3-P07 | 508 | 43de252d303849ec7edf4d0b0e21d9a5be320eb35dc399b13066ceee615bb4fd |

## MCP results

Before create_horizon, the exact project horizon catalog was complete: 6 returned, has_more=false, partial=false. R3 was created as a new independent horizon with no upsert and no parent/child edge. Initial create returned ACTIVE, based_on_seq 0, 17 nodes, 22 edges. Maximum node snippet was 2,323 bytes. The root stored the R3 artifact hash/size, ordered part metadata, source hashes, and correction note before create.

sync_horizon_spec returned success with 7 sections indexed and 0 additional nodes/edges compiled. Strict validate_scope_horizon returned VALID with 0 isolated nodes and 0 unresolved dependencies.

A fresh post-sync MCP search returned 24 overlays (17 explicit nodes and 7 Markdown sections). All seven recovered exact_content payloads matched their source parts. query_graph returned 24 horizon nodes and all 22 edges, including ordered part links. MCP reconstruction was 3,797 bytes with SHA-256 d701224de1587c13e37802c4da8c623e4535710a1ad390ab04ad5dcfee859567; root, file, and reconstruction hashes are equal.

R1 and R2 files, horizons, and reports remain unchanged.

## Independent Session B: blind MCP-only recovery

The independent reader received only the exact project and the topic `UI conversation recovery`. It received no horizon ID, artifact path, expected hash, source quotation, or answer, and read no physical file. The complete catalog returned 7/7 horizons (`has_more=false`, `partial=false`, `unreadable_entries=0`). It selected R3 from the retrieved provenance and contents: R1 had altered UTF-8 characters, R2 restored the source quotation but used unsupported `? User` speaker labels, and R3 corrects the turn/speaker labels while leaving the quoted user content and design record unchanged. Selection followed content and recorded corrections, not timestamps.

Through MCP only, Session B recovered all 7/7 ordered parts. Each part's byte count and SHA-256 matched its metadata; concatenation yielded 3,797 bytes and SHA-256 `d701224de1587c13e37802c4da8c623e4535710a1ad390ab04ad5dcfee859567`, equal to the artifact root. The selected horizon returned 24 overlay nodes and 22 edges; the part chain and edge endpoints were complete, and strict validation returned `VALID` with 0 isolated nodes and 0 unresolved dependencies. It recovered the card conversation flow and recorded scope, but explicitly retained unknowns for colors, typography, dimensions, responsive behavior, and accessibility. It also treated broad acceptance of the approach as such, not as approval of every design detail. The original source files were not independently retrievable by the reader through `search_code`; the source hashes therefore remain writer-verified rather than revalidated by Session B.

This closes the happy-path test for automatic horizon discovery and content-integral recovery in a new MCP-reading session without the physical source file. It does not prove long-term retention. The selected horizon was `ACTIVE`, linked by `explicit_project`, `based_on_seq=0`, and had `owner_alive=true` during the reader test.

## Catalog retention follow-up

A later complete MCP catalog contained 7 horizons, while the historical post-writer smoke recorded 10. Four earlier probe horizons were absent: `h_fractal_real_20261001_190429`, `_190712`, `_190755`, and `_200159`. Fresh MCP `search_graph` and `query_graph` calls for each former ID returned no overlay nodes or edges (without tool errors). Earlier catalog evidence recorded the first three owners as dead; it recorded `_200159` as alive at that earlier observation. The current observation does not establish why any horizon disappeared, so it is not attributed conclusively to TTL cleanup, a reaper, manual removal, or another storage event. R1, R2, and R3 remained discoverable and active under owner PID 4932, which was alive; see [catalog-retention-followup.json](catalog-retention-followup.json).

Consequently, the R3 proof itself is cross-client discovery and reconstruction while its horizon owner process is alive; that run did not test owner exit or daemon restart. A separate isolated persistence gate later tested owner exit and a new daemon against a synthetic horizon on a copied HarnessKit graph snapshot; see the [retention/restart report](../retention-restart-20261002/result.md). Neither run establishes TTL expiry, durable retention in the shared cache, or actual scheduled cleanup. The Python reaper harness is a test-only approximation and does not exercise the production C function or its scheduling.

## Separate owner-exit and daemon-restart gate

On 2026-10-02, run `20261002-161234-6916` passed an isolated persistence test using the installed CBM binary and a private cache/runtime. Its base graph was an SQLite online-backup snapshot of the real HarnessKit graph. A writer created synthetic horizon `h_retention_restart_20261002_161234_6916`, compiled 3 semantic nodes and 3 edges, and passed strict validation. After its last MCP client exited, the owner daemon stopped automatically. An offline SQLite check still found the horizon (`integrity_check=ok`, 7 symbolic nodes, 3 virtual edges). A new daemon (PID 5404, after writer PID 19200) then rediscovered the horizon as ACTIVE with `owner_alive=false`; an independent MCP reader reconstructed the exact 82-byte synthetic artifact and matched SHA-256 `d1b75ba4a8468e6a58149c1c51d032145aa05b0049e1bca4b7089e72b7b7b24`. MCP edge recovery and strict validation passed after restart. The reader did not open the human artifact.

The shared daemon (PID 4932) remained running, and the shared HarnessKit graph DB hash matched before and after (`6b6dcb5ee3aeed5bfbca71f01b659d7a2eb60edb504c0066f15fe0d3e87d655d`). This gate verifies persistence across owner-process exit and one daemon restart in the isolated snapshot. It does not test the R3 horizon, the shared live cache, the 3,600-second TTL, device reboot, application upgrade, or long-term retention. Earlier attempts in this folder exposed test-harness reporting/parser errors; only the canonical run above is the passing result.

[Human-readable result](../retention-restart-20261002/result.md) ? [machine report](../retention-restart-20261002/20261002-161234-6916/report.json) ? [MCP transcript](../retention-restart-20261002/20261002-161234-6916/mcp-transcript.json) ? [offline owner-exit snapshot](../retention-restart-20261002/20261002-161234-6916/horizon-after-owner-exit.json).

## Progression decision

The continuation prompt contained no new UI fact, requirement, or design decision. No material progression was added and no R4 horizon was created. The next UI revision should be triggered by a real change in the shared dimension, such as an explicit choice on screen layout, visual tokens, interaction details, accessibility, or a resolved open question, not by repetition of the test request.
