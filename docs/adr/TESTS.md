---
doc_type: adr
domain: testing
stack: [C, Python, pytest, ASan, UBSan]
node_id: "adr:tests"
tags: [testing, unit-tests, federation-tests, sanitizers]
edges:
  - relation: references
    target: "adr:architecture"
updated: 2026-09-11
---
# Testing Protocol

## OVERVIEW
Multi-tier test protocol combining pure C unit tests under Address and Undefined Behavior Sanitizers, mock-driven daemon isolation suites, and end-to-end Python regression testing.

## COMMANDS
| Type | Command | Description |
|---|---|---|
| Foundation Unit | `make -f Makefile.cbm test-foundation` | Executes foundational C unit tests with ASan and UBSan |
| Federation Unit | `make -f Makefile.cbm test` | Builds and runs all C test suites including federation and admission |
| Thread Sanitizer | `make -f Makefile.cbm test-tsan` | Runs C test suite under ThreadSanitizer (TSan) for race detection |
| Federation Python | `python -m unittest tests/test_multi_graph_federation.py` | Runs Python multi-graph federation unit and regression tests |
| Full E2E Suite | `make -f Makefile.cbm test-e2e` (or `python tests/e2e/run_e2e.py`) | Runs complete black-box MCP stdio JSON-RPC E2E test suite |
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
REQUIRED: Clean up all temporary SQLite horizon databases and WAL files in test fixture teardown functions.
REQUIRED: Use isolated temporary directories for horizon databases to avoid cross-test pollution.
FORBIDDEN: Hardcoding live host PIDs or executing unbounded sleeps in reaper daemon tests.
FORBIDDEN: Suppressing sanitizer errors or ignoring memory leaks in C test suites.

## TOOLING
- **Framework:** C11 custom test harness runner (via `Makefile.cbm`) and pytest 8.x for Python integration tests.
- **Assertions:** Native C assertion macros with formatted failure diagnostics and standard pytest assertions.
- **Mocks/Stubs:** Ephemeral in-memory SQLite instances and isolated disk horizon fixtures.
- **Sanitizers:** LLVM / GCC AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and ThreadSanitizer (TSan).
- **CI Integration:** Automated test execution via GitHub Actions running `Makefile.cbm` test targets on Linux, macOS, and Windows.

## TROUBLESHOOTING
- **Flaky tests:** Run with `TEST_SEAMS=1` to enforce deterministic scheduling and enable synthetic fault injection.
- **Debug mode:** Compile with `make -f Makefile.cbm CFLAGS_EXTRA="-g -O0"` and run under `lldb` or `gdb`.

## REFERENCES

- [**ARCHITECTURE.md**](./ARCHITECTURE.md): System architecture, layers, and pattern definitions.
- [**multi_graph_federation.md**](../feature/multi_graph_federation.md): Multi-graph federation implementation, tests, and source routing.
- [**multi_graph_federation_e2e.md**](../feature/multi_graph_federation_e2e.md): End-to-end testing suite for multi-graph federation over MCP stdio.
