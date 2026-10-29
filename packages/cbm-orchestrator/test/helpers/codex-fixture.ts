import { readFileSync } from "node:fs";
import { StdioMcpClient } from "../../src/mcp-transport.ts";

const args = process.argv.slice(2);
if (args.includes("--version")) { console.log("codex-cli 0.0.0-test-fixture"); process.exit(0); }
if (args.includes("--help")) { console.log("--ignore-user-config --ignore-rules --output-schema --json"); process.exit(0); }
if (!args.includes("--ignore-user-config") || !args.includes("--ignore-rules") || !args.includes("features.shell_tool=false") ||
  args[args.indexOf("--sandbox") + 1] !== "read-only") throw new Error("Runner did not isolate the host");
const schema = JSON.parse(readFileSync(args[args.indexOf("--output-schema") + 1], "utf8"));
let input = "";
for await (const chunk of process.stdin) input += chunk.toString();
if (input.includes("FIXTURE_HANG")) await new Promise(() => setInterval(() => {}, 1000));
if (input.includes("FIXTURE_NO_USAGE")) {
  process.stdout.write(JSON.stringify({ type: "item.completed", item: { type: "agent_message", text: "{}" } }) + "\n");
  process.exit(0);
}
let gatewayPath = "";
let channelAddress = "";
let channelToken = "";

const mcpConfig = args.find((value) => value.startsWith("mcp_servers="));
if (mcpConfig) {
  channelAddress = JSON.parse(mcpConfig.match(/CBM_AGENT_CHANNEL=("(?:[^"\\]|\\.)*")/)![1]);
  channelToken = JSON.parse(mcpConfig.match(/CBM_AGENT_TOKEN=("(?:[^"\\]|\\.)*")/)![1]);
  gatewayPath = JSON.parse(mcpConfig.match(/args=\[("(?:[^"\\]|\\.)*")\]/)![1]);
} else {
  const argsArg = args.find((value) => value.startsWith("mcp_servers.cbm.args=")) || "";
  const matchArgs = argsArg.match(/mcp_servers\.cbm\.args=\['([^']+)'\]/);
  gatewayPath = matchArgs ? matchArgs[1] : "";

  const chArg = args.find((value) => value.startsWith("mcp_servers.cbm.env.CBM_AGENT_CHANNEL=")) || "";
  const matchCh = chArg.match(/mcp_servers\.cbm\.env\.CBM_AGENT_CHANNEL='([^']+)'/);
  channelAddress = matchCh ? matchCh[1] : "";

  const tokArg = args.find((value) => value.startsWith("mcp_servers.cbm.env.CBM_AGENT_TOKEN=")) || "";
  const matchTok = tokArg.match(/mcp_servers\.cbm\.env\.CBM_AGENT_TOKEN='([^']+)'/);
  channelToken = matchTok ? matchTok[1] : "";
}

const gateway = new StdioMcpClient({ command: process.execPath, args: [gatewayPath],
  env: { CBM_AGENT_CHANNEL: channelAddress, CBM_AGENT_TOKEN: channelToken } });
try {
  await gateway.initialize();
  const tools = await gateway.listTools();
  if (tools.some((tool) => tool.name === "promote_horizon")) throw new Error("Privileged tool leaked to agent discovery");
  await gateway.callTool("search_graph", {});
  try { await gateway.callTool("promote_horizon", {}); throw new Error("Direct promotion succeeded"); }
  catch (error) { if ((error as Error).message === "Direct promotion succeeded") throw error; }
} finally { await gateway.close(); }
const context = JSON.parse(input.split("Context:\n")[1].split("\nReturn only")[0]);
const output = schema.properties.artifactContent
  ? { artifactContent: JSON.stringify(Object.fromEntries(Object.keys(context.files).map((file) => [file, "export function solve() { return 42; }\n"]))) }
  : { candidateVersionId: context.candidateVersion.versionId, verdictStatus: "GREEN", reasons: "Reviewed exact sealed candidate and its allocated change" };
process.stdout.write(JSON.stringify({ type: "item.completed", item: { type: "agent_message", text: JSON.stringify(output) } }) + "\n");
process.stdout.write(JSON.stringify({ type: "turn.completed", usage: { input_tokens: 12, cached_input_tokens: 4, output_tokens: 3 } }) + "\n");
