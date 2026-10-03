# UI thematic graph E2E status

Date: 2026-10-02  
Scenario: HarnessKit assistant conversation workspace, horizon `h_ui_visual_ideation_e2e_20261002_a1`  
Theme: `@harnesskit/ui-design-practices@1.0.0`

## Environment created

The thematic source is [`themes/harnesskit-ui-design/README.md`](../../../themes/harnesskit-ui-design/README.md). It covers applicability, spacing, hierarchy, components and states, color and contrast, responsive composition, motion, accessibility, the HarnessKit scenario mapping, and its evidence boundary. All HarnessKit dimensions and palette values are explicitly synthetic; the theme grants no product approval.

CBM indexed it as the separate project `HarnessKit-UI-Design-Practices`, and its durable catalog record points at `cbm-project://HarnessKit-UI-Design-Practices`. The record is persisted at the shared cache's `theme_registry.json` so a newly started CBM process can load the pin. No HarnessKit source or scenario file was edited for this run. The test snapshot helper copies only the two indexed graph databases and seeds the horizon from an MCP-exported fixture; it reports zero physical scenario files copied.

The UI horizon now contains a `FractalThemeReference` node pinned to the theme ID and version. The existing visual-token/typography section points to it with `INFORMED_BY`. The reference payload stores one primary and ten supporting exact section URIs, plus `epistemic_status=PROPOSED` and `product_approval=NONE`.

## Checks against the live CBM graph

| Check | Result |
| --- | --- |
| Shared theme index after production reindex | `ready`; all 12 README sections indexed and 32 typed topic relations present |
| Theme graph traversal | A live `File -[:DEFINES]-> Section` query returned all 12 README sections, including the ten stable `T-01` through `T-10` topics |
| New-build isolated index | 12 sections indexed; all 32 declared topic relations materialized as typed graph edges |
| Typed topic traversal | In the shared cache, a fresh Windows MCP process traversed `T-02 -[:SUPPORTS]-> T-03` to Typography and visual hierarchy |
| Search by practice | `search_graph("spacing")` returned the T-02 spacing/alignment/layout-rhythm section |
| UI horizon structure | The live horizon overlay query returned 15 nodes and 12 edges, including the theme-reference node and typed `INFORMED_BY` edge |
| Fresh retrieval | A separate Windows MCP process discovered `@harnesskit/ui-design-practices` and recovered the pinned theme reference from the shared UI horizon |
| UI fixture recovery | It recovered all 15 horizon overlays and verified the `INFORMED_BY` edge without reading the physical UI Markdown |
| C regression suites | ASan/UBSan `union_theme_registry` 8/8, `union_routing` 7/7, and `union_workflow_e2e` 20/20 passed |

The exact primary topic URI is:

```text
cbm://HarnessKit-UI-Design-Practices/README.md#HarnessKit-UI-Design-Practices.README.T-02-Spacing,-alignment,-and-layout-rhythm
```

## Handler regression checks

An AddressSanitizer run of the first reader process exposed a use-after-free in `theme_search`: the handler freed the input JSON document before copying its `query` string into the output. The handler now copies that string before freeing the document, and the workflow test includes a regression assertion.

The focused ASan/UBSan executable was then run twice: once with an in-memory catalog, and once in a new process loading the persisted shared `theme_registry.json`. Both runs passed these checks:

1. `theme_search` preserves the query and returns the catalog hit.
2. Thematic graph search constructs the exact URI for the indexed T-02 section.
3. `validate_provenance` accepts that exact pinned anchor and returns `ANCHOR_NOT_FOUND` for a URI with a changed fragment.

The handler harness uses a controlled graph-search response to isolate URI construction and validation. The existence and exact qualified name of that section were independently confirmed against the live indexed CBM graph above.

The full two-process JSON-RPC E2E then passed with a freshly rebuilt ASan/UBSan test runner and a private cache snapshot. The writer and reader were separate MCP processes. The writer seeded the UI horizon from the MCP-exported fixture and registered the theme; the reader discovered the catalog record, traversed 12 README sections (including all T-01..T-10 topics), verified the exact T-02 citation, rejected a missing fragment, recovered the theme reference and `INFORMED_BY` edge from the UI horizon, and reconstructed the UI fixture without reading a physical scenario file. The run reported `fixture_recovered_without_source_file_read=true`.

The commands used in WSL were:

```sh
CBM_TEST_CACHE_DIR=/home/corre/.cache/cbm-ui-design-e2e-final3 \
  python3 docs/temenos-tests/ui-e2e-20261002-a1/snapshot-theme-e2e-cache.py

CBM_CACHE_DIR=/home/corre/.cache/cbm-ui-design-e2e-final3 \
CBM_TEST_STANDALONE_MCP=1 \
CBM_BINARY_PATH=/mnt/c/Users/corre/Documents/codebase-memory-mcp/build/c/test-runner \
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:log_path=stderr \
  python3 docs/temenos-tests/ui-e2e-20261002-a1/run-theme-graph-e2e.py
```

## Windows production-binary MCP smoke

The separately built Windows production executable was first tested through
its real stdio MCP entry point using a private cache and runtime under
`%LOCALAPPDATA%`. It checked `initialize`,
`tools/list`, `index_repository`, `theme_register`, `get_graph_schema`, and
`theme_graph_query`. It found the five required MCP tools, indexed all 12
sections, confirmed 32 typed thematic edges, and traversed
`T-02 -[:SUPPORTS]-> T-03`. The test stopped only its own isolated daemon and
removed its temporary cache.

After the old daemon sessions were closed, the same production binary reindexed
the theme into the shared cache using Codex's default runtime. It reused the
existing immutable theme version, verified all 32 typed relations there, and
stopped its test daemon. A second fresh MCP process then recovered the theme,
the 15 UI horizon overlays, and the `INFORMED_BY` edge from that shared cache;
the UI Markdown was not opened. The repeatable read-only check is
[`verify-shared-ui-horizon.py`](./verify-shared-ui-horizon.py).

The pipeline regression suite passed 278 tests, including the new full and
incremental relation-indexing case. The isolated two-process E2E, shared-cache
reindex, and fresh-process UI recovery all passed.

## Remaining boundary

The repeatable private-cache runner is [`run-theme-graph-e2e.py`](./run-theme-graph-e2e.py); the Windows production-binary checks also exercised daemon IPC and stdio MCP on both isolated and shared caches.

The thematic README now declares typed relationships with the visible
`CBM_RELATIONS_V1` format. Full indexing and incremental postpasses validate
the declared relation types and targets, materialize those links between
sections, and remove stale links when declarations change. Declarations must
appear immediately below each section heading because indexed section prose
is capped at 500 bytes.

The four old-build MCP clients were closed at the user's direction, and the
old daemon was stopped. Codex's MCP command points to the tested executable,
with the previous `config.toml` saved as
`C:\Users\corre\.codex\config.toml.backup-20261002-fractal-theme`.
The test daemon was also stopped after verification. The current chat's MCP
tool list cannot refresh mid-session; a new Codex session will load the new
tool schema and start the validated build against the now-updated shared cache.

The original UI fixture's artifact root still says `digest_status=PENDING_POST_ROUND_TRIP`; the earlier session report records its byte/hash round trip and its open digest limitation. This thematic run did not promote or declare the UI horizon complete.
