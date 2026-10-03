"""Make a private, transaction-consistent snapshot for the Linux MCP E2E run.

The shared Windows cache is intentionally not opened by a Linux CBM process:
its mounted-drive ACL ancestry cannot satisfy CBM's private-cache admission
check. SQLite's backup API copies only the two indexed project graphs and the
existing UI horizon; this test snapshot never reads the source Markdown.
"""

import os
import sqlite3
from pathlib import Path


SOURCE = Path("/mnt/c/Users/corre/.cache/codebase-memory-mcp")
DEST = Path(os.environ.get(
    "CBM_TEST_CACHE_DIR",
    "/home/corre/.cache/cbm-ui-theme-e2e-20261002-a1-c",
))
DATABASES = (
    "C-Users-corre-Documents-harness-kit.db",
    "HarnessKit-UI-Design-Practices.db",
)


def backup_database(relative: str) -> None:
    source = SOURCE / relative
    target = DEST / relative
    if not source.is_file():
        raise FileNotFoundError(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(f"file:{source}?mode=ro", uri=True, timeout=30) as src:
        with sqlite3.connect(target, timeout=30) as dst:
            src.backup(dst)
            result = dst.execute("PRAGMA integrity_check").fetchone()
            if not result or result[0] != "ok":
                raise RuntimeError(f"integrity_check failed for {relative}: {result}")


def main() -> None:
    DEST.mkdir(mode=0o700, parents=True, exist_ok=False)
    os.chmod(DEST, 0o700)
    for relative in DATABASES:
        backup_database(relative)
    print(f"snapshot={DEST}")
    print(f"databases={len(DATABASES)}")
    print("sqlite_integrity=ok")
    print("physical_scenario_files_copied=0")
    print("ui_horizon_source=MCP-exported fixture will be seeded by the writer process")


if __name__ == "__main__":
    main()
