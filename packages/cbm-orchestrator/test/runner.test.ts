import test from "node:test";
import assert from "node:assert/strict";
import {
  parseCodexTurnUsage,
  calculateEstimatedCostUsd,
  killProcessTree,
} from "../src/runner.ts";
import { spawn } from "node:child_process";

test("Runner / Q04: extração de métricas de tokens a partir de evento JSONL do Codex", () => {
  const jsonlLine = JSON.stringify({
    type: "turn.completed",
    usage: {
      input_tokens: 24763,
      cached_input_tokens: 24448,
      output_tokens: 122,
      reasoning_output_tokens: 0,
    },
  });

  const usage = parseCodexTurnUsage(jsonlLine);
  assert.ok(usage);
  assert.equal(usage.inputTokens, 24763);
  assert.equal(usage.cachedInputTokens, 24448);
  assert.equal(usage.outputTokens, 122);
  assert.equal(usage.totalTokens, 24885); // input + output

  // Linhas que não são turn.completed devem retornar null
  assert.equal(parseCodexTurnUsage('{"type":"message.delta"}'), null);
  assert.equal(parseCodexTurnUsage("non-json output"), null);
});

test("Runner / Q04: cálculo de custo estimado em USD", () => {
  const usage = {
    inputTokens: 100000,
    cachedInputTokens: 50000,
    outputTokens: 10000,
    totalTokens: 110000,
  };

  // Tarifas ilustrativas: Input $2.50/M, Cached $1.25/M, Output $10.00/M
  // Custo: (50000 * 2.50 + 50000 * 1.25 + 10000 * 10.00) / 1_000_000
  // = (125000 + 62500 + 100000) / 1_000_000 = 287500 / 1_000_000 = $0.2875
  const cost = calculateEstimatedCostUsd(usage, {
    inputPerMillion: 2.5,
    cachedPerMillion: 1.25,
    outputPerMillion: 10.0,
  });

  assert.equal(cost, 0.2875);
});

test("Runner / ADR-005 (Critério 3): cancelamento de processo e árvore via taskkill", async () => {
  // Dispara um subprocesso que fica em loop aguardando (ex: timeout ou powershell sleep)
  const child = spawn("powershell", ["-Command", "Start-Sleep -Seconds 30"], {
    windowsHide: true,
    stdio: "ignore",
  });

  assert.ok(child.pid);

  // Executa o cancelamento da árvore de processos
  const killResult = await killProcessTree(child.pid);
  assert.equal(killResult.killed, true);

  // Aguarda confirmação de encerramento do processo
  await new Promise((resolve) => setTimeout(resolve, 500));

  // Verifica que o processo não está mais rodando
  let isRunning = true;
  try {
    // Process.kill com sinal 0 testa existência do processo
    process.kill(child.pid, 0);
  } catch {
    isRunning = false;
  }

  assert.equal(isRunning, false, "Processo filho deve ter sido terminado pelo taskkill");
});
