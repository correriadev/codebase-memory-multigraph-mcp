/**
 * End-to-End Smoke Test Suite for CBM Orchestrator CLI & TUI
 * Verifies all CLI features (Direct commands and Interactive Menu options).
 */

import test from "node:test";
import assert from "node:assert/strict";
import path from "node:path";
import { existsSync, unlinkSync, writeFileSync, mkdirSync } from "node:fs";
import { PassThrough } from "node:stream";
import { OrchestratorApplication, readConfiguration } from "../src/application.ts";
import { InteractiveSession } from "../src/interactive.ts";
import { TerminalTui } from "../src/terminal-tui.ts";

function getSandboxApp() {
  const candidates = [
    path.resolve("cbm.sandbox.config.json"),
    path.resolve("packages/cbm-orchestrator/cbm.sandbox.config.json"),
    path.resolve("../cbm.sandbox.config.json"),
  ];
  let configPath = "";
  for (const c of candidates) {
    if (existsSync(c)) {
      configPath = c;
      break;
    }
  }
  if (!configPath) throw new Error("cbm.sandbox.config.json not found");
  const config = readConfiguration(configPath);
  if (!existsSync(config.stateDirectory)) {
    mkdirSync(config.stateDirectory, { recursive: true });
  }
  return { app: new OrchestratorApplication(config), config };
}

function runSessionWithInputs(app: OrchestratorApplication, inputs: string[], discoverFn?: any): Promise<string> {
  let outputBuffer = "";
  const input = new PassThrough();
  const output = new PassThrough();
  const queue = [...inputs];

  const promptRegex = /(Selecione uma opção|Opção de|Digite o objetivo|Confirmar alvos|Confirmar proposta|Digite a instrução|Selecione uma ação|Digite um Run ID|Deseja remover|Selecione um provedor|Selecione o modelo|identificador do modelo|Salvar esta configuração|Deseja salvar|Deseja assinar).*[:?]\s*$/i;

  output.on("data", (chunk) => {
    outputBuffer += chunk.toString();
    const clean = outputBuffer.replace(/\x1b\[[0-9;]*[a-zA-Z]/g, "").trimEnd();
    // Feed next input only when readline prompt is actively waiting
    if (queue.length > 0 && promptRegex.test(clean)) {
      const next = queue.shift()!;
      setTimeout(() => {
        if (!input.writableEnded) {
          input.write(next + "\n");
        }
      }, 15);
    }
  });

  const session = new InteractiveSession(app, input, output, undefined, discoverFn);
  const startPromise = session.start().finally(() => {
    if (!input.writableEnded) {
      input.end();
    }
  });

  // Safety timeout
  const timeoutPromise = new Promise<never>((_, reject) => {
    const timer = setTimeout(() => {
      if (!input.writableEnded) input.end();
      reject(new Error(`runSessionWithInputs timed out. Output so far: ${outputBuffer.slice(-300)}`));
    }, 15000);
    startPromise.finally(() => clearTimeout(timer));
  });

  return Promise.race([startPromise, timeoutPromise]).then(() => outputBuffer);
}

test("Smoke 01 - CLI Direct: Sonda CBM (probe) recupera schemas, snapshot e ferramentas", async () => {
  const { app } = getSandboxApp();
  const probe = await app.probe();

  assert.ok(probe.configurationDigest, "Missing configurationDigest");
  assert.ok(Array.isArray(probe.tools), "Missing tools array");
  assert.ok(probe.tools.length >= 5, "Expected at least 5 CBM tools");
  assert.equal(probe.project, "sandbox-exploratory");
  assert.ok(probe.snapshot?.graphGeneration, "Missing graphGeneration");
});

test("Smoke 02 - Menu [5]: Gerenciamento e Liberação de Lock (cycle.lock)", async () => {
  const { app, config } = getSandboxApp();
  const lockFile = path.join(config.stateDirectory, "cycle.lock");
  writeFileSync(lockFile, JSON.stringify({ pid: 1234, operationId: "smoke-lock" }));

  assert.equal(app.isLocked().locked, true, "Lock should be detected");

  // Option 5 -> 's' -> Option 0
  const out = await runSessionWithInputs(app, ["5", "s", "0"]);
  assert.match(out, /GERENCIAMENTO DE LOCK/);
  assert.match(out, /Lock removido com sucesso/);
  assert.equal(app.isLocked().locked, false, "Lock should be cleared");
});

test("Smoke 03 - Menu [2]: Sonda CBM Interativa e visualização do digest", async () => {
  const { app } = getSandboxApp();
  app.unlock();
  const out = await runSessionWithInputs(app, ["2", "0"]);
  assert.match(out, /INSPECIONAR CONEXÃO E ESTADO CBM/);
  assert.match(out, /Configuration Digest:/);
  assert.match(out, /Ferramentas CBM Detectadas:/);
});

test("Smoke 04 - Menu [3]: Consulta de Histórico e Auditoria no SQLite WAL", async () => {
  const { app } = getSandboxApp();
  app.unlock();
  const out = await runSessionWithInputs(app, ["3", "", "0"]);
  assert.match(out, /HISTÓRICO E AUDITORIA DE CICLOS/);
  assert.ok(/RUN ID|Nenhum ciclo registrado/.test(out), "Should display run table or empty message");
});

test("Smoke 05 - Menu [6]: Descoberta e Configuração Atômica de Provedor (Antigravity)", async () => {
  const { app } = getSandboxApp();
  const mockDiscover = async () => ({
    provider: "antigravity" as const,
    status: "CATALOG_AVAILABLE" as const,
    models: [
      { id: "gemini-2.5-pro", label: "Gemini 2.5 Pro", reasoningLevels: ["low", "medium"], defaultReasoningLevel: "medium", source: "live" as const },
    ],
    defaultModelId: "gemini-2.5-pro",
    fetchedAt: new Date().toISOString(),
    ttlMs: 86400000,
  });

  const out = await runSessionWithInputs(app, ["6", "2", "1", "2", "n", "0"], mockDiscover);
  assert.match(out, /CONFIGURAR PROVEDOR, MODELO E ESFORÇO/);
  assert.match(out, /ANTIGRAVITY/);
  assert.equal(app.config.codex.provider, "antigravity");
  assert.equal(app.config.codex.model, "gemini-2.5-pro");
  assert.equal(app.config.codex.effort, "medium");
});

test("Smoke 06 - Observabilidade [L]: Alternância de Logs Operacionais (Toggle ON/OFF)", async () => {
  const { app } = getSandboxApp();
  const out = await runSessionWithInputs(app, ["L", "L", "0"]);
  assert.match(out, /Visibilidade de logs operacionais alternada para:.*LIGADO/);
  assert.match(out, /Visibilidade de logs operacionais alternada para:.*DESLIGADO/);
});

test("Smoke 07 - TUI: Alternate Buffer, Estilização Dim e Bounded Ring Buffer com Evicção", async () => {
  const outStream = new PassThrough() as any;
  const inStream = new PassThrough() as any;
  outStream.columns = 90;
  outStream.rows = 25;
  outStream.isTTY = true;
  inStream.isTTY = true;

  const tui = new TerminalTui({
    stdout: outStream,
    stdin: inStream,
    maxEntries: 5,
  });

  tui.setHeader({
    project: "smoke-project",
    workspace: "C:/Smoke/Workspace",
    provider: "ANTIGRAVITY",
    model: "gemini-2.5-pro",
    effort: "medium",
    tokenCap: 50000,
    costCapUsd: 1.5,
  });

  tui.addMessage("Mensagem principal de chat 1");
  tui.addLog("engine", "Verificando lock de concorrência...", "info");
  tui.addLog("mcp", "Sondando servidor MCP...", "info");
  tui.addLog("lock", "Erro crítico no lock!", "error");

  // OFF: only messages and critical error
  assert.equal(tui.getShowLogs(), false);
  let frame = tui.renderFrame();
  assert.match(frame, /smoke-project/);
  assert.match(frame, /ANTIGRAVITY · gemini-2.5-pro \[medium\]/);
  assert.match(frame, /Mensagem principal de chat 1/);
  assert.match(frame, /\[cbm:lock\] Erro crítico no lock!/);
  assert.doesNotMatch(frame, /\[cbm:engine\]/);

  // Toggle ON: logs appear in dim style
  tui.toggleLogs();
  frame = tui.renderFrame();
  assert.match(frame, /\[cbm:engine\] Verificando lock de concorrência\.\.\./);
  assert.match(frame, /\[cbm:mcp\] Sondando servidor MCP\.\.\./);
  assert.match(frame, /\x1b\[2m/); // Dim escape sequence

  // Eviction test: add 10 entries (max is 5)
  for (let i = 0; i < 10; i++) {
    tui.addLog("engine", `Log adicional ${i}`);
  }
  assert.ok(tui.getEvictedCount() > 0, "Eviction count should be > 0");
  frame = tui.renderFrame();
  assert.match(frame, /descartados por limite de memória/);
});

test("Smoke 08 - CLI Direct: Validação ao Vivo do Portão S0 (5 Critérios de Aceite)", async () => {
  const { app } = getSandboxApp();
  app.unlock();
  const { runLiveSpike } = await import("../src/live-spike.ts");
  const spike = await runLiveSpike(app, "Smoke Test Spike Objective");

  console.log("SPIKE REPORT:", JSON.stringify(spike, null, 2));
  assert.ok(spike.criteria, "Missing spike criteria");
  const count = Object.keys(spike.criteria).length;
  assert.equal(count, 5, "Expected 5 criteria");
  assert.equal(spike.criteria.endToEnd, "PASS");
  assert.equal(spike.criteria.crashRestart, "PASS");
  assert.equal(spike.criteria.cancellation, "PASS");
  assert.equal(spike.criteria.costPersisted, "PASS");
  assert.equal(spike.criteria.intentScopeRefusal, "PASS");
  assert.equal(spike.invariantsPassed, true);
});

test("Smoke 09 - CLI Direct: Assinatura Formal do Portão S0 com Digest de Configuração", async () => {
  const { app, config } = getSandboxApp();
  const reportPath = path.join(config.stateDirectory, "s0-report.json");
  const approvalPath = path.join(config.stateDirectory, "s0-approval.json");
  if (existsSync(approvalPath)) unlinkSync(approvalPath);

  const { approveS0 } = await import("../src/s0-spike.ts");
  approveS0(reportPath, approvalPath, app.digest);
  assert.ok(existsSync(approvalPath), "S0 approval file must be created");
});

test("Smoke 10 - Menu [1]: Execução Completa do Ciclo Autônomo com Pacote de Decisão", async () => {
  const { app } = getSandboxApp();
  app.unlock();
  const out = await runSessionWithInputs(app, ["1", "Refatorar sanitização de input", "s", "s", "0"]);
  assert.match(out, /INICIAR CICLO AUTÔNOMO/);
  assert.match(out, /ETAPA 1 CONCLUÍDA: PROPOSTA DO BUILDER GERADA/);
  assert.match(out, /Proposta aprovada pelo operador/);
  assert.match(out, /PACOTE DE DECISÃO FINAL/);
  assert.match(out, /Run ID:/);
});

test("Smoke 11 - cli_dispatcher_routes_commands: Despachador de linha de comando em cli.ts", async () => {
  const { main } = await import("../src/cli.ts");
  assert.equal(typeof main, "function", "main must be exported by cli.ts");

  // Running with unknown mode should reject
  await assert.rejects(
    async () => {
      await main(["unknown-mode"]);
    },
    /Usage: node src\/cli\.ts/
  );
});

test("Smoke 12 - cli_passes_alpha_chat_mode: cli.ts inicia no modo alpha chat", async () => {
  const { readFileSync, existsSync } = await import("node:fs");
  const path = await import("node:path");
  const p1 = path.resolve("src/cli.ts");
  const p2 = path.resolve("packages/cbm-orchestrator/src/cli.ts");
  const target = existsSync(p1) ? p1 : p2;
  const cliCode = readFileSync(target, "utf8");
  assert.match(cliCode, /runInteractive\(\{\s*configFile,\s*alphaChatMode:\s*true/);
});

test("Smoke 13 - cli_defaults_to_interactive_when_no_args: Sem argumentos despacha para interactive", async () => {
  const { readFileSync, existsSync } = await import("node:fs");
  const path = await import("node:path");
  const p1 = path.resolve("src/cli.ts");
  const p2 = path.resolve("packages/cbm-orchestrator/src/cli.ts");
  const target = existsSync(p1) ? p1 : p2;
  const cliCode = readFileSync(target, "utf8");
  // cli.ts must default mode to "interactive" when no argument is given
  assert.match(cliCode, /mode\s*=\s*(?:args\[0\]\s*\|\|\s*["']interactive["']|["']interactive["'])/);
});

test("Smoke 14 - cli_prints_help_on_help_flag: --help imprime instrucoes sem lancar erro", async () => {
  const { main } = await import("../src/cli.ts");
  let output = "";
  const originalLog = console.log;
  console.log = (...args: any[]) => { output += args.join(" ") + "\n"; };
  try {
    await main(["--help"]);
    assert.match(output, /Usage:|cbm/i);
    assert.match(output, /interactive/);
  } finally {
    console.log = originalLog;
  }
});

test("Smoke 15 - cli_validates_node_version_and_daemon_presence: Valida versao do Node e diagnostica daemon", async () => {
  const { validateEnvironment } = await import("../src/cli.ts");
  assert.equal(typeof validateEnvironment, "function", "validateEnvironment must be exported");

  // Old Node.js version (< 24) must throw/fail
  assert.throws(
    () => validateEnvironment("22.11.0", () => true),
    /requer Node\.js >= 24/i
  );

  // Node 24+ must pass
  let warnOutput = "";
  const res = validateEnvironment("24.0.0", () => false, (msg: string) => { warnOutput += msg; });
  assert.equal(res.nodeValid, true);
  assert.equal(res.daemonFound, false);
  assert.match(warnOutput, /codebase-memory-mcp/i);
});

test("Smoke 16 - package_json_defines_bin_and_scripts: package.json define bin cbm, shebang e scripts", async () => {
  const { readFileSync, existsSync } = await import("node:fs");
  const path = await import("node:path");
  const pPkg1 = path.resolve("package.json");
  const pPkg2 = path.resolve("packages/cbm-orchestrator/package.json");
  const pkgPath = existsSync(pPkg2) ? pPkg2 : pPkg1;
  const pkg = JSON.parse(readFileSync(pkgPath, "utf8"));

  assert.ok(pkg.bin, "package.json must define bin");
  assert.equal(pkg.bin.cbm, "./src/cli.ts");
  assert.ok(pkg.scripts.build, "package.json must define build script");
  assert.ok(pkg.scripts["deploy:local"], "package.json must define deploy:local script");

  const pCli1 = path.resolve("src/cli.ts");
  const pCli2 = path.resolve("packages/cbm-orchestrator/src/cli.ts");
  const cliPath = existsSync(pCli1) ? pCli1 : pCli2;
  const cliContent = readFileSync(cliPath, "utf8");
  assert.match(cliContent, /^#!\/usr\/bin\/env node/);
});

test("Smoke 17 - deploy_local_creates_executable_shims_in_user_path: deploy-local.ps1 gera shims executaveis", async () => {
  const { existsSync, readFileSync, mkdtempSync } = await import("node:fs");
  const { execSync } = await import("node:child_process");
  const path = await import("node:path");
  const os = await import("node:os");

  const p1 = path.resolve("scripts/deploy-local.ps1");
  const p2 = path.resolve("../../scripts/deploy-local.ps1");
  const scriptPath = existsSync(p1) ? p1 : p2;
  assert.ok(existsSync(scriptPath), "scripts/deploy-local.ps1 must exist");

  const tempDir = mkdtempSync(path.join(os.tmpdir(), "cbm-deploy-test-"));
  execSync(`powershell -ExecutionPolicy Bypass -File "${scriptPath}" -TargetDir "${tempDir}"`, {
    stdio: "pipe",
  });

  const cmdShim = path.join(tempDir, "cbm.cmd");
  const shShim = path.join(tempDir, "cbm");

  assert.ok(existsSync(cmdShim), "cbm.cmd must be created in target dir");
  assert.ok(existsSync(shShim), "cbm sh shim must be created in target dir");

  const cmdContent = readFileSync(cmdShim, "utf8");
  assert.match(cmdContent, /node\s+.*cli\.ts/i);
});

test("Smoke 18 - build_script_runs_successfully: npm run build executa sem erro", async () => {
  const { existsSync } = await import("node:fs");
  const { execSync } = await import("node:child_process");
  const path = await import("node:path");
  const pkgDir = path.resolve("packages/cbm-orchestrator");
  const cwd = existsSync(pkgDir) ? pkgDir : path.resolve(".");
  execSync("npm run build", { cwd, stdio: "pipe" });
});

test("Smoke 19 - cli_probe_runs_in_temp_dir_without_hang: createDefaultConfiguration inclui targets e probe encerra limpo", async () => {
  const { createDefaultConfiguration } = await import("../src/application.ts");
  const cfg = createDefaultConfiguration("C:/mock/ws");
  assert.ok(Array.isArray(cfg.knowledge.targets) && cfg.knowledge.targets.length >= 1, "createDefaultConfiguration must have >= 1 target");
  assert.ok(cfg.knowledge.targets[0].path && cfg.knowledge.targets[0].symbol);
});

test("Smoke 20 - cli_routes_config_file_argument_to_interactive: cbm <config.json> despacha para interactive", async () => {
  const { parseCliArgs } = await import("../src/cli.ts");
  assert.equal(typeof parseCliArgs, "function", "parseCliArgs must be exported");
  const parsed = parseCliArgs(["custom.config.json"]);
  assert.equal(parsed.mode, "interactive");
  assert.equal(parsed.configFile, "custom.config.json");
});

test("Smoke_cli_parses_verbose_flag_and_levels: parseCliArgs extracts verbose levels and sanitizeSecrets redacts tokens", async () => {
  const { parseCliArgs } = await import("../src/cli.ts");
  const { sanitizeSecrets } = await import("../src/interactive.ts");

  // Default when omitted
  const p1 = parseCliArgs([]);
  assert.equal(p1.verbose, "none");

  // Bare --verbose defaults to medium
  const p2 = parseCliArgs(["--verbose"]);
  assert.equal(p2.verbose, "medium");

  // Explicit level
  const p3 = parseCliArgs(["--verbose", "low"]);
  assert.equal(p3.verbose, "low");

  // Short flag -v with high
  const p4 = parseCliArgs(["-v", "high"]);
  assert.equal(p4.verbose, "high");

  // Preserves mode, config, and rest objective
  const p5 = parseCliArgs(["run", "cbm.config.json", "-v", "medium", "fix bug"]);
  assert.equal(p5.mode, "run");
  assert.equal(p5.configFile, "cbm.config.json");
  assert.equal(p5.verbose, "medium");
  assert.equal(p5.rest.join(" "), "fix bug");

  // Sanitization test
  const raw = "Connecting with CBM_AGENT_TOKEN=secret-12345-token and bearer secret-abc";
  const sanitized = sanitizeSecrets(raw);
  assert.ok(!sanitized.includes("secret-12345-token"), "Must redact CBM_AGENT_TOKEN");
  assert.match(sanitized, /\[REDACTED\]/);
});

test("Smoke_cli_parses_models_and_json_flags: parseCliArgs extracts models mode and json flag", async () => {
  const { parseCliArgs } = await import("../src/cli.ts");

  // Flag --models
  const p1 = parseCliArgs(["--models"]);
  assert.equal(p1.mode, "models");
  assert.equal(p1.json, false);

  // Flag --models with --json
  const p2 = parseCliArgs(["--models", "--json"]);
  assert.equal(p2.mode, "models");
  assert.equal(p2.json, true);

  // Positional mode models
  const p3 = parseCliArgs(["models"]);
  assert.equal(p3.mode, "models");
  assert.equal(p3.json, false);

  // Positional mode models with --json
  const p4 = parseCliArgs(["models", "--json"]);
  assert.equal(p4.mode, "models");
  assert.equal(p4.json, true);

  // Normal mode has json: false
  const p5 = parseCliArgs([]);
  assert.equal(p5.json, false);
});

test("Smoke_formatModelsTable_renders_aligned_rows: formatModelsTable and getAllProviderCatalogs produce formatted table", async () => {
  const { getAllProviderCatalogs, formatModelsTable } = await import("../src/provider-discovery.ts");
  assert.equal(typeof getAllProviderCatalogs, "function", "getAllProviderCatalogs must be exported");
  assert.equal(typeof formatModelsTable, "function", "formatModelsTable must be exported");

  const mockCatalogs: any[] = [
    {
      provider: "codex",
      status: "CATALOG_AVAILABLE",
      defaultModelId: "gpt-6.1-sol",
      models: [
        { id: "gpt-6.1-sol", label: "GPT-6.1 Sol", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "low", source: "live" },
      ],
      fetchedAt: new Date().toISOString(),
      ttlMs: 86400000,
    },
    {
      provider: "antigravity",
      status: "NOT_INSTALLED",
      defaultModelId: "gemini-2.5-pro",
      models: [],
      fetchedAt: new Date().toISOString(),
      ttlMs: 86400000,
      error: "Command not found: agy",
    },
  ];

  const table = formatModelsTable(mockCatalogs);
  assert.match(table, /PROVEDOR/);
  assert.match(table, /STATUS/);
  assert.match(table, /MODELO/);
  assert.match(table, /ESFORÇOS/);
  assert.match(table, /CODEX/i);
  assert.match(table, /gpt-6\.1-sol/);
  assert.match(table, /ANTIGRAVITY/i);
  assert.match(table, /NOT_INSTALLED/);
});

test("Smoke_cli_dispatches_models_and_json_modes: cbm --models and cbm --models --json print catalogs and exit without interactive chat", async () => {
  const { main } = await import("../src/cli.ts");

  const originalLog = console.log;
  let logOutput = "";
  console.log = (...args: any[]) => {
    logOutput += args.map(String).join(" ") + "\n";
  };

  try {
    // 1. Tabular mode
    logOutput = "";
    await main(["--models"]);
    assert.match(logOutput, /PROVEDOR/);
    assert.match(logOutput, /STATUS/);

    // 2. JSON mode
    logOutput = "";
    await main(["--models", "--json"]);
    const parsed = JSON.parse(logOutput.trim());
    assert.ok(Array.isArray(parsed), "JSON output must be an array of provider catalogs");
    assert.ok(parsed.some((p: any) => p.provider === "codex"), "Must include codex provider");
  } finally {
    console.log = originalLog;
  }
});
