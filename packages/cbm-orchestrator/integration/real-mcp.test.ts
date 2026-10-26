import test from "node:test";
import assert from "node:assert/strict";
import { StdioMcpClient } from "../src/mcp-transport.ts";

test("Installed CBM real MCP: initialize, discover actual schemas and read projects", async () => {
  const client = new StdioMcpClient({ command: process.env.CBM_TEST_BINARY ?? "codebase-memory-mcp" }, 30_000);
  try {
    await client.initialize();
    const tools = await client.listTools();
    assert.ok(tools.some((tool) => tool.name === "promote_horizon"));
    assert.ok(tools.some((tool) => tool.name === "union_session_open"));
    const promoteTool = tools.find((tool) => tool.name === "promote_horizon");
    const props = promoteTool?.inputSchema.properties as Record<string, any>;
    assert.ok(props.expected_graph_generation, "expected_graph_generation must be in schema");
    assert.ok(props.candidate_version_id, "candidate_version_id must be in schema");
    assert.ok(props.idempotency_key, "idempotency_key must be in schema");
    assert.ok(props.expected_snapshot_id, "expected_snapshot_id must be in schema");
    assert.ok(props.snapshot_poles, "snapshot_poles must be in schema");
    assert.ok(props.anchors, "anchors must be in schema");
    assert.ok(props.project, "project must be in schema");
    assert.ok(props.repo_path, "repo_path must be in schema");
    const projects = await client.callTool("list_projects", { format: "json", limit: 5 });
    assert.ok(projects && typeof projects === "object");
    console.log(JSON.stringify({ server: client.serverInfo, toolCount: tools.length, guardedPromotion: true }));
  } finally { await client.close(); }
});
