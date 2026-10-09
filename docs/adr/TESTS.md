---
doc_type: adr
domain: testing
stack: [C, Python, pytest, ASan, UBSan]
node_id: "adr:tests"
tags: [testing, unit-tests, e2e-tests, coverage, union-workflows]
edges:
  - relation: references
    target: "adr:architecture"
updated: 2026-10-09
---
# Testing Protocol

## OVERVIEW
Multi-tier test protocol combining pure C unit tests under Address and Undefined Behavior Sanitizers, mock-driven daemon isolation suites, Union workflow integration tests, and end-to-end Python regression testing over real MCP stdio channels.

## COMMANDS
| Type | Command | Description |
|---|---|---|
| Foundation Unit | `make -f Makefile.cbm test-foundation` | Executes foundational C unit tests with ASan and UBSan |
| Federation Unit | `make -f Makefile.cbm test` | Builds and runs all C test suites including federation and admission |
| Thread Sanitizer | `make -f Makefile.cbm test-tsan` | Runs C test suite under ThreadSanitizer (TSan) for race detection |
| Admission & Recall (C) | `./build/c/test-runner anchor_checker recall_engine` | Runs 33 native C unit/integration tests for AST anchors and recall |
| AST Anchor Isolation | `python -m unittest tests/test_ast_anchor_isolation.py` | Runs Tree-sitter anchor isolation, relocation, and trivia rejection tests |
| Atomic Admission | `python -m unittest tests/test_atomic_admission_concurrency.py` | Runs two-phase promotion, fail-fast consolidation, and refusal tests |
| Transitive Recall | `python -m unittest tests/test_transitive_recall_closure.py` | Runs reverse causal BFS traversal and atomic contestation tests |
| Cross-Horizon Admission | `python -m unittest tests/test_cross_horizon_admission.py` | Runs cross-horizon concurrency arbitration and conflict anticipation tests |
| Complete Python Suite | `python -m unittest discover tests` | Runs full 186-test Python regression test suite across all modules |
| Union Workflow | `make -f Makefile.cbm test-union-workflow` | Builds and executes the complete Union workflow E2E test suite |

## MINIMUM COVERAGE
REQUIRED: Maintain the following minimum coverage levels:

| Layer | Coverage | Description |
|---|---|---|
| Core Addressing & URI | 90% | Canonical parsing, FNV-1a hashing, and interning invariants |
| Horizon Pool & Reaper | 85% | LRU cache eviction, handle exhaustion bounds, and orphan file reaping |
| Query & K-Way Merge | 85% | Streaming heap ordering, shadowing, and LIMIT/SKIP pagination |
| Admission & Recall | 90% | Two-tier anchor verification, promotion gate, and acyclic BFS recall |
| Union Sessions & Contracts | 90% | Session lifecycles, contract verification, and restricted mode gates |
| Gateways & Contestation | 85% | Effect classification, refusal taxonomy, and caller-blind verification |
| Thematic KB & Doc Plane | 85% | Theme registries, binding claims, founding proposals, and drift checks |
| Global Project Average | 80% | Across all production C modules and MCP handlers |

## PATTERNS & BEST PRACTICES
REQUIRED: Execute all C unit test suites with AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`) enabled.
REQUIRED: Execute E2E integration tests against real compiled binaries communicating over stdio pipes, avoiding mock algorithms.
REQUIRED: Validate restricted mode behavior when opening union sessions without registered contracts.
REQUIRED: Ensure blocking contestations prevent horizon promotion in all admission test fixtures.
REQUIRED: Clean up all temporary SQLite horizon databases and WAL files in test fixture teardown functions.
FORBIDDEN: Hardcoding live host PIDs or executing unbounded sleeps in reaper daemon tests.
FORBIDDEN: Suppressing sanitizer errors or ignoring memory leaks in C test suites.

## TOOLING
- **Framework:** C11 custom test harness runner (via `Makefile.cbm`), `test-runner` for sharded suites, and unittest / pytest for Python E2E integration tests.
- **Assertions:** Native C assertion macros with formatted failure diagnostics and standard Python assertions.
- **Specimens:** Concrete TypeScript specimens modeled after `harness-kit` to test AST mutation resilience.
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
