import path from "node:path";
import { pathToFileURL } from "node:url";
import { OrchestratorApplication, readConfiguration } from "./application.ts";
import { serveMcp, type ToolDefinition } from "./mcp-transport.ts";
import { approveS0 } from "./s0-spike.ts";

export const ORCHESTRATOR_TOOLS: ToolDefinition[] = [
  { name: "orchestrator_probe", description: "Inspect live CBM schemas, exact project and snapshot; starts no model calls", inputSchema: { type: "object", properties: {}, additionalProperties: false } },
  { name: "orchestrator_run_cycle", description: "Run Builder and independent verifier using real Codex processes and CBM; requires operator-approved live S0 evidence", inputSchema: { type: "object", properties: { objective: { type: "string", minLength: 1, maxLength: 32000 } }, required: ["objective"], additionalProperties: false } },
  { name: "orchestrator_get_run", description: "Read persisted execution, promotion outcome and usage", inputSchema: { type: "object", properties: { runId: { type: "string" } }, required: ["runId"], additionalProperties: false } },
  { name: "orchestrator_cancel_cycle", description: "Stop new dispatches and cancel owned agent processes; remote effects may require reconciliation", inputSchema: { type: "object", properties: { operationId: { type: "string" } }, additionalProperties: false } },
];

export async function main(args = process.argv.slice(2)): Promise<void> {
  const [mode, configFile, ...rest] = args;
  if (!configFile || !["serve", "probe", "run", "spike", "approve-s0"].includes(mode)) {
    throw new Error("Usage: node src/cli.ts <serve|probe|run|spike|approve-s0> <config.json> [objective or S0 report]");
  }
  const config = readConfiguration(configFile);
  const app = new OrchestratorApplication(config);
  const stop = () => app.cancel();
  process.on("SIGINT", stop);
  process.on("SIGTERM", stop);
  try {
    if (mode === "serve") {
      await serveMcp(ORCHESTRATOR_TOOLS, async (name, values, signal) => {
        if (name === "orchestrator_probe") return app.probe();
        if (name === "orchestrator_run_cycle") return app.runCycle(values.objective, signal);
        if (name === "orchestrator_get_run") return app.getRun(values.runId);
        if (name === "orchestrator_cancel_cycle") return app.cancel(values.operationId);
        throw new Error("Unknown orchestrator tool");
      });
    } else if (mode === "probe") {
      console.log(JSON.stringify(await app.probe(), null, 2));
    } else if (mode === "approve-s0") {
      approveS0(rest[0], path.join(config.stateDirectory, "s0-approval.json"), app.digest);
      console.log(JSON.stringify({ status: "S0_APPROVED", configurationDigest: app.digest }));
    } else if (mode === "spike") {
      const { runLiveSpike } = await import("./live-spike.ts");
      console.log(JSON.stringify(await runLiveSpike(app, rest.join(" ")), null, 2));
    } else {
      console.log(JSON.stringify(await app.runCycle(rest.join(" ")), null, 2));
    }
  } finally {
    process.off("SIGINT", stop);
    process.off("SIGTERM", stop);
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main().catch((error) => { process.stderr.write((error as Error).message + "\n"); process.exitCode = 1; });
}
