import path from "node:path";
import { mkdirSync, readFileSync, writeFileSync, openSync, closeSync, unlinkSync, existsSync } from "node:fs";
import { randomUUID } from "node:crypto";
import { StdioMcpClient, type ProcessCommand } from "./mcp-transport.ts";
import { CbmBackend, type CbmBackendConfig } from "./cbm-backend.ts";
import { CodexAgentRunner, type CodexRunnerConfig } from "./codex-runner.ts";
import {
  CycleOrchestrator,
  validateBudgetPolicy,
  type Stage1Proposal,
  type Stage1Decision,
  type PauseContext,
  type OperatorDecision,
} from "./orchestrator.ts";
import { Journal, type BudgetPolicy } from "./journal.ts";
import { configurationDigest, loadS0Approval } from "./s0-spike.ts";

export interface OrchestratorConfig {
  backend: ProcessCommand;
  knowledge: CbmBackendConfig;
  codex: CodexRunnerConfig;
  policy: BudgetPolicy;
  stateDirectory: string;
}

function resolveArgPath(arg: string, configDir: string): string {
  if (path.isAbsolute(arg)) return arg;
  const fromConfig = path.resolve(configDir, arg);
  if (existsSync(fromConfig)) return fromConfig;
  const fromCwd = path.resolve(arg);
  if (existsSync(fromCwd)) return fromCwd;
  return arg;
}

export function readConfiguration(file: string): OrchestratorConfig {
  const configDir = path.dirname(path.resolve(file));
  const config = JSON.parse(readFileSync(file, "utf8")) as OrchestratorConfig;
  validateBudgetPolicy(config.policy);
  if (!config.policy.breakerFailureThreshold || !config.policy.breakerWindowSeconds) throw new Error("Live runtime requires explicit circuit breaker threshold and window");
  if (!config.backend?.command || !config.stateDirectory || !path.isAbsolute(config.stateDirectory) ||
    !config.knowledge?.workspace || !path.isAbsolute(config.knowledge.workspace) || !config.codex?.model) {
    throw new Error("Configuration requires backend command, absolute state/workspace directories and a Codex model");
  }
  if (Array.isArray(config.backend.args)) {
    config.backend.args = config.backend.args.map((a) => resolveArgPath(a, configDir));
  }
  if (Array.isArray(config.codex.args)) {
    config.codex.args = config.codex.args.map((a) => resolveArgPath(a, configDir));
  }
  config.codex.evidenceDirectory = path.join(config.stateDirectory, "evidence");
  mkdirSync(config.stateDirectory, { recursive: true });
  return config;
}

export function createDefaultConfiguration(workspacePath: string = process.cwd()): OrchestratorConfig {
  const ws = path.resolve(workspacePath);
  const stateDir = path.join(ws, ".cbm-orchestrator");
  mkdirSync(stateDir, { recursive: true });

  let targetFile = "README.md";
  const candidates = ["package.json", "README.md", "src/cli.ts", "src/index.ts", "index.ts", "index.js"];
  const found = candidates.find((c) => existsSync(path.join(ws, c)));
  if (found) {
    targetFile = found;
  } else {
    targetFile = ".cbm-target";
    const targetAbs = path.join(ws, targetFile);
    if (!existsSync(targetAbs)) {
      writeFileSync(targetAbs, "// cbm target\n", "utf8");
    }
  }

  return {
    backend: {
      command: "codebase-memory-mcp",
    },
    knowledge: {
      project: path.basename(ws),
      workspace: ws,
      targets: [{ path: targetFile, symbol: "main" }],
      poles: ["main"],
    },
    codex: {
      command: "codex",
      model: "gpt-6.1-sol",
      rates: {
        inputPerMillion: 1.5,
        cachedPerMillion: 0.375,
        outputPerMillion: 6.0,
        revision: "openai-2026",
      },
      verification: {
        command: "npm",
        args: ["test"],
      },
      evidenceDirectory: path.join(stateDir, "evidence"),
    },
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 20000,
      adversarialReservation: 10000,
      maxIterations: 2,
      agentTimeoutSeconds: 180,
      cycleTimeoutSeconds: 900,
      cycleMonetaryCapUsd: 2.0,
      breakerFailureThreshold: 3,
      breakerWindowSeconds: 600,
    },
    stateDirectory: stateDir,
  };
}

export class OrchestratorApplication {
  readonly config: OrchestratorConfig;
  readonly digest: string;
  private active = new Map<string, AbortController>();
  constructor(config: OrchestratorConfig) {
    this.config = config;
    this.digest = configurationDigest(config);
  }

  private connection(overrideKnowledge?: KnowledgeConfig): { client: StdioMcpClient; backend: CbmBackend } {
    const backendCmd: ProcessCommand = {
      ...this.config.backend,
      env: {
        ...process.env,
        ...this.config.backend.env,
        FIXTURE_WORKSPACE: this.config.knowledge.workspace,
      },
    };
    const client = new StdioMcpClient(backendCmd, 15_000);
    try {
      const backend = new CbmBackend(client, overrideKnowledge || this.config.knowledge);
      return { client, backend };
    } catch (err) {
      client.close();
      throw err;
    }
  }

  async probe(): Promise<unknown> {
    const { client, backend } = this.connection();
    try { await backend.connect(); return { ...backend.capabilities() as object, snapshot: await backend.snapshot(), configurationDigest: this.digest }; }
    finally { await client.close(); }
  }

  async runCycle(
    objective: string,
    signal?: AbortSignal,
    diagnostic = false,
    onStage1Proposal?: (proposal: Stage1Proposal) => Promise<Stage1Decision>,
    options?: {
      shouldPause?: () => boolean;
      onPause?: (context: PauseContext) => Promise<OperatorDecision>;
    },
    runTargets?: WorkspaceTarget[],
    enrichedPrompt?: string
  ): Promise<unknown> {
    if (typeof objective !== "string" || !objective.trim()) throw new Error("A nonempty objective is required");
    if (objective.length > 32_000) throw new Error("Objective exceeds 32000 characters");
    if (!diagnostic) loadS0Approval(path.join(this.config.stateDirectory, "s0-approval.json"), this.digest);
    const lockPath = path.join(this.config.stateDirectory, "cycle.lock");
    let lock: number;
    try { lock = openSync(lockPath, "wx"); }
    catch { throw new Error("Cycle lock exists; inspect interrupted execution before starting another cycle"); }
    const controller = new AbortController();
    const operationId = randomUUID();
    const abort = () => controller.abort();
    signal?.addEventListener("abort", abort, { once: true });
    if (signal?.aborted) controller.abort();
    this.active.set(operationId, controller);
    let client: StdioMcpClient | undefined;
    let backend: CbmBackend | undefined;
    const dbPath = path.join(this.config.stateDirectory, "journal.db");
    let orchestrator: CycleOrchestrator | undefined;
    let accepted = false;
    let recoveryRequired = false;
    try {
      writeFileSync(lock, JSON.stringify({ pid: process.pid, operationId, startedAt: new Date().toISOString() }));
      const activeTargets = runTargets && runTargets.length > 0 ? runTargets : this.config.knowledge.targets;
      const activeKnowledge: KnowledgeConfig = {
        ...this.config.knowledge,
        targets: activeTargets,
        poles: activeTargets.map((t) => t.symbol),
      };
      const conn = this.connection(activeKnowledge);
      client = conn.client;
      backend = conn.backend;
      await backend.connect();
      const horizonId = await backend.begin(objective);
      const runner = new CodexAgentRunner(this.config.codex, backend);
      // The composition root supplies this privilege after validating live evidence; MCP callers cannot set it.
      const journal = new Journal(dbPath);
      if (!diagnostic) journal.recordS0Approval(this.config.knowledge.project, this.digest);
      journal.close();
      orchestrator = new CycleOrchestrator(dbPath, backend, runner, { s0Probe: diagnostic, configurationDigest: this.digest });
      const result = await orchestrator.executeCycle({
        projectId: this.config.knowledge.project,
        horizonId,
        objective,
        builderPrompt: enrichedPrompt || objective,
        policy: this.config.policy,
        signal: controller.signal,
        builderIntentScope: activeTargets.map((target) => ({ path: target.path, operation: "write" })),
        onStage1Proposal,
        shouldPause: options?.shouldPause,
        onPause: options?.onPause,
      });
      accepted = result.promoted;
      recoveryRequired = result.effects.some((effect) => effect.status === "PENDING" || effect.status === "UNKNOWN");
      const telemetry = orchestrator.queryTelemetry({ runId: result.runId });
      recoveryRequired ||= Boolean(telemetry?.hasUnknownConsumption) || result.status === "PAUSED";
      const report = { operationId, ...result, telemetry: orchestrator.queryTelemetry({ runId: result.runId }),
        workspace: backend.directory, evidenceDirectory: this.config.codex.evidenceDirectory, diagnostic, recoveryRequired };
      writeFileSync(path.join(this.config.stateDirectory, `${result.runId}.json`), JSON.stringify(report, null, 2));
      return report;
    } catch (error) {
      recoveryRequired = Boolean(backend?.horizonId);
      writeFileSync(path.join(this.config.stateDirectory, `${operationId}.failure.json`), JSON.stringify({ operationId,
        horizonId: backend?.horizonId, recoveryRequired, error: (error as Error).message }, null, 2));
      throw error;
    } finally {
      signal?.removeEventListener("abort", abort);
      this.active.delete(operationId);
      orchestrator?.close();
      try { if (!recoveryRequired && backend) await backend.finish(!accepted); }
      finally {
        if (client) await client.close();
        closeSync(lock);
        if (!recoveryRequired) unlinkSync(lockPath);
      }
    }
  }

  getRun(runId: string): unknown {
    if (!/^run-[A-Za-z0-9-]+$/.test(runId)) throw new Error("Invalid run ID");
    return JSON.parse(readFileSync(path.join(this.config.stateDirectory, `${runId}.json`), "utf8"));
  }

  listRuns(limit = 20): unknown[] {
    const dbPath = path.join(this.config.stateDirectory, "journal.db");
    if (!existsSync(dbPath)) return [];
    const journal = new Journal(dbPath);
    try {
      return journal.listRuns(limit);
    } finally {
      journal.close();
    }
  }

  isLocked(): { locked: boolean; info?: Record<string, unknown> } {
    const lockPath = path.join(this.config.stateDirectory, "cycle.lock");
    if (!existsSync(lockPath)) return { locked: false };
    try {
      const info = JSON.parse(readFileSync(lockPath, "utf8"));
      return { locked: true, info };
    } catch {
      return { locked: true };
    }
  }

  unlock(): boolean {
    const lockPath = path.join(this.config.stateDirectory, "cycle.lock");
    if (existsSync(lockPath)) {
      unlinkSync(lockPath);
      return true;
    }
    return false;
  }

  cancel(operationId?: string): unknown {
    if (operationId) this.active.get(operationId)?.abort();
    else for (const controller of this.active.values()) controller.abort();
    return { cancellationRequested: true, operations: [...this.active.keys()] };
  }
}
