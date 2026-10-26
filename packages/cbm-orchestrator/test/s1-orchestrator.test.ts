import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { SimulatedAgentRunner } from "./helpers/simulated-runner.ts";
import { Journal } from "../src/journal.ts";
import {
  CycleOrchestrator,
  type StartCycleRequest,
  type OrchestratorTelemetryQuery,
} from "../src/orchestrator.ts";
import { type CbmClientMock } from "../src/mcp-proxy.ts";

function getTempDbPath(): string {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "cbm-s1-test-"));
  return path.join(tmpDir, "s1.db");
}

test("S1 Orchestrator: ciclo autônomo completo com aprovação (PBI-01 a PBI-05, PBI-16, PBI-31)", async () => {
  const dbPath = getTempDbPath();

  let cbmPromoted = false;
  const mockCbm: CbmClientMock = {
    async callTool(tool, args) {
      if (tool === "index_status") {
        return { graphGeneration: 1, baselineDigest: "sha256-baseline-root", rulesRevision: "rules-s1-root", coverageRevision: "cov-s1-root" };
      }
      if (tool === "promote_horizon") {
        cbmPromoted = true;
        return { success: true, promoted: true };
      }
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, new SimulatedAgentRunner());

  const req: StartCycleRequest = {
    projectId: "cbm-multigraph-s1",
    horizonId: "horizon-s1-01",
    objective: "Implementar parser de eventos C06",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 20000,
      adversarialReservation: 10000,
      maxIterations: 2,
      agentTimeoutSeconds: 180,
      cycleTimeoutSeconds: 900,
      cycleMonetaryCapUsd: 2.0,
    },
    builderPrompt: "Crie o parser de eventos",
    builderIntentScope: [{ path: "src/parser.ts", operation: "write" }],
  };

  // Verificação de bloqueio pré-S0 (ER-06)
  await assert.rejects(async () => {
    await orchestrator.executeCycle(req);
  }, /Prerequisite S0_APPROVED gate required/i);

  const approval = new Journal(dbPath);
  approval.recordS0Approval(req.projectId, "fixture-only");
  approval.close();

  const cycleResult = await orchestrator.executeCycle(req);

  assert.equal(cycleResult.status, "COMPLETED");
  assert.equal(cycleResult.terminationReason, "ACCEPTED");
  assert.equal(cycleResult.promoted, true);
  assert.equal(cbmPromoted, true);
  assert.ok(cycleResult.candidateVersionId.startsWith("sha256:"));
  assert.ok(cycleResult.totalTokensUsed > 0);
  assert.ok(cycleResult.totalCostUsd >= 0);

  // Consulta de telemetria (OV-01, OV-02, OV-03)
  const query: OrchestratorTelemetryQuery = { runId: cycleResult.runId };
  const telemetry = orchestrator.queryTelemetry(query);
  assert.ok(telemetry);
  assert.equal(telemetry.runId, cycleResult.runId);
  assert.equal(telemetry.status, "COMPLETED");
  assert.equal(telemetry.terminationReason, "ACCEPTED");
  assert.equal(telemetry.confirmedTokens, cycleResult.totalTokensUsed);

  orchestrator.close();
});

test("S1 Orchestrator: rejeição após atingir teto de iterações", async () => {
  const dbPath = getTempDbPath();

  const mockCbm: CbmClientMock = {
    async callTool(tool) {
      if (tool === "index_status") return { graphGeneration: 1, baselineDigest: "fixture-baseline", rulesRevision: "rules-s1-root", coverageRevision: "cov-s1-root" };
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, new SimulatedAgentRunner({ simulateAlwaysRed: true }));

  // Força simulação de verificador RED em todas as iterações
  const req: StartCycleRequest = {
    projectId: "cbm-multigraph-s1",
    horizonId: "horizon-s1-breaker",
    objective: "Tarefa que falha repetidamente",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 20000,
      adversarialReservation: 10000,
      maxIterations: 2,
      agentTimeoutSeconds: 180,
      cycleTimeoutSeconds: 900,
      cycleMonetaryCapUsd: 2.0,
    },
    builderPrompt: "Código com erro recorrente",
    builderIntentScope: [{ path: "src/broken.ts", operation: "write" }],
  };

  const approval = new Journal(dbPath);
  approval.recordS0Approval(req.projectId, "fixture-only");
  approval.close();
  const cycleResult = await orchestrator.executeCycle(req);

  // Deve parar no teto de iterações com REJECTED
  assert.equal(cycleResult.status, "COMPLETED");
  assert.equal(cycleResult.terminationReason, "REJECTED");
  assert.equal(cycleResult.promoted, false);
  assert.equal(cycleResult.iterationsRun, 2);

  orchestrator.close();
});

test("S1 Orchestrator: diagnóstico de crash pré-F05 sem retomada automática (ER-10 / PBI-20)", async () => {
  const dbPath = getTempDbPath();
  const mockCbm: CbmClientMock = {
    async callTool() {
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, new SimulatedAgentRunner());

  // Simula um ciclo que sofreu crash
  const crashedRunId = "run-crashed-pre-f05";
  const activeReservationId = orchestrator.simulateInterruptedRun(crashedRunId);

  // Tenta diagnosticar e recuperar
  const diagnosis = orchestrator.diagnoseAndRecoverPreF05(crashedRunId, activeReservationId);
  assert.equal(diagnosis.canResumeAutomatically, false); // Pré-F05 NÃO autoriza retomada automática!
  assert.equal(diagnosis.actionTaken, "NEW_RUN_CREATED_FROM_OBJECTIVE");
  assert.ok(diagnosis.newRunId);
  assert.equal(diagnosis.parentRunId, crashedRunId);

  // Tentar recuperar o mesmo ciclo pai uma segunda vez deve ser impedido
  assert.throws(() => {
    orchestrator.diagnoseAndRecoverPreF05(crashedRunId, activeReservationId);
  }, /already been recovered/i);

  orchestrator.close();
});

test("S1 Orchestrator: transição para AWAITING_PLANNING e aprovação da proposta no Stage 1", async () => {
  const dbPath = getTempDbPath();
  const mockCbm: CbmClientMock = {
    async callTool(tool) {
      if (tool === "index_status") return { graphGeneration: 1, baselineDigest: "digest-stage1", rulesRevision: "r1", coverageRevision: "c1" };
      if (tool === "promote_horizon") return { success: true, promoted: true };
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, new SimulatedAgentRunner());
  const approval = new Journal(dbPath);
  approval.recordS0Approval("proj-stage1-approve", "fixture-only");
  approval.close();

  let stage1ProposalReceived: any = null;
  let phaseObservedInDb: string | null = null;

  const req: StartCycleRequest = {
    projectId: "proj-stage1-approve",
    horizonId: "horizon-stage1-approve",
    objective: "Implementar portão Stage 1 com aprovação",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 20000,
      adversarialReservation: 10000,
      maxIterations: 1,
      agentTimeoutSeconds: 60,
      cycleTimeoutSeconds: 300,
      cycleMonetaryCapUsd: 1.0,
    },
    builderPrompt: "Gerar proposta de código",
    builderIntentScope: [{ path: "src/sample.ts", operation: "write" }],
    onStage1Proposal: async (proposal) => {
      stage1ProposalReceived = proposal;
      const inspectJournal = new Journal(dbPath);
      const run = inspectJournal.getRun(proposal.runId);
      phaseObservedInDb = run?.phase ?? null;
      inspectJournal.close();
      return true; // Operador aprova
    },
  };

  const result = await orchestrator.executeCycle(req);
  assert.equal(result.status, "COMPLETED");
  assert.equal(result.terminationReason, "ACCEPTED");
  assert.equal(result.promoted, true);
  assert.ok(stage1ProposalReceived, "onStage1Proposal deve ter sido invocado");
  assert.equal(stage1ProposalReceived.iteration, 1);
  assert.ok(stage1ProposalReceived.artifactContent);
  assert.equal(phaseObservedInDb, "AWAITING_PLANNING", "Fase durante confirmação deve ser AWAITING_PLANNING");

  orchestrator.close();
});

test("S1 Orchestrator: transição para AWAITING_PLANNING e rejeição da proposta no Stage 1", async () => {
  const dbPath = getTempDbPath();
  const mockCbm: CbmClientMock = {
    async callTool() {
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, new SimulatedAgentRunner());
  const approval = new Journal(dbPath);
  approval.recordS0Approval("proj-stage1-reject", "fixture-only");
  approval.close();

  let stage1ProposalReceived = false;

  const req: StartCycleRequest = {
    projectId: "proj-stage1-reject",
    horizonId: "horizon-stage1-reject",
    objective: "Implementar portão Stage 1 com rejeição",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 20000,
      adversarialReservation: 10000,
      maxIterations: 1,
      agentTimeoutSeconds: 60,
      cycleTimeoutSeconds: 300,
      cycleMonetaryCapUsd: 1.0,
    },
    builderPrompt: "Gerar proposta de código",
    builderIntentScope: [{ path: "src/sample.ts", operation: "write" }],
    onStage1Proposal: async () => {
      stage1ProposalReceived = true;
      return false; // Operador rejeita proposta
    },
  };

  const result = await orchestrator.executeCycle(req);
  assert.equal(result.status, "COMPLETED");
  assert.equal(result.terminationReason, "REJECTED");
  assert.equal(result.promoted, false);
  assert.equal(stage1ProposalReceived, true, "onStage1Proposal deve ter sido invocado");

  orchestrator.close();
});

test("S1 Orchestrator: operador interage com Builder no Stage 1 via STEER para corrigir desvio", async () => {
  const dbPath = getTempDbPath();
  const mockCbm: CbmClientMock = {
    async callTool(tool) {
      if (tool === "index_status") return { graphGeneration: 1, baselineDigest: "digest-steer", rulesRevision: "r1", coverageRevision: "c1" };
      if (tool === "promote_horizon") return { success: true, promoted: true };
      return { success: true };
    },
  };

  const capturedPrompts: string[] = [];
  const customRunner = new SimulatedAgentRunner();
  const origInvoke = customRunner.invokeAgent.bind(customRunner);
  customRunner.invokeAgent = async (opts) => {
    capturedPrompts.push(opts.prompt);
    return origInvoke(opts);
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, customRunner);
  const approval = new Journal(dbPath);
  approval.recordS0Approval("proj-steer-stage1", "fixture-only");
  approval.close();

  let proposalCount = 0;

  const req: StartCycleRequest = {
    projectId: "proj-steer-stage1",
    horizonId: "horizon-steer-stage1",
    objective: "Implementar feature com correção iterativa no Stage 1",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 10000,
      adversarialReservation: 5000,
      maxIterations: 3,
      agentTimeoutSeconds: 60,
      cycleTimeoutSeconds: 300,
      cycleMonetaryCapUsd: 1.0,
    },
    builderPrompt: "Prompt inicial do Builder",
    builderIntentScope: [{ path: "src/sample.ts", operation: "write" }],
    onStage1Proposal: async (proposal) => {
      proposalCount++;
      if (proposalCount === 1) {
        // Operador intervém e instrui o Builder a corrigir um desvio
        return { action: "STEER", guidance: "Ajuste o retorno para incluir campo version" };
      }
      return { action: "APPROVE" };
    },
  };

  const result = await orchestrator.executeCycle(req);
  assert.equal(result.status, "COMPLETED");
  assert.equal(result.terminationReason, "ACCEPTED");
  assert.equal(result.promoted, true);
  assert.equal(proposalCount, 2, "Builder deve ter sido re-executado após orientação do operador");
  assert.ok(
    capturedPrompts.some((p) => p.includes("Ajuste o retorno para incluir campo version")),
    "Prompt do Builder deve conter a instrução de correção enviada pelo operador"
  );

  orchestrator.close();
});

test("S1 Orchestrator: pausa sob demanda durante VERIFYING com interação para corrigir desvio", async () => {
  const dbPath = getTempDbPath();
  const mockCbm: CbmClientMock = {
    async callTool(tool) {
      if (tool === "index_status") return { graphGeneration: 1, baselineDigest: "digest-pause", rulesRevision: "r1", coverageRevision: "c1" };
      if (tool === "promote_horizon") return { success: true, promoted: true };
      return { success: true };
    },
  };

  const customRunner = new SimulatedAgentRunner({ simulateAlwaysRed: false });
  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, customRunner);
  const approval = new Journal(dbPath);
  approval.recordS0Approval("proj-pause-demand", "fixture-only");
  approval.close();

  let pauseTriggered = false;
  let pauseObservedPhase: string | null = null;
  let pauseCount = 0;

  const req: StartCycleRequest = {
    projectId: "proj-pause-demand",
    horizonId: "horizon-pause-demand",
    objective: "Implementar pausa dinâmica em outros steps",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 10000,
      adversarialReservation: 5000,
      maxIterations: 2,
      agentTimeoutSeconds: 60,
      cycleTimeoutSeconds: 300,
      cycleMonetaryCapUsd: 1.0,
    },
    builderPrompt: "Prompt inicial",
    builderIntentScope: [{ path: "src/sample.ts", operation: "write" }],
    shouldPause: () => {
      // Simula operador solicitando pausa sob demanda durante o ciclo
      return !pauseTriggered;
    },
    onPause: async (ctx) => {
      pauseTriggered = true;
      pauseCount++;
      pauseObservedPhase = ctx.phase;
      // Operador interage e orienta a continuar
      return { action: "APPROVE" };
    },
  };

  const result = await orchestrator.executeCycle(req);
  assert.equal(result.status, "COMPLETED");
  assert.equal(result.terminationReason, "ACCEPTED");
  assert.equal(pauseCount, 1, "onPause deve ter sido chamado exatamente uma vez");
  assert.ok(pauseObservedPhase, "Fase deve ter sido registrada no contexto de pausa");

  orchestrator.close();
});

test("S1 Orchestrator: pausa sob demanda com decisão de abortar pelo operador", async () => {
  const dbPath = getTempDbPath();
  const mockCbm: CbmClientMock = {
    async callTool(tool) {
      if (tool === "index_status") return { graphGeneration: 1, baselineDigest: "digest-abort", rulesRevision: "r1", coverageRevision: "c1" };
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm, new SimulatedAgentRunner());
  const approval = new Journal(dbPath);
  approval.recordS0Approval("proj-pause-abort", "fixture-only");
  approval.close();

  let pauseCount = 0;

  const req: StartCycleRequest = {
    projectId: "proj-pause-abort",
    horizonId: "horizon-pause-abort",
    objective: "Ciclo que é pausado e abortado pelo operador",
    policy: {
      cycleTokenCap: 60000,
      builderReservation: 10000,
      adversarialReservation: 5000,
      maxIterations: 2,
      agentTimeoutSeconds: 60,
      cycleTimeoutSeconds: 300,
      cycleMonetaryCapUsd: 1.0,
    },
    builderPrompt: "Prompt inicial",
    builderIntentScope: [{ path: "src/sample.ts", operation: "write" }],
    shouldPause: () => true,
    onPause: async () => {
      pauseCount++;
      return { action: "REJECT" };
    },
  };

  const result = await orchestrator.executeCycle(req);
  assert.equal(result.status, "COMPLETED");
  assert.equal(result.terminationReason, "REJECTED");
  assert.equal(result.promoted, false);
  assert.equal(pauseCount, 1);

  orchestrator.close();
});
