import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { S0SpikeCoordinator, type S0SpikeReport } from "./helpers/simulated-spike.ts";

function getTempDbPath(): string {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "cbm-s0-spike-"));
  return path.join(tmpDir, "spike.db");
}

test("S0 fixture: simulação dos critérios do ADR-005, sem comprovação de viabilidade real", async () => {
  const dbPath = getTempDbPath();
  const coordinator = new S0SpikeCoordinator(dbPath);

  // Executa a suíte de validação dos 5 critérios
  const report: S0SpikeReport = await coordinator.runFullSpike();

  // Critério 1: Ciclo ponta a ponta com decisão final registrada
  assert.equal(report.criteria.endToEnd, "PASS");

  // Critério 2: Crash e reinício consistente (kill do orquestrador e recuperação via SQLite WAL)
  assert.equal(report.criteria.crashRestart, "PASS");

  // Critério 3: Cancelamento encerra o processo OS e registra interrupção
  assert.equal(report.criteria.cancellation, "PASS");

  // Critério 4: Custo/tokens medido por ciclo e persistido no journal
  assert.equal(report.criteria.costPersisted, "PASS");

  // Critério 5: Recusa do admission gate para operações fora do intent_scope
  assert.equal(report.criteria.intentScopeRefusal, "PASS");

  // Invariantes adicionais do slice inicial (INV-01, INV-02)
  assert.equal(report.invariantsPassed, true);
  assert.equal(report.evidenceMatchesCurrentConfiguration, true);

  // Avaliação do gate S0_APPROVED
  const gateEvaluation = coordinator.evaluateGate(report);
  assert.equal(gateEvaluation.approved, true);
  assert.equal(gateEvaluation.gateStatus, "S0_APPROVED");

  coordinator.close();
});
