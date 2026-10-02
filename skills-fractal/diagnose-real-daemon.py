"""Capture native daemon startup diagnostics; never bypass its checks."""
from pathlib import Path
import os
import subprocess
import time

root = Path(__file__).resolve().parent.parent
binary = Path.home() / ".local/bin/codebase-memory-mcp.exe"
output = root / "docs/temenos-tests/daemon-startup-diagnostic.log"
env = dict(os.environ)
env["CBM_RUNTIME_DIR"] = str(Path.home() / ".cache/codebase-memory-mcp/fractal-runtime")
with output.open("wb") as log:
    process = subprocess.Popen([str(binary), "--cbm-daemon-internal"], cwd=root, env=env,
        stdin=subprocess.DEVNULL, stdout=log, stderr=log,
        creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        process.wait(timeout=8)
        print("Daemon startup exited:", process.returncode)
    except subprocess.TimeoutExpired:
        print("Daemon startup still running; stopping this diagnostic process only:", process.pid)
        process.terminate()
        process.wait(timeout=8)
print(output.read_text(encoding="utf-8", errors="replace")[-6000:])
