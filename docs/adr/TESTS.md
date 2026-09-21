---
doc_type: adr
domain: testing
stack: [C, Python, pytest, ASan, UBSan]
node_id: "adr:tests"
tags: [testing, unit-tests, e2e-tests, coverage]
edges:
  - relation: references
    target: "adr:architecture"
updated: 2026-09-20
---
# Testing Protocol

## OVERVIEW
Multi-tier test protocol combining pure C unit tests under Address and Undefined Behavior Sanitizers, mock-driven daemon isolation suites, and end-to-end Python regression testing over real MCP stdio channels.

## COMMANDS
| Type | Command | Description |
|---|---|---|
| Foundation Unit | `make -f Makefile.cbm test-foundation` | Executes foundational C unit tests with ASan and UBSan |
| Federation Unit | `make -f Makefile.cbm test` | Builds and runs all C test suites including federation and admission |
| Thread Sanitizer | `make -f Makefile.cbm test-tsan` | Runs C test suite under ThreadSanitizer (TSan) for race detection |
| Federation Python | `python -m unittest tests/test_multi_graph_federation.py` | Runs Python multi-graph federation unit and regression tests |
| Full E2E Suite | `python3 tests/e2e/run_e2e.py` | Runs complete 12-scenario black-box MCP stdio JSON-RPC E2E suite |
| Refactoring Admission | `python3 -m unittest tests.e2e.test_refactoring_admission` | Validates Two-Tier anchor checks, paths, and Base Graph consolidation |
| Standalone C | `./build/test_kway_merge` | Runs standalone compiled C test binary with verbose output |

## MINIMUM COVERAGE
REQUIRED: Maintain the following minimum coverage levels:

| Layer | Coverage | Description |
|---|---|---|
| Core Addressing & URI | 90% | Canonical parsing, FNV-1a hashing, and interning invariants |
| Horizon Pool & Reaper | 85% | LRU cache eviction, handle exhaustion bounds, and orphan file reaping |
| Query & K-Way Merge | 85% | Streaming heap ordering, shadowing, and LIMIT/SKIP pagination |
| Admission & Recall | 90% | Two-tier anchor verification, promotion gate, and acyclic BFS recall |
| Global Project Average | 80% | Across all production C modules and MCP handlers |

## PATTERNS & BEST PRACTICES
REQUIRED: Execute all C unit test suites with AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`) enabled.
REQUIRED: Execute E2E integration tests against the real compiled binary (`codebase-memory-mcp`) communicating over stdio pipes, avoiding mock algorithms.
REQUIRED: Generate real TypeScript specimen structures (`HarnessKitSpecimen`) to validate AST two-tier anchor verification and drift rejection.
REQUIRED: Clean up all temporary SQLite horizon databases and WAL files in test fixture teardown functions.
REQUIRED: Use isolated temporary directories for horizon databases to avoid cross-test pollution.
FORBIDDEN: Hardcoding live host PIDs or executing unbounded sleeps in reaper daemon tests.
FORBIDDEN: Suppressing sanitizer errors or ignoring memory leaks in C test suites.

## TOOLING
- **Framework:** C11 custom test harness runner (via `Makefile.cbm`) and unittest / pytest for Python E2E integration tests.
- **Assertions:** Native C assertion macros with formatted failure diagnostics and standard Python assertions.
- **Specimens:** Concrete TypeScript and source specimens modeled after `harness-kit` to test AST mutation resilience.
- **Sanitizers:** LLVM / GCC AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and ThreadSanitizer (TSan).
- **Subprocess Driver:** `MCPProcessSession` managing stdio pipes, non-blocking asynchronous reader threads, and forceful termination.

## TROUBLESHOOTING
- **WSL/DrvFs Timeouts:** Under Windows/WSL2 cross-filesystem execution, set initialization timeout to `15.0s` (`session.initialize(timeout=15.0)`) to accommodate process startup overhead.
- **Flaky tests:** Run with `TEST_SEAMS=1` to enforce deterministic scheduling and enable synthetic fault injection.
- **Debug mode:** Compile with `make -f Makefile.cbm CFLAGS_EXTRA="-g -O0"` and run under `lldb` or `gdb`.

<!-- DOCUMENT MAP: omitted — this baseline ADR has exactly 1 edge. The ## REFERENCES section below carries the relation. Include ## DOCUMENT MAP with Mermaid graph TD only when 2+ edges exist. -->

## REFERENCES

- [**README.md**](../README.md): Main documentation index.
- [**ARCHITECTURE.md**](./ARCHITECTURE.md): System architecture and patterns.
