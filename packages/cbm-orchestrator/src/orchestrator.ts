import { createHash } from "node:crypto";
import {
  Journal,
  type BudgetPolicy,
  type RunState,
  type RunPhase,
  type TerminationReason,
  type EffectRecord,
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
  signal?: AbortSignal;
}

export interface AgentInvocationResult {
  artifactContent?: string;
  verdictStatus?: "GREEN" | "RED" | "INCONCLUSIVE" | "UNAVAILABLE";
  evidenceRefs?: string[];
  evidenceRecords?: EvidenceRecord[];
  usage: CodexUsage | null;
  costUsd: number | null;
  error?: string;
  failureClass?: "TEST_FAILURE" | "REVIEW_REJECTED";
}

export interface AgentRunner {
  invokeAgent(options: AgentInvocationOptions): Promise<AgentInvocationResult>;
}

export interface Stage1Proposal {
  runId: string;
  iteration: number;
  artifactContent: string;
  usage: CodexUsage | null;
  costUsd: number | null;
}

export interface StartCycleRequest {
  projectId: string;
  horizonId: string;
  objective: string;
  policy: BudgetPolicy;
  builderPrompt: string;
  builderIntentScope: IntentScopePair[];
  signal?: AbortSignal;
  onStage1Proposal?: (proposal: Stage1Proposal) => Promise<boolean>;
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
  effects: EffectRecord[];
  pauseReason?: string;
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
  private configuration: { s0Probe?: boolean; configurationDigest?: string };

  constructor(dbPath: string, cbmClient: CbmClientMock, runner: AgentRunner,
    configuration: { s0Probe?: boolean; configurationDigest?: string } = {}) {
    if (!runner) throw new Error("A concrete AgentRunner is required");
    this.journal = new Journal(dbPath);
    this.proxy = new McpProxy(cbmClient);
    this.runner = runner;
    this.configuration = configuration;
  }

  /**
   * Executa um ciclo completo de desenvolvimento autônomo (PBI-01 a PBI-05, PBI-16, PBI-31).
   */
  public async executeCycle(request: StartCycleRequest): Promise<CycleExecutionResult> {
    // Verificação estrita do pré-requisito S0_APPROVED (ER-06 / Tactical Design)
    if (!this.configuration.s0Probe && !this.journal.hasS0Approval(request.projectId, this.configuration.configurationDigest)) {
      throw new Error(
        `Prerequisite S0_APPROVED gate required before starting S1 cycle for project '${request.projectId}' (ER-06)`
      );
    }

    const runId = `run-${Date.now()}-${Math.floor(Math.random() * 1000)}`;
    const cycleStartTime = performance.now();
    const cycleTimeoutMs = request.policy.cycleTimeoutSeconds * 1000;

    validateBudgetPolicy(request.policy);
    request.signal?.throwIfAborted();
    const runner = this.runner;

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
    let pauseReason: string | undefined;
    const failures: { failureClass: string; at: number }[] = [];

    const invokeWithTimeout = async (
      options: AgentInvocationOptions,
      timeoutMs: number
    ): Promise<AgentInvocationResult> => {
      let timer: NodeJS.Timeout | undefined;
      const controller = new AbortController();
      const abort = () => controller.abort();
      request.signal?.addEventListener("abort", abort, { once: true });
      const timeoutPromise = new Promise<never>((_, reject) => {
        timer = setTimeout(() => {
          controller.abort();
          // Give the concrete runner time to reap its process tree before returning.
          timer = setTimeout(() => reject(new Error(`Agent invocation timed out after ${timeoutMs}ms`)), 2500);
        }, timeoutMs);
      });

      try {
        return await Promise.race([runner.invokeAgent({ ...options, signal: controller.signal }), timeoutPromise]);
      } finally {
        if (timer) clearTimeout(timer);
        request.signal?.removeEventListener("abort", abort);
      }
    };

    for (let i = 1; i <= request.policy.maxIterations; i++) {
      if (request.signal?.aborted) { terminationReason = "CANCELLED"; break; }
      iteration = i;

      // Verificação do timeout global do ciclo (C03)
      if (performance.now() - cycleStartTime >= cycleTimeoutMs) {
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

      const remainingCycleMs = Math.max(0, cycleTimeoutMs - (performance.now() - cycleStartTime));
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
        terminationReason = request.signal?.aborted ? "CANCELLED" : "FAILED";
        break;
      }

      // Reconciliação atômica do Builder imediatamente com tokens medidos e reservationId verificado
      this.journal.reconcileBudget({
        runId,
        reservationId: builderRes.reservationId,
        actualTokens: builderOutput.usage?.totalTokens ?? null,
        costUsd: builderOutput.costUsd,
        status: builderOutput.usage && builderOutput.costUsd !== null ? "MEASURED" : "UNKNOWN",
      });
      if (builderOutput.error || !builderOutput.usage || builderOutput.costUsd === null || !builderOutput.artifactContent) {
        terminationReason = request.signal?.aborted ? "CANCELLED" : "FAILED";
        break;
      }

      // Verifica teto monetário e deadline do ciclo
      let currentLedger = this.journal.getLedger(runId);
      if (
        currentLedger.totalCostUsd >= request.policy.cycleMonetaryCapUsd ||
        performance.now() - cycleStartTime >= cycleTimeoutMs
      ) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // Etapa 1 concluída: proposta gerada pelo Builder.
      // Se gancho de confirmação humana estiver configurado, transiciona para AWAITING_PLANNING.
      if (request.onStage1Proposal) {
        this.journal.transitionRun(runId, { state: "PAUSED", phase: "AWAITING_PLANNING" });
        const approved = await request.onStage1Proposal({
          runId,
          iteration: i,
          artifactContent: builderOutput.artifactContent,
          usage: builderOutput.usage,
          costUsd: builderOutput.costUsd,
        });

        if (!approved || request.signal?.aborted) {
          terminationReason = request.signal?.aborted ? "CANCELLED" : "REJECTED";
          break;
        }

        this.journal.transitionRun(runId, { state: "RUNNING", phase: "BUILDING" });
      }

      // Consulta estado e frescor do backend de forma limitada pelo deadline do ciclo
      const remainingForState = Math.max(0, cycleTimeoutMs - (performance.now() - cycleStartTime));
      if (remainingForState <= 0) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      let currentGraphGeneration: string | number;
      let currentBaselineDigest: string;
      let revisions: { rulesRevision?: string; coverageRevision?: string } = {};
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
          if (!stateRes.allowed || stateRes.error || !stateRes.data?.graphGeneration || !stateRes.data?.baselineDigest) {
            throw new Error("Backend did not supply a verified candidate snapshot");
          }
          currentGraphGeneration = stateRes.data.graphGeneration;
          currentBaselineDigest = stateRes.data.baselineDigest;
          revisions = stateRes.data;
        } finally {
          if (stateTimer) clearTimeout(stateTimer);
        }
      } catch {
        // A snapshot failure cannot authorize a candidate using invented revisions.
        terminationReason = "FAILED";
        break;
      }

      // Criação e selagem da versão candidata C01
      if (!revisions.rulesRevision || !revisions.coverageRevision) {
        throw new Error("Backend did not supply rulesRevision and coverageRevision");
      }
      const artifactContent = builderOutput.artifactContent;
      latestVersion = createCandidateVersion({
        projectId: request.projectId,
        runId,
        horizonId: request.horizonId,
        artifactContent,
        baselineDigest: currentBaselineDigest,
        graphGeneration: currentGraphGeneration,
        rulesRevision: revisions.rulesRevision,
        coverageRevision: revisions.coverageRevision,
        policyRevision: "sha256:" + createHash("sha256").update(JSON.stringify(request.policy)).digest("hex"),
        contractVersion: "1.0",
      });
      this.journal.recordEvent(runId, "CANDIDATE_SEALED", latestVersion);

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

      const remainingForAdv = Math.max(0, cycleTimeoutMs - (performance.now() - cycleStartTime));
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
        terminationReason = request.signal?.aborted ? "CANCELLED" : "VERIFIER_UNAVAILABLE";
        break;
      }

      // Reconciliação do Adversarial
      this.journal.reconcileBudget({
        runId,
        reservationId: advRes.reservationId,
        actualTokens: advOutput.usage?.totalTokens ?? null,
        costUsd: advOutput.costUsd,
        status: advOutput.usage && advOutput.costUsd !== null ? "MEASURED" : "UNKNOWN",
      });
      if (advOutput.error || !advOutput.usage || advOutput.costUsd === null) {
        terminationReason = request.signal?.aborted ? "CANCELLED" : "VERIFIER_UNAVAILABLE";
        break;
      }

      currentLedger = this.journal.getLedger(runId);
      if (
        currentLedger.totalCostUsd >= request.policy.cycleMonetaryCapUsd ||
        performance.now() - cycleStartTime >= cycleTimeoutMs
      ) {
        terminationReason = "BUDGET_EXHAUSTED";
        break;
      }

      // 3. Fase AWAITING_DECISION & PROMOTING (C04 / C07 Outbox com verificação de evidência real)
      const verdictStatus = advOutput.verdictStatus || "INCONCLUSIVE";
      if (verdictStatus === "UNAVAILABLE" || verdictStatus === "INCONCLUSIVE") {
        terminationReason = "VERIFIER_UNAVAILABLE";
        break;
      }
      const evidenceRefs = advOutput.evidenceRefs || [];
      const evidenceRecords = advOutput.evidenceRecords || [];

      const verdict: IndependentVerdict = {
        verdictId: `verd-${runId}-iter-${i}`,
        candidateVersionId: latestVersion.versionId,
        verifierIdentity: advAgentId,
        status: verdictStatus,
        evidenceRefs,
      };
      this.journal.recordEvent(runId, "INDEPENDENT_VERDICT", { verdict, evidenceRecords });

      if (verdict.status === "GREEN") {
        if (request.signal?.aborted) { terminationReason = "CANCELLED"; break; }
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

        const remainingForPromo = Math.max(0, cycleTimeoutMs - (performance.now() - cycleStartTime));
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
          this.journal.recordEvent(runId, "PROMOTION_DECISION", { result: "ALLOW", candidateVersion: latestVersion, verdict });
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
        request = { ...request, builderPrompt: `${request.builderPrompt}\nIndependent review of ${latestVersion.versionId}: ${JSON.stringify(evidenceRecords)}` };
        const failureClass = advOutput.failureClass ?? "REVIEW_REJECTED";
        const at = performance.now();
        failures.push({ failureClass, at });
        if (request.policy.breakerFailureThreshold && request.policy.breakerWindowSeconds &&
          failures.filter((failure) => failure.failureClass === failureClass && at - failure.at <= request.policy.breakerWindowSeconds! * 1000)
            .length >= request.policy.breakerFailureThreshold) {
          pauseReason = `Circuit breaker: ${failureClass}`;
          this.journal.recordEvent(runId, "CIRCUIT_BREAKER_OPENED", { failureClass, failures });
          break;
        }
        if (i === request.policy.maxIterations) {
          terminationReason = "REJECTED";
        }
      }
    }

    if (!terminationReason && !pauseReason) {
      terminationReason = promoted ? "ACCEPTED" : "REJECTED";
    }

    if (pauseReason) this.journal.transitionRun(runId, { state: "PAUSED", phase: "AWAITING_DECISION" });
    else this.journal.transitionRun(runId, { state: "COMPLETED", phase: "CLEANING_UP", terminationReason: terminationReason! });

    const ledger = this.journal.getLedger(runId);

    return {
      runId,
      status: pauseReason ? "PAUSED" : "COMPLETED",
      terminationReason,
      promoted,
      candidateVersionId: latestVersion?.versionId ?? "",
      totalTokensUsed: ledger.confirmedTokens,
      totalCostUsd: ledger.totalCostUsd,
      iterationsRun: iteration,
      effects: this.journal.getEffects(runId),
      pauseReason,
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
    this.journal.transitionRun(crashedRunId, { state: "RUNNING", phase: "BUILDING" });
    return res.reservationId!;
  }

  /**
   * Diagnóstico e tratamento de crash pré-F05 (ER-10 / PBI-20):
   * Reconcilia reservas pendentes como UNKNOWN, impede dupla recuperação e transfere saldo remanescente.
   */
  public diagnoseAndRecoverPreF05(crashedRunId: string, activeReservationId?: string): CrashRecoveryDiagnosis {
    const run = this.journal.getRun(crashedRunId);
    if (!run) throw new Error(`Run ${crashedRunId} not found`);
    if (run.isRecovered) throw new Error("Run has already been recovered");

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

export function validateBudgetPolicy(policy: BudgetPolicy): void {
  const fields = ["cycleTokenCap", "builderReservation", "adversarialReservation", "maxIterations",
    "agentTimeoutSeconds", "cycleTimeoutSeconds", "cycleMonetaryCapUsd"] as const;
  if (!policy || fields.some((field) => !Number.isFinite(policy[field]) || policy[field] <= 0)) {
    throw new Error("Budget policy requires explicit positive finite limits");
  }
  if (!Number.isSafeInteger(policy.maxIterations) || policy.builderReservation + policy.adversarialReservation > policy.cycleTokenCap) {
    throw new Error("Budget reservations must fit the shared cycle cap and iterations must be an integer");
  }
  if ([policy.cycleTokenCap, policy.builderReservation, policy.adversarialReservation].some((value) => !Number.isSafeInteger(value)) ||
    (policy.breakerFailureThreshold !== undefined && (!Number.isSafeInteger(policy.breakerFailureThreshold) || policy.breakerFailureThreshold <= 0)) ||
    (policy.breakerWindowSeconds !== undefined && (!Number.isFinite(policy.breakerWindowSeconds) || policy.breakerWindowSeconds <= 0)) ||
    Boolean(policy.breakerFailureThreshold) !== Boolean(policy.breakerWindowSeconds)) {
    throw new Error("Token limits must be integers; circuit breaker requires a positive threshold and window together");
  }
}
