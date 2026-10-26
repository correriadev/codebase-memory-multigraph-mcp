import test from "node:test";
import assert from "node:assert/strict";
import { PassThrough } from "node:stream";
import { TerminalTui } from "../src/terminal-tui.ts";

function createMockTerminal(cols = 80, rows = 24, isTTY = true) {
  let outputBuffer = "";
  const stdout = new PassThrough() as any;
  const stdin = new PassThrough() as any;

  stdout.columns = cols;
  stdout.rows = rows;
  stdout.isTTY = isTTY;
  stdin.isTTY = isTTY;

  stdout.on("data", (chunk: any) => {
    outputBuffer += chunk.toString();
  });

  const tui = new TerminalTui({
    stdout,
    stdin,
    maxEntries: 10,
    maxBufferBytes: 5000,
  });

  return {
    tui,
    stdout,
    stdin,
    getOutput: () => outputBuffer,
  };
}

test("TerminalTui: detects interactive environment correctly", () => {
  const interactiveTerm = createMockTerminal(80, 24, true);
  assert.equal(interactiveTerm.tui.isInteractive(), true);

  const nonInteractiveTerm = createMockTerminal(80, 24, false);
  assert.equal(nonInteractiveTerm.tui.isInteractive(), false);
});

test("TerminalTui: manages alternate screen buffer in interactive mode", () => {
  const { tui, getOutput } = createMockTerminal(80, 24, true);

  tui.enterAlternateBuffer();
  assert.match(getOutput(), /\x1b\[\?1049h/);

  tui.exitAlternateBuffer();
  assert.match(getOutput(), /\x1b\[\?1049l/);
});

test("TerminalTui: renders geometry warning when terminal is below 60x15", () => {
  const { tui } = createMockTerminal(50, 10, true);
  const frame = tui.renderFrame();
  assert.match(frame, /Terminal insuficiente \(50x10\)\. Mínimo exigido: 60x15\./);
});

test("TerminalTui: toggle logs switches state and replays retained logs retrospectively", () => {
  const { tui } = createMockTerminal(80, 24, true);

  tui.setHeader({
    project: "cbm-core",
    workspace: "C:/Projects/cbm",
    provider: "CODEX",
    model: "gpt-6.1-sol",
    effort: "low",
  });

  tui.addMessage("Olá operador! Iniciando ciclo autônomo.");
  tui.addLog("engine", "Verificando lock de concorrência...", "info");
  tui.addLog("mcp", "Sondando servidor MCP e geração do grafo", "info");
  tui.addMessage("Passo 1 concluído com sucesso.");

  // Default: showLogs is false
  assert.equal(tui.getShowLogs(), false);
  let frame = tui.renderFrame();
  assert.match(frame, /\[Logs: OFF\]/);
  assert.match(frame, /Olá operador! Iniciando ciclo autônomo\./);
  assert.match(frame, /Passo 1 concluído com sucesso\./);
  // Logs should not appear in frame
  assert.doesNotMatch(frame, /\[cbm:engine\]/);
  assert.doesNotMatch(frame, /\[cbm:mcp\]/);

  // Toggle ON
  const newState = tui.toggleLogs();
  assert.equal(newState, true);
  assert.equal(tui.getShowLogs(), true);

  frame = tui.renderFrame();
  assert.match(frame, /\[Logs: ON\]/);
  // Logs should now appear retrospectively interleaved with dim style
  assert.match(frame, /\[cbm:engine\] Verificando lock de concorrência\.\.\./);
  assert.match(frame, /\[cbm:mcp\] Sondando servidor MCP e geração do grafo/);
  assert.match(frame, /\x1b\[2m/); // ANSI dim code

  // Toggle OFF again
  tui.toggleLogs();
  assert.equal(tui.getShowLogs(), false);
  frame = tui.renderFrame();
  assert.match(frame, /\[Logs: OFF\]/);
  assert.doesNotMatch(frame, /\[cbm:engine\]/);
});

test("TerminalTui: critical errors remain visible even when logs are OFF", () => {
  const { tui } = createMockTerminal(80, 24, true);

  tui.addLog("lock", "Lock já adquirido por outro processo!", "error");

  const frame = tui.renderFrame();
  assert.equal(tui.getShowLogs(), false);
  // Critical error log must be displayed
  assert.match(frame, /\[cbm:lock\] Lock já adquirido por outro processo!/);
});

test("TerminalTui: memory bounding evicts old entries and truncates massive single lines", () => {
  const { tui } = createMockTerminal(80, 24, true);

  // Add 15 entries (maxEntries is 10)
  for (let i = 1; i <= 15; i++) {
    tui.addLog("engine", `Evento de teste #${i}`);
  }

  assert.equal(tui.getEntries().length, 10);
  assert.equal(tui.getEvictedCount(), 5);

  tui.setShowLogs(true);
  const frame = tui.renderFrame();
  assert.match(frame, /5 eventos anteriores descartados por limite de memória/);

  // Test truncation of massive string (>10 KB)
  const hugeText = "A".repeat(15000);
  tui.addLog("engine", hugeText);
  const lastEntry = tui.getEntries()[tui.getEntries().length - 1];
  assert.match(lastEntry.content, /\[truncado por limite de tamanho\]/);
  assert.ok(lastEntry.content.length <= 11000);
});

test("TerminalTui: NO_COLOR disables ANSI escape codes while preserving textual prefixes", () => {
  const stdout = new PassThrough() as any;
  const stdin = new PassThrough() as any;
  stdout.columns = 80;
  stdout.rows = 24;
  stdout.isTTY = true;
  stdin.isTTY = true;

  const tui = new TerminalTui({
    stdout,
    stdin,
    noColor: true,
  });

  tui.setShowLogs(true);
  tui.addLog("token", "Contagem de tokens: 1.250");

  const frame = tui.renderFrame();
  assert.match(frame, /\[cbm:token\] Contagem de tokens: 1\.250/);
  // Must NOT contain ANSI dim escape code \x1b[2m
  assert.doesNotMatch(frame, /\x1b\[2m/);
});

test("TerminalTui: scrolling up pauses auto-scroll and displays pause indicator", () => {
  const { tui } = createMockTerminal(80, 24, true);

  for (let i = 1; i <= 30; i++) {
    tui.addMessage(`Mensagem linha #${i}`);
  }

  assert.equal(tui.getScrollOffset(), 0);
  let frame = tui.renderFrame();
  assert.doesNotMatch(frame, /\[HISTÓRICO PAUSADO\]/);

  // Scroll up
  tui.scrollUp(10);
  assert.equal(tui.getScrollOffset(), 10);
  frame = tui.renderFrame();
  assert.match(frame, /\[HISTÓRICO PAUSADO\]/);

  // Scroll back to bottom
  tui.scrollToBottom();
  assert.equal(tui.getScrollOffset(), 0);
  frame = tui.renderFrame();
  assert.doesNotMatch(frame, /\[HISTÓRICO PAUSADO\]/);
});
