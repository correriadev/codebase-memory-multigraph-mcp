import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import {
  Journal,
  type CreateRunParams,
  type BudgetPolicy,
} from "../src/journal.ts";

function getTempDbPath(): string {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "cbm-journal-test-"));
  return path.join(tmpDir, "journal.db");
}

const DEFAULT_POLICY: BudgetPolicy = {
  cycleTokenCap: 60000,
  builderReservation: 20000,
  adversarialReservation: 10000,
  maxIterations: 2,
  agentTimeoutSeconds: 180,
  cycleTimeoutSeconds: 900,
  cycleMonetaryCapUsd: 2.0,
};

test("Journal: inicialização SQLite WAL e durabilidade", () => {
  const dbPath = getTempDbPath();
  const journal = new Journal(dbPath);

  assert.ok(fs.existsSync(dbPath));

  journal.close();
});

test("Journal: criação de ciclo (run) e transição de estados C02", () => {
  const dbPath = getTempDbPath();
  const journal = new Journal(dbPath);

  const runParams: CreateRunParams = {
    runId: "run-001",
    projectId: "cbm-multigraph",
    horizonId: "horizon-h1",
    objective: "Implementar fix trivial em add()",
    policy: DEFAULT_POLICY,
  };

  const run = journal.createRun(runParams);
  assert.equal(run.runId, "run-001");
  assert.equal(run.state, "CREATED");
  assert.equal(run.phase, "PLANNING");
  assert.equal(run.terminationReason, null);

  // Transição CREATED -> RUNNING na fase BUILDING
  journal.transitionRun("run-001", { state: "RUNNING", phase: "BUILDING" });
  const updatedRun = journal.getRun("run-001");
  assert.equal(updatedRun?.state, "RUNNING");
  assert.equal(updatedRun?.phase, "BUILDING");

  // Transição RUNNING -> PAUSED na fase AWAITING_PLANNING (portão Stage 1)
  journal.transitionRun("run-001", { state: "PAUSED", phase: "AWAITING_PLANNING" });
  const pausedRun = journal.getRun("run-001");
  assert.equal(pausedRun?.state, "PAUSED");
  assert.equal(pausedRun?.phase, "AWAITING_PLANNING");

  // Retomada PAUSED -> RUNNING de volta para BUILDING
  journal.transitionRun("run-001", { state: "RUNNING", phase: "BUILDING" });
  const resumedRun = journal.getRun("run-001");
  assert.equal(resumedRun?.state, "RUNNING");
  assert.equal(resumedRun?.phase, "BUILDING");

  // Transição RUNNING -> COMPLETED com motivo ACCEPTED
  journal.transitionRun("run-001", {
    state: "COMPLETED",
    phase: "CLEANING_UP",
    terminationReason: "ACCEPTED",
  });
  const completedRun = journal.getRun("run-001");
  assert.equal(completedRun?.state, "COMPLETED");
  assert.equal(completedRun?.terminationReason, "ACCEPTED");

  journal.close();
});

test("Journal / C03: reserva atômica e reconciliação de orçamento", () => {
  const dbPath = getTempDbPath();
  const journal = new Journal(dbPath);

  journal.createRun({
    runId: "run-002",
    projectId: "cbm-multigraph",
    horizonId: "horizon-h1",
    objective: "Teste de orçamento",
    policy: { ...DEFAULT_POLICY, cycleTokenCap: 50000 },
  });

  // Saldo inicial: 50.000 tokens disponíveis
  let ledger = journal.getLedger("run-002");
  assert.equal(ledger.tokenCap, 50000);
  assert.equal(ledger.reservedTokens, 0);
  assert.equal(ledger.confirmedTokens, 0);
  assert.equal(ledger.availableTokens, 50000);

  // 1. Reserva para Builder: 20.000 tokens
  const res1 = journal.reserveBudget("run-002", "builder", 20000);
  assert.equal(res1.success, true);
  ledger = journal.getLedger("run-002");
  assert.equal(ledger.reservedTokens, 20000);
  assert.equal(ledger.availableTokens, 30000);

  // 2. Reconciliação do Builder: gastou apenas 12.000 tokens reais
  journal.reconcileBudget({
    runId: "run-002",
    reservationId: res1.reservationId!,
    actualTokens: 12000,
    costUsd: 0.15,
    status: "MEASURED",
  });

  ledger = journal.getLedger("run-002");
  assert.equal(ledger.reservedTokens, 0);
  assert.equal(ledger.confirmedTokens, 12000);
  assert.equal(ledger.availableTokens, 38000); // 50000 - 12000
  assert.equal(ledger.totalCostUsd, 0.15);

  // 3. Tentar reservar mais do que o saldo disponível (ex: 40.000 quando restam 38.000)
  const resExceeded = journal.reserveBudget("run-002", "builder", 40000);
  assert.equal(resExceeded.success, false);
  assert.match(resExceeded.reason!, /insufficient budget/i);

  // 4. Falha sem medição de tokens (UNKNOWN) marca o ledger e bloqueia novos despachos
  const resUnknown = journal.reserveBudget("run-002", "builder", 10000);
  journal.reconcileBudget({
    runId: "run-002",
    reservationId: resUnknown.reservationId!,
    actualTokens: null,
    costUsd: null,
    status: "UNKNOWN",
  });

  ledger = journal.getLedger("run-002");
  assert.equal(ledger.hasUnknownConsumption, true);

  // Novos despachos devem ser bloqueados quando há consumo desconhecido
  const resBlockedByUnknown = journal.reserveBudget("run-002", "builder", 5000);
  assert.equal(resBlockedByUnknown.success, false);
  assert.match(resBlockedByUnknown.reason!, /unknown consumption/i);

  journal.close();
});

test("Journal / C07: registro idempotente de efeitos externos", () => {
  const dbPath = getTempDbPath();
  const journal = new Journal(dbPath);

  journal.createRun({
    runId: "run-003",
    projectId: "cbm-multigraph",
    horizonId: "horizon-h1",
    objective: "Teste de efeitos",
    policy: DEFAULT_POLICY,
  });

  // Registrar intenção de efeito
  const effectKey = "promote:horizon-h1:attempt-1";
  const rec1 = journal.recordEffectIntent({
    runId: "run-003",
    idempotencyKey: effectKey,
    toolName: "promote_horizon",
    argsDigest: "sha256-args-001",
  });
  assert.equal(rec1.isNew, true);

  // Tentar registrar a mesma intenção novamente (deve detectar idempotência)
  const rec2 = journal.recordEffectIntent({
    runId: "run-003",
    idempotencyKey: effectKey,
    toolName: "promote_horizon",
    argsDigest: "sha256-args-001",
  });
  assert.equal(rec2.isNew, false);

  // Confirmar resultado do efeito
  journal.confirmEffectResult(effectKey, "CONFIRMED", JSON.stringify({ promoted: true }));
  const effect = journal.getEffect(effectKey);
  assert.equal(effect?.status, "CONFIRMED");
  assert.equal(JSON.parse(effect?.result!).promoted, true);

  journal.close();
});

test("Journal / Concorrência: rejeição de reserva com saldo insuficiente ou concorrência e recusa de dupla reconciliação", () => {
  const dbPath = getTempDbPath();
  const journal = new Journal(dbPath);

  journal.createRun({
    runId: "run-concurrency",
    projectId: "cbm-multigraph",
    horizonId: "horizon-c1",
    objective: "Teste de concorrência e reserva única",
    policy: { ...DEFAULT_POLICY, cycleTokenCap: 10000 },
  });

  const res = journal.reserveBudget("run-concurrency", "builder", 5000);
  assert.equal(res.success, true);
  assert.ok(res.reservationId);

  // Primeira reconciliação
  journal.reconcileBudget({
    runId: "run-concurrency",
    reservationId: res.reservationId!,
    actualTokens: 3000,
    costUsd: 0.03,
    status: "MEASURED",
  });

  let ledger = journal.getLedger("run-concurrency");
  assert.equal(ledger.confirmedTokens, 3000);
  assert.equal(ledger.availableTokens, 7000); // 10000 - 3000

  // Segunda reconciliação com o MESMO reservationId (deve ser ignorada sem estornar saldo duas vezes)
  journal.reconcileBudget({
    runId: "run-concurrency",
    reservationId: res.reservationId!,
    actualTokens: 3000,
    costUsd: 0.03,
    status: "MEASURED",
  });

  ledger = journal.getLedger("run-concurrency");
  assert.equal(ledger.availableTokens, 7000, "Saldo não pode ser aumentado por dupla reconciliação!");

  // Tentar reconciliar reservation de outro run_id deve lançar erro
  assert.throws(() => {
    journal.reconcileBudget({
      runId: "wrong-run-id",
      reservationId: res.reservationId!,
      actualTokens: 1000,
      costUsd: 0.01,
      status: "MEASURED",
    });
  }, /belongs to run/i);

  // Reconciliação com tokens ou custo negativo deve ser terminantemente rejeitada
  const resNeg = journal.reserveBudget("run-concurrency", "adversarial", 1000);
  assert.throws(() => {
    journal.reconcileBudget({
      runId: "run-concurrency",
      reservationId: resNeg.reservationId!,
      actualTokens: -500,
      costUsd: 0.01,
      status: "MEASURED",
    });
  }, /negative.*rejected/i);

  assert.throws(() => {
    journal.reconcileBudget({
      runId: "run-concurrency",
      reservationId: resNeg.reservationId!,
      actualTokens: 500,
      costUsd: -0.05,
      status: "MEASURED",
    });
  }, /negative.*rejected/i);

  // Reserva com quantidade negativa ou zero de tokens deve ser estritamente recusada (Point 6)
  const resNegativeTokens = journal.reserveBudget("run-concurrency", "builder", -100);
  assert.equal(resNegativeTokens.success, false);
  assert.match(resNegativeTokens.reason!, /positive number/i);
  const ledgerAfterNeg = journal.getLedger("run-concurrency");
  assert.equal(ledgerAfterNeg.availableTokens, 6000); // 7000 - 1000 da resNeg

  journal.close();
});
