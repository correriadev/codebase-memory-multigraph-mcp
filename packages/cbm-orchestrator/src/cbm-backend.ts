import path from "node:path";
import { readFileSync, writeFileSync, realpathSync, lstatSync } from "node:fs";
import { createHash } from "node:crypto";
import { StdioMcpClient, type ToolDefinition } from "./mcp-transport.ts";
import { McpProxy, READ_ONLY_TOOLS, type CbmClient } from "./mcp-proxy.ts";
import type { AgentWorkspace } from "./codex-runner.ts";
import type { AgentInvocationOptions } from "./orchestrator.ts";

export interface WorkspaceTarget { path: string; symbol: string; }
export interface CbmBackendConfig {
  project: string;
  workspace: string;
  targets: WorkspaceTarget[];
  poles: string[];
}

const hash = (value: string) => "sha256:" + createHash("sha256").update(value).digest("hex");

/** Translates logical contracts to the schemas actually announced by CBM. */
export class CbmBackend implements CbmClient, AgentWorkspace {
  directory: string;
  tools: ToolDefinition[] = [];
  horizonId = "";
  private baseline = "";
  private originalFiles = new Map<string, string>();
  private sealedArtifact = "";
  private announced: ToolDefinition[] = [];
  private sessions = new Map<string, string>();
  private snapshots = new Map<string, string>();
  private proxy: McpProxy;
  readonly transport: StdioMcpClient;
  readonly config: CbmBackendConfig;

  constructor(transport: StdioMcpClient, config: CbmBackendConfig) {
    this.transport = transport;
    this.config = config;
    this.directory = realpathSync(config.workspace);
    if (!config.project || !Array.isArray(config.targets) || config.targets.length < 1 || config.targets.length > 4 ||
      !Array.isArray(config.poles) || !config.poles.length) throw new Error("Workspace requires exact project, 1–4 targets and graph poles");
    const seen = new Set<string>();
    for (const target of config.targets) {
      if (!target.symbol || seen.has(target.path)) throw new Error("Target symbols and unique paths are required");
      seen.add(target.path);
      this.targetPath(target.path);
    }
    this.proxy = new McpProxy({ callTool: (name, args) => this.readTool(name, args) });
  }

  private targetPath(relative: string): string {
    if (!relative || path.isAbsolute(relative) || relative.split(/[\\/]/).some((part) => part === ".." || part === "." || !part)) {
      throw new Error(`Invalid workspace target: ${relative}`);
    }
    const absolute = path.resolve(this.directory, relative);
    let current = this.directory;
    for (const part of relative.split(/[\\/]/)) {
      current = path.join(current, part);
      if (lstatSync(current).isSymbolicLink()) throw new Error(`Symlink target denied: ${relative}`);
    }
    if (!lstatSync(absolute).isFile()) throw new Error(`Target must be an existing regular file: ${relative}`);
    return absolute;
  }

  private artifact(): string {
    return JSON.stringify(Object.fromEntries([...this.config.targets].sort((a, b) => a.path.localeCompare(b.path))
      .map((target) => [target.path, readFileSync(this.targetPath(target.path), "utf8")])));
  }

  async connect(): Promise<void> {
    await this.transport.initialize();
    this.announced = await this.transport.listTools();
    for (const name of ["index_status", "extract_harness_bundle", "union_session_open", "union_session_close", "create_horizon", "promote_horizon"]) {
      if (!this.announced.some((tool) => tool.name === name)) throw new Error(`CBM capability unavailable: ${name}`);
    }
    const status = await this.transport.callTool("index_status", { project: this.config.project, format: "json" });
    const statusRoot = status?.root_path ? path.resolve(realpathSync(status.root_path)) : "";
    const expectedRoot = path.resolve(this.directory);
    const matchesWorkspace = process.platform === "win32"
      ? statusRoot.toLowerCase() === expectedRoot.toLowerCase()
      : statusRoot === expectedRoot;
    if (status?.status !== "ready" || !status.root_path || !matchesWorkspace) {
      throw new Error("CBM project does not resolve to the configured indexed workspace");
    }
    const boundedReads = new Set(["search_graph", "query_graph", "trace_path", "get_code_snippet", "get_graph_schema",
      "get_architecture", "search_code", "index_status", "check_index_coverage"]);
    this.tools = this.announced.filter((tool) => boundedReads.has(tool.name));
    this.baseline = hash(this.artifact());
  }

  capabilities(): unknown {
    const promotion = this.announced.find((tool) => tool.name === "promote_horizon");
    const properties = promotion?.inputSchema?.properties as Record<string, unknown> | undefined;
    return { server: this.transport.serverInfo, project: this.config.project,
      tools: this.announced.map((tool) => tool.name),
      guardedPromotion: Boolean(properties?.expected_graph_generation && properties?.expected_snapshot_id),
      workspace: this.directory };
  }

  async begin(objective: string): Promise<string> {
    const session = await this.transport.callTool("union_session_open", {
      identity: "cbm-orchestrator", client_pid: process.pid, host: "codex", context_id: `cycle-${process.pid}-${Date.now()}`,
      intent_key: `cycle-${Date.now()}`, grounding_kind: "declared_invention", rationale: objective,
      intent_scope: this.config.targets.map((target) => ({ path: this.targetPath(target.path), operation: "modify" })),
    });
    if (!session?.horizon_id) throw new Error("CBM did not create a Union session");
    this.horizonId = session.horizon_id;
    return this.horizonId;
  }

  async snapshot(): Promise<any> {
    const result = await this.transport.callTool("extract_harness_bundle", {
      project: this.config.project, poles: this.config.poles, change_kind: "behavior_change", k_hop: 1,
    });
    const bundle = result?.bundle ?? result;
    if (!bundle?.graph_generation || !bundle?.snapshot_id || !bundle?.rules_revision || !bundle?.coverage_revision) {
      throw new Error("CBM harness did not supply graph/rules/coverage snapshot revisions");
    }
    const baselineDigest = hash(this.baseline + ":" + bundle.snapshot_id);
    this.snapshots.set(baselineDigest, bundle.snapshot_id);
    return { graphGeneration: bundle.graph_generation, baselineDigest,
      rulesRevision: bundle.rules_revision, coverageRevision: bundle.coverage_revision, snapshotId: bundle.snapshot_id };
  }

  async callTool(name: string, args: Record<string, any>, options?: AgentInvocationOptions): Promise<any> {
    if (options) {
      const sessionId = this.sessions.get(options.agentId);
      if (!sessionId) throw new Error("Agent session is not registered");
      const result = await this.proxy.dispatchToolCall({ sessionId, role: options.role, agentId: options.agentId, intentScope: options.intentScope },
        { toolName: name, arguments: args });
      if (!result.allowed || result.error) throw new Error(result.error ?? "Agent operation denied");
      return result.data;
    }
    if (name === "index_status") return this.snapshot();
    if (name === "promote_horizon") {
      await this.verifyUnchanged();
      if (!(this.capabilities() as any).guardedPromotion) {
        return { isError: true, error: "CBM does not advertise atomic expected_graph_generation; promotion blocked (AD-09)" };
      }
      await this.transport.callTool("create_horizon", { project: this.config.project, horizon_id: this.horizonId,
        nodes: this.config.targets.map((target) => ({ cbm_uri: `cbm://${this.config.project}/${target.path.replace(/\\/g, "/")}#${target.symbol}`,
          label: target.symbol, epistemic_status: "ACCEPTED", is_dangling: false,
          code_snippet: readFileSync(this.targetPath(target.path), "utf8") })) });
      const anchors = this.config.targets.flatMap((target) => {
        const text = readFileSync(this.targetPath(target.path), "utf8");
        if (!text.length) throw new Error("Empty-file anchor is unavailable");
        const result = [];
        let offset = 0;
        for (let i = 0; i < text.length;) {
          let expected = "";
          while (i < text.length && Buffer.byteLength(expected + String.fromCodePoint(text.codePointAt(i)!)) <= 200) {
            const character = String.fromCodePoint(text.codePointAt(i)!); expected += character; i += character.length;
          }
          result.push({ file_path: target.path, symbol_name: target.symbol, byte_start: offset,
            byte_len: Buffer.byteLength(expected), expected_text: expected });
          offset += Buffer.byteLength(expected);
        }
        return result;
      });
      return this.transport.callTool("promote_horizon", { horizon_id: this.horizonId, project: this.config.project,
        repo_path: this.directory, anchors, expected_graph_generation: args.expected_graph_generation,
        expected_snapshot_id: this.snapshots.get(args.expected_baseline_digest), snapshot_poles: this.config.poles,
        candidate_version_id: args.candidate_version_id, idempotency_key: args.idempotency_key });
    }
    throw new Error(`Unsupported privileged operation: ${name}`);
  }

  private async readTool(name: string, args: Record<string, unknown>): Promise<unknown> {
    if (!READ_ONLY_TOOLS.has(name) || !this.tools.some((tool) => tool.name === name)) throw new Error(`Agent tool denied: ${name}`);
    // Project and horizon identity come from trusted composition, never the agent.
    if ((args.project && args.project !== this.config.project) || args.project_id || args.repo_path || args.path || args.active_horizons) {
      throw new Error("Agent cannot override its project or filesystem boundary");
    }
    return this.transport.callTool(name, { ...args, project: this.config.project, format: "json" });
  }

  async context(options: AgentInvocationOptions): Promise<unknown> {
    return { project: this.config.project, files: JSON.parse(this.artifact()), snapshot: await this.snapshot(),
      candidateVersion: options.candidateVersion, allocatedPaths: this.config.targets.map((target) => target.path) };
  }

  async stage(artifact: string): Promise<string> {
    const files = JSON.parse(artifact);
    if (!files || Array.isArray(files) || typeof files !== "object" ||
      Object.keys(files).length !== this.config.targets.length || this.config.targets.some((target) => typeof files[target.path] !== "string")) {
      throw new Error("Builder artifact must contain exactly the allocated file paths");
    }
    // Save original pristine copies before any modification if not already captured
    if (this.originalFiles.size === 0) {
      for (const target of this.config.targets) {
        this.originalFiles.set(target.path, readFileSync(this.targetPath(target.path), "utf8"));
      }
    }
    // Validate every path and value before any write.
    for (const target of this.config.targets) {
      this.targetPath(target.path);
      if (Buffer.byteLength(files[target.path]) > 1024 * 1024) throw new Error("Candidate file exceeds 1 MiB limit");
    }
    for (const target of this.config.targets) writeFileSync(this.targetPath(target.path), files[target.path]);
    this.sealedArtifact = this.artifact();
    return this.sealedArtifact;
  }

  restoreOriginal(): void {
    if (this.originalFiles.size > 0) {
      for (const [targetPath, content] of this.originalFiles.entries()) {
        writeFileSync(this.targetPath(targetPath), content);
      }
      this.originalFiles.clear();
      this.sealedArtifact = "";
    }
  }

  async verifyUnchanged(): Promise<void> {
    if (!this.sealedArtifact || this.artifact() !== this.sealedArtifact) throw new Error("Sealed candidate changed after Builder output");
  }

  async openSession(options: AgentInvocationOptions, pid: number): Promise<string> {
    const result = await this.transport.callTool("union_session_open", { identity: options.agentId, client_pid: pid });
    if (!result?.horizon_id) throw new Error("Agent Union session missing");
    this.sessions.set(options.agentId, result.horizon_id);
    return result.horizon_id;
  }

  async closeSession(sessionId: string, abnormal: boolean): Promise<void> {
    await this.transport.callTool("union_session_close", { horizon_id: sessionId, reason: abnormal ? "ABNORMAL" : "EMPTY" });
  }

  async finish(abnormal: boolean): Promise<void> {
    if (abnormal) this.restoreOriginal();
    if (this.horizonId) await this.closeSession(this.horizonId, abnormal);
  }
}
