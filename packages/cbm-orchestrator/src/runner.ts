import { execFile, spawn } from "node:child_process";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

export interface CodexUsage {
  inputTokens: number;
  cachedInputTokens: number;
  outputTokens: number;
  totalTokens: number;
}

export interface ModelRates {
  inputPerMillion: number;
  cachedPerMillion: number;
  outputPerMillion: number;
}

export interface AgentRunOptions {
  command: string;
  args: string[];
  inputPrompt: string;
  timeoutSeconds: number;
  cwd?: string;
  env?: NodeJS.ProcessEnv;
  signal?: AbortSignal;
  onSpawn?: (pid: number) => void | Promise<void>;
  onEvent?: (event: any) => void;
  maxOutputBytes?: number;
}

export interface AgentRunResult {
  exitCode: number | null;
  stdout: string;
  stderr: string;
  usage: CodexUsage | null;
  timedOut: boolean;
  cancelled?: boolean;
  error?: string;
}

/**
 * Faz o parsing de linhas JSONL emitidas pelo codex exec procurando o evento turn.completed.
 */
export function parseCodexTurnUsage(line: string): CodexUsage | null {
  try {
    const data = JSON.parse(line.trim());
    if (data.type === "turn.completed" && data.usage) {
      const input = Number(data.usage.input_tokens ?? 0);
      const cached = Number(data.usage.cached_input_tokens ?? 0);
      const output = Number(data.usage.output_tokens ?? 0);
      if (![input, cached, output].every((value) => Number.isSafeInteger(value) && value >= 0) || cached > input) return null;

      return {
        inputTokens: input,
        cachedInputTokens: cached,
        outputTokens: output,
        totalTokens: input + output,
      };
    }
  } catch {
    // Linha não é JSON válido ou não é turn.completed
  }
  return null;
}

/**
 * Calcula o custo estimado em USD a partir dos tokens medidos e tarifas por milhão.
 */
export function calculateEstimatedCostUsd(usage: CodexUsage, rates: ModelRates): number {
  const nonCachedInput = Math.max(0, usage.inputTokens - usage.cachedInputTokens);
  const cost =
    (nonCachedInput * rates.inputPerMillion +
      usage.cachedInputTokens * rates.cachedPerMillion +
      usage.outputTokens * rates.outputPerMillion) /
    1_000_000;

  return Number(cost.toFixed(6));
}

function isProcessAlive(pid: number): boolean {
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

/**
 * Encerra de forma confiável a árvore de processos de um agente no Windows via taskkill com verificação de vida (ADR-005 Critério 3).
 */
export async function killProcessTree(pid: number): Promise<{ killed: boolean; reason?: string }> {
  if (process.platform === "win32") {
    try {
      await execFileAsync("taskkill", ["/PID", String(pid), "/T", "/F"], { windowsHide: true });
    } catch (err: any) {
      // Ignora erro apenas se o processo já não existir
      if (isProcessAlive(pid)) {
        return { killed: false, reason: `taskkill failed: ${err.message}` };
      }
    }

    // Confirma que o processo de fato deixou de existir
    let attempts = 5;
    while (attempts > 0 && isProcessAlive(pid)) {
      await new Promise((r) => setTimeout(r, 100));
      attempts--;
    }

    const alive = isProcessAlive(pid);
    return {
      killed: !alive,
      reason: alive ? `Process ${pid} is still alive after taskkill /T /F` : undefined,
    };
  } else {
    try {
      process.kill(-pid, "SIGKILL");
    } catch {
      try {
        process.kill(pid, "SIGKILL");
      } catch {}
    }
    return { killed: !isProcessAlive(pid) };
  }
}

/**
 * Executa um processo de agente com supervisão ativa de timeout e coleta de tokens.
 */
export function executeAgentProcess(options: AgentRunOptions): Promise<AgentRunResult> {
  return new Promise((resolve) => {
    if (options.signal?.aborted) {
      resolve({ exitCode: null, stdout: "", stderr: "", usage: null, timedOut: false, cancelled: true });
      return;
    }
    let timedOut = false;
    let cancelled = false;
    let stdoutBuffer = "";
    let stderrBuffer = "";
    let usageFound: CodexUsage | null = null;
    let unparsedStdout = "";
    let resolved = false;
    let startupError: string | undefined;

    let forceCloseTimer: NodeJS.Timeout | undefined;

    const safeResolve = (res: AgentRunResult) => {
      if (!resolved) {
        resolved = true;
        clearTimeout(timeoutTimer);
        options.signal?.removeEventListener("abort", abort);
        if (forceCloseTimer) clearTimeout(forceCloseTimer);
        resolve(res);
      }
    };

    const child = spawn(options.command, options.args, {
      cwd: options.cwd,
      env: options.env,
      stdio: ["pipe", "pipe", "pipe"],
      windowsHide: true,
      detached: process.platform !== "win32",
    });
    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");

    const stop = async () => {
      if (child.pid) await killProcessTree(child.pid);
      if (resolved) return;
      forceCloseTimer = setTimeout(() => {
        safeResolve({ exitCode: -1, stdout: stdoutBuffer, stderr: stderrBuffer, usage: usageFound,
          timedOut, cancelled, error: "Process did not close after cancellation" });
      }, 2000);
    };
    const abort = () => { cancelled = true; void stop(); };
    options.signal?.addEventListener("abort", abort, { once: true });

    const timeoutTimer = setTimeout(async () => {
      timedOut = true;
      if (child.pid) {
        await killProcessTree(child.pid);
      }
      // Não trava indefinidamente se o processo zumbificar e nunca emitir close
      forceCloseTimer = setTimeout(() => {
        safeResolve({
          exitCode: -1,
          stdout: stdoutBuffer,
          stderr: stderrBuffer,
          usage: usageFound,
          timedOut: true,
          error: "Process timed out and did not close within grace period",
        });
      }, 2000);
    }, options.timeoutSeconds * 1000);

    if (child.stdin) {
      child.stdin.on("error", () => {}); // The process may exit before consuming its prompt.
      Promise.resolve().then(() => child.pid ? options.onSpawn?.(child.pid) : undefined)
        .then(() => { if (!resolved && !cancelled) child.stdin.end(options.inputPrompt); })
        .catch((error) => { startupError = (error as Error).message; cancelled = true; void stop(); });
    }

    child.stdout?.on("data", (text: string) => {
      stdoutBuffer += text;
      if (Buffer.byteLength(stdoutBuffer) > (options.maxOutputBytes ?? 8 * 1024 * 1024)) {
        cancelled = true;
        void stop();
        return;
      }

      // Preserva fragmentos de linha entre chunks JSONL
      unparsedStdout += text;
      const lines = unparsedStdout.split(/\r?\n/);
      unparsedStdout = lines.pop() ?? "";

      for (const line of lines) {
        if (!line.trim()) continue;
        const u = parseCodexTurnUsage(line);
        if (u) {
          usageFound = usageFound ? {
            inputTokens: usageFound.inputTokens + u.inputTokens,
            cachedInputTokens: usageFound.cachedInputTokens + u.cachedInputTokens,
            outputTokens: usageFound.outputTokens + u.outputTokens,
            totalTokens: usageFound.totalTokens + u.totalTokens,
          } : u;
        }
        try { options.onEvent?.(JSON.parse(line)); } catch {}
      }
    });

    child.stderr?.on("data", (chunk: string) => {
      stderrBuffer = (stderrBuffer + chunk).slice(-8000);
    });

    child.on("close", (code) => {
      if (unparsedStdout.trim()) {
        const u = parseCodexTurnUsage(unparsedStdout);
        if (u) usageFound = usageFound ? {
          inputTokens: usageFound.inputTokens + u.inputTokens,
          cachedInputTokens: usageFound.cachedInputTokens + u.cachedInputTokens,
          outputTokens: usageFound.outputTokens + u.outputTokens,
          totalTokens: usageFound.totalTokens + u.totalTokens,
        } : u;
        try { options.onEvent?.(JSON.parse(unparsedStdout)); } catch {}
      }

      safeResolve({
        exitCode: code,
        stdout: stdoutBuffer,
        stderr: stderrBuffer,
        usage: usageFound,
        timedOut,
        cancelled,
        error: startupError,
      });
    });

    child.on("error", (err) => {
      safeResolve({
        exitCode: -1,
        stdout: stdoutBuffer,
        stderr: stderrBuffer,
        usage: usageFound,
        timedOut,
        error: err.message,
      });
    });
  });
}
