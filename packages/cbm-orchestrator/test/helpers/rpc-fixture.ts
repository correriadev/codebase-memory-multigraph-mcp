import { serveMcp } from "../../src/mcp-transport.ts";

const root = process.env.FIXTURE_WORKSPACE ?? process.cwd();
let session = 0;
await serveMcp([
  ...["index_status", "extract_harness_bundle", "union_session_open", "union_session_close", "create_horizon", "search_graph"].map((name) => ({ name, inputSchema: { type: "object", properties: {} } })),
  { name: "promote_horizon", inputSchema: { type: "object", properties: { expected_graph_generation: { type: "string" }, expected_snapshot_id: { type: "string" } } } },
], async (name, args, signal) => {
  if (args.wait) await new Promise<void>((resolve, reject) => {
    const timer = setTimeout(resolve, 30_000);
    signal.addEventListener("abort", () => { clearTimeout(timer); reject(new Error("Cancelled")); }, { once: true });
  });
  if (args.fail) throw new Error("Observed backend refusal");
  if (name === "index_status") return { status: "ready", root_path: root };
  if (name === "extract_harness_bundle") return { graph_generation: "u0123456789abcdefg1", snapshot_id: "fixture-snapshot",
    rules_revision: "fixture-rules", coverage_revision: "fixture-coverage" };
  if (name === "union_session_open") return { success: true, horizon_id: `fixture-session-${++session}` };
  if (name === "promote_horizon") {
    if (!args.anchors?.length || !args.project || !args.repo_path || !args.expected_graph_generation || !args.expected_snapshot_id || !args.snapshot_poles?.length) throw new Error("Missing real promotion parameters");
    return { promoted: true, horizon_id: args.horizon_id, status: "PROMOTED" };
  }
  return { success: true, received: args };
});
