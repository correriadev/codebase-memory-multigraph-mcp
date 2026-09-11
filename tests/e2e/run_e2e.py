"""
Unified E2E Test Suite Runner for Multi-Graph Federation.
Executes all end-to-end scenarios, measures timings, and reports structured metrics.
"""

import sys
import time
import unittest
from pathlib import Path

# Ensure repository root is on sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


def run_e2e_suite() -> int:
    suite_start = time.monotonic()
    print("=" * 70)
    print("   Codebase Memory MCP — Multi-Graph Federation E2E Test Suite   ")
    print("=" * 70)

    # Discover and load all test cases under tests/e2e
    e2e_dir = Path(__file__).resolve().parent
    loader = unittest.TestLoader()
    suite = loader.discover(start_dir=str(e2e_dir), pattern="test_*.py")

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    elapsed = time.monotonic() - suite_start
    print("-" * 70)
    print(f"E2E Execution Time : {elapsed:.3f} seconds")
    print(f"Total Scenarios    : {result.testsRun}")
    print(f"Passed             : {result.testsRun - len(result.failures) - len(result.errors)}")
    print(f"Failures           : {len(result.failures)}")
    print(f"Errors             : {len(result.errors)}")
    print("=" * 70)

    if result.wasSuccessful():
        print(">>> ALL E2E SCENARIOS COMPLETED SUCCESSFULLY <<<")
        return 0
    else:
        print(">>> SOME E2E SCENARIOS FAILED <<<")
        return 1


if __name__ == "__main__":
    sys.exit(run_e2e_suite())
