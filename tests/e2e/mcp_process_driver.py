"""
Subprocess Driver for MCP Server Native Executable over Stdio.
Handles non-blocking I/O, process trees, and abrupt crash termination.
"""

import os
import sys
import time
import queue
import shutil
import threading
import subprocess
from typing import Dict, List, Optional, Any
from pathlib import Path

from tests.e2e.jsonrpc_client import (
    JsonRpcRequest,
    JsonRpcResponse,
    ProtocolFrameCorruptionError,
    make_initialize_request,
    make_tools_list_request,
    make_tool_call_request
)


class MCPTimeoutError(TimeoutError):
    pass


class MCPProcessError(RuntimeError):
    pass


def resolve_mcp_binary() -> str:
    """Finds the codebase-memory-mcp executable."""
    # Check environment variable first
    if os.environ.get("CBM_BINARY_PATH"):
        path = os.environ["CBM_BINARY_PATH"]
        if os.path.isfile(path):
            return path

    # Check PATH first (fast native filesystem on Linux/WSL)
    which_bin = shutil.which("codebase-memory-mcp")
    if which_bin:
        return which_bin

    # Check local build directory
    root = Path(__file__).resolve().parent.parent.parent
    local_builds = [
        root / "build" / "c" / "codebase-memory-mcp.exe",
        root / "build" / "c" / "codebase-memory-mcp",
    ]
    for b in local_builds:
        if b.is_file():
            return str(b)

    # Check user local appdata
    appdata_bin = Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "codebase-memory-mcp" / "codebase-memory-mcp.exe"
    if appdata_bin.is_file():
        return str(appdata_bin)

    raise FileNotFoundError("Could not find codebase-memory-mcp executable on PATH or local build paths.")


class MCPProcessSession:
    """Manages an active MCP server subprocess speaking JSON-RPC 2.0 over stdio."""

    def __init__(self, binary_path: Optional[str] = None, env: Optional[Dict[str, str]] = None, cwd: Optional[str] = None):
        self.binary_path = binary_path or resolve_mcp_binary()
        self.env = dict(os.environ)
        if env:
            self.env.update(env)
        self.cwd = cwd

        # Set ASan options if running with address sanitizer
        self.env.setdefault("ASAN_OPTIONS", "detect_leaks=1:abort_on_error=1:log_path=stderr")

        self.proc: Optional[subprocess.Popen] = None
        self._stdout_queue: queue.Queue = queue.Queue()
        self._stderr_lines: List[str] = []
        self._stdout_thread: Optional[threading.Thread] = None
        self._stderr_thread: Optional[threading.Thread] = None
        self._is_running = False
        self._request_counter = 0

    @property
    def pid(self) -> Optional[int]:
        return self.proc.pid if self.proc else None

    def start(self) -> None:
        """Launches the subprocess and background reader threads."""
        if self._is_running:
            return

        cmd = [self.binary_path]
        # In stdio mode, codebase-memory-mcp defaults to MCP server
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=self.env,
            cwd=self.cwd,
            bufsize=0  # unbuffered binary I/O
        )
        self._is_running = True

        self._stdout_thread = threading.Thread(target=self._read_stdout, daemon=True)
        self._stdout_thread.start()

        self._stderr_thread = threading.Thread(target=self._read_stderr, daemon=True)
        self._stderr_thread.start()

    def _read_stdout(self) -> None:
        """Drains stdout continuously and puts lines into the queue."""
        assert self.proc and self.proc.stdout
        while self._is_running and self.proc.poll() is None:
            try:
                line = self.proc.stdout.readline()
                if not line:
                    break
                decoded = line.decode("utf-8", errors="replace").strip()
                if decoded:
                    self._stdout_queue.put(decoded)
            except Exception:
                break

    def _read_stderr(self) -> None:
        """Drains stderr continuously into the diagnostic buffer."""
        assert self.proc and self.proc.stderr
        while self._is_running and self.proc.poll() is None:
            try:
                line = self.proc.stderr.readline()
                if not line:
                    break
                decoded = line.decode("utf-8", errors="replace").rstrip()
                if decoded:
                    self._stderr_lines.append(decoded)
            except Exception:
                break

    def next_request_id(self) -> int:
        self._request_counter += 1
        return self._request_counter

    def send_request(self, req: JsonRpcRequest, timeout: float = 10.0) -> JsonRpcResponse:
        """Sends a JSON-RPC request and synchronously awaits the matching response."""
        if not self._is_running or not self.proc or self.proc.poll() is not None:
            raise MCPProcessError("Process is not running or terminated prematurely.")

        payload_bytes = req.to_bytes()
        try:
            assert self.proc.stdin
            self.proc.stdin.write(payload_bytes)
            self.proc.stdin.flush()
        except BrokenPipeError as exc:
            raise MCPProcessError("Failed to write to stdin: pipe broken.") from exc

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = max(0.05, deadline - time.monotonic())
            try:
                line = self._stdout_queue.get(timeout=remaining)
                resp = JsonRpcResponse.from_line(line)
                if resp.id == req.id:
                    return resp
                # If notification or different ID, keep waiting
            except queue.Empty:
                break

        stderr_tail = "\n".join(self._stderr_lines[-10:])
        raise MCPTimeoutError(
            f"Timed out waiting for response to request ID {req.id} (method={req.method}).\n"
            f"Recent stderr:\n{stderr_tail}"
        )

    def initialize(self, timeout: float = 10.0) -> JsonRpcResponse:
        """Performs standard MCP initialization sequence."""
        req = make_initialize_request(self.next_request_id())
        resp = self.send_request(req, timeout=timeout)
        # Send notifications/initialized if required by protocol
        return resp

    def list_tools(self, timeout: float = 10.0) -> JsonRpcResponse:
        """Lists available MCP tools."""
        req = make_tools_list_request(self.next_request_id())
        return self.send_request(req, timeout=timeout)

    def call_tool(self, name: str, arguments: Dict[str, Any], timeout: float = 10.0) -> JsonRpcResponse:
        """Calls a specific MCP tool with arguments."""
        req = make_tool_call_request(name, arguments, self.next_request_id())
        return self.send_request(req, timeout=timeout)

    def kill_force(self) -> None:
        """Abruptly kills the subprocess (simulating SIGKILL / crash)."""
        self._is_running = False
        if not self.proc:
            return

        pid = self.proc.pid
        try:
            if sys.platform == "win32":
                # Use taskkill to terminate tree forcibly
                subprocess.run(["taskkill", "/F", "/T", "/PID", str(pid)], capture_output=True)
            else:
                self.proc.kill()
        except Exception:
            pass

        try:
            self.proc.wait(timeout=2.0)
        except Exception:
            pass

    def close(self) -> None:
        """Gracefully shuts down the subprocess and cleans up handles."""
        self._is_running = False
        if not self.proc:
            return

        try:
            if self.proc.stdin:
                self.proc.stdin.close()
        except Exception:
            pass

        try:
            self.proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            self.kill_force()

        try:
            if self.proc.stdout:
                self.proc.stdout.close()
            if self.proc.stderr:
                self.proc.stderr.close()
        except Exception:
            pass

        if self._stdout_thread and self._stdout_thread.is_alive():
            self._stdout_thread.join(timeout=1.0)
        if self._stderr_thread and self._stderr_thread.is_alive():
            self._stderr_thread.join(timeout=1.0)

    @property
    def stderr_output(self) -> str:
        return "\n".join(self._stderr_lines)

    def __enter__(self) -> "MCPProcessSession":
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()
