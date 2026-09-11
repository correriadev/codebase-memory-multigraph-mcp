"""
Horizon Reaper Harness for E2E Crash Recovery Testing.
Implements the exact OS process liveness check and TTL eviction algorithm
from src/daemon/horizon_reaper.c.
"""

import os
import sys
import time
import sqlite3
from pathlib import Path
from typing import Dict, List


def is_pid_alive(pid: int) -> bool:
    """
    Checks if a PID is alive on the operating system.
    Matches cbm_process_alive() in src/daemon/horizon_reaper.c.
    """
    if pid <= 0:
        return False

    if sys.platform == "win32":
        import ctypes
        import ctypes.wintypes

        # PROCESS_QUERY_LIMITED_INFORMATION = 0x1000, SYNCHRONIZE = 0x00100000
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
        if not handle:
            # Check GetLastError() == ERROR_ACCESS_DENIED (5) which means process exists
            error = kernel32.GetLastError()
            return error == 5

        exit_code = ctypes.wintypes.DWORD()
        STILL_ACTIVE = 259
        if kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
            kernel32.CloseHandle(handle)
            return exit_code.value == STILL_ACTIVE

        kernel32.CloseHandle(handle)
        return False
    else:
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return True


def run_reaper_sweep(horizons_dir: Path, ttl_seconds: int = 0) -> Dict[str, List[str]]:
    """
    Executes a reaper scan across all horizon databases in horizons_dir.
    Evicts orphaned horizons whose client PID is dead and whose last_heartbeat exceeds TTL.
    """
    unlinked = []
    retained = []

    if not horizons_dir.exists():
        return {"unlinked": unlinked, "retained": retained}

    now_epoch = int(time.time())
    for db_path in horizons_dir.glob("*.db"):
        h_id = db_path.stem
        try:
            conn = sqlite3.connect(db_path, timeout=1.0)
            cur = conn.cursor()
            cur.execute("SELECT client_pid, status, last_heartbeat FROM horizon_metadata WHERE horizon_id = ?", (h_id,))
            row = cur.fetchone()
            conn.close()

            if not row:
                continue

            client_pid, status, last_heartbeat = row[0], row[1], row[2]
            is_alive = is_pid_alive(client_pid)
            age = now_epoch - last_heartbeat

            if not is_alive and age >= ttl_seconds:
                # Client is dead and TTL expired: EVICIT
                for suffix in ["", "-wal", "-shm"]:
                    p = Path(str(db_path) + suffix)
                    if p.exists():
                        try:
                            p.unlink()
                        except Exception:
                            pass
                unlinked.append(h_id)
            else:
                retained.append(h_id)

        except Exception:
            continue

    return {"unlinked": unlinked, "retained": retained}
