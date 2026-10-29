import { mkdtempSync, writeFileSync, mkdirSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { randomUUID } from "node:crypto";
import { executeAgentProcess, calculateEstimatedCostUsd, type ModelRates } from "./runner.ts";
import type { AgentRunner, AgentInvocationOptions, AgentInvocationResult } from "./orchestrator.ts";
import type { EvidenceRecord } from "./candidate.ts";
import { openAgentChannel } from "./agent-channel.ts";
import type { ProcessCommand, ToolDefinition } from "./mcp-transport.ts";

export interface CodexRunnerConfig {
  command: string;
  args?: string[];
  provider?: "codex" | "antigravity" | "opencode" | "claude";
  model: string;
  effort?: "none" | "minimal" | "low" | "medium" | "high" | "xhigh" | "max" | string;
  rates: ModelRates & { revision: string };
  evidenceDirectory: string;
  verification: ProcessCommand;
}

export interface AgentWorkspace {
  context(options: AgentInvocationOptions): Promise<unknown>;
  stage(artifact: string): Promise<string>;
  verifyUnchanged(): Promise<void>;
  directory: string;
  tools: ToolDefinition[];
  callTool(name: string, args: Record<string, unknown>, options: AgentInvocationOptions): Promise<unknown>;
  openSession(options: AgentInvocationOptions, pid: number): Promise<string>;
  closeSession(sessionId: string, abnormal: boolean): Promise<void>;
}

const objectSchema = (properties: Record<string, unknown>) => ({
  type: "object", additionalProperties: false, properties, required: Object.keys(properties),
});

export class CodexAgentRunner implements AgentRunner {
  private config: CodexRunnerConfig;
  private workspace: AgentWorkspace;
  private hostVersion?: string;
  constructor(config: CodexRunnerConfig, workspace: AgentWorkspace) {
    this.config = config;
    this.workspace = workspace;
    if (!config.command || !config.model || !config.rates.revision || !config.verification.command ||
      ![config.rates.inputPerMillion, config.rates.cachedPerMillion, config.rates.outputPerMillion]
        .every((value) => Number.isFinite(value) && value >= 0)) {
      throw new Error("Codex runner requires command, model, verification and explicit versioned rates");
    }
    mkdirSync(config.evidenceDirectory, { recursive: true });
  }

  async inspectHost(signal?: AbortSignal): Promise<string> {
    if (this.hostVersion) return this.hostVersion;
    if (this.config.provider === "antigravity" || this.config.provider === "opencode") {
      this.hostVersion = `${this.config.provider} (delegated runner)`;
      return this.hostVersion;
    }
    const version = await executeAgentProcess({ command: this.config.command, args: [...this.config.args ?? [], "--version"],
      inputPrompt: "", timeoutSeconds: 10, signal });
    if (version.exitCode !== 0 || !/^codex(?:-cli)?\s+\d/i.test(version.stdout.trim())) throw new Error("Configured agent command is not a supported Codex CLI");
    const help = await executeAgentProcess({ command: this.config.command, args: [...this.config.args ?? [], "exec", "--help"],
      inputPrompt: "", timeoutSeconds: 10, signal });
    if (help.exitCode !== 0 || ["--ignore-user-config", "--ignore-rules", "--output-schema", "--json"].some((flag) => !help.stdout.includes(flag))) {
      throw new Error("Codex CLI does not support the required isolation and output contract");
    }
    this.hostVersion = version.stdout.trim();
    return this.hostVersion;
  }

  async invokeAgent(options: AgentInvocationOptions): Promise<AgentInvocationResult> {
    await this.inspectHost(options.signal);
    const invocationId = randomUUID();
    const directory = mkdtempSync(path.join(tmpdir(), "cbm-codex-"));
    const schemaPath = path.join(directory, "output.schema.json");
    const schema = options.role === "builder"
      ? objectSchema({ artifactContent: { type: "string", description: "JSON object mapping every allocated relative file path to its complete new contents" } })
      : objectSchema({ candidateVersionId: { type: "string" }, verdictStatus: { type: "string", enum: ["GREEN", "RED", "INCONCLUSIVE"] }, reasons: { type: "string" } });
    writeFileSync(schemaPath, JSON.stringify(schema));
    const channel = await openAgentChannel(this.workspace.tools, (name, args) => this.workspace.callTool(name, args, options));
    const gateway = fileURLToPath(new URL("./agent-gateway.ts", import.meta.url));
    const nodeExec = process.execPath.replace(/\\/g, "/");
    const gatewayPath = gateway.replace(/\\/g, "/");
    const args = [...this.config.args ?? [], "exec", "--ignore-user-config", "--ignore-rules", "--skip-git-repo-check", "--ephemeral",
      "--sandbox", "read-only", "--model", this.config.model, "--json", "--color", "never", "--cd", directory,
      "--output-schema", schemaPath, "-c", 'approval_policy="never"', "-c", 'web_search="disabled"',
      "-c", "features.shell_tool=false", "-c", "features.apps=false", "-c", "features.plugins=false",
      "-c", "features.multi_agent=false", "-c", "features.hooks=false", "-c", "features.memories=false",
      "-c", "features.computer_use=false", "-c", "features.browser_use=false", "-c", "features.image_generation=false",
      "-c", "features.unified_exec=false", "-c", "features.code_mode=false", "-c", "features.code_mode_host=false",
      "-c", `mcp_servers.cbm.command='${nodeExec}'`,
      "-c", `mcp_servers.cbm.args=['${gatewayPath}']`,
      "-c", "mcp_servers.cbm.required=true",
      "-c", `mcp_servers.cbm.env.CBM_AGENT_CHANNEL='${channel.address}'`,
      "-c", `mcp_servers.cbm.env.CBM_AGENT_TOKEN='${channel.token}'`];

    if (this.config.effort) {
      args.push("-c", `model_reasoning_effort="${this.config.effort}"`);
    }

    args.push("-");
    let finalText = "";
    let sessionId: string | undefined;
    let sessionPromise: Promise<string> | undefined;
    let abnormal = true;
    let measuredUsage: AgentInvocationResult["usage"] = null;
    let measuredCost: number | null = null;
    const logPath = path.join(this.config.evidenceDirectory, `${invocationId}.json`);
    try {
      const context = await this.workspace.context(options);
      const result = await executeAgentProcess({
        command: this.config.command, args, cwd: directory, signal: options.signal,
        timeoutSeconds: options.timeoutSeconds,
        inputPrompt: `${options.prompt}\nRole: ${options.role}. Work only through the allocated CBM context. Never promote or invoke another host.\nContext:\n${JSON.stringify(context)}\nReturn only the required structured response.`,
        onSpawn: (pid) => { sessionPromise = this.workspace.openSession(options, pid); return sessionPromise.then(() => undefined); },
        onEvent: (event) => {
          if (event.type === "item.completed" && event.item?.type === "agent_message") finalText = event.item.text;
        },
      });
      sessionId = await sessionPromise;
      writeFileSync(logPath, JSON.stringify({ invocationId, agentId: options.agentId, role: options.role,
        model: this.config.model, hostVersion: this.hostVersion, ratesRevision: this.config.rates.revision, sessionId,
        candidateVersion: options.candidateVersion, result }, null, 2));
      const costUsd = result.usage ? calculateEstimatedCostUsd(result.usage, this.config.rates) : null;
      measuredUsage = result.usage;
      measuredCost = costUsd;
      if (result.error || result.exitCode !== 0 || result.timedOut || result.cancelled || !result.usage) {
        return { usage: result.usage, costUsd, error: result.error ?? `Agent failed or usage missing (${result.exitCode})`, verdictStatus: "UNAVAILABLE" };
      }
      let output: any;
      try { output = JSON.parse(finalText); } catch { return { usage: result.usage, costUsd, error: "Agent final response is not JSON", verdictStatus: "INCONCLUSIVE" }; }
      if (options.role === "builder") {
        if (typeof output.artifactContent !== "string" || !output.artifactContent) throw new Error("Builder did not return an artifact");
        const artifactContent = await this.workspace.stage(output.artifactContent);
        abnormal = false;
        return { artifactContent, usage: result.usage, costUsd };
      }
      if (!options.candidateVersion || output.candidateVersionId !== options.candidateVersion.versionId ||
        !["GREEN", "RED", "INCONCLUSIVE"].includes(output.verdictStatus) || typeof output.reasons !== "string" || !output.reasons.trim()) {
        throw new Error("Verifier output is malformed or refers to a different candidate");
      }
      await this.workspace.verifyUnchanged();
      const verification = await executeAgentProcess({
        ...this.config.verification, args: this.config.verification.args ?? [], cwd: this.workspace.directory,
        inputPrompt: "", timeoutSeconds: options.timeoutSeconds, signal: options.signal,
      });
      await this.workspace.verifyUnchanged();
      const verificationPath = path.join(this.config.evidenceDirectory, `${invocationId}.verification.json`);
      writeFileSync(verificationPath, JSON.stringify(verification, null, 2));
      const passed = verification.exitCode === 0 && !verification.timedOut && !verification.cancelled && !verification.error;
      const evidence: EvidenceRecord[] = [
        { evidenceId: `${invocationId}:review`, candidateVersionId: options.candidateVersion.versionId,
          artifactDigest: options.candidateVersion.artifactDigest, origin: options.agentId,
          producerRole: "adversarial", evidenceType: "review", status: output.verdictStatus === "GREEN" ? "PASS" : "FAIL", reference: logPath },
        { evidenceId: `${invocationId}:test`, candidateVersionId: options.candidateVersion.versionId,
          artifactDigest: options.candidateVersion.artifactDigest, origin: options.agentId,
          producerRole: "adversarial", evidenceType: "test", status: passed ? "PASS" : "FAIL", reference: verificationPath },
      ];
      abnormal = false;
      return { usage: result.usage, costUsd, verdictStatus: passed ? output.verdictStatus : "RED",
        failureClass: passed ? "REVIEW_REJECTED" : "TEST_FAILURE",
        evidenceRefs: evidence.map((item) => item.evidenceId), evidenceRecords: evidence };
    } catch (error) {
      return { usage: measuredUsage, costUsd: measuredCost, verdictStatus: "UNAVAILABLE", error: (error as Error).message };
    } finally {
      if (!sessionId && sessionPromise) sessionId = await sessionPromise.catch(() => undefined);
      try { if (sessionId) await this.workspace.closeSession(sessionId, abnormal); }
      finally { await channel.close(); rmSync(directory, { recursive: true, force: true }); }
    }
  }

  async converse(
    messages: Array<{ role: string; content: string }>,
    signal?: AbortSignal
  ): Promise<{ text: string; proposal?: any }> {
    await this.inspectHost(signal);
    const directory = mkdtempSync(path.join(tmpdir(), "cbm-codex-disc-"));
    const schemaPath = path.join(directory, "discovery.schema.json");
    const schema = {
      type: "object",
      properties: {
        message: { type: "string", description: "Mensagem ou perguntas de alinhamento para o operador" },
        proposal: {
          type: ["object", "null"],
          description: "Proposta PBB quando convergida, ou null se ainda estiver clarificando",
          properties: {
            problem: { type: "string" },
            targets: { type: "array", items: { type: "string" } },
            acceptanceCriteria: { type: "array", items: { type: "string" } },
          },
          required: ["problem", "targets", "acceptanceCriteria"],
          additionalProperties: false,
        },
      },
      required: ["message", "proposal"],
      additionalProperties: false,
    };
    writeFileSync(schemaPath, JSON.stringify(schema));

    const promptText = messages.map((m) => `[${m.role.toUpperCase()}]: ${m.content}`).join("\n\n") +
      "\n\nVocê é o CBM Discovery Engine. Conduza o alinhamento conversacional com o operador. Se os requisitos e alvos estiverem claros, retorne a proposta PBB completa. Se houver ambiguidade, faça perguntas de clarificação em 'message' e omita 'proposal'.";

    const args = [
      ...this.config.args ?? [],
      "exec",
      "--ignore-user-config",
      "--ignore-rules",
      "--skip-git-repo-check",
      "--ephemeral",
      "--sandbox",
      "read-only",
      "--model",
      this.config.model,
      "--json",
      "--color",
      "never",
      "--cd",
      directory,
      "--output-schema",
      schemaPath,
      "-c",
      'approval_policy="never"',
      "-c",
      'web_search="disabled"',
      "-c",
      "features.shell_tool=false",
    ];

    if (this.config.effort) {
      args.push("-c", `model_reasoning_effort="${this.config.effort}"`);
    }

    args.push("-");

    try {
      const result = await executeAgentProcess({
        command: this.config.command,
        args,
        cwd: directory,
        inputPrompt: promptText,
        timeoutSeconds: 60,
        signal,
      });

      if (result.exitCode !== 0 || !result.stdout) {
        return { text: "Não foi possível obter resposta do discovery runner." };
      }

      let parsedMessage = "";
      let proposal: any;
      const lines = result.stdout.trim().split("\n");
      for (const line of lines) {
        try {
          const item = JSON.parse(line);
          if (item?.type === "item.completed" && item?.item?.type === "agent_message" && item?.item?.text) {
            const parsed = JSON.parse(item.item.text);
            parsedMessage = parsed.message || "";
            if (parsed.proposal && Array.isArray(parsed.proposal.targets)) {
              proposal = parsed.proposal;
            }
          }
        } catch {
          // ignora linhas não-JSON
        }
      }

      return { text: parsedMessage || result.stdout, proposal };
    } finally {
      try {
        rmSync(directory, { recursive: true, force: true });
      } catch {
        // ignora erro de limpeza no Windows
      }
    }
  }
}
