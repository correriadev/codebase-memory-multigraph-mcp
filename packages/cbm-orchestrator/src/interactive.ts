import path from "node:path";
import os from "node:os";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import readline from "node:readline/promises";
import { stdin as defaultInput, stdout as defaultOutput } from "node:process";
import { OrchestratorApplication, readConfiguration, createDefaultConfiguration, type OrchestratorConfig } from "./application.ts";
import { approveS0 } from "./s0-spike.ts";
import {
  discoverProvider,
  sanitizeCustomModel,
  normalizeReasoningEffort,
  createInitialTuple,
  type ProviderId,
  type ProviderCatalog,
} from "./provider-discovery.ts";
import { TerminalTui } from "./terminal-tui.ts";
import type { KnowledgeTarget } from "./cbm-backend.ts";
import type {
  Stage1Proposal,
  Stage1Decision,
  PauseContext,
  OperatorDecision,
} from "./orchestrator.ts";

// ANSI Styling Helpers (graceful fallback)
const isColorSupported = Boolean(process.stdout.isTTY && !process.env.NO_COLOR);
const style = {
  bold: (t: string) => (isColorSupported ? `\x1b[1m${t}\x1b[0m` : t),
  dim: (t: string) => (isColorSupported ? `\x1b[2m${t}\x1b[0m` : t),
  green: (t: string) => (isColorSupported ? `\x1b[32m${t}\x1b[0m` : t),
  red: (t: string) => (isColorSupported ? `\x1b[31m${t}\x1b[0m` : t),
  yellow: (t: string) => (isColorSupported ? `\x1b[33m${t}\x1b[0m` : t),
  cyan: (t: string) => (isColorSupported ? `\x1b[36m${t}\x1b[0m` : t),
  magenta: (t: string) => (isColorSupported ? `\x1b[35m${t}\x1b[0m` : t),
};

export type VerboseLevel = "none" | "low" | "medium" | "high";

export function sanitizeSecrets(text: string): string {
  if (!text || typeof text !== "string") return "";
  let result = text
    .replace(/(CBM_AGENT_TOKEN=)(["']?)[^\s"']+(["']?)/gi, "$1$2[REDACTED]$3")
    .replace(/(Bearer\s+)[^\s"']+/gi, "$1[REDACTED]")
    .replace(/(token["']?\s*[:=]\s*["']?)[^\s"',}]+/gi, "$1[REDACTED]");
  if (process.env.CBM_AGENT_TOKEN && process.env.CBM_AGENT_TOKEN.length > 3) {
    result = result.replaceAll(process.env.CBM_AGENT_TOKEN, "[REDACTED]");
  }
  return result;
}

export interface PbbProposal {
  problem: string;
  targets: string[];
  acceptanceCriteria: string[];
}

export function enrichBuilderPrompt(objective: string, proposal: PbbProposal): string {
  const criteriaText = proposal.acceptanceCriteria.map((c) => ` • ${c}`).join("\n");
  return `${objective}\n\n[Especificação PBB Aprovada]\nProblema: ${proposal.problem}\nAlvos: ${proposal.targets.join(", ")}\nCritérios de Aceite:\n${criteriaText}`;
}

export function normalizePbbTargets(targets: string[]): KnowledgeTarget[] {
  if (!Array.isArray(targets) || targets.length === 0) {
    throw new Error("PBB proposal must contain at least one target file");
  }
  const result: KnowledgeTarget[] = [];
  for (const raw of targets) {
    if (typeof raw !== "string" || !raw.trim()) continue;
    const clean = raw.trim().replace(/\\/g, "/");
    if (
      path.posix.isAbsolute(clean) ||
      path.win32.isAbsolute(raw.trim()) ||
      clean.startsWith("/") ||
      clean.startsWith("//") ||
      /^[a-zA-Z]:/.test(clean)
    ) {
      throw new Error(`Absolute or UNC paths denied in target path: ${raw}`);
    }
    const parts = clean.split("/").filter((p) => p !== "" && p !== ".");
    if (parts.some((p) => p === "..")) {
      throw new Error(`Path traversal denied in target path: ${raw}`);
    }
    const normalized = parts.join("/");
    result.push({ path: normalized, symbol: "main" });
  }
  if (result.length === 0) {
    throw new Error("No valid target paths after normalization");
  }
  return result;
}

export function isControlCommand(input: string): boolean {
  const normalized = (input || "").trim().toLowerCase();
  return (
    !normalized ||
    normalized === "q" ||
    normalized === "0" ||
    normalized === "sair" ||
    normalized === "exit" ||
    normalized === ":help" ||
    normalized === "--help" ||
    normalized.startsWith(":verbose")
  );
}

export interface SessionOptions {
  alphaChatMode?: boolean;
  verbose?: VerboseLevel;
  discoveryRunner?: (
    messages: Array<{ role: string; content: string }>,
    signal?: AbortSignal
  ) => Promise<{ text: string; proposal?: PbbProposal }>;
}

export interface InteractiveOptions {
  configFile?: string;
  app?: OrchestratorApplication;
  input?: NodeJS.ReadableStream;
  output?: NodeJS.WritableStream;
  discoverFn?: typeof discoverProvider;
  tui?: TerminalTui;
  alphaChatMode?: boolean;
  verbose?: VerboseLevel;
  discoveryRunner?: (
    messages: Array<{ role: string; content: string }>,
    signal?: AbortSignal
  ) => Promise<{ text: string; proposal?: PbbProposal }>;
}

export function findDefaultConfig(): string | null {
  const candidates = [
    path.resolve("orchestrator.config.json"),
    path.resolve("cbm.config.json"),
    path.resolve("cbm.sandbox.config.json"),
    path.resolve("packages/cbm-orchestrator/cbm.sandbox.config.json"),
    path.resolve("../cbm.config.json"),
    path.resolve("../cbm.sandbox.config.json"),
    path.join(os.homedir(), ".cbm", "cbm.config.json"),
  ];
  for (const c of candidates) {
    if (existsSync(c)) return c;
  }
  return null;
}

export class InteractiveSession {
  private rl: readline.Interface;
  private app: OrchestratorApplication;
  private output: NodeJS.WritableStream;
  private configFile?: string;
  private activeController: AbortController | null = null;
  private discoverFn?: typeof discoverProvider;
  private tui: TerminalTui;
  private alphaChatMode: boolean;
  private verbose: VerboseLevel;
  private discoveryRunner?: (
    messages: Array<{ role: string; content: string }>,
    signal?: AbortSignal
  ) => Promise<{ text: string; proposal?: PbbProposal }>;
  private conversationHistory: Array<{ role: string; content: string }> = [];
  private isExecutingCycle: boolean = false;
  private shouldPauseFlag: boolean = false;

  constructor(
    app: OrchestratorApplication,
    input: NodeJS.ReadableStream,
    output: NodeJS.WritableStream,
    configFile?: string,
    discoverFn?: typeof discoverProvider,
    tui?: TerminalTui,
    options: SessionOptions = {}
  ) {
    this.app = app;
    const originalWrite = output.write.bind(output);
    const sanitizedOutput = new Proxy(output, {
      get(target, prop, receiver) {
        if (prop === "write") {
          return (chunk: any, encoding?: any, cb?: any) => {
            if (typeof chunk === "string") {
              return originalWrite(sanitizeSecrets(chunk), encoding, cb);
            }
            if (Buffer.isBuffer(chunk)) {
              return originalWrite(Buffer.from(sanitizeSecrets(chunk.toString("utf8"))), encoding, cb);
            }
            return originalWrite(chunk, encoding, cb);
          };
        }
        return Reflect.get(target, prop, receiver);
      },
    });
    this.output = sanitizedOutput;
    this.configFile = configFile;
    this.discoverFn = discoverFn;
    this.alphaChatMode = Boolean(options.alphaChatMode);
    this.verbose = options.verbose || "none";
    this.discoveryRunner = options.discoveryRunner;
    this.tui = tui ?? new TerminalTui({ stdout: sanitizedOutput as any, stdin: input as any });
    this.rl = readline.createInterface({ input, output: sanitizedOutput });

    if (!this.alphaChatMode) {
      this.tui.setHeader({
        project: app.config.knowledge.project,
        workspace: app.config.knowledge.workspace,
        provider: (app.config.codex.provider || "codex").toUpperCase(),
        model: app.config.codex.model,
        effort: app.config.codex.effort,
        tokenCap: app.config.policy.cycleTokenCap,
        costCapUsd: app.config.policy.cycleMonetaryCapUsd,
      });
    }
  }

  private write(text: string): void {
    if (!this.alphaChatMode && this.tui.isInteractive()) {
      this.tui.addMessage(text);
      this.tui.draw();
    } else {
      this.output.write(text + "\n");
    }
  }

  private printBanner(): void {
    this.write("");
    this.write(style.cyan("╔══════════════════════════════════════════════════════════════════╗"));
    this.write(style.cyan("║") + style.bold("            CBM SYSTEM INTELLIGENCE ENGINE ORCHESTRATOR           ") + style.cyan("║"));
    this.write(style.cyan("║") + style.dim("           Autonomous Outer Loop · Interactive CLI v4            ") + style.cyan("║"));
    this.write(style.cyan("╚══════════════════════════════════════════════════════════════════╝"));
    this.write("");
    this.write(`${style.bold("Projeto:")}     ${style.green(this.app.config.knowledge.project)}`);
    this.write(`${style.bold("Workspace:")}   ${this.app.config.knowledge.workspace}`);
    const provider = this.app.config.codex.provider || "codex";
    const effort = this.app.config.codex.effort ? ` [esforço: ${this.app.config.codex.effort}]` : "";
    this.write(`${style.bold("Provedor:")}    ${style.cyan(provider.toUpperCase())} · ${style.bold(this.app.config.codex.model)}${style.yellow(effort)}`);
    this.write(`${style.bold("Cap Tokens:")}  ${this.app.config.policy.cycleTokenCap.toLocaleString()} tokens`);
    this.write(`${style.bold("Teto Custo:")}  $${this.app.config.policy.cycleMonetaryCapUsd.toFixed(2)} USD`);
    this.write("");
  }

  private printMenu(): void {
    this.write(style.bold("─── MENU DE OPERAÇÃO ──────────────────────────────────────────────"));
    this.write(`  ${style.cyan("[1]")} ${style.bold("Executar Ciclo Autônomo")}       ${style.dim("(Builder ➔ Sealer ➔ Adversarial)")}`);
    this.write(`  ${style.cyan("[2]")} ${style.bold("Inspecionar CBM / Sonda")}        ${style.dim("(Capacidades, schemas e geração do grafo)")}`);
    this.write(`  ${style.cyan("[3]")} ${style.bold("Histórico e Auditoria")}        ${style.dim("(Consultar ciclos e ledger no SQLite WAL)")}`);
    this.write(`  ${style.cyan("[4]")} ${style.bold("Validar e Assinar Portão S0")}    ${style.dim("(Spike ao vivo de 5 critérios de aceite)")}`);
    this.write(`  ${style.cyan("[5]")} ${style.bold("Gerenciamento de Lock")}         ${style.dim("(Verificar ou liberar cycle.lock)")}`);
    this.write(`  ${style.cyan("[6]")} ${style.bold("Configurar Provedor/Modelo")}   ${style.dim("(Codex, Antigravity, OpenCode & Esforço)")}`);
    this.write(`  ${style.cyan("[L]")} ${style.bold("Alternar Logs")}                 ${style.dim(this.tui.getShowLogs() ? "(Logs: [LIGADO])" : "(Logs: [DESLIGADO])")}`);
    this.write(`  ${style.cyan("[0]")} ${style.yellow("Sair")}`);
    this.write(style.bold("───────────────────────────────────────────────────────────────────"));
  }

  public async start(): Promise<void> {
    const onSigInt = () => {
      if (this.alphaChatMode) {
        if (this.isExecutingCycle) {
          this.shouldPauseFlag = true;
          this.output.write("\n" + style.yellow("[SINAL] Solicitando pausa da execução ao orquestrador...") + "\n");
        } else {
          this.rl.close();
          process.exit(0);
        }
      } else {
        if (this.activeController) {
          this.activeController.abort();
        } else {
          this.tui.exitAlternateBuffer();
          process.exit(0);
        }
      }
    };
    const onExit = () => {
      if (!this.alphaChatMode) {
        this.tui.exitAlternateBuffer();
      }
    };
    process.on("SIGINT", onSigInt);
    process.on("exit", onExit);

    try {
      if (this.alphaChatMode) {
        await this.startAlphaChat();
        return;
      }

      this.tui.enterAlternateBuffer();
      this.printBanner();

      let running = true;
      while (running) {
        this.printMenu();
        let choice = "";
        try {
          choice = (await this.rl.question(style.bold("\nSelecione uma opção [0-6, L]: "))).trim();
        } catch {
          // EOF or interrupted
          break;
        }

        switch (choice) {
          case "1":
            await this.handleRunCycle();
            break;
          case "2":
            await this.handleProbe();
            break;
          case "3":
            await this.handleAuditHistory();
            break;
          case "4":
            await this.handleS0Approval();
            break;
          case "5":
            await this.handleLockManagement();
            break;
          case "6":
            await this.handleConfigureProvider();
            break;
          case "l":
          case "L":
          case ":logs":
            this.handleToggleLogs();
            break;
          case "0":
          case "q":
          case "exit":
            this.write(style.dim("\nEncerrando sessão interativa do CBM Orchestrator. Até breve!\n"));
            running = false;
            break;
          default:
            this.write(style.yellow("\n⚠️ Opção inválida. Digite um número de 0 a 6 ou L."));
            break;
        }
      }
    } finally {
      process.off("SIGINT", onSigInt);
      process.off("exit", onExit);
      if (!this.alphaChatMode) {
        this.tui.exitAlternateBuffer();
      }
      this.rl.close();
    }
  }

  private formatLog(subsystem: string, message: string): string {
    const now = new Date();
    const hh = String(now.getHours()).padStart(2, "0");
    const mm = String(now.getMinutes()).padStart(2, "0");
    const ss = String(now.getSeconds()).padStart(2, "0");
    const ts = `[${hh}:${mm}:${ss}]`;
    return sanitizeSecrets(`${style.dim(ts)} [${style.cyan(subsystem.toUpperCase())}] ${message}`);
  }

  private async startAlphaChat(): Promise<void> {
    this.output.write("\n" + style.cyan("╔══════════════════════════════════════════════════════════════════╗") + "\n");
    this.output.write(style.cyan("║") + style.bold("           CBM SYSTEM INTELLIGENCE ENGINE · Alpha Chat            ") + style.cyan("║") + "\n");
    this.output.write(style.cyan("╚══════════════════════════════════════════════════════════════════╗") + "\n");
    this.output.write(`${style.bold("Projeto:")}     ${style.green(this.app.config.knowledge.project)}\n`);
    const provider = this.app.config.codex.provider || "codex";
    this.output.write(`${style.bold("Provedor:")}    ${style.cyan(provider.toUpperCase())} · ${style.bold(this.app.config.codex.model)}\n`);
    this.output.write(`${style.dim("Digite seu objetivo ou 'q' para sair.")}\n\n`);

    let running = true;
    while (running) {
      let input = "";
      try {
        input = (await this.rl.question(style.bold("CBM> "))).trim();
      } catch {
        break;
      }
      if (!input || input === "q" || input === "0" || input === "sair" || input === "exit") {
        this.output.write(style.dim("\nSessão encerrada.\n"));
        break;
      }

      if (input.startsWith(":verbose")) {
        const parts = input.split(/\s+/);
        if (parts[1] && ["none", "low", "medium", "high"].includes(parts[1].toLowerCase())) {
          this.verbose = parts[1].toLowerCase() as VerboseLevel;
          this.output.write(style.green(`[OBSERVABILIDADE] Nível alterado para: ${this.verbose}\n`));
        } else {
          this.output.write(style.yellow(`[OBSERVABILIDADE] Nível atual: ${this.verbose}. Opções: none, low, medium, high\n`));
        }
        continue;
      }

      if (input === ":help" || input === "--help") {
        this.output.write(style.cyan("Comandos: :verbose <none|low|medium|high>, :help, q (sair)\n"));
        continue;
      }

      if (this.discoveryRunner) {
        const proposal = await this.handleAlphaDiscovery(input);
        if (proposal) {
          const decision = await this.confirmPbbGate(proposal);
          if (decision.action === "APPROVE") {
            const runTargets = normalizePbbTargets(proposal.targets);
            const enrichedPrompt = enrichBuilderPrompt(input, proposal);
            await this.handleAlphaCycle(input, runTargets, enrichedPrompt);
          } else if (decision.action === "STEER" && decision.guidance) {
            this.conversationHistory.push({ role: "user", content: `[Correção do PBB]: ${decision.guidance}` });
          } else {
            this.output.write(style.yellow("\n[PBB GATE] Execução cancelada pelo operador. Retornando ao chat.\n\n"));
          }
        }
        continue;
      }

      await this.handleAlphaCycle(input);
    }
  }

  public async confirmPbbGate(proposal: PbbProposal): Promise<OperatorDecision> {
    this.output.write("\n" + style.bold("╔══════════════════════════════════════════════════════════════════╗") + "\n");
    this.output.write(style.bold("║                 PROPOSTA PBB CONVERGIDA (STAGE 0)                ║") + "\n");
    this.output.write(style.bold("╚══════════════════════════════════════════════════════════════════╝") + "\n");
    this.output.write(`${style.bold("Problema:")}  ${proposal.problem}\n`);
    this.output.write(`${style.bold("Alvos:")}     ${proposal.targets.join(", ")}\n`);
    this.output.write(`${style.bold("Critérios:")}\n`);
    for (const crit of proposal.acceptanceCriteria) {
      this.output.write(` • ${crit}\n`);
    }
    this.output.write(style.bold("───────────────────────────────────────────────────────────────────") + "\n");

    let answer = "";
    try {
      answer = (
        await this.rl.question(
          `\n${style.bold("Confirmar e executar especificação PBB? [s=Aprovar / c=Corrigir / q=Cancelar]: ")}`
        )
      ).trim().toLowerCase();
    } catch {
      return { action: "REJECT" };
    }

    if (answer === "c" || answer === "corrigir") {
      let guidance = "";
      try {
        guidance = (
          await this.rl.question(
            `\n${style.bold("Digite a instrução de correção para o PBB: ")}`
          )
        ).trim();
      } catch {
        return { action: "REJECT" };
      }
      return { action: "STEER", guidance };
    }

    if (answer === "s" || answer === "sim" || answer === "y" || answer === "yes" || answer === "") {
      return { action: "APPROVE" };
    }

    return { action: "REJECT" };
  }

  public async handleAlphaDiscovery(objective: string): Promise<PbbProposal | null> {
    if (!this.discoveryRunner) return null;
    this.conversationHistory.push({ role: "user", content: objective });
    this.output.write(this.formatLog("DISCOVERY", `Consultando agente de descoberta...`) + "\n");

    const controller = new AbortController();
    this.activeController = controller;
    try {
      const res = await this.discoveryRunner(this.conversationHistory, controller.signal);
      if (res.text) {
        this.conversationHistory.push({ role: "assistant", content: res.text });
        this.output.write(`\n${style.cyan("[DISCOVERY]")} ${res.text}\n\n`);
      }
      return res.proposal || null;
    } finally {
      this.activeController = null;
    }
  }

  private async handleAlphaCycle(objective: string, runTargets?: KnowledgeTarget[], enrichedPrompt?: string): Promise<void> {
    const lockStatus = this.app.isLocked();
    if (lockStatus.locked) {
      this.output.write(style.red("\n❌ Bloqueado: Já existe um ciclo ativo ou lock pendente.\n"));
      return;
    }

    this.output.write(this.formatLog("PLANNER", `Analisando objetivo: "${objective}"...`) + "\n");

    const controller = new AbortController();
    this.activeController = controller;
    this.isExecutingCycle = true;
    this.shouldPauseFlag = false;

    const onStage1Proposal = async (proposal: Stage1Proposal): Promise<Stage1Decision> => {
      this.output.write("\n" + style.bold("─── PROPOSTA DA ETAPA 1 (BUILDER) ──────────────────────────────────") + "\n");
      const lines = (proposal.artifactContent || "").trim().split("\n");
      for (const line of lines.slice(0, 10)) {
        this.output.write(style.dim("│ ") + line + "\n");
      }
      if (lines.length > 10) {
        this.output.write(style.dim(`│ ... (${lines.length - 10} linhas omitidas)\n`));
      }
      this.output.write(style.bold("───────────────────────────────────────────────────────────────────") + "\n");

      let answer = "";
      try {
        answer = (
          await this.rl.question(
            `\n${style.bold("Confirmar proposta da Etapa 1? [s=Aprovar / n=Rejeitar / c=Corrigir]: ")}`
          )
        ).trim().toLowerCase();
      } catch {
        return false;
      }

      if (answer === "c" || answer === "corrigir") {
        let guidance = "";
        try {
          guidance = (
            await this.rl.question(
              `\n${style.bold("Digite a instrução de correção para o Builder: ")}`
            )
          ).trim();
        } catch {
          return false;
        }
        this.output.write(this.formatLog("OPERATOR", `Enviando correção: "${guidance}"`) + "\n");
        return { action: "STEER", guidance };
      }

      const approved = answer === "s" || answer === "sim" || answer === "y" || answer === "yes";
      if (approved) {
        this.output.write(this.formatLog("OPERATOR", "Proposta aprovada pelo operador.") + "\n");
        this.output.write("\n" + style.cyan("=== INÍCIO DA EXECUÇÃO (STREAMING DE LOGS) ===") + "\n");
        this.output.write(this.formatLog("BUILDER", "Gerando modificações e artefatos de código...") + "\n");
        return true;
      }

      this.output.write(this.formatLog("OPERATOR", "Proposta rejeitada pelo operador.") + "\n");
      return false;
    };

    const shouldPause = () => this.shouldPauseFlag;
    const onPause = async (context: PauseContext): Promise<OperatorDecision> => {
      this.shouldPauseFlag = false;
      this.output.write("\n" + style.yellow(`[PAUSA] Execução congelada na fase ${context.phase}.`) + "\n");
      let choice = "";
      try {
        choice = (
          await this.rl.question(
            style.bold("Opção de pausa [1=Continuar / 2=Corrigir / 3=Abortar]: ")
          )
        ).trim();
      } catch {
        return { action: "REJECT" };
      }
      if (choice === "2") {
        const guidance = (await this.rl.question("Digite a instrução corretiva: ")).trim();
        return { action: "STEER", guidance };
      }
      if (choice === "3" || choice === "a") {
        return { action: "REJECT" };
      }
      return { action: "APPROVE" };
    };

    try {
      const s0Path = path.join(this.app.config.stateDirectory, "s0-approval.json");
      const isDiagnostic = !existsSync(s0Path);
      const result: any = await this.app.runCycle(
        objective,
        controller.signal,
        isDiagnostic,
        onStage1Proposal,
        {
          shouldPause,
          onPause,
        },
        runTargets,
        enrichedPrompt
      );

      const filesModified = result?.effects?.map((e: any) => e.target).filter(Boolean) || this.app.config.knowledge.targets.map((t: any) => t.path);
      this.output.write("\n" + style.bold("─── RESUMO OPERACIONAL DE CONCLUSÃO ────────────────────────────────") + "\n");
      this.output.write(`${style.bold("Status:")}             ${result?.promoted ? style.green("✔ SUCESSO (PROMOVIDO)") : style.red("❌ RECUSADO")}\n`);
      if (result?.failureReason) {
        this.output.write(`${style.bold("Motivo da Recusa:")}   ${style.yellow(result.failureReason)}\n`);
      }
      if (result?.runId) this.output.write(`${style.bold("Run ID:")}             ${result.runId}\n`);
      if (this.verbose !== "none" && this.app.config.stateDirectory && result?.runId) {
        const evidenceFile = path.join(this.app.config.stateDirectory, `${result.runId}.json`);
        this.output.write(`${style.bold("Evidência:")}          ${style.dim(evidenceFile)}\n`);
      }
      if (filesModified && filesModified.length > 0) {
        this.output.write(`${style.bold("Arquivos Alterados:")}\n`);
        for (const f of filesModified) {
          this.output.write(` • ${style.cyan(f)}\n`);
        }
      }
      if (result?.iterationsRun !== undefined) this.output.write(`${style.bold("Iterações:")}          ${result.iterationsRun}\n`);
      if (result?.totalTokensUsed !== undefined) this.output.write(`${style.bold("Tokens:")}             ${result.totalTokensUsed.toLocaleString()} tk\n`);
      if (result?.totalCostUsd !== undefined) this.output.write(`${style.bold("Custo Total:")}        $${result.totalCostUsd.toFixed(4)} USD\n`);
      this.output.write(style.bold("───────────────────────────────────────────────────────────────────\n"));

      if (!result?.promoted && !process.env.NODE_TEST_CONTEXT) {
        process.exitCode = 1;
      }
    } catch (err: any) {
      if (!process.env.NODE_TEST_CONTEXT) {
        process.exitCode = 1;
      }
      if (controller.signal.aborted) {
        this.output.write(style.yellow("\n⚠️ Ciclo cancelado pelo operador.\n"));
      } else {
        this.output.write(style.red(`\n❌ Erro durante a execução: ${err.message}\n`));
      }
    } finally {
      this.activeController = null;
      this.isExecutingCycle = false;
      this.shouldPauseFlag = false;
    }
  }

  private handleToggleLogs(): void {
    const active = this.tui.toggleLogs();
    this.write(`\nVisibilidade de logs operacionais alternada para: ${active ? style.green("[LIGADO]") : style.yellow("[DESLIGADO]")}`);
    if (active) {
      this.write(style.dim("Os logs operacionais serão exibidos no histórico com estilo atenuado [cbm:*]."));
    } else {
      this.write(style.dim("Apenas mensagens do agente e erros acionáveis serão exibidos no fluxo principal."));
    }
  }

  private async handleRunCycle(): Promise<void> {
    this.write(style.bold("\n▶ INICIAR CICLO AUTÔNOMO"));
    const lockStatus = this.app.isLocked();
    if (lockStatus.locked) {
      this.write(style.red("\n❌ Bloqueado: Já existe um ciclo ativo ou lock pendente."));
      this.write(style.dim(`   Detalhes: ${JSON.stringify(lockStatus.info)}`));
      this.write(`   Use a opção [5] no menu para liberar o lock se a execução anterior caiu.`);
      return;
    }

    let objective = "";
    try {
      objective = (await this.rl.question("\nDigite o objetivo da mudança (ou vazio para cancelar): ")).trim();
    } catch {
      return;
    }
    if (!objective) {
      this.write(style.dim("Ciclo cancelado pelo operador."));
      return;
    }

    this.write("\nAlvos de intenção padrão configurados:");
    for (const t of this.app.config.knowledge.targets) {
      this.write(` • ${style.cyan(t.path)} (${t.symbol})`);
    }

    let confirmTargets = "";
    try {
      confirmTargets = (await this.rl.question("\nConfirmar alvos padrão? [S/n]: ")).trim().toLowerCase();
    } catch {
      return;
    }
    if (confirmTargets === "n") {
      this.write(style.dim("Personalização de alvos em tempo de execução cancelada."));
      return;
    }

    this.write("\n" + style.cyan("Iniciando ciclo autônomo... (Pressione Ctrl+C para abortar se necessário)"));
    const controller = new AbortController();
    this.activeController = controller;

    try {
      this.tui.setTelemetry({ phase: "BUILDING", isLocked: true });
      this.write(style.dim(" [1/3] 🔨 Fase BUILDING: instanciando Builder sob proxy CBM..."));

      const onStage1Proposal = async (proposal: Stage1Proposal): Promise<boolean> => {
        this.tui.setTelemetry({
          phase: "AWAITING_PLANNING",
          tokensUsed: proposal.usage?.totalTokens || 0,
          costUsd: proposal.costUsd || 0,
        });
        this.write("");
        this.write(style.bold("╔══════════════════════════════════════════════════════════════════╗"));
        this.write(style.bold("║         ETAPA 1 CONCLUÍDA: PROPOSTA DO BUILDER GERADA           ║"));
        this.write(style.bold("╚══════════════════════════════════════════════════════════════════╝"));
        this.write(`${style.bold("Iteração:")}       ${proposal.iteration}`);
        this.write(`${style.bold("Tokens Usados:")}  ${(proposal.usage?.totalTokens || 0).toLocaleString()} tokens`);
        this.write(`${style.bold("Custo Parcial:")}  $${(proposal.costUsd || 0).toFixed(4)} USD`);
        this.write("");
        this.write(style.bold("─── Resumo da Proposta (Artifact Content) ───────────────────────────"));

        const lines = (proposal.artifactContent || "").trim().split("\n");
        const preview = lines.slice(0, 15);
        for (const line of preview) {
          this.write(style.dim("│ ") + line);
        }
        if (lines.length > 15) {
          this.write(style.dim(`│ ... (${lines.length - 15} linhas omitidas)`));
        }
        this.write(style.bold("───────────────────────────────────────────────────────────────────"));

        let answer = "";
        try {
          answer = (
            await this.rl.question(
              `\n${style.bold("Confirmar proposta da Etapa 1? [S=Aprovar / n=Rejeitar / c=Corrigir]: ")}`
            )
          )
            .trim()
            .toLowerCase();
        } catch {
          return false;
        }

        if (answer === "c" || answer === "corrigir") {
          let guidance = "";
          try {
            guidance = (
              await this.rl.question(
                `\n${style.bold("Digite a instrução de correção para o Builder: ")}`
              )
            ).trim();
          } catch {
            return false;
          }
          this.write(style.yellow(`\n↺ Enviando instrução ao Builder: "${guidance}"...`));
          return { action: "STEER", guidance };
        }

        const approved = answer === "" || answer === "s" || answer === "sim" || answer === "y" || answer === "yes";
        if (approved) {
          this.write(style.green("\n✓ Proposta aprovada pelo operador. Avançando para selagem e verificação..."));
          this.tui.setTelemetry({ phase: "SEALING" });
          this.write(style.dim(" [2/3] 🔒 Fase SEALING: calculando hash canônico da candidata..."));
          this.tui.setTelemetry({ phase: "VERIFYING" });
          this.write(style.dim(" [3/3] 🛡️  Fase VERIFYING: verificador adversarial independente analisando..."));
          return true;
        } else {
          this.write(style.yellow("\n⚠️ Proposta rejeitada pelo operador. Abortando ciclo antes da selagem."));
          return false;
        }
      };

      const onPause = async (ctx: PauseContext): Promise<OperatorDecision> => {
        this.tui.setTelemetry({ phase: "AWAITING_DECISION" });
        this.write("");
        this.write(style.bold("╔══════════════════════════════════════════════════════════════════╗"));
        this.write(style.bold("║                PAUSA DO CICLO / INTERVENÇÃO HUMANA               ║"));
        this.write(style.bold("╚══════════════════════════════════════════════════════════════════╝"));
        this.write(`${style.bold("Fase atual:")}   ${ctx.phase}`);
        this.write(`${style.bold("Iteração:")}     ${ctx.iteration}`);
        this.write("");
        this.write(`  ${style.cyan("[1]")} ${style.bold("Retomar execução")}               ${style.dim("(Aprovar e prosseguir)")}`);
        this.write(`  ${style.cyan("[2]")} ${style.bold("Interagir com o agente")}         ${style.dim("(Enviar instrução de correção)")}`);
        this.write(`  ${style.cyan("[3]")} ${style.yellow("Abortar ciclo")}              ${style.dim("(Encerrar com REJECTED)")}`);

        let choice = "";
        try {
          choice = (await this.rl.question(`\n${style.bold("Selecione uma ação [1-3]: ")}`)).trim();
        } catch {
          return { action: "REJECT" };
        }

        if (choice === "2") {
          let guidance = "";
          try {
            guidance = (await this.rl.question(`\n${style.bold("Digite a instrução para o agente: ")}`)).trim();
          } catch {
            return { action: "REJECT" };
          }
          this.write(style.yellow(`\n↺ Instrução enviada ao agente: "${guidance}". Retomando...`));
          return { action: "STEER", guidance };
        }

        if (choice === "3") {
          this.write(style.yellow("\n⚠️ Ciclo cancelado pelo operador na pausa."));
          return { action: "REJECT" };
        }

        this.write(style.green("\n✓ Retomando execução do ciclo..."));
        return { action: "APPROVE" };
      };

      const result = (await this.app.runCycle(objective, controller.signal, false, onStage1Proposal, {
        onPause,
      })) as any;
      this.tui.setTelemetry({
        phase: "COMPLETED",
        tokensUsed: result.totalTokensUsed || 0,
        costUsd: result.totalCostUsd || 0,
        isLocked: false
      });
      this.printDecisionPackage(result);
    } catch (err: any) {
      this.tui.setTelemetry({ phase: "ERROR", isLocked: false });
      if (controller.signal.aborted) {
        this.write(style.yellow("\n⚠️ Ciclo interrompido via solicitação de cancelamento."));
      } else {
        this.write(style.red(`\n❌ Falha durante a execução do ciclo: ${err.message}`));
      }
    } finally {
      this.activeController = null;
    }
  }

  private printDecisionPackage(result: any): void {
    this.write("");
    this.write(style.bold("╔══════════════════════════════════════════════════════════════════╗"));
    this.write(style.bold("║                     PACOTE DE DECISÃO FINAL                      ║"));
    this.write(style.bold("╚══════════════════════════════════════════════════════════════════╝"));
    this.write(`${style.bold("Run ID:")}            ${result.runId}`);
    this.write(`${style.bold("Status:")}            ${result.status === "COMPLETED" ? style.green(result.status) : style.yellow(result.status)}`);
    this.write(`${style.bold("Término:")}           ${result.terminationReason === "ACCEPTED" ? style.green(result.terminationReason) : style.red(result.terminationReason || "N/A")}`);
    this.write(`${style.bold("Candidata ID:")}      ${style.cyan(result.candidateVersionId || "N/A")}`);
    this.write(`${style.bold("Tokens Usados:")}     ${(result.totalTokensUsed || 0).toLocaleString()} tokens`);
    this.write(`${style.bold("Custo Total:")}       $${(result.totalCostUsd || 0).toFixed(4)} USD`);
    this.write(`${style.bold("Iterações:")}         ${result.iterationsRun || 1}`);

    if (result.promoted) {
      this.write("");
      this.write(style.green("✅ CANDIDATO APROVADO COM SUCESSO"));
      this.write(style.dim("   Veredicto independente auditado e selado com evidências verdes."));
    } else {
      this.write("");
      this.write(style.yellow("⚠️  CANDIDATO NÃO APROVADO OU REQUER REVISÃO"));
      if (result.terminationReason) {
        this.write(style.dim(`   Motivo registrado: ${result.terminationReason}`));
      }
    }
    this.write("");
  }

  private async handleProbe(): Promise<void> {
    this.write(style.bold("\n▶ INSPECIONAR CONEXÃO E ESTADO CBM"));
    try {
      this.write(style.dim("Consultando servidor MCP e geração do grafo..."));
      const probeResult = (await this.app.probe()) as any;
      this.write("");
      this.write(`${style.bold("Configuration Digest:")} ${style.cyan(probeResult.configurationDigest || "N/A")}`);
      if (probeResult.snapshot) {
        this.write(`${style.bold("Baseline Digest:")}      ${probeResult.snapshot.baselineDigest || "N/A"}`);
        this.write(`${style.bold("Geração do Grafo:")}     ${probeResult.snapshot.graphGeneration ?? "N/A"}`);
      }
      this.write(style.bold("\nFerramentas CBM Detectadas:"));
      const tools = probeResult.tools || [];
      if (Array.isArray(tools)) {
        for (const t of tools) {
          const toolName = typeof t === "string" ? t : t?.name || "unknown";
          const desc = typeof t === "object" && t?.description ? `: ${style.dim(t.description)}` : "";
          this.write(` • ${style.green(toolName)}${desc}`);
        }
      }
      this.write("");
    } catch (err: any) {
      this.write(style.red(`\n❌ Falha na sonda CBM: ${err.message}\n`));
    }
  }

  private async handleAuditHistory(): Promise<void> {
    this.write(style.bold("\n▶ HISTÓRICO E AUDITORIA DE CICLOS (SQLite WAL)"));
    try {
      const runs = (this.app.listRuns(15) || []) as any[];
      if (runs.length === 0) {
        this.write(style.dim("\nNenhum ciclo registrado no journal ainda.\n"));
        return;
      }

      this.write("");
      this.write(
        style.bold(
          `${"RUN ID".padEnd(20)} ${"ESTADO".padEnd(12)} ${"FASE".padEnd(14)} ${"TÉRMINO".padEnd(16)} ${"CRIADO EM"}`
        )
      );
      this.write("─".repeat(80));
      for (const r of runs) {
        const id = r.runId.padEnd(20);
        const st = (r.state || "").padEnd(12);
        const ph = (r.phase || "").padEnd(14);
        const tr = (r.terminationReason || "-").padEnd(16);
        const dt = r.createdAt ? r.createdAt.substring(0, 19).replace("T", " ") : "";
        this.write(`${style.cyan(id)} ${st} ${ph} ${tr} ${dt}`);
      }
      this.write("");

      let detailId = "";
      try {
        detailId = (await this.rl.question("Digite um Run ID para ver detalhes (ou Enter para voltar): ")).trim();
      } catch {
        return;
      }
      if (detailId) {
        try {
          const detail = this.app.getRun(detailId);
          this.write("\n" + JSON.stringify(detail, null, 2) + "\n");
        } catch (err: any) {
          this.write(style.red(`Não foi possível carregar detalhes de '${detailId}': ${err.message}`));
        }
      }
    } catch (err: any) {
      this.write(style.red(`\n❌ Erro ao consultar histórico: ${err.message}\n`));
    }
  }

  private async handleS0Approval(): Promise<void> {
    this.write(style.bold("\n▶ VALIDAÇÃO E ASSINATURA DO PORTÃO S0"));
    this.write(style.dim("Executando sonda S0 contra o ambiente ao vivo..."));

    try {
      const { runLiveSpike } = await import("./live-spike.ts");
      const report = await runLiveSpike(this.app, "S0 Interactive Preflight Check");

      this.write("");
      this.write(style.bold("Critérios de Aceite S0:"));
      for (const [criterion, result] of Object.entries(report.criteria)) {
        const mark = result === "PASS" ? style.green("✔ PASS") : style.red("✖ FAIL");
        this.write(` • ${criterion.padEnd(22)}: ${mark}`);
      }
      this.write("");

      const allPassed = Object.values(report.criteria).every((v) => v === "PASS");
      if (!allPassed) {
        this.write(style.red("❌ Nem todos os critérios passaram. Não é seguro aprovar o portão S0."));
        return;
      }

      const answer = (await this.rl.question("Todos os critérios passaram! Deseja assinar formalmente o S0? [S/n]: "))
        .trim()
        .toLowerCase();

      if (answer === "" || answer === "s" || answer === "y") {
        const approvalPath = path.join(this.app.config.stateDirectory, "s0-approval.json");
        const reportPath = path.join(this.app.config.stateDirectory, "s0-report.json");
        approveS0(reportPath, approvalPath, this.app.digest);
        this.write(style.green(`\n✔ Portão S0 assinado e registrado em: ${approvalPath}\n`));
      } else {
        this.write(style.dim("\nAssinatura de S0 cancelada pelo operador.\n"));
      }
    } catch (err: any) {
      this.write(style.red(`\n❌ Falha na execução da sonda S0: ${err.message}\n`));
    }
  }

  private async handleLockManagement(): Promise<void> {
    this.write(style.bold("\n▶ GERENCIAMENTO DE LOCK (cycle.lock)"));
    const lock = this.app.isLocked();
    if (!lock.locked) {
      this.write(style.green("\n✔ Nenhum lock ativo. O orquestrador está pronto para novos ciclos.\n"));
      return;
    }

    this.write(style.yellow("\n⚠️ Lock ativo detectado:"));
    this.write(JSON.stringify(lock.info, null, 2));

    let ans = "";
    try {
      ans = (await this.rl.question("\nDeseja remover o arquivo de lock e liberar o orquestrador? [s/N]: "))
        .trim()
        .toLowerCase();
    } catch {
      return;
    }

    if (ans === "s" || ans === "y") {
      const removed = this.app.unlock();
      if (removed) {
        this.write(style.green("✔ Lock removido com sucesso."));
      } else {
        this.write(style.red("Falha ao remover lock."));
      }
    } else {
      this.write(style.dim("Nenhuma ação realizada."));
    }
    this.write("");
  }

  private async handleConfigureProvider(): Promise<void> {
    this.write(style.bold("\n▶ CONFIGURAR PROVEDOR, MODELO E ESFORÇO"));
    const currentProvider = (this.app.config.codex.provider || "codex") as ProviderId;
    const currentModel = this.app.config.codex.model;
    const currentEffort = this.app.config.codex.effort || "low";
    const stateDir = this.app.config.stateDirectory || path.join(process.cwd(), ".cbm-orchestrator");

    this.write(
      `Configuração atual: ${style.cyan(currentProvider.toUpperCase())} · Modelo: ${style.bold(currentModel)} · Esforço: ${style.yellow(currentEffort)}\n`
    );

    // 1. Provedor
    this.write(style.bold("Escolha o Provedor:"));
    this.write(`  [1] OpenAI Codex (codex)`);
    this.write(`  [2] Google Antigravity (agy)`);
    this.write(`  [3] OpenCode CLI (opencode)`);
    this.write(`  [4] Claude Code (claude)`);
    this.write(`  [R] Forçar atualização do catálogo (probe vivo)`);
    this.write(`  [Enter] Manter atual (${currentProvider})\n`);

    let pChoice = "";
    try {
      pChoice = (await this.rl.question("Opção de provedor [1-4 / R]: ")).trim();
    } catch {
      return;
    }

    let forceRefresh = false;
    if (pChoice.toLowerCase() === "r") {
      forceRefresh = true;
      this.write(style.dim("Atualização forçada solicitada."));
      try {
        pChoice = (await this.rl.question("Agora selecione o provedor [1-4]: ")).trim();
      } catch {
        return;
      }
    }

    let provider: ProviderId = currentProvider;
    let command = this.app.config.codex.command;
    if (pChoice === "1") {
      provider = "codex";
      command = "codex";
    } else if (pChoice === "2") {
      provider = "antigravity";
      command = "agy";
    } else if (pChoice === "3") {
      provider = "opencode";
      command = "opencode";
    } else if (pChoice === "4") {
      provider = "claude";
      command = "claude";
    }

    // 2. Descoberta dinâmica de catálogo
    this.write(style.dim(`\nConsultando catálogo de modelos para ${provider.toUpperCase()}...`));
    const discover = this.discoverFn || discoverProvider;
    const catalog = await discover(provider, { stateDir, forceRefresh });

    if (catalog.error) {
      this.write(style.yellow(`⚠ Aviso: ${catalog.error}`));
    }
    this.write(
      `Status do provedor: ${style.bold(catalog.status)} · Origem: ${style.cyan(catalog.models[0]?.source?.toUpperCase() || "CATALOG")}`
    );

    // Se trocou de provedor, reinicializa a tupla atômica para os padrões recomendados
    const initialTuple = createInitialTuple(provider, catalog);
    let selectedModel = provider === currentProvider ? currentModel : initialTuple.model;
    let selectedEffort = provider === currentProvider ? currentEffort : initialTuple.effort;

    // 3. Seleção de Modelo
    this.write(style.bold(`\nModelos Disponíveis para ${provider.toUpperCase()}:`));
    catalog.models.forEach((m, idx) => {
      const srcTag = `[${m.source.toUpperCase()}]`;
      const lvlTag = m.reasoningLevels ? ` [raciocínio: ${m.reasoningLevels.join(", ")}]` : "";
      this.write(`  [${idx + 1}] ${style.bold(m.id)} — ${m.label} ${style.dim(srcTag)}${style.dim(lvlTag)}`);
    });
    this.write(`  [${catalog.models.length + 1}] Outro (digitar identificador customizado)`);
    this.write(`  [Enter] Padrão do provedor (${selectedModel})\n`);

    let mChoice = "";
    try {
      mChoice = (await this.rl.question(`Opção de modelo [1-${catalog.models.length + 1}]: `)).trim();
    } catch {
      return;
    }

    const mNum = parseInt(mChoice, 10);
    if (!isNaN(mNum) && mNum >= 1 && mNum <= catalog.models.length) {
      selectedModel = catalog.models[mNum - 1].id;
    } else if (mNum === catalog.models.length + 1) {
      try {
        const rawCustom = (await this.rl.question("Digite o identificador do modelo: ")).trim();
        const sanitized = sanitizeCustomModel(rawCustom);
        if (sanitized) {
          selectedModel = sanitized;
        } else {
          this.write(style.red(`Entrada inválida ou rejeitada por conter caracteres perigosos. Mantido: ${selectedModel}`));
        }
      } catch {
        return;
      }
    }

    // 4. Seleção e Normalização de Esforço de Raciocínio
    const chosenModelOpt = catalog.models.find((m) => m.id === selectedModel);
    const availableLevels = chosenModelOpt?.reasoningLevels || ["none", "minimal", "low", "medium", "high", "xhigh", "max"];

    this.write(style.bold(`\nNíveis de Raciocínio (Reasoning Effort) para ${selectedModel}:`));
    availableLevels.forEach((eff, idx) => {
      const defMarker = eff === chosenModelOpt?.defaultReasoningLevel || eff === "low" ? " (recomendado)" : "";
      this.write(`  [${idx + 1}] ${eff}${defMarker}`);
    });
    this.write(`  [Enter] Padrão do modelo (${selectedEffort})\n`);

    let eChoice = "";
    try {
      eChoice = (await this.rl.question(`Opção de esforço [1-${availableLevels.length}]: `)).trim();
    } catch {
      return;
    }

    const eNum = parseInt(eChoice, 10);
    if (!isNaN(eNum) && eNum >= 1 && eNum <= availableLevels.length) {
      selectedEffort = availableLevels[eNum - 1];
    }

    // Validação de esforço sem escalonamento para cima
    const norm = normalizeReasoningEffort(chosenModelOpt, selectedEffort);
    if (norm.wasClamped && norm.warning) {
      this.write(style.yellow(`⚠ ${norm.warning}`));
    }
    selectedEffort = norm.effort;

    // Atualização atômica da configuração do orquestrador
    this.app.config.codex.provider = provider;
    this.app.config.codex.command = command;
    this.app.config.codex.model = selectedModel;
    this.app.config.codex.effort = selectedEffort;

    this.tui.setHeader({
      provider: provider.toUpperCase(),
      model: selectedModel,
      effort: selectedEffort,
    });

    this.write(style.green(`\n✔ Configuração atualizada com sucesso!`));
    this.write(`  • Provedor: ${style.bold(provider.toUpperCase())} (${command})`);
    this.write(`  • Modelo:   ${style.bold(selectedModel)}`);
    this.write(`  • Esforço:  ${style.bold(selectedEffort)}`);

    if (this.configFile && existsSync(this.configFile)) {
      try {
        const saveAns = (
          await this.rl.question("\nDeseja salvar esta alteração no arquivo de configuração ativo? [s/N]: ")
        )
          .trim()
          .toLowerCase();
        if (saveAns === "s" || saveAns === "y") {
          writeFileSync(this.configFile, JSON.stringify(this.app.config, null, 2), "utf8");
          this.write(style.green(`✔ Salvo em: ${this.configFile}`));
        }
      } catch {
        // EOF
      }
    }
    this.write("");
  }
}

export function createDefaultDiscoveryRunner(app: any): (
  messages: Array<{ role: string; content: string }>,
  signal?: AbortSignal
) => Promise<{ text: string; proposal?: PbbProposal }> {
  return async (messages: Array<{ role: string; content: string }>, signal?: AbortSignal) => {
    if (app?.config?.codex?.command) {
      try {
        const { CodexAgentRunner } = await import("./codex-runner.ts");
        const runner = new CodexAgentRunner(app.config.codex, {
          directory: app.config.knowledge?.workspace || process.cwd(),
        } as any);
        return await runner.converse(messages, signal);
      } catch {
        // fallback
      }
    }
    const lastMsg = messages.filter((m) => m.role === "user").pop()?.content || "";
    const defaultTargets =
      app?.config?.knowledge?.targets && app.config.knowledge.targets.length > 0
        ? app.config.knowledge.targets.map((t: any) => t.path)
        : ["README.md"];
    return {
      text: `Especificação preliminar formulada para: "${lastMsg}".`,
      proposal: {
        problem: lastMsg,
        targets: defaultTargets,
        acceptanceCriteria: [`Executar "${lastMsg}" garantindo validação e integridade`],
      },
    };
  };
}

export async function runInteractive(options: InteractiveOptions = {}): Promise<void> {
  let app = options.app;
  let cfgFile = options.configFile;

  if (!app) {
    if (!cfgFile) {
      cfgFile = findDefaultConfig() || undefined;
    }
    let config: OrchestratorConfig;
    if (!cfgFile) {
      const out = options.output || defaultOutput;
      out.write("\x1b[33m[AVISO] Nenhum 'cbm.config.json' encontrado. Inicializando com configuração padrão para: " + process.cwd() + "\x1b[0m\n");
      config = createDefaultConfiguration(process.cwd());
    } else {
      config = readConfiguration(cfgFile);
    }
    app = new OrchestratorApplication(config);
  }

  const discoveryRunner =
    options.discoveryRunner ||
    (options.alphaChatMode ? createDefaultDiscoveryRunner(app) : undefined);

  const session = new InteractiveSession(
    app,
    options.input || defaultInput,
    options.output || defaultOutput,
    cfgFile,
    options.discoverFn,
    options.tui,
    {
      alphaChatMode: options.alphaChatMode,
      verbose: options.verbose,
      discoveryRunner,
    }
  );

  await session.start();
}
