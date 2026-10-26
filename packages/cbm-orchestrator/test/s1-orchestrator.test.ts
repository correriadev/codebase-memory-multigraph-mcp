import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
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
        return { graphGeneration: 1, baselineDigest: "sha256-baseline-root" };
      }
      if (tool === "promote_horizon") {
        cbmPromoted = true;
        return { success: true, promoted: true };
      }
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm);

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
    s0GateStatus: "S0_APPROVED",
  };

  // Verificação de bloqueio pré-S0 (ER-06)
  await assert.rejects(async () => {
    await orchestrator.executeCycle({ ...req, s0GateStatus: undefined });
  }, /Prerequisite S0_APPROVED gate required/i);

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

test("S1 Orchestrator: interrupção por circuit breaker após falhas repetidas (ER-11 / PBI-03)", async () => {
  const dbPath = getTempDbPath();

  const mockCbm: CbmClientMock = {
    async callTool() {
      return { success: true };
    },
  };

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm);

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
    s0GateStatus: "S0_APPROVED",
    _simulateAlwaysRed: true,
  };

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

  const orchestrator = new CycleOrchestrator(dbPath, mockCbm);

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
