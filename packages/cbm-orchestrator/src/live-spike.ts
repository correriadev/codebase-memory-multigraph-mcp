import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { mkdtempSync, writeFileSync } from "node:fs";
import path from "node:path";
import { Journal } from "./journal.ts";
import { killProcessTree } from "./runner.ts";
import type { OrchestratorApplication } from "./application.ts";
import { McpProxy } from "./mcp-proxy.ts";
import type { S0SpikeReport } from "./s0-spike.ts";

/** Crash and cancellation use a real child holding an open SQLite WAL connection. */
export async function probeCrashAndCancellation(directory: string): Promise<{ crashRestart: boolean; cancellation: boolean; evidence: string }> {
  const folder = mkdtempSync(path.join(directory, "crash-"));
  const dbPath = path.join(folder, "journal.db");
  const child = spawn(process.execPath, [fileURLToPath(new URL("./journal-crash-probe.ts", import.meta.url)), dbPath],
    { windowsHide: true, stdio: ["ignore", "pipe", "pipe"], detached: process.platform !== "win32" });
  try {
    await new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("Crash probe did not become ready")), 10_000);
      child.on("error", (error) => { clearTimeout(timer); reject(error); });
      child.on("exit", (code) => { clearTimeout(timer); reject(new Error(`Crash probe exited early (${code})`)); });
      child.stdout.once("data", (data) => { clearTimeout(timer); if (data.toString().includes("READY")) resolve(); else reject(new Error("Unexpected crash probe output")); });
    });
    const killed = child.pid ? await killProcessTree(child.pid) : { killed: false };
    const journal = new Journal(dbPath);
    try {
      const effect = journal.getEffect("s0-crash-effect");
      const persisted = Boolean(journal.getRun("crash-probe") && journal.getLedger("crash-probe").reservedTokens === 50 && effect?.status === "PENDING");
      const evidence = path.join(folder, "result.json");
      writeFileSync(evidence, JSON.stringify({ killed, persisted, effect }, null, 2));
      return { crashRestart: persisted, cancellation: killed.killed, evidence };
    } finally { journal.close(); }
  } finally {
    if (child.pid && child.exitCode === null) await killProcessTree(child.pid);
  }
}

export async function runLiveSpike(app: OrchestratorApplication, objective: string): Promise<S0SpikeReport> {
  const run = await app.runCycle(objective, undefined, true) as any;
  const processProbe = await probeCrashAndCancellation(app.config.stateDirectory);
  // The same dispatcher protects direct tool names as well as discovery.
  let escaped = false;
  const proxy = new McpProxy({ async callTool() { escaped = true; throw new Error("An unauthorized call reached the backend"); } });
  const session = { sessionId: "s0-authorization", role: "builder" as const, agentId: "s0-builder",
    intentScope: [{ path: "allowed.ts", operation: "write" as const }] };
  const outOfScope = await proxy.dispatchToolCall(session, { toolName: "write_file", arguments: { path: "forbidden.ts" } });
  const direct = await proxy.dispatchToolCall(session, { toolName: "promote_horizon", arguments: {} });
  const source = path.join(app.config.stateDirectory, `${run.runId}.json`);
  const report: S0SpikeReport = {
    source: "live", configurationDigest: app.digest, timestamp: new Date().toISOString(),
    criteria: { endToEnd: run.promoted && run.terminationReason === "ACCEPTED" ? "PASS" : "FAIL",
      crashRestart: processProbe.crashRestart ? "PASS" : "FAIL", cancellation: processProbe.cancellation ? "PASS" : "FAIL",
      costPersisted: run.totalTokensUsed > 0 && Number.isFinite(run.totalCostUsd) && !run.telemetry.hasUnknownConsumption ? "PASS" : "FAIL",
      intentScopeRefusal: !outOfScope.allowed && !escaped ? "PASS" : "FAIL" },
    invariantsPassed: !direct.allowed && !escaped && run.promoted === true,
    evidenceRefs: [source, processProbe.evidence],
  };
  writeFileSync(path.join(app.config.stateDirectory, "s0-report.json"), JSON.stringify(report, null, 2));
  return report;
}
