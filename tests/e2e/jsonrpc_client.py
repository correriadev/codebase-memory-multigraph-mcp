"""
JSON-RPC 2.0 Protocol Client and Data Types for MCP Stdio Transport.
"""

from dataclasses import dataclass, field
from typing import Any, Dict, Optional
import json


class ProtocolFrameCorruptionError(Exception):
    """Raised when stdout receives data that violates JSON-RPC 2.0 framing."""
    pass


@dataclass(frozen=True)
class JsonRpcRequest:
    method: str
    id: int
    params: Dict[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        if not self.method:
            raise ValueError("JSON-RPC method must not be empty.")

    def to_bytes(self) -> bytes:
        payload = {
            "jsonrpc": "2.0",
            "id": self.id,
            "method": self.method,
            "params": self.params,
        }
        return (json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8")


@dataclass(frozen=True)
class JsonRpcResponse:
    id: Optional[int]
    result: Optional[Dict[str, Any]] = None
    error: Optional[Dict[str, Any]] = None

    def is_success(self) -> bool:
        return self.error is None and self.result is not None

    @classmethod
    def from_line(cls, line: str) -> "JsonRpcResponse":
        try:
            data = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ProtocolFrameCorruptionError(f"Malformed JSON frame received on stdout: {line.strip()!r}") from exc

        if not isinstance(data, dict):
            raise ProtocolFrameCorruptionError(f"Expected JSON object, got {type(data).__name__}")

        resp_id = data.get("id")
        result = data.get("result")
        error = data.get("error")

        return cls(id=resp_id, result=result, error=error)


def make_initialize_request(req_id: int = 1) -> JsonRpcRequest:
    """Builds an MCP initialize request."""
    return JsonRpcRequest(
        method="initialize",
        id=req_id,
        params={
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "roots": {"listChanged": True}
            },
            "clientInfo": {
                "name": "cbm-e2e-harness",
                "version": "1.0.0"
            }
        }
    )


def make_tools_list_request(req_id: int = 2) -> JsonRpcRequest:
    """Builds an MCP tools/list request."""
    return JsonRpcRequest(
        method="tools/list",
        id=req_id,
        params={}
    )


def make_tool_call_request(tool_name: str, arguments: Dict[str, Any], req_id: int) -> JsonRpcRequest:
    """Builds an MCP tools/call request."""
    return JsonRpcRequest(
        method="tools/call",
        id=req_id,
        params={
            "name": tool_name,
            "arguments": arguments
        }
    )
