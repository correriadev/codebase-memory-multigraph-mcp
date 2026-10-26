import path from "node:path";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import readline from "node:readline/promises";
import { stdin as defaultInput, stdout as defaultOutput } from "node:process";
import { OrchestratorApplication, readConfiguration, type OrchestratorConfig } from "./application.ts";
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
import type { Stage1Proposal } from "./orchestrator.ts";

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

export interface InteractiveOptions {
  configFile?: string;
  app?: OrchestratorApplication;
  input?: NodeJS.ReadableStream;
  output?: NodeJS.WritableStream;
  discoverFn?: typeof discoverProvider;
  tui?: TerminalTui;
}

export function findDefaultConfig(): string | null {
  const candidates = [
    path.resolve("orchestrator.config.json"),
    path.resolve("cbm.config.json"),
    path.resolve("spike.config.json"),
    path.resolve("packages/cbm-orchestrator/spike.config.json"),
    path.resolve("../spike.config.json"),
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

  constructor(
    app: OrchestratorApplication,
    input: NodeJS.ReadableStream,
    output: NodeJS.WritableStream,
    configFile?: string,
    discoverFn?: typeof discoverProvider,
    tui?: TerminalTui
  ) {
    this.app = app;
    this.output = output;
    this.configFile = configFile;
    this.discoverFn = discoverFn;
    this.tui = tui ?? new TerminalTui({ stdout: output as any, stdin: input as any });
    this.rl = readline.createInterface({ input, output });

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

  private write(text: string): void {
    if (this.tui.isInteractive()) {
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
      if (this.activeController) {
        this.activeController.abort();
      } else {
        this.tui.exitAlternateBuffer();
        process.exit(0);
      }
    };
    const onExit = () => {
      this.tui.exitAlternateBuffer();
    };
    process.on("SIGINT", onSigInt);
    process.on("exit", onExit);

    try {
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
      this.tui.exitAlternateBuffer();
      this.rl.close();
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
              `\n${style.bold("Confirmar proposta da Etapa 1 e prosseguir para selagem e verificação? [S/n]: ")}`
            )
          )
            .trim()
            .toLowerCase();
        } catch {
          return false;
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

      const result = (await this.app.runCycle(objective, controller.signal, false, onStage1Proposal)) as any;
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

export async function runInteractive(options: InteractiveOptions = {}): Promise<void> {
  let app = options.app;
  let cfgFile = options.configFile;

  if (!app) {
    if (!cfgFile) {
      cfgFile = findDefaultConfig() || undefined;
    }
    if (!cfgFile) {
      throw new Error(
        "Nenhum arquivo de configuração especificado ou encontrado. Crie um 'cbm.config.json' ou execute com: node src/cli.ts interactive <config.json>"
      );
    }
    const config = readConfiguration(cfgFile);
    app = new OrchestratorApplication(config);
  }

  const session = new InteractiveSession(
    app,
    options.input || defaultInput,
    options.output || defaultOutput,
    cfgFile,
    options.discoverFn,
    options.tui
  );

  await session.start();
}
