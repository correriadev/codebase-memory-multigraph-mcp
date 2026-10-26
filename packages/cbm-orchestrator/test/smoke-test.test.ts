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

  const promptRegex = /(Selecione uma opção|Opção de|Digite o objetivo|Confirmar alvos|Confirmar proposta|Digite um Run ID|Deseja remover|Selecione um provedor|Selecione o modelo|identificador do modelo|Salvar esta configuração|Deseja salvar|Deseja assinar).*[:?]\s*$/i;

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
