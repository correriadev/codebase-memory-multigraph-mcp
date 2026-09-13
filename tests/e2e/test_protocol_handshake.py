"""
Protocol Handshake and Subprocess Stdio E2E Tests.
Validates Task 01: JsonRpc protocol exchange, handshake, and tool listing.
"""

import unittest
from tests.e2e.jsonrpc_client import (
    JsonRpcRequest,
    JsonRpcResponse,
    ProtocolFrameCorruptionError,
    make_initialize_request
)
from tests.e2e.mcp_process_driver import MCPProcessSession, resolve_mcp_binary


class TestProtocolHandshakeE2E(unittest.TestCase):

    def test_jsonrpc_request_serialization(self):
        req = JsonRpcRequest(method="initialize", id=1, params={"client": "test"})
        data = req.to_bytes()
        self.assertTrue(data.endswith(b"\n"))
        self.assertIn(b'"jsonrpc": "2.0"', data)
        self.assertIn(b'"id": 1', data)

    def test_jsonrpc_request_rejects_empty_method(self):
        with self.assertRaises(ValueError):
            JsonRpcRequest(method="", id=1)

    def test_jsonrpc_response_deserialization(self):
        line = '{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2024-11-05"}}\n'
        resp = JsonRpcResponse.from_line(line)
        self.assertEqual(resp.id, 1)
        self.assertTrue(resp.is_success())
        self.assertEqual(resp.result["protocolVersion"], "2024-11-05")

    def test_jsonrpc_response_corruption_error(self):
        corrupted_line = "Not A Valid JSON\n"
        with self.assertRaises(ProtocolFrameCorruptionError):
            JsonRpcResponse.from_line(corrupted_line)

    def test_subprocess_handshake_and_tools_list(self):
        binary = resolve_mcp_binary()
        self.assertTrue(binary, "Binary should be resolved")

        with MCPProcessSession(binary) as session:
            init_resp = session.initialize(timeout=15.0)
            self.assertTrue(init_resp.is_success(), f"Init failed: {init_resp.error}")
            self.assertIsNotNone(init_resp.result)
            self.assertIn("capabilities", init_resp.result)

            tools_resp = session.list_tools(timeout=15.0)
            self.assertTrue(tools_resp.is_success(), f"Tools list failed: {tools_resp.error}")
            tools = tools_resp.result.get("tools", [])
            tool_names = [t.get("name") for t in tools]

            # Verify standard tools exist
            self.assertIn("search_graph", tool_names)
            self.assertIn("query_graph", tool_names)
            self.assertIn("trace_path", tool_names)


if __name__ == "__main__":
    unittest.main()
