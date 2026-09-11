"""
E2E Scenario: Client Crash Recovery and Horizon Reaper Eviction.
Validates Task 05: Abrupt client termination (SIGKILL), zombie detection, and orphaned horizon eviction.
"""

import os
import sys
import time
import subprocess
import unittest
from pathlib import Path

from tests.e2e.sandbox_environment import TestSandboxEnvironment
from tests.e2e.fixtures.base_seeder import seed_horizon_db
from tests.e2e.reaper_harness import is_pid_alive, run_reaper_sweep


class TestCrashRecoveryReaperE2E(unittest.TestCase):

    def test_reaper_evicts_orphaned_horizon_on_client_crash(self):
        with TestSandboxEnvironment() as sandbox:
            # 1. Spawn a real client subprocess that simulates an IDE session
            client_proc = subprocess.Popen(
                [sys.executable, "-c", "import time; time.sleep(60)"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            client_pid = client_proc.pid
            self.assertTrue(is_pid_alive(client_pid), f"PID {client_pid} must be alive")

            # 2. Register and seed Horizon associated with this client PID
            h_id = f"horizon_zombie_{sandbox.run_id}"
            h_db = sandbox.register_horizon(h_id)
            seed_horizon_db(
                db_path=str(h_db),
                horizon_id=h_id,
                client_pid=client_pid,
                status="ACTIVE"
            )
            self.assertTrue(h_db.exists())

            # 3. Reaper sweep while client is alive -> MUST NOT EVICIT
            sweep1 = run_reaper_sweep(sandbox.horizons_dir, ttl_seconds=0)
            self.assertIn(h_id, sweep1["retained"])
            self.assertNotIn(h_id, sweep1["unlinked"])
            self.assertTrue(h_db.exists(), "Horizon must remain while client is alive")

            # 4. Simulate catastrophic client crash via SIGKILL / kill
            client_proc.kill()
            client_proc.wait(timeout=2.0)
            self.assertFalse(is_pid_alive(client_pid), f"PID {client_pid} must be dead after kill")

            # 5. Reaper sweep after crash -> MUST EVICIT
            sweep2 = run_reaper_sweep(sandbox.horizons_dir, ttl_seconds=0)
            self.assertIn(h_id, sweep2["unlinked"])
            self.assertNotIn(h_id, sweep2["retained"])
            self.assertFalse(h_db.exists(), "Orphaned horizon must be deleted from disk")

    def test_reaper_preserves_live_client_while_evicting_crashed_client(self):
        with TestSandboxEnvironment() as sandbox:
            # Client A: stays alive
            live_proc = subprocess.Popen(
                [sys.executable, "-c", "import time; time.sleep(60)"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            live_pid = live_proc.pid

            # Client B: will crash
            crashed_proc = subprocess.Popen(
                [sys.executable, "-c", "import time; time.sleep(60)"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            crashed_pid = crashed_proc.pid

            try:
                # Seed Horizon A (Live)
                h_live_id = f"h_live_{sandbox.run_id}"
                h_live_db = sandbox.register_horizon(h_live_id)
                seed_horizon_db(str(h_live_db), h_live_id, live_pid)

                # Seed Horizon B (Crashed)
                h_crash_id = f"h_crash_{sandbox.run_id}"
                h_crash_db = sandbox.register_horizon(h_crash_id)
                seed_horizon_db(str(h_crash_db), h_crash_id, crashed_pid)

                # Kill Client B abruptly
                crashed_proc.kill()
                crashed_proc.wait(timeout=2.0)
                self.assertFalse(is_pid_alive(crashed_pid))
                self.assertTrue(is_pid_alive(live_pid))

                # Reaper Sweep
                sweep = run_reaper_sweep(sandbox.horizons_dir, ttl_seconds=0)
                self.assertIn(h_crash_id, sweep["unlinked"])
                self.assertIn(h_live_id, sweep["retained"])

                self.assertFalse(h_crash_db.exists(), "Crashed client horizon must be evicted")
                self.assertTrue(h_live_db.exists(), "Live client horizon must be preserved")

            finally:
                # Cleanup live proc
                try:
                    live_proc.kill()
                    live_proc.wait(timeout=1.0)
                except Exception:
                    pass


if __name__ == "__main__":
    unittest.main()
