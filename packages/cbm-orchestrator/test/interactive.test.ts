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

  output.on("data", (chunk) => {
    outputBuffer += chunk.toString();
    if (queue.length > 0) {
      const next = queue.shift()!;
      setTimeout(() => input.write(next + "\n"), 5);
    }
  });

  const session = new InteractiveSession(app, input, output, undefined, discoverFn);
  const startPromise = session.start().finally(() => {
    input.end();
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
