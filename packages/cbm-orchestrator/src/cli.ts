#!/usr/bin/env node
import path from "node:path";
import { execSync } from "node:child_process";
import { pathToFileURL } from "node:url";
import { OrchestratorApplication, readConfiguration, createDefaultConfiguration, type OrchestratorConfig } from "./application.ts";
import { serveMcp, type ToolDefinition } from "./mcp-transport.ts";
import { approveS0 } from "./s0-spike.ts";
import { runInteractive, findDefaultConfig } from "./interactive.ts";

export function isDaemonInPath(): boolean {
  try {
    const isWin = process.platform === "win32";
    const cmd = isWin ? "where codebase-memory-mcp" : "which codebase-memory-mcp";
    execSync(cmd, { stdio: "ignore" });
    return true;
  } catch {
    return false;
  }
}

export function validateEnvironment(
  nodeVersion = process.versions.node,
  checkDaemonFn = isDaemonInPath,
  warnFn: (msg: string) => void = (msg) => process.stderr.write(msg + "\n")
): { nodeValid: boolean; daemonFound: boolean } {
  const major = parseInt(nodeVersion.split(".")[0], 10);
  if (isNaN(major) || major < 24) {
    throw new Error(`CBM requer Node.js >= 24 (versão atual: ${nodeVersion}). Atualize seu runtime.`);
  }

  const daemonFound = checkDaemonFn();
  if (!daemonFound) {
    warnFn(
      `\x1b[33m[AVISO] Daemon 'codebase-memory-mcp' não encontrado no PATH.\n` +
      `Execute 'powershell -File install.ps1' para instalar o binário CBM antes de executar ciclos com grafo.\x1b[0m`
    );
  }

  return { nodeValid: true, daemonFound };
}

export const ORCHESTRATOR_TOOLS: ToolDefinition[] = [
  {
    name: "orchestrator_probe",
    description: "Inspect live CBM schemas, exact project and snapshot; starts no model calls",
    inputSchema: { type: "object", properties: {}, additionalProperties: false },
  },
  {
    name: "orchestrator_run_cycle",
    description: "Run Builder and independent verifier using real Codex processes and CBM; requires operator-approved live S0 evidence",
    inputSchema: {
      type: "object",
      properties: { objective: { type: "string", minLength: 1, maxLength: 32000 } },
      required: ["objective"],
      additionalProperties: false,
    },
  },
  {
    name: "orchestrator_get_run",
    description: "Read persisted execution, promotion outcome and usage",
    inputSchema: {
      type: "object",
      properties: { runId: { type: "string" } },
      required: ["runId"],
      additionalProperties: false,
    },
  },
  {
    name: "orchestrator_cancel_cycle",
    description: "Stop new dispatches and cancel owned agent processes; remote effects may require reconciliation",
    inputSchema: {
      type: "object",
      properties: { operationId: { type: "string" } },
      additionalProperties: false,
    },
  },
];

export type VerboseLevel = "none" | "low" | "medium" | "high";

export function parseCliArgs(args: string[]): {
  mode: string;
  configFile?: string;
  verbose: VerboseLevel;
  json: boolean;
  rest: string[];
} {
  let verbose: VerboseLevel = "none";
  let json = false;
  let isModelsFlag = false;
  const filteredArgs: string[] = [];

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "--verbose" || arg === "-v") {
      const next = args[i + 1];
      if (next && ["none", "low", "medium", "high"].includes(next.toLowerCase())) {
        verbose = next.toLowerCase() as VerboseLevel;
        i++;
      } else {
        verbose = "medium";
      }
    } else if (arg.startsWith("--verbose=")) {
      const val = arg.split("=")[1]?.toLowerCase();
      if (val && ["none", "low", "medium", "high"].includes(val)) {
        verbose = val as VerboseLevel;
      } else {
        verbose = "medium";
      }
    } else if (arg === "--json") {
      json = true;
    } else if (arg === "--models") {
      isModelsFlag = true;
    } else {
      filteredArgs.push(arg);
    }
  }

  let mode = isModelsFlag ? "models" : filteredArgs[0] || (args[0] || "interactive");
  if (!isModelsFlag && !filteredArgs[0]) {
    mode = "interactive";
  }
  let rest = filteredArgs.length > 0 ? (isModelsFlag && filteredArgs[0] !== "models" ? filteredArgs : filteredArgs.slice(1)) : [];

  if (mode.endsWith(".json")) {
    return { mode: "interactive", configFile: mode, verbose, json, rest };
  }

  const validModes = ["interactive", "serve", "probe", "run", "spike", "approve-s0", "models"];
  if (!validModes.includes(mode)) {
    throw new Error(`Usage: node src/cli.ts <${validModes.join("|")}> [config.json] [objective or S0 report]`);
  }

  let configFile: string | undefined = rest[0];
  if (!configFile || !configFile.endsWith(".json")) {
    configFile = undefined;
  } else {
    rest = rest.slice(1);
  }

  return { mode, configFile, verbose, json, rest };
}

export async function main(args = process.argv.slice(2)): Promise<void> {
  if (args.includes("--help") || args.includes("-h")) {
    console.log(`CBM System Intelligence Engine Orchestrator

Usage:
  cbm [interactive] [config.json]      Inicia chat interativo Alpha (padrão)
  cbm --models [--json]                Lista provedores, modelos e níveis de esforço
  cbm probe [config.json]              Inspeciona schemas e estado do CBM
  cbm run [config.json] <objetivo>     Executa ciclo autônomo completo
  cbm serve [config.json]              Expõe tools do orquestrador via stdio MCP
  cbm spike [config.json]              Executa spike de validação S0
  cbm approve-s0 <report.json>         Assina formalmente o portão S0

Opções:
  -h, --help                           Exibe esta mensagem de ajuda
`);
    return;
  }

  validateEnvironment();

  const { mode, configFile: parsedConfig, verbose, json, rest } = parseCliArgs(args);
  let configFile = parsedConfig;

  if (mode === "models") {
    const { getAllProviderCatalogs, formatModelsTable } = await import("./provider-discovery.ts");
    const stateDir = configFile ? path.dirname(configFile) : undefined;
    const catalogs = await getAllProviderCatalogs({ stateDir });
    if (json) {
      console.log(JSON.stringify(catalogs, null, 2));
    } else {
      console.log(formatModelsTable(catalogs));
    }
    return;
  }

  if (mode === "interactive") {
    if (!configFile) {
      configFile = findDefaultConfig() || undefined;
    }
    await runInteractive({ configFile, alphaChatMode: true, verbose });
    return;
  }

  let config: OrchestratorConfig;
  if (!configFile) {
    configFile = findDefaultConfig() || undefined;
  }
  if (!configFile) {
    config = createDefaultConfiguration(process.cwd());
  } else {
    config = readConfiguration(configFile);
  }
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
      approveS0(rest[1] || rest[0], path.join(config.stateDirectory, "s0-approval.json"), app.digest);
      console.log(JSON.stringify({ status: "S0_APPROVED", configurationDigest: app.digest }));
    } else if (mode === "spike") {
      const { runLiveSpike } = await import("./live-spike.ts");
      console.log(JSON.stringify(await runLiveSpike(app, rest.slice(1).join(" ")), null, 2));
    } else {
      console.log(JSON.stringify(await app.runCycle(rest.slice(1).join(" ")), null, 2));
    }
  } finally {
    process.off("SIGINT", stop);
    process.off("SIGTERM", stop);
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main().catch((error) => {
    process.stderr.write((error as Error).message + "\n");
    if (!process.env.NODE_TEST_CONTEXT) {
      process.exit(1);
    } else {
      process.exitCode = 1;
    }
  });
}
