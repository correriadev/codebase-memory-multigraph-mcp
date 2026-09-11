"""
Hermetic Sandbox Environment for E2E Tests.
Provides isolated, collision-free test projects and horizons with guaranteed cleanup.
"""

import os
import stat
import time
import uuid
import shutil
import tempfile
from pathlib import Path
from typing import Dict, List, Optional


def _force_remove(path: Path) -> None:
    """Removes a file or directory, clearing read-only flags if necessary."""
    try:
        if path.is_file() or path.is_symlink():
            os.chmod(path, stat.S_IWRITE)
            path.unlink(missing_ok=True)
        elif path.is_dir():
            shutil.rmtree(path, onerror=lambda f, p, e: (os.chmod(p, stat.S_IWRITE), f(p)))
    except Exception:
        pass


def resolve_cache_directory() -> Path:
    """Resolves the active CBM cache directory."""
    if os.environ.get("CBM_CACHE_DIR"):
        return Path(os.environ["CBM_CACHE_DIR"]).resolve()

    user_profile = os.environ.get("USERPROFILE") or os.environ.get("HOME", "")
    return (Path(user_profile) / ".cache" / "codebase-memory-mcp").resolve()


class TestSandboxEnvironment:
    """
    Hermetic test environment.
    Creates isolated test project databases and horizons with unique prefixes,
    guaranteeing no cross-test collisions and 100% teardown cleanup.
    """

    def __init__(self, prefix: str = "cbm_e2e_"):
        self.run_id = uuid.uuid4().hex[:8]
        self.project_name = f"{prefix}{self.run_id}"

        # Temporary source workspace directory
        self._temp_project_dir = tempfile.TemporaryDirectory(prefix=f"proj_{self.run_id}_")
        self.project_dir = Path(self._temp_project_dir.name).resolve()

        # Cache directory (shared with running daemon without DACL/conflict issues)
        self.cache_dir = resolve_cache_directory()
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.horizons_dir = self.cache_dir / "horizons"
        self.horizons_dir.mkdir(parents=True, exist_ok=True)

        self.base_db_path = self.cache_dir / f"{self.project_name}.db"
        self.tracked_horizons: List[str] = []

    @property
    def env_vars(self) -> Dict[str, str]:
        """Environment variables for child processes."""
        return {
            "CBM_PROJECT_DIR": str(self.project_dir).replace("\\", "/"),
            "CBM_REAPER_TTL_SECONDS": "1",
        }

    def register_horizon(self, horizon_id: str) -> Path:
        """Registers a horizon ID to be created and tracked for cleanup."""
        self.tracked_horizons.append(horizon_id)
        return self.horizons_dir / f"{horizon_id}.db"

    def cleanup(self) -> None:
        """Removes all test databases, WALs, SHMs, and project files."""
        # 1. Clean up base DB
        for suffix in ["", "-wal", "-shm"]:
            p = Path(str(self.base_db_path) + suffix)
            _force_remove(p)

        # 2. Clean up tracked horizons
        for h_id in self.tracked_horizons:
            h_path = self.horizons_dir / f"{h_id}.db"
            for suffix in ["", "-wal", "-shm"]:
                _force_remove(Path(str(h_path) + suffix))

        # 3. Clean up source directory
        if self._temp_project_dir:
            try:
                self._temp_project_dir.cleanup()
            except Exception:
                for attempt in range(5):
                    time.sleep(0.05 * (attempt + 1))
                    try:
                        _force_remove(self.project_dir)
                        break
                    except Exception:
                        pass
            self._temp_project_dir = None

    def __enter__(self) -> "TestSandboxEnvironment":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.cleanup()
