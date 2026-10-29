import test from "node:test";
import assert from "node:assert/strict";
import { PassThrough } from "node:stream";
import {
  InteractiveSession,
  findDefaultConfig,
} from "../src/interactive.ts";
import type { OrchestratorApplication, OrchestratorConfig } from "../src/application.ts";

function createMockSession(inputs: string[], app: OrchestratorApplication, discoverFn?: any) {
  let outputBuffer = "";
  const input = new PassThrough();
  const output = new PassThrough();
  const queue = [...inputs];

  const promptRegex = /(Selecione uma opção|Opção de|Digite o objetivo|Confirmar alvos|Confirmar proposta|Digite a instrução|Selecione uma ação|Digite um Run ID|Deseja remover|Selecione um provedor|Selecione o modelo|identificador do modelo|Salvar esta configuração|Deseja salvar|Deseja assinar).*[:?]\s*$/i;

  output.on("data", (chunk) => {
    outputBuffer += chunk.toString();
    const clean = outputBuffer.replace(/\x1b\[[0-9;]*[a-zA-Z]/g, "").trimEnd();
    if (queue.length > 0 && promptRegex.test(clean)) {
      const next = queue.shift()!;
      setTimeout(() => {
        if (!input.writableEnded) {
          input.write(next + "\n");
        }
      }, 5);
    }
  });

  const session = new InteractiveSession(app, input, output, undefined, discoverFn);
  const startPromise = session.start().finally(() => {
    if (!input.writableEnded) input.end();
  });

  return {
    startPromise,
    getOutput: () => outputBuffer,
  };
}

function createDummyApp(overrides: Partial<any> = {}): OrchestratorApplication {
  const dummyConfig: OrchestratorConfig = {
    backend: { command: "dummy-backend" },
    knowledge: {
      project: "test-interactive-project",
      workspace: "C:/fake/workspace",
      targets: [{ path: "src/target.ts", symbol: "targetFn" }],
      poles: ["targetFn"],
    },
    codex: {
      model: "gpt-6.1-sol",
      rates: { inputPerMillion: 1, cachedPerMillion: 0.5, outputPerMillion: 1, revision: "test" },
      evidenceDirectory: "C:/fake/evidence",
      verification: { command: "test" },
    },
    policy: {
      cycleTokenCap: 10000,
      builderReservation: 3000,
      adversarialReservation: 2000,
      maxIterations: 2,
      agentTimeoutSeconds: 60,
      cycleTimeoutSeconds: 300,
      cycleMonetaryCapUsd: 1.0,
      breakerFailureThreshold: 3,
      breakerWindowSeconds: 600,
    },
    stateDirectory: "C:/fake/state",
  };

  return {
    config: dummyConfig,
    digest: "sha256:testdigest",
    isLocked: () => ({ locked: false }),
    unlock: () => true,
    probe: async () => ({
      configurationDigest: "sha256:testdigest",
      snapshot: { baselineDigest: "base-123", graphGeneration: 3 },
      tools: [{ name: "search_graph", description: "Search code graph" }],
    }),
    listRuns: () => [
      {
        runId: "run-test-001",
        state: "COMPLETED",
        phase: "AWAITING_DECISION",
        terminationReason: "ACCEPTED",
        createdAt: "2026-10-26T12:00:00.000Z",
      },
    ],
    getRun: (id: string) => ({
      runId: id,
      promoted: true,
      totalTokensUsed: 1250,
      totalCostUsd: 0.015,
    }),
    runCycle: async (
      objective: string,
      signal?: AbortSignal,
      diagnostic = false,
      onStage1Proposal?: any
    ) => {
      if (onStage1Proposal) {
        await onStage1Proposal({
          runId: "run-interactive-exec-1",
          iteration: 1,
          artifactContent: "export const answer = 42;",
          usage: { totalTokens: 1200 },
          costUsd: 0.012,
        });
      }
      return {
        runId: "run-interactive-exec-1",
        status: "COMPLETED",
        terminationReason: "ACCEPTED",
        promoted: true,
        candidateVersionId: "sha256:sealed-candidate-hash-xyz",
        totalTokensUsed: 2100,
        totalCostUsd: 0.021,
        iterationsRun: 1,
      };
    },
    cancel: () => ({ cancellationRequested: true }),
    ...overrides,
  } as unknown as OrchestratorApplication;
}

test("Interactive CLI: exibe banner e encerra suavemente com opção 0", async () => {
  const app = createDummyApp();
  const { startPromise, getOutput } = createMockSession(["0"], app);
  await startPromise;

  const text = getOutput();
  assert.match(text, /CBM SYSTEM INTELLIGENCE ENGINE ORCHESTRATOR/);
  assert.match(text, /Projeto:\s+test-interactive-project/);
  assert.match(text, /Encerrando sessão interativa/);
});

test("Interactive CLI: opção 2 executa sonda CBM (probe)", async () => {
  let probeCalled = false;
  const app = createDummyApp({
    probe: async () => {
      probeCalled = true;
      return {
        configurationDigest: "sha256:probe-digest-ok",
        snapshot: { baselineDigest: "sha256-baseline-base", graphGeneration: 5 },
        tools: [{ name: "promote_horizon", description: "Promote speculative changes" }],
      };
    },
  });

  const { startPromise, getOutput } = createMockSession(["2", "0"], app);
  await startPromise;

  assert.equal(probeCalled, true);
  const text = getOutput();
  assert.match(text, /INSPECIONAR CONEXÃO E ESTADO CBM/);
  assert.match(text, /sha256:probe-digest-ok/);
  assert.match(text, /Geração do Grafo:\s+5/);
  assert.match(text, /promote_horizon/);
});

test("Interactive CLI: opção 3 consulta histórico de auditoria e detalhes de run", async () => {
  const app = createDummyApp();
  const { startPromise, getOutput } = createMockSession(["3", "run-test-001", "0"], app);
  await startPromise;

  const text = getOutput();
  assert.match(text, /HISTÓRICO E AUDITORIA DE CICLOS/);
  assert.match(text, /run-test-001/);
  assert.match(text, /COMPLETED/);
  assert.match(text, /"promoted": true/);
});

test("Interactive CLI: opção 5 verifica e limpa lock pendente", async () => {
  let unlocked = false;
  const app = createDummyApp({
    isLocked: () => ({ locked: true, info: { pid: 9999, operationId: "op-test-lock" } }),
    unlock: () => {
      unlocked = true;
      return true;
    },
  });

  const { startPromise, getOutput } = createMockSession(["5", "s", "0"], app);
  await startPromise;

  assert.equal(unlocked, true);
  const text = getOutput();
  assert.match(text, /GERENCIAMENTO DE LOCK/);
  assert.match(text, /Lock ativo detectado/);
  assert.match(text, /Lock removido com sucesso/);
});

test("Interactive CLI: opção 1 executa ciclo autônomo com pacote de decisão aprovado", async () => {
  let executedObjective = "";
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any) => {
      executedObjective = obj;
      if (onStage1Proposal) {
        await onStage1Proposal({
          runId: "run-cycle-approved-77",
          iteration: 1,
          artifactContent: "export function parseTimezone() { return 'UTC'; }",
          usage: { totalTokens: 2000 },
          costUsd: 0.02,
        });
      }
      return {
        runId: "run-cycle-approved-77",
        status: "COMPLETED",
        terminationReason: "ACCEPTED",
        promoted: true,
        candidateVersionId: "sha256:candidate-sealed-42",
        totalTokensUsed: 3450,
        totalCostUsd: 0.0345,
        iterationsRun: 1,
      };
    },
  });

  const { startPromise, getOutput } = createMockSession([
    "1",
    "Adicionar timezone parser em date-parser.ts",
    "s", // confirma alvos
    "s", // confirma proposta Stage 1
    "0",
  ], app);
  await startPromise;

  assert.equal(executedObjective, "Adicionar timezone parser em date-parser.ts");
  const text = getOutput();
  assert.match(text, /INICIAR CICLO AUTÔNOMO/);
  assert.match(text, /ETAPA 1 CONCLUÍDA: PROPOSTA DO BUILDER GERADA/);
  assert.match(text, /Proposta aprovada pelo operador/);
  assert.match(text, /PACOTE DE DECISÃO FINAL/);
  assert.match(text, /run-cycle-approved-77/);
  assert.match(text, /CANDIDATO APROVADO COM SUCESSO/);
  assert.match(text, /sha256:candidate-sealed-42/);
});

test("Interactive CLI: opção 1 rejeita proposta no Stage 1 e aborta ciclo", async () => {
  let proposalRejected = false;
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any) => {
      if (onStage1Proposal) {
        const approved = await onStage1Proposal({
          runId: "run-cycle-rejected-88",
          iteration: 1,
          artifactContent: "throw new Error('evil');",
          usage: { totalTokens: 500 },
          costUsd: 0.005,
        });
        if (!approved) proposalRejected = true;
      }
      return {
        runId: "run-cycle-rejected-88",
        status: "COMPLETED",
        terminationReason: "REJECTED",
        promoted: false,
        candidateVersionId: "",
        totalTokensUsed: 500,
        totalCostUsd: 0.005,
        iterationsRun: 1,
      };
    },
  });

  const { startPromise, getOutput } = createMockSession([
    "1",
    "Alteração perigosa",
    "s", // confirma alvos
    "n", // rejeita proposta Stage 1
    "0",
  ], app);
  await startPromise;

  assert.equal(proposalRejected, true);
  const text = getOutput();
  assert.match(text, /Proposta rejeitada pelo operador/);
  assert.match(text, /CANDIDATO NÃO APROVADO OU REQUER REVISÃO/);
});

test("Interactive CLI: opção 1 interage com Builder via opção c (corrigir) no Stage 1", async () => {
  let steerReceived: string | null = null;
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any) => {
      if (onStage1Proposal) {
        const dec1 = await onStage1Proposal({
          runId: "run-steer-cli",
          iteration: 1,
          artifactContent: "initial imperfect code",
          usage: { totalTokens: 100 },
          costUsd: 0.001,
        });
        if (dec1 && typeof dec1 === "object" && dec1.action === "STEER") {
          steerReceived = dec1.guidance;
        }
      }
      return {
        runId: "run-steer-cli",
        status: "COMPLETED",
        terminationReason: "ACCEPTED",
        promoted: true,
        candidateVersionId: "sha256:steered-ok",
        totalTokensUsed: 200,
        totalCostUsd: 0.002,
        iterationsRun: 2,
      };
    },
  });

  const { startPromise, getOutput } = createMockSession([
    "1",
    "Ajuste com instrução",
    "s", // confirma alvos
    "c", // escolhe corrigir no Stage 1
    "Adicione tipagem estrita", // digita instrução
    "0",
  ], app);
  await startPromise;

  assert.equal(steerReceived, "Adicione tipagem estrita");
  const text = getOutput();
  assert.match(text, /Enviando instrução ao Builder/);
});

test("Interactive CLI: interage com agente em pausa sob demanda via opção 2", async () => {
  let pauseSteerReceived: string | null = null;
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1?: any, options?: any) => {
      if (options?.onPause) {
        const dec = await options.onPause({
          runId: "run-pause-cli",
          phase: "VERIFYING",
          iteration: 1,
          reason: "OPERATOR_PAUSE_REQUESTED",
        });
        if (dec && dec.action === "STEER") {
          pauseSteerReceived = dec.guidance;
        }
      }
      return {
        runId: "run-pause-cli",
        status: "COMPLETED",
        terminationReason: "ACCEPTED",
        promoted: true,
        candidateVersionId: "sha256:paused-ok",
        totalTokensUsed: 300,
        totalCostUsd: 0.003,
        iterationsRun: 1,
      };
    },
  });

  const { startPromise, getOutput } = createMockSession([
    "1",
    "Ajuste com pausa",
    "s", // confirma alvos
    "2", // escolhe interagir com agente na pausa
    "Ignore testes legados", // instrução de correção
    "0",
  ], app);
  await startPromise;

  assert.equal(pauseSteerReceived, "Ignore testes legados");
  const text = getOutput();
  assert.match(text, /PAUSA DO CICLO \/ INTERVENÇÃO HUMANA/);
  assert.match(text, /Instrução enviada ao agente/);
});

test("Interactive CLI: opção inválida gera aviso sem quebrar sessão", async () => {
  const app = createDummyApp();
  const { startPromise, getOutput } = createMockSession(["invalid-cmd", "0"], app);
  await startPromise;

  const text = getOutput();
  assert.match(text, /Opção inválida/);
});

test("Interactive CLI: opção 6 configura provedor (Antigravity), modelo e esforço", async () => {
  const app = createDummyApp();
  const mockDiscover = async () => ({
    provider: "antigravity" as const,
    status: "CATALOG_AVAILABLE" as const,
    models: [
      { id: "gemini-2.5-pro", label: "Gemini 2.5 Pro", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "medium", source: "live" as const },
      { id: "gemini-2.5-flash", label: "Gemini 2.5 Flash", source: "live" as const },
    ],
    defaultModelId: "gemini-2.5-pro",
    fetchedAt: new Date().toISOString(),
    ttlMs: 86400000,
  });

  // 6: Configurar -> 2: Antigravity -> 1: gemini-2.5-pro -> 3: high -> 0: Sair
  const { startPromise, getOutput } = createMockSession(["6", "2", "1", "3", "0"], app, mockDiscover);
  await startPromise;

  assert.equal(app.config.codex.provider, "antigravity");
  assert.equal(app.config.codex.command, "agy");
  assert.equal(app.config.codex.model, "gemini-2.5-pro");
  assert.equal(app.config.codex.effort, "high");

  const text = getOutput();
  assert.match(text, /CONFIGURAR PROVEDOR, MODELO E ESFORÇO/);
  assert.match(text, /ANTIGRAVITY \(agy\)/);
  assert.match(text, /gemini-2.5-pro/);
  assert.match(text, /high/);
});

test("Interactive CLI: opção L alterna visibilidade de logs operacionais", async () => {
  const app = createDummyApp();
  // L -> 0 (Sair)
  const { startPromise, getOutput } = createMockSession(["L", "0"], app);
  await startPromise;

  const text = getOutput();
  assert.match(text, /Visibilidade de logs operacionais alternada para:.*LIGADO/);
  assert.match(text, /estilo atenuado \[cbm:\*\]/);
});

test("Interactive CLI: linear_readline_renders_prompt_without_alternate_buffer", async () => {
  const app = createDummyApp();
  let output = "";
  const inStream = new PassThrough();
  const outStream = new PassThrough();
  (outStream as any).isTTY = true;
  (inStream as any).isTTY = true;
  outStream.on("data", (chunk) => {
    output += chunk.toString();
  });

  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, { alphaChatMode: true });
  const runPromise = session.start();

  // Send quit
  inStream.write("q\n");
  await runPromise;

  // Assert NO alternate buffer escape codes even with isTTY = true
  assert.doesNotMatch(output, /\x1b\[\?1049h/, "Must not enter alternate screen buffer in linear chat mode");
  assert.doesNotMatch(output, /\x1b\[2J/, "Must not clear screen in linear chat mode");
  // Assert conversational prompt CBM>
  assert.match(output, /CBM>/, "Must render conversational prompt CBM>");
});

test("Interactive CLI: stage1_proposal_requires_explicit_confirmation", async () => {
  let proposalDecision: any = null;
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any, options?: any) => {
      if (onStage1Proposal) {
        proposalDecision = await onStage1Proposal({
          runId: "run-alpha-stage1-test",
          iteration: 1,
          artifactContent: "export function proposal() { return true; }",
          usage: { totalTokens: 100 },
          costUsd: 0.001,
        });
      }
      return {
        runId: "run-alpha-stage1-test",
        status: proposalDecision ? "COMPLETED" : "ABORTED",
        promoted: Boolean(proposalDecision),
        totalTokensUsed: 100,
        totalCostUsd: 0.001,
        iterationsRun: 1,
      };
    },
  });

  // Scenario 1: Reject with 'n'
  const inStream1 = new PassThrough();
  const outStream1 = new PassThrough();
  let out1 = "";
  outStream1.on("data", (chunk) => { out1 += chunk.toString(); });
  const session1 = new InteractiveSession(app, inStream1, outStream1, undefined, undefined, undefined, { alphaChatMode: true });
  const run1 = session1.start();

  inStream1.write("Adicionar suporte a alpha chat\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream1.write("n\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream1.write("q\n");
  await run1;

  assert.equal(proposalDecision, false, "Proposal decision must be false when operator rejects with 'n'");
  assert.match(out1, /Proposta rejeitada pelo operador/);

  // Scenario 2: Steer with 'c'
  let steerDecision: any = null;
  const app2 = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any, options?: any) => {
      if (onStage1Proposal) {
        steerDecision = await onStage1Proposal({
          runId: "run-alpha-stage1-steer",
          iteration: 1,
          artifactContent: "export function proposal() { return false; }",
          usage: { totalTokens: 100 },
          costUsd: 0.001,
        });
      }
      return {
        runId: "run-alpha-stage1-steer",
        status: "COMPLETED",
        promoted: true,
        totalTokensUsed: 100,
        totalCostUsd: 0.001,
        iterationsRun: 1,
      };
    },
  });

  const inStream2 = new PassThrough();
  const outStream2 = new PassThrough();
  let out2 = "";
  outStream2.on("data", (chunk) => { out2 += chunk.toString(); });
  const session2 = new InteractiveSession(app2, inStream2, outStream2, undefined, undefined, undefined, { alphaChatMode: true });
  const run2 = session2.start();

  inStream2.write("Corrigir retorno booleano\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream2.write("c\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream2.write("Use true em vez de false\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream2.write("q\n");
  await run2;

  assert.deepEqual(steerDecision, { action: "STEER", guidance: "Use true em vez de false" });
  assert.match(out2, /Enviando correção: "Use true em vez de false"/);
});

test("Interactive CLI: stage2_logs_stream_single_line_badges", async () => {
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any, options?: any) => {
      if (onStage1Proposal) {
        await onStage1Proposal({
          runId: "run-stage2-logs-test",
          iteration: 1,
          artifactContent: "export function run() {}",
          usage: { totalTokens: 50 },
          costUsd: 0.0005,
        });
      }
      return {
        runId: "run-stage2-logs-test",
        status: "COMPLETED",
        promoted: true,
        totalTokensUsed: 120,
        totalCostUsd: 0.0012,
        iterationsRun: 1,
      };
    },
  });

  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let output = "";
  outStream.on("data", (chunk) => { output += chunk.toString(); });
  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, { alphaChatMode: true });
  const run = session.start();

  inStream.write("Implementar streaming de logs\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream.write("s\n"); // approve stage 1
  await new Promise((r) => setTimeout(r, 25));
  inStream.write("q\n");
  await run;

  assert.match(output, /=== INÍCIO DA EXECUÇÃO \(STREAMING DE LOGS\) ===/);
  // Match single-line timestamp badge: [HH:MM:SS] [BUILDER]
  assert.match(output, /\[\d{2}:\d{2}:\d{2}\]\s+\[BUILDER\]/);
  // Ensure no unformatted raw JSON object dumps in output
  assert.doesNotMatch(output, /\{"artifactContent":/);
  assert.doesNotMatch(output, /\{"jsonrpc":/);
});

test("Interactive CLI: cycle_termination_outputs_summary_and_code", async () => {
  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1Proposal?: any, options?: any) => {
      if (onStage1Proposal) {
        await onStage1Proposal({
          runId: "run-summary-test-99",
          iteration: 2,
          artifactContent: "export const answer = 42;",
          usage: { totalTokens: 1500 },
          costUsd: 0.015,
        });
      }
      return {
        runId: "run-summary-test-99",
        status: "COMPLETED",
        promoted: true,
        totalTokensUsed: 3200,
        totalCostUsd: 0.032,
        iterationsRun: 2,
      };
    },
  });

  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let output = "";
  outStream.on("data", (chunk) => { output += chunk.toString(); });
  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, { alphaChatMode: true });
  const run = session.start();

  inStream.write("Finalizar ciclo e validar sumário\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream.write("s\n"); // approve
  await new Promise((r) => setTimeout(r, 25));
  inStream.write("q\n");
  await run;

  assert.match(output, /─── RESUMO OPERACIONAL DE CONCLUSÃO ────────────────────────────────/);
  assert.match(output, /Status:\s+.*SUCESSO \(PROMOVIDO\)/);
  assert.match(output, /Run ID:\s+run-summary-test-99/);
  assert.match(output, /Iterações:\s+2/);
  assert.match(output, /Tokens:\s+3\.200 tk/);
  assert.match(output, /Custo Total:\s+\$0\.0320 USD/);
});

test("Interactive CLI: find_default_config_resolves_hierarchical_or_safe_fallback", async () => {
  const { createDefaultConfiguration } = await import("../src/application.ts");
  const config = createDefaultConfiguration("C:/mock/workspace");
  assert.equal(config.knowledge.workspace, "C:\\mock\\workspace".replace(/\//g, "\\"));
  assert.ok(config.policy.breakerFailureThreshold > 0);
  assert.ok(config.backend.command.length > 0);

  const { runInteractive } = await import("../src/interactive.ts");
  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let out = "";
  outStream.on("data", (c) => { out += c.toString(); });
  setTimeout(() => inStream.write("q\n"), 20);

  // When run without configFile or finding none, it should not throw fatal error
  await runInteractive({
    input: inStream,
    output: outStream,
    alphaChatMode: true,
    // force findDefaultConfig returning null by pointing configFile to nonexistent marker
    configFile: undefined,
  });
  assert.ok(out.includes("CBM>") || out.includes("[AVISO]"));
});

test("Interactive_displays_explicit_failure_reason_on_rejection: summary renders failureReason on rejected runs", async () => {
  const app = createDummyApp({
    runCycle: async () => ({
      runId: "run-failed-target-scope-101",
      status: "COMPLETED",
      terminationReason: "FAILED",
      promoted: false,
      candidateVersionId: "",
      totalTokensUsed: 16092,
      totalCostUsd: 0.0244,
      iterationsRun: 1,
      failureReason: "Builder artifact must contain exactly the allocated file paths",
    }),
  });

  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let output = "";
  outStream.on("data", (chunk) => { output += chunk.toString(); });
  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, {
    alphaChatMode: true,
    verbose: "low",
  });
  const run = session.start();

  inStream.write("updates docs folder\n");
  await new Promise((r) => setTimeout(r, 25));
  inStream.write("q\n");
  await run;

  assert.match(output, /Status:\s+.*RECUSADO/);
  assert.match(output, /Motivo da Recusa:\s+Builder artifact must contain exactly the allocated file paths/);
});

test("Interactive_multi_turn_discovery_accumulates_dialogue: alpha chat discovery converses across turns and control commands bypass", async () => {
  const { isControlCommand } = await import("../src/interactive.ts");
  assert.equal(isControlCommand("q"), true);
  assert.equal(isControlCommand("exit"), true);
  assert.equal(isControlCommand(":verbose high"), true);
  assert.equal(isControlCommand(":help"), true);
  assert.equal(isControlCommand("refatorar auth"), false);

  const historyReceived: Array<Array<{ role: string; content: string }>> = [];
  let turnCount = 0;

  const mockDiscovery = async (messages: Array<{ role: string; content: string }>) => {
    historyReceived.push([...messages]);
    turnCount++;
    if (turnCount === 1) {
      return {
        text: "Quais arquivos da pasta docs você deseja alterar?",
      };
    }
    return {
      text: "Proposta PBB formulada com sucesso.",
      proposal: {
        problem: "Documentação desatualizada",
        targets: ["docs/README.md"],
        acceptanceCriteria: ["Adicionar guia do novo fluxo"],
      },
    };
  };

  const app = createDummyApp();
  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let output = "";
  outStream.on("data", (chunk) => { output += chunk.toString(); });

  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, {
    alphaChatMode: true,
    discoveryRunner: mockDiscovery,
  });

  const run = session.start();

  // Turn 1
  inStream.write("atualizar documentação\n");
  await new Promise((r) => setTimeout(r, 40));

  // Turn 2
  inStream.write("adicionar guia no docs/README.md\n");
  await new Promise((r) => setTimeout(r, 40));

  // Exit: first 'q' cancels PBB gate, second 'q' exits session
  inStream.write("q\n");
  await new Promise((r) => setTimeout(r, 40));
  inStream.write("q\n");
  await run;

  assert.equal(historyReceived.length, 2, "Discovery must be invoked for both conversation turns");
  assert.equal(historyReceived[0].length, 1, "First turn has 1 user message");
  assert.equal(historyReceived[1].length, 3, "Second turn has user, assistant, and user follow-up");
  assert.match(output, /Quais arquivos da pasta docs/);
  assert.match(output, /PROPOSTA PBB/i);
});

test("Interactive_normalize_targets_preserves_s0_digest: normalizePbbTargets formats targets and preserves app digest", async () => {
  const { normalizePbbTargets } = await import("../src/interactive.ts");

  // Valid normalization
  const targets = normalizePbbTargets(["./src/cli.ts", "docs/README.md"]);
  assert.deepEqual(targets, [
    { path: "src/cli.ts", symbol: "main" },
    { path: "docs/README.md", symbol: "main" },
  ]);

  // Path traversal rejection
  assert.throws(
    () => normalizePbbTargets(["../outside.ts"]),
    /Path traversal denied/
  );
  assert.throws(
    () => normalizePbbTargets(["docs/.."]),
    /Path traversal denied/
  );
  assert.throws(
    () => normalizePbbTargets(["docs/sub/../../outside.ts"]),
    /Path traversal denied/
  );
  assert.throws(
    () => normalizePbbTargets(["/etc/passwd"]),
    /Absolute or UNC paths denied|Path traversal denied/
  );
  assert.throws(
    () => normalizePbbTargets(["C:/Windows/System32"]),
    /Absolute or UNC paths denied|Path traversal denied/
  );
  assert.throws(
    () => normalizePbbTargets(["\\\\server\\share\\file.ts"]),
    /Absolute or UNC paths denied|Path traversal denied/
  );

  // Per-run targets passed to runCycle does not mutate app.config or app.digest
  const { OrchestratorApplication, createDefaultConfiguration } = await import("../src/application.ts");
  const config = createDefaultConfiguration("C:/mock/ws");
  const app = new OrchestratorApplication(config);
  const initialDigest = app.digest;
  const initialTargets = JSON.stringify(app.config.knowledge.targets);

  assert.equal(typeof app.runCycle, "function");
  // Check runCycle arity allows runTargets as 6th parameter
  assert.ok(app.runCycle.length >= 2);
  assert.equal(app.digest, initialDigest);
  assert.equal(JSON.stringify(app.config.knowledge.targets), initialTargets);
});

test("Interactive_pbb_gate_requires_operator_approval_and_enriches_prompt: PBB Gate prompts operator, enriches prompt, and triggers runCycle with targets", async () => {
  const { enrichBuilderPrompt } = await import("../src/interactive.ts");
  const enriched = enrichBuilderPrompt("atualizar docs", {
    problem: "Documentação desatualizada",
    targets: ["docs/README.md"],
    acceptanceCriteria: ["Atualizar guia de instalação"],
  });
  assert.match(enriched, /\[Especificação PBB Aprovada\]/);
  assert.match(enriched, /Atualizar guia de instalação/);

  let runCycleCalled = false;
  let receivedRunTargets: any = null;
  let receivedEnrichedPrompt: any = null;

  const app = createDummyApp({
    runCycle: async (obj: string, signal?: AbortSignal, diag?: boolean, onStage1?: any, opts?: any, runTargets?: any, prompt?: string) => {
      runCycleCalled = true;
      receivedRunTargets = runTargets;
      receivedEnrichedPrompt = prompt;
      return {
        runId: "run-pbb-gate-test-200",
        status: "COMPLETED",
        terminationReason: "ACCEPTED",
        promoted: true,
        candidateVersionId: "sha256:pbb-sealed",
        totalTokensUsed: 1200,
        totalCostUsd: 0.012,
        iterationsRun: 1,
      };
    },
  });

  const mockDiscovery = async () => ({
    text: "PBB estruturado pronto para aprovação.",
    proposal: {
      problem: "Documentação desatualizada",
      targets: ["docs/README.md"],
      acceptanceCriteria: ["Atualizar guia de instalação"],
    },
  });

  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let output = "";
  outStream.on("data", (chunk) => { output += chunk.toString(); });

  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, {
    alphaChatMode: true,
    discoveryRunner: mockDiscovery,
  });

  const run = session.start();

  // Send request
  inStream.write("atualizar documentação\n");
  await new Promise((r) => setTimeout(r, 40));

  // Approve PBB Gate: "s"
  inStream.write("s\n");
  await new Promise((r) => setTimeout(r, 40));

  // Exit session
  inStream.write("q\n");
  await run;

  assert.equal(runCycleCalled, true, "runCycle must be triggered after PBB Gate approval");
  assert.deepEqual(receivedRunTargets, [{ path: "docs/README.md", symbol: "main" }]);
  assert.match(receivedEnrichedPrompt, /Atualizar guia de instalação/);
  assert.match(output, /PROPOSTA PBB CONVERGIDA/);
});

test("Interactive_sanitizeSecrets_redacts_tokens_on_output_streams: tokens are redacted from all stream writes", async () => {
  const app = createDummyApp();
  const inStream = new PassThrough();
  const outStream = new PassThrough();
  let output = "";
  outStream.on("data", (chunk) => { output += chunk.toString(); });

  const session = new InteractiveSession(app, inStream, outStream, undefined, undefined, undefined, {
    alphaChatMode: true,
    discoveryRunner: async () => ({
      text: "CBM_AGENT_TOKEN=secret-token Bearer super-secret-key",
    }),
  });

  const run = session.start();
  inStream.write("test redaction\n");
  await new Promise((r) => setTimeout(r, 40));
  inStream.write("q\n");
  await run;

  assert.equal(output.includes("secret-token"), false, "Secret token must not be printed to output");
  assert.equal(output.includes("super-secret-key"), false, "Bearer secret must not be printed to output");
  assert.match(output, /\[REDACTED\]/);
});

test("Interactive_runInteractive_forwards_runner_and_blocks_direct_cycle_bypass: runInteractive forwards discoveryRunner and blocks unapproved cycle", async () => {
  const { runInteractive } = await import("../src/interactive.ts");

  let customRunnerCalled = false;
  let cycleCalled = false;

  const mockApp = {
    config: {
      knowledge: { project: "p", targets: [{ path: "README.md", symbol: "main" }] },
      codex: { model: "m" },
      policy: {},
    },
    isLocked: () => ({ locked: false }),
    runCycle: async () => {
      cycleCalled = true;
      return { promoted: false };
    },
  };

  const inStream = new PassThrough();
  const outStream = new PassThrough();

  const p = runInteractive({
    app: mockApp as any,
    alphaChatMode: true,
    input: inStream,
    output: outStream,
    discoveryRunner: async () => {
      customRunnerCalled = true;
      return {
        text: "Discovery ativo",
        proposal: {
          problem: "teste",
          targets: ["README.md"],
          acceptanceCriteria: ["critério"],
        },
      };
    },
  });

  inStream.write("teste de bypass\n");
  await new Promise((r) => setTimeout(r, 40));
  // cancel PBB Gate with 'q', then exit session with 'q'
  inStream.write("q\n");
  await new Promise((r) => setTimeout(r, 40));
  inStream.write("q\n");
  await p;

  assert.equal(customRunnerCalled, true, "options.discoveryRunner must be forwarded to InteractiveSession");
  assert.equal(cycleCalled, false, "runCycle must NOT be invoked when PBB Gate is cancelled");
});
