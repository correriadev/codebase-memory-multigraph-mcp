import {
  Journal,
  type BudgetPolicy,
  type RunState,
  type RunPhase,
  type TerminationReason,
} from "./journal.ts";
import {
  createCandidateVersion,
  type CandidateVersionRef,
  type IndependentVerdict,
  type EvidenceRecord,
} from "./candidate.ts";
import {
  McpProxy,
  type AgentSessionContext,
  type CbmClientMock,
  type IntentScopePair,
} from "./mcp-proxy.ts";
import { type CodexUsage } from "./runner.ts";

export interface AgentInvocationOptions {
  role: "builder" | "adversarial";
  agentId: string;
  prompt: string;
  intentScope: IntentScopePair[];
  timeoutSeconds: number;
  candidateVersion?: CandidateVersionRef;
}

export interface AgentInvocationResult {
  artifactContent?: string;
  verdictStatus?: "GREEN" | "RED" | "INCONCLUSIVE" | "UNAVAILABLE";
  evidenceRefs?: string[];
  evidenceRecords?: EvidenceRecord[];
  usage: CodexUsage;
  costUsd: number;
  error?: string;
}

export interface AgentRunner {
  invokeAgent(options: AgentInvocationOptions): Promise<AgentInvocationResult>;
}

export class DefaultAgentRunner implements AgentRunner {
  private simulateAlwaysRed: boolean;

  constructor(options?: { simulateAlwaysRed?: boolean }) {
    this.simulateAlwaysRed = options?.simulateAlwaysRed ?? false;
  }

  async invokeAgent(options: AgentInvocationOptions): Promise<AgentInvocationResult> {
    if (options.role === "builder") {
      const artifactContent = `/* Output for prompt: ${options.prompt.slice(0, 50)} */\nexport function solve(): boolean { return true; }`;
      return {
        artifactContent,
        usage: {
          inputTokens: 12000,
          cachedInputTokens: 8000,
          outputTokens: 500,
          totalTokens: 12500,
        },
        costUsd: 0.125,
      };
    } else {
      const isRed = this.simulateAlwaysRed;
      const evidenceId = `ev-${options.candidateVersion?.versionId ?? "default"}`;
      const rec: EvidenceRecord = {
        evidenceId,
        candidateVersionId: options.candidateVersion?.versionId ?? "unknown",
        artifactDigest: options.candidateVersion?.artifactDigest ?? "unknown",
        origin: options.agentId,
        producerRole: "adversarial",
        evidenceType: "test",
        status: isRed ? "FAIL" : "PASS",
        reference: `test/reports/report-${evidenceId}.json`,
      };
      return {
        verdictStatus: isRed ? "RED" : "GREEN",
        evidenceRefs: [evidenceId],
        evidenceRecords: [rec],
        usage: {
          inputTokens: 4000,
          cachedInputTokens: 3000,
          outputTokens: 200,
          totalTokens: 4200,
        },
        costUsd: 0.042,
      };
    }
  }
}

export interface StartCycleRequest {
  projectId: string;
  horizonId: string;
  objective: string;
  policy: BudgetPolicy;
  builderPrompt: string;
  builderIntentScope: IntentScopePair[];
  s0GateStatus?: "S0_APPROVED" | "DENIED";
  _simulateAlwaysRed?: boolean;
}

export interface CycleExecutionResult {
  runId: string;
  status: RunState;
  terminationReason: TerminationReason | null;
  promoted: boolean;
  candidateVersionId: string;
  totalTokensUsed: number;
  totalCostUsd: number;
  iterationsRun: number;
}

export interface OrchestratorTelemetryQuery {
  runId: string;
}

export interface TelemetryReport {
  runId: string;
  status: RunState;
  phase: RunPhase;
  terminationReason: TerminationReason | null;
  confirmedTokens: number;
  availableTokens: number;
  totalCostUsd: number;
  hasUnknownConsumption: boolean;
}

export interface CrashRecoveryDiagnosis {
  canResumeAutomatically: boolean;
  actionTaken: "NEW_RUN_CREATED_FROM_OBJECTIVE" | "RESUME_CHECKPOINT_REJECTED";
  newRunId: string;
  parentRunId: string;
  carriedOverAvailableTokens: number;
}

export class CycleOrchestrator {
  private journal: Journal;
  private proxy: McpProxy;
  private runner: AgentRunner;

  constructor(dbPath: string, cbmClient: CbmClientMock, runner?: AgentRunner) {
    this.journal = new Journal(dbPath);
    this.proxy = new McpProxy(cbmClient);
    this.runner = runner || new DefaultAgentRunner();
  }

  /**
   * Executa um ciclo completo de desenvolvimento autônomo (PBI-01 a PBI-05, PBI-16, PBI-31).
   */
  public async executeCycle(request: StartCycleRequest): Promise<CycleExecutionResult> {
    // Verificação estrita do pré-requisito S0_APPROVED (ER-06 / Tactical Design)
    if (request.s0GateStatus !== "S0_APPROVED" && !this.journal.hasS0Approval(request.projectId)) {
      throw new Error(
        `Prerequisite S0_APPROVED gate required before starting S1 cycle for project '${request.projectId}' (ER-06)`
      );
    }

    const runId = `run-${Date.now()}-${Math.floor(Math.random() * 1000)}`;
    const cycleStartTime = Date.now();
    const cycleTimeoutMs = request.policy.cycleTimeoutSeconds * 1000;

    const runner =
      request._simulateAlwaysRed !== undefined
        ? new DefaultAgentRunner({ simulateAlwaysRed: request._simulateAlwaysRed })
        : this.runner;

    this.journal.createRun({
      runId,
      projectId: request.projectId,
      horizonId: request.horizonId,
      objective: request.objective,
      policy: request.policy,
    });

    this.journal.transitionRun(runId, { state: "RUNNING", phase: "BUILDING" });

    let latestVersion: CandidateVersionRef | null = null;
    let iteration = 0;
    let promoted = false;
    let terminationReason: TerminationReason | null = null;

    const invokeWithTimeout = async (
      options: AgentInvocationOptions,
      timeoutMs: number
    ): Promise<AgentInvocationResult> => {
      let timer: NodeJS.Timeout | undefined;
      const timeoutPromise = new Promise<never>((_, reject) => {
        timer = setTimeout(() => {
          reject(new Error(`Agent invocation timed out after ${timeoutMs}ms`));
        }, timeoutMs);
      });

      try {
        return await Promise.race([runner.invokeAgent(options), timeoutPromise]);
      } finally {
        if (timer) clearTimeout(timer);
      }
    };

    for (let i = 1; i <= request.policy.maxIterations; i++) {
      iteration = i;

      // Verificação do timeout global do ciclo (C03)
      if (Date.now() - cycleStartTime >= cycleTimeoutMs) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // 1. Fase BUILDING: reserva orçamento para o Builder
      const builderAgentId = `agent-builder-${runId}-iter-${i}`;
      const builderRes = this.journal.reserveBudget(
        runId,
        "builder",
        request.policy.builderReservation
      );
      if (!builderRes.success || !builderRes.reservationId) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      const remainingCycleMs = Math.max(0, cycleTimeoutMs - (Date.now() - cycleStartTime));
      const builderTimeoutMs = Math.min(
        request.policy.agentTimeoutSeconds * 1000,
        remainingCycleMs
      );

      let builderOutput: AgentInvocationResult;
      try {
        builderOutput = await invokeWithTimeout(
          {
            role: "builder",
            agentId: builderAgentId,
            prompt: request.builderPrompt,
            intentScope: request.builderIntentScope,
            timeoutSeconds: Math.floor(builderTimeoutMs / 1000),
          },
          builderTimeoutMs
        );
      } catch (err: any) {
        this.journal.reconcileBudget({
          runId,
          reservationId: builderRes.reservationId,
          actualTokens: null,
          costUsd: null,
          status: "UNKNOWN",
        });
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // Reconciliação atômica do Builder imediatamente com tokens medidos e reservationId verificado
      this.journal.reconcileBudget({
        runId,
        reservationId: builderRes.reservationId,
        actualTokens: builderOutput.usage.totalTokens,
        costUsd: builderOutput.costUsd,
        status: "MEASURED",
      });

      // Verifica teto monetário e deadline do ciclo
      let currentLedger = this.journal.getLedger(runId);
      if (
        currentLedger.totalCostUsd >= request.policy.cycleMonetaryCapUsd ||
        Date.now() - cycleStartTime >= cycleTimeoutMs
      ) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // Consulta estado e frescor do backend de forma limitada pelo deadline do ciclo
      const remainingForState = Math.max(0, cycleTimeoutMs - (Date.now() - cycleStartTime));
      if (remainingForState <= 0) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      let currentGraphGeneration = 1;
      let currentBaselineDigest = "sha256-baseline-root";
      try {
        let stateTimer: NodeJS.Timeout | undefined;
        const stateTimeout = new Promise<never>((_, reject) => {
          stateTimer = setTimeout(
            () => reject(new Error("index_status timeout")),
            Math.min(5000, remainingForState)
          );
        });
        try {
          const stateRes = await Promise.race([
            this.proxy.dispatchToolCall(
              {
                sessionId: `sess-state-${runId}`,
                role: "gatekeeper",
                agentId: "orchestrator",
                intentScope: [],
              },
              {
                toolName: "index_status",
                arguments: { project_id: request.projectId, horizon_id: request.horizonId },
              }
            ),
            stateTimeout,
          ]);
          if (stateRes.allowed && stateRes.data) {
            if (stateRes.data.graphGeneration) currentGraphGeneration = Number(stateRes.data.graphGeneration);
            if (stateRes.data.baselineDigest) currentBaselineDigest = String(stateRes.data.baselineDigest);
          }
        } finally {
          if (stateTimer) clearTimeout(stateTimer);
        }
      } catch {
        // Fallback para defaults caso timeout ou erro
      }

      // Criação e selagem da versão candidata C01
      const artifactContent = builderOutput.artifactContent || `/* iter ${i} */`;
      latestVersion = createCandidateVersion({
        projectId: request.projectId,
        runId,
        horizonId: request.horizonId,
        artifactContent,
        baselineDigest: currentBaselineDigest,
        graphGeneration: currentGraphGeneration,
        rulesRevision: `rev-s1-iter-${i}`,
        coverageRevision: `cov-s1-iter-${i}`,
        policyRevision: "pol-s1",
        contractVersion: "1.0",
      });

      // 2. Fase VERIFYING: reserva orçamento para o Adversarial
      this.journal.transitionRun(runId, { phase: "VERIFYING" });
      const advAgentId = `agent-adv-${runId}-iter-${i}`;
      const advRes = this.journal.reserveBudget(
        runId,
        "adversarial",
        request.policy.adversarialReservation
      );
      if (!advRes.success || !advRes.reservationId) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      const remainingForAdv = Math.max(0, cycleTimeoutMs - (Date.now() - cycleStartTime));
      if (remainingForAdv <= 0) {
        this.journal.reconcileBudget({
          runId,
          reservationId: advRes.reservationId,
          actualTokens: null,
          costUsd: null,
          status: "UNKNOWN",
        });
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      const advTimeoutMs = Math.min(
        request.policy.agentTimeoutSeconds * 1000,
        remainingForAdv
      );

      let advOutput: AgentInvocationResult;
      try {
        advOutput = await invokeWithTimeout(
          {
            role: "adversarial",
            agentId: advAgentId,
            prompt: `Verify sealed candidate ${latestVersion.versionId}`,
            intentScope: [],
            timeoutSeconds: Math.floor(advTimeoutMs / 1000),
            candidateVersion: latestVersion,
          },
          advTimeoutMs
        );
      } catch (err: any) {
        this.journal.reconcileBudget({
          runId,
          reservationId: advRes.reservationId,
          actualTokens: null,
          costUsd: null,
          status: "UNKNOWN",
        });
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // Reconciliação do Adversarial
      this.journal.reconcileBudget({
        runId,
        reservationId: advRes.reservationId,
        actualTokens: advOutput.usage.totalTokens,
        costUsd: advOutput.costUsd,
        status: "MEASURED",
      });

      currentLedger = this.journal.getLedger(runId);
      if (
        currentLedger.totalCostUsd >= request.policy.cycleMonetaryCapUsd ||
        Date.now() - cycleStartTime >= cycleTimeoutMs
      ) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // 3. Fase AWAITING_DECISION & PROMOTING (C04 / C07 Outbox com verificação de evidência real)
      const verdictStatus = advOutput.verdictStatus || "INCONCLUSIVE";
      const evidenceRefs = advOutput.evidenceRefs || [];
      const evidenceRecords = advOutput.evidenceRecords || [];

      const verdict: IndependentVerdict = {
        verdictId: `verd-${runId}-iter-${i}`,
        candidateVersionId: latestVersion.versionId,
        verifierIdentity: advAgentId,
        status: verdictStatus,
        evidenceRefs,
      };

      if (verdict.status === "GREEN") {
        this.journal.transitionRun(runId, { phase: "PROMOTING" });
        const gatekeeperSession: AgentSessionContext = {
          sessionId: `sess-gatekeeper-${runId}`,
          role: "gatekeeper",
          agentId: "gatekeeper-core",
          intentScope: [],
        };

        const effectKey = `promote:${request.horizonId}:${latestVersion.versionId}`;
        this.journal.recordEffectIntent({
          runId,
          idempotencyKey: effectKey,
          toolName: "promote_horizon",
          argsDigest: latestVersion.artifactDigest,
        });

        const remainingForPromo = Math.max(0, cycleTimeoutMs - (Date.now() - cycleStartTime));
        if (remainingForPromo <= 0) {
          this.journal.confirmEffectResult(effectKey, "UNKNOWN", "Promotion aborted due to cycle timeout");
          terminationReason = "BUDGET_EXHAUSTED";
          break;
        }

        let promoResult: { promoted: boolean; status?: "CONFIRMED" | "FAILED" | "UNKNOWN"; reason?: string };
        try {
          let promoTimer: NodeJS.Timeout | undefined;
          const promoTimeout = new Promise<never>((_, reject) => {
            promoTimer = setTimeout(() => reject(new Error("Promotion call timed out")), remainingForPromo);
          });
          try {
            promoResult = await Promise.race([
              this.proxy.promoteCandidate(
                gatekeeperSession,
                latestVersion,
                verdict,
                builderAgentId,
                {
                  idempotencyKey: effectKey,
                  evidenceRecords,
                }
              ),
              promoTimeout,
            ]);
          } finally {
            if (promoTimer) clearTimeout(promoTimer);
          }
        } catch (err: any) {
          // Timeout ou exceção de transporte em efeito irreversível deixa resultado como UNKNOWN (C04/C07)
          this.journal.confirmEffectResult(
            effectKey,
            "UNKNOWN",
            `Ambiguous promotion outcome due to transport/timeout: ${err.message}`
          );
          terminationReason = "BUDGET_EXHAUSTED";
          break;
        }

        if (promoResult.promoted) {
          this.journal.confirmEffectResult(effectKey, "CONFIRMED", JSON.stringify(promoResult));
          promoted = true;
          terminationReason = "ACCEPTED";
          break;
        } else if (promoResult.status === "UNKNOWN") {
          this.journal.confirmEffectResult(
            effectKey,
            "UNKNOWN",
            promoResult.reason || "Ambiguous promotion outcome"
          );
          terminationReason = "BUDGET_EXHAUSTED";
          break;
        } else {
          this.journal.confirmEffectResult(
            effectKey,
            "FAILED",
            JSON.stringify({ reason: promoResult.reason })
          );
        }
      } else {
        if (i === request.policy.maxIterations) {
          terminationReason = "REJECTED";
        }
      }
    }

    if (!terminationReason) {
      terminationReason = promoted ? "ACCEPTED" : "REJECTED";
    }

    this.journal.transitionRun(runId, {
      state: "COMPLETED",
      phase: "CLEANING_UP",
      terminationReason,
    });

    const ledger = this.journal.getLedger(runId);

    return {
      runId,
      status: "COMPLETED",
      terminationReason,
      promoted,
      candidateVersionId: latestVersion?.versionId ?? "",
      totalTokensUsed: ledger.confirmedTokens,
      totalCostUsd: ledger.totalCostUsd,
      iterationsRun: iteration,
    };
  }

  /**
   * Consulta projeção de telemetria da execução (OV-01 a OV-04 / PBI-31).
   */
  public queryTelemetry(query: OrchestratorTelemetryQuery): TelemetryReport | null {
    const run = this.journal.getRun(query.runId);
    if (!run) return null;

    const ledger = this.journal.getLedger(query.runId);

    return {
      runId: run.runId,
      status: run.state,
      phase: run.phase,
      terminationReason: run.terminationReason,
      confirmedTokens: ledger.confirmedTokens,
      availableTokens: ledger.availableTokens,
      totalCostUsd: ledger.totalCostUsd,
      hasUnknownConsumption: ledger.hasUnknownConsumption,
    };
  }

  /**
   * Simula um ciclo interrompido por crash abrupto para testes de recuperação pré-F05.
   */
  public simulateInterruptedRun(crashedRunId: string): string {
    this.journal.createRun({
      runId: crashedRunId,
      projectId: "cbm-crash-sim",
      horizonId: "horizon-crashed",
      objective: "Objetivo interrompido por crash",
      policy: {
        cycleTokenCap: 60000,
        builderReservation: 20000,
        adversarialReservation: 10000,
        maxIterations: 2,
        agentTimeoutSeconds: 180,
        cycleTimeoutSeconds: 900,
        cycleMonetaryCapUsd: 2.0,
      },
    });

    const res = this.journal.reserveBudget(crashedRunId, "builder", 20000);
    return res.reservationId!;
  }

  /**
   * Diagnóstico e tratamento de crash pré-F05 (ER-10 / PBI-20):
   * Reconcilia reservas pendentes como UNKNOWN, impede dupla recuperação e transfere saldo remanescente.
   */
  public diagnoseAndRecoverPreF05(crashedRunId: string, activeReservationId?: string): CrashRecoveryDiagnosis {
    const run = this.journal.getRun(crashedRunId);
    if (!run) throw new Error(`Run ${crashedRunId} not found`);

    this.journal.transitionRun(crashedRunId, {
      state: "COMPLETED",
      phase: "RECOVERING",
      terminationReason: "RECOVERY_REQUIRED",
    });

    // Reconcilia todas as reservas ativas consultadas diretamente do SQLite
    const activeReservations = this.journal.getActiveReservations(crashedRunId);
    for (const res of activeReservations) {
      this.journal.reconcileBudget({
        runId: crashedRunId,
        reservationId: res.reservation_id,
        actualTokens: null,
        costUsd: null,
        status: "UNKNOWN",
      });
    }

    if (
      activeReservationId &&
      !activeReservations.some((r) => r.reservation_id === activeReservationId)
    ) {
      try {
        this.journal.reconcileBudget({
          runId: crashedRunId,
          reservationId: activeReservationId,
          actualTokens: null,
          costUsd: null,
          status: "UNKNOWN",
        });
      } catch {
        // Ignora caso já reconciliada
      }
    }

    const parentLedger = this.journal.getLedger(crashedRunId);
    const newRunId = `${crashedRunId}-recovered-${Date.now()}`;
    const policy = JSON.parse(run.policyJson) as BudgetPolicy;

    // Novo run herda parentRunId, o saldo remanescente disponível e status de unknown consumption
    this.journal.createRun({
      runId: newRunId,
      projectId: run.projectId,
      horizonId: run.horizonId,
      objective: run.objective,
      policy,
      parentRunId: crashedRunId,
      initialAvailableTokens: parentLedger.availableTokens,
      initialTotalCostUsd: parentLedger.totalCostUsd,
      inheritUnknownConsumption: parentLedger.hasUnknownConsumption,
    });

    return {
      canResumeAutomatically: false,
      actionTaken: "NEW_RUN_CREATED_FROM_OBJECTIVE",
      newRunId,
      parentRunId: crashedRunId,
      carriedOverAvailableTokens: parentLedger.availableTokens,
    };
  }

  public close(): void {
    this.journal.close();
  }
}
