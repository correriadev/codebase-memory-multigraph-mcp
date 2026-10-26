import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { StdioMcpClient, decodeToolResult } from "../src/mcp-transport.ts";
import { CbmBackend } from "../src/cbm-backend.ts";
import { CodexAgentRunner } from "../src/codex-runner.ts";
import { CycleOrchestrator } from "../src/orchestrator.ts";
import { probeCrashAndCancellation } from "../src/live-spike.ts";

const fixture = fileURLToPath(new URL("./helpers/rpc-fixture.ts", import.meta.url));
const codexFixture = fileURLToPath(new URL("./helpers/codex-fixture.ts", import.meta.url));

test("MCP stdio negotiates schemas, distinguishes refusal from transport loss, and cancels requests", async () => {
  const client = new StdioMcpClient({ command: process.execPath, args: [fixture] }, 400);
  try {
    await client.initialize();
    assert.ok((await client.listTools()).some((tool) => tool.name === "search_graph"));
    await assert.rejects(client.callTool("search_graph", { fail: true }), /Observed backend refusal/);
    await assert.rejects(client.callTool("search_graph", { wait: true }), /outcome unknown/);
    const controller = new AbortController();
    const call = client.callTool("search_graph", { wait: true }, controller.signal);
    controller.abort();
    await assert.rejects(call, /cancelled/);
    assert.deepEqual(decodeToolResult({ content: [{ type: "text", text: '{"ready":true}' }] }), { ready: true });
  } finally { await client.close(); }
});

test("Real subprocess composition connects agents to mediated MCP and seals independently verified code", async () => {
  const folder = mkdtempSync(path.join(tmpdir(), "cbm-runtime-test-"));
  writeFileSync(path.join(folder, "solver.ts"), "export function solve() { return 0; }\n");
  const transport = new StdioMcpClient({ command: process.execPath, args: [fixture], env: { FIXTURE_WORKSPACE: folder } });
  const backend = new CbmBackend(transport, { project: "fixture", workspace: folder,
    targets: [{ path: "solver.ts", symbol: "solve" }], poles: ["solve"] });
  let orchestrator: CycleOrchestrator | undefined;
  try {
    await backend.connect();
    const horizonId = await backend.begin("Change return value");
    const runner = new CodexAgentRunner({ command: process.execPath, args: [codexFixture], model: "fixture-host",
      rates: { inputPerMillion: 1, cachedPerMillion: 0.5, outputPerMillion: 1, revision: "fixture" },
      evidenceDirectory: path.join(folder, "evidence"), verification: { command: process.execPath, args: ["-e", "process.exit(0)"] } }, backend);
    orchestrator = new CycleOrchestrator(path.join(folder, "journal.db"), backend, runner, { s0Probe: true });
    const result = await orchestrator.executeCycle({ projectId: "fixture", horizonId, objective: "Change return value", builderPrompt: "Change return value",
      builderIntentScope: [{ path: "solver.ts", operation: "write" }],
      policy: { cycleTokenCap: 1000, builderReservation: 100, adversarialReservation: 100, maxIterations: 1,
        agentTimeoutSeconds: 15, cycleTimeoutSeconds: 60, cycleMonetaryCapUsd: 1 } });
    assert.equal(result.terminationReason, "ACCEPTED");
    assert.equal(result.promoted, true);
    assert.equal(result.totalTokensUsed, 30);
    assert.match(readFileSync(path.join(folder, "solver.ts"), "utf8"), /return 42/);
    assert.equal(result.effects[0].status, "CONFIRMED");
    await assert.rejects(backend.stage('{"../outside":"bad"}'), /allocated/);
    writeFileSync(path.join(folder, "solver.ts"), "drift");
    await assert.rejects(backend.verifyUnchanged(), /changed/);
  } finally {
    orchestrator?.close();
    await transport.close();
    rmSync(folder, { recursive: true, force: true });
  }
});

test("S0 crash probe kills an actual process and recovers its pending SQLite effects", async () => {
  const folder = mkdtempSync(path.join(tmpdir(), "cbm-crash-test-"));
  try {
    const result = await probeCrashAndCancellation(folder);
    assert.equal(result.cancellation, true);
    assert.equal(result.crashRestart, true);
  } finally { rmSync(folder, { recursive: true, force: true }); }
});
