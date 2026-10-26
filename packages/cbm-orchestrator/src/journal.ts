import { DatabaseSync } from "node:sqlite";

export type RunState = "CREATED" | "RUNNING" | "PAUSED" | "COMPLETED";

export type RunPhase =
  | "PLANNING"
  | "BUILDING"
  | "AWAITING_PLANNING"
  | "VERIFYING"
  | "AWAITING_DECISION"
  | "PROMOTING"
  | "RECOVERING"
  | "CLEANING_UP";

export type TerminationReason =
  | "ACCEPTED"
  | "REJECTED"
  | "VERIFIER_UNAVAILABLE"
  | "BUDGET_EXHAUSTED"
  | "CANCELLED"
  | "FAILED"
  | "RECOVERY_REQUIRED";

export interface BudgetPolicy {
  cycleTokenCap: number;
  builderReservation: number;
  adversarialReservation: number;
  maxIterations: number;
  agentTimeoutSeconds: number;
  cycleTimeoutSeconds: number;
  cycleMonetaryCapUsd: number;
  breakerFailureThreshold?: number;
  breakerWindowSeconds?: number;
}

export interface CreateRunParams {
  runId: string;
  projectId: string;
  horizonId: string;
  objective: string;
  policy: BudgetPolicy;
  parentRunId?: string;
  initialAvailableTokens?: number;
  initialTotalCostUsd?: number;
  inheritUnknownConsumption?: boolean;
}

export interface RunRecord {
  runId: string;
  projectId: string;
  horizonId: string;
  objective: string;
  state: RunState;
  phase: RunPhase;
  terminationReason: TerminationReason | null;
  parentRunId: string | null;
  isRecovered: boolean;
  policyJson: string;
  createdAt: string;
  updatedAt: string;
}

export interface LedgerRecord {
  runId: string;
  tokenCap: number;
  reservedTokens: number;
  confirmedTokens: number;
  availableTokens: number;
  totalCostUsd: number;
  hasUnknownConsumption: boolean;
}

export interface ReconcileBudgetParams {
  runId: string;
  reservationId: string;
  actualTokens: number | null;
  costUsd: number | null;
  status: "MEASURED" | "UNKNOWN" | "ESTIMATED";
}

export interface EffectRecord {
  idempotencyKey: string;
  runId: string;
  toolName: string;
  argsDigest: string;
  status: "PENDING" | "CONFIRMED" | "FAILED" | "UNKNOWN";
  result: string | null;
  createdAt: string;
}

export class Journal {
  private db: DatabaseSync;

  constructor(dbPath: string) {
    this.db = new DatabaseSync(dbPath);
    this.initPragmas();
    this.initSchema();
  }

  private initPragmas(): void {
    this.db.exec("PRAGMA journal_mode = WAL;");
    this.db.exec("PRAGMA synchronous = FULL;");
    this.db.exec("PRAGMA foreign_keys = ON;");
    this.db.exec("PRAGMA busy_timeout = 5000;");
  }

  private initSchema(): void {
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS execution_events (
        event_id INTEGER PRIMARY KEY AUTOINCREMENT,
        run_id TEXT NOT NULL,
        kind TEXT NOT NULL,
        payload TEXT NOT NULL,
        created_at TEXT NOT NULL
      );
      CREATE TABLE IF NOT EXISTS runs (
        run_id TEXT PRIMARY KEY,
        project_id TEXT NOT NULL,
        horizon_id TEXT NOT NULL,
        objective TEXT NOT NULL,
        state TEXT NOT NULL,
        phase TEXT NOT NULL,
        termination_reason TEXT,
        parent_run_id TEXT,
        is_recovered INTEGER NOT NULL DEFAULT 0,
        policy_json TEXT NOT NULL,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL
      );

      CREATE TABLE IF NOT EXISTS ledger (
        run_id TEXT PRIMARY KEY,
        token_cap INTEGER NOT NULL,
        reserved_tokens INTEGER NOT NULL,
        confirmed_tokens INTEGER NOT NULL,
        available_tokens INTEGER NOT NULL,
        total_cost_usd REAL NOT NULL DEFAULT 0.0,
        has_unknown_consumption INTEGER NOT NULL DEFAULT 0,
        FOREIGN KEY (run_id) REFERENCES runs(run_id)
      );

      CREATE TABLE IF NOT EXISTS reservations (
        reservation_id TEXT PRIMARY KEY,
        run_id TEXT NOT NULL,
        role TEXT NOT NULL,
        tokens INTEGER NOT NULL,
        status TEXT NOT NULL,
        created_at TEXT NOT NULL,
        FOREIGN KEY (run_id) REFERENCES runs(run_id)
      );

      CREATE TABLE IF NOT EXISTS effects (
        idempotency_key TEXT PRIMARY KEY,
        run_id TEXT NOT NULL,
        tool_name TEXT NOT NULL,
        args_digest TEXT NOT NULL,
        status TEXT NOT NULL,
        result TEXT,
        created_at TEXT NOT NULL,
        FOREIGN KEY (run_id) REFERENCES runs(run_id)
      );

      CREATE TABLE IF NOT EXISTS project_gates (
        project_id TEXT PRIMARY KEY,
        gate_name TEXT NOT NULL,
        status TEXT NOT NULL,
        report_digest TEXT,
        updated_at TEXT NOT NULL
      );
    `);
  }

  public createRun(params: CreateRunParams): RunRecord {
    const now = new Date().toISOString();
    const policyJson = JSON.stringify(params.policy);
    const initialCap = params.policy.cycleTokenCap;
    const initialAvailable = params.initialAvailableTokens ?? initialCap;
    const initialCost = params.initialTotalCostUsd ?? 0.0;
    const initialUnknown = params.inheritUnknownConsumption ? 1 : 0;

    this.db.exec("BEGIN IMMEDIATE;");
    try {
      if (params.parentRunId) {
        const checkParent = this.db.prepare("SELECT is_recovered FROM runs WHERE run_id = ?");
        const parentRow = checkParent.get(params.parentRunId) as any;
        if (parentRow && parentRow.is_recovered === 1) {
          throw new Error(`Parent run '${params.parentRunId}' has already been recovered; duplicate recovery rejected`);
        }
        const markParent = this.db.prepare("UPDATE runs SET is_recovered = 1 WHERE run_id = ?");
        markParent.run(params.parentRunId);
      }

      const insertRun = this.db.prepare(`
        INSERT INTO runs (
          run_id, project_id, horizon_id, objective, state, phase, termination_reason, parent_run_id, is_recovered, policy_json, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, ?)
      `);

      insertRun.run(
        params.runId,
        params.projectId,
        params.horizonId,
        params.objective,
        "CREATED",
        "PLANNING",
        null,
        params.parentRunId ?? null,
        policyJson,
        now,
        now
      );

      const insertLedger = this.db.prepare(`
        INSERT INTO ledger (
          run_id, token_cap, reserved_tokens, confirmed_tokens, available_tokens, total_cost_usd, has_unknown_consumption
        ) VALUES (?, ?, 0, 0, ?, ?, ?)
      `);

      insertLedger.run(params.runId, initialCap, initialAvailable, initialCost, initialUnknown);
      this.recordEvent(params.runId, "RUN_CREATED", { objective: params.objective, policy: params.policy, parentRunId: params.parentRunId });
      this.db.exec("COMMIT;");
    } catch (err) {
      this.db.exec("ROLLBACK;");
      throw err;
    }

    return {
      runId: params.runId,
      projectId: params.projectId,
      horizonId: params.horizonId,
      objective: params.objective,
      state: "CREATED",
      phase: "PLANNING",
      terminationReason: null,
      parentRunId: params.parentRunId ?? null,
      isRecovered: false,
      policyJson,
      createdAt: now,
      updatedAt: now,
    };
  }

  public getRun(runId: string): RunRecord | null {
    const stmt = this.db.prepare("SELECT * FROM runs WHERE run_id = ?");
    const row = stmt.get(runId) as any;
    if (!row) return null;

    return {
      runId: row.run_id,
      projectId: row.project_id,
      horizonId: row.horizon_id,
      objective: row.objective,
      state: row.state as RunState,
      phase: row.phase as RunPhase,
      terminationReason: (row.termination_reason as TerminationReason) || null,
      parentRunId: row.parent_run_id || null,
      isRecovered: Boolean(row.is_recovered),
      policyJson: row.policy_json,
      createdAt: row.created_at,
      updatedAt: row.updated_at,
    };
  }

  public transitionRun(
    runId: string,
    update: { state?: RunState; phase?: RunPhase; terminationReason?: TerminationReason }
  ): void {
    this.db.exec("BEGIN IMMEDIATE;");
    try {
      const current = this.getRun(runId);
      if (!current) throw new Error(`Run ${runId} not found`);
      const newState = update.state ?? current.state;
      const allowed: Record<RunState, RunState[]> = { CREATED: ["RUNNING"], RUNNING: ["PAUSED", "COMPLETED"], PAUSED: ["RUNNING", "COMPLETED"], COMPLETED: [] };
      if (current.state === "COMPLETED" || (newState !== current.state && !allowed[current.state].includes(newState))) {
        throw new Error(`Invalid execution transition ${current.state} -> ${newState}`);
      }
      const newPhase = update.phase ?? current.phase;
      const newReason = update.terminationReason !== undefined ? update.terminationReason : current.terminationReason;
      if (newState === "COMPLETED" && !newReason) throw new Error("Completion requires a termination reason");
      const result = this.db.prepare(`UPDATE runs SET state = ?, phase = ?, termination_reason = ?, updated_at = ?
        WHERE run_id = ? AND state = ? AND phase = ? AND updated_at = ?`)
        .run(newState, newPhase, newReason, new Date().toISOString(), runId, current.state, current.phase, current.updatedAt);
      if (result.changes !== 1) throw new Error("Concurrent execution transition rejected");
      this.recordEvent(runId, "STATE_CHANGED", { from: current.state, state: newState, phase: newPhase, terminationReason: newReason });
      this.db.exec("COMMIT;");
    } catch (error) { this.db.exec("ROLLBACK;"); throw error; }
  }

  public getLedger(runId: string): LedgerRecord {
    const stmt = this.db.prepare("SELECT * FROM ledger WHERE run_id = ?");
    const row = stmt.get(runId) as any;
    if (!row) throw new Error(`Ledger for run ${runId} not found`);

    return {
      runId: row.run_id,
      tokenCap: Number(row.token_cap),
      reservedTokens: Number(row.reserved_tokens),
      confirmedTokens: Number(row.confirmed_tokens),
      availableTokens: Number(row.available_tokens),
      totalCostUsd: Number(row.total_cost_usd ?? 0.0),
      hasUnknownConsumption: Boolean(row.has_unknown_consumption),
    };
  }

  /**
   * Reserva atômica de orçamento com proteção contra corridas concorrentes (C03).
   */
  public reserveBudget(
    runId: string,
    role: string,
    tokens: number
  ): { success: boolean; reservationId?: string; reason?: string } {
    this.db.exec("BEGIN IMMEDIATE;");
    try {
      if (!Number.isSafeInteger(tokens) || tokens <= 0) {
        this.db.exec("ROLLBACK;");
        return {
          success: false,
          reason: "Reservation tokens must be a positive number greater than zero",
        };
      }

      const ledger = this.getLedger(runId);

      if (ledger.hasUnknownConsumption) {
        this.db.exec("ROLLBACK;");
        return {
          success: false,
          reason: "Unknown consumption present in ledger; autonomous dispatches blocked (INV-07)",
        };
      }

      if (tokens > ledger.availableTokens) {
        this.db.exec("ROLLBACK;");
        return {
          success: false,
          reason: `Insufficient budget: requested reservation of ${tokens} tokens exceeds available balance of ${ledger.availableTokens}`,
        };
      }

      const updateStmt = this.db.prepare(`
        UPDATE ledger
        SET reserved_tokens = reserved_tokens + ?, available_tokens = available_tokens - ?
        WHERE run_id = ? AND has_unknown_consumption = 0 AND available_tokens >= ?
      `);

      const result = updateStmt.run(tokens, tokens, runId, tokens);
      if (result.changes !== 1) {
        this.db.exec("ROLLBACK;");
        return {
          success: false,
          reason: "Concurrent update prevented budget reservation",
        };
      }

      const reservationId = `res-${runId}-${Date.now()}-${Math.floor(Math.random() * 1000)}`;
      const now = new Date().toISOString();
      const insertRes = this.db.prepare(`
        INSERT INTO reservations (reservation_id, run_id, role, tokens, status, created_at)
        VALUES (?, ?, ?, ?, 'ACTIVE', ?)
      `);
      insertRes.run(reservationId, runId, role, tokens, now);

      this.db.exec("COMMIT;");
      return { success: true, reservationId };
    } catch (err: any) {
      this.db.exec("ROLLBACK;");
      return { success: false, reason: err.message };
    }
  }

  /**
   * Reconciliação atômica de orçamento vinculada à reserva única (C03).
   */
  public reconcileBudget(params: ReconcileBudgetParams): void {
    if (params.status !== "UNKNOWN" && (!Number.isSafeInteger(params.actualTokens) || params.actualTokens! < 0 ||
      !Number.isFinite(params.costUsd) || params.costUsd! < 0)) {
      throw new Error("Negative or invalid measured tokens/cost rejected; finite nonnegative values are required");
    }
    if (
      (params.actualTokens !== undefined && params.actualTokens !== null && params.actualTokens < 0) ||
      (params.costUsd !== undefined && params.costUsd !== null && params.costUsd < 0)
    ) {
      throw new Error("Negative actualTokens or costUsd rejected");
    }

    this.db.exec("BEGIN IMMEDIATE;");
    try {
      const checkRes = this.db.prepare("SELECT run_id, tokens, status FROM reservations WHERE reservation_id = ?");
      const row = checkRes.get(params.reservationId) as any;
      if (!row) {
        throw new Error(`Reservation '${params.reservationId}' does not exist`);
      }
      if (row.run_id !== params.runId) {
        throw new Error(`Reservation '${params.reservationId}' belongs to run '${row.run_id}', not '${params.runId}'`);
      }
      if (row.status === "RECONCILED") {
        this.db.exec("ROLLBACK;");
        return; // Previne devolução duplicada
      }

      const markRes = this.db.prepare("UPDATE reservations SET status = 'RECONCILED' WHERE reservation_id = ?");
      markRes.run(params.reservationId);

      const reservedTokens = Number(row.tokens);
      const ledger = this.getLedger(params.runId);

      if (params.status === "UNKNOWN") {
        const newReserved = Math.max(0, ledger.reservedTokens - reservedTokens);
        const stmt = this.db.prepare(`
          UPDATE ledger
          SET reserved_tokens = ?, has_unknown_consumption = 1
          WHERE run_id = ?
        `);
        stmt.run(newReserved, params.runId);
        this.db.exec("COMMIT;");
        return;
      }

      const actual = params.actualTokens ?? 0;
      const cost = params.costUsd ?? 0.0;
      const newReserved = Math.max(0, ledger.reservedTokens - reservedTokens);
      const newConfirmed = ledger.confirmedTokens + actual;
      const newAvailable = ledger.availableTokens + (reservedTokens - actual);
      const newCost = Number((ledger.totalCostUsd + cost).toFixed(6));

      const stmt = this.db.prepare(`
        UPDATE ledger
        SET reserved_tokens = ?, confirmed_tokens = ?, available_tokens = ?, total_cost_usd = ?
        WHERE run_id = ?
      `);

      stmt.run(newReserved, newConfirmed, newAvailable, newCost, params.runId);
      this.db.exec("COMMIT;");
    } catch (err) {
      this.db.exec("ROLLBACK;");
      throw err;
    }
  }

  public getActiveReservations(
    runId: string
  ): Array<{ reservation_id: string; role: string; tokens: number }> {
    const stmt = this.db.prepare(
      "SELECT reservation_id, role, tokens FROM reservations WHERE run_id = ? AND status = 'ACTIVE'"
    );
    return stmt.all(runId) as any;
  }

  public recordEffectIntent(params: {
    runId: string;
    idempotencyKey: string;
    toolName: string;
    argsDigest: string;
  }): { isNew: boolean } {
    const existing = this.getEffect(params.idempotencyKey);
    if (existing) {
      return { isNew: false };
    }

    const now = new Date().toISOString();
    const stmt = this.db.prepare(`
      INSERT INTO effects (idempotency_key, run_id, tool_name, args_digest, status, result, created_at)
      VALUES (?, ?, ?, ?, 'PENDING', NULL, ?)
    `);

    stmt.run(params.idempotencyKey, params.runId, params.toolName, params.argsDigest, now);
    return { isNew: true };
  }

  public confirmEffectResult(
    idempotencyKey: string,
    status: "CONFIRMED" | "FAILED" | "UNKNOWN",
    result: string
  ): void {
    const stmt = this.db.prepare(`
      UPDATE effects
      SET status = ?, result = ?
      WHERE idempotency_key = ?
    `);

    stmt.run(status, result, idempotencyKey);
  }

  public getEffect(idempotencyKey: string): EffectRecord | null {
    const stmt = this.db.prepare("SELECT * FROM effects WHERE idempotency_key = ?");
    const row = stmt.get(idempotencyKey) as any;
    if (!row) return null;

    return {
      idempotencyKey: row.idempotency_key,
      runId: row.run_id,
      toolName: row.tool_name,
      argsDigest: row.args_digest,
      status: row.status,
      result: row.result,
      createdAt: row.created_at,
    };
  }

  public recordEvent(runId: string, kind: string, payload: unknown): void {
    this.db.prepare("INSERT INTO execution_events (run_id, kind, payload, created_at) VALUES (?, ?, ?, ?)")
      .run(runId, kind, JSON.stringify(payload), new Date().toISOString());
  }

  public getEvents(runId: string): unknown[] {
    return this.db.prepare("SELECT event_id, kind, payload, created_at FROM execution_events WHERE run_id = ? ORDER BY event_id")
      .all(runId).map((row: any) => ({ ...row, payload: JSON.parse(row.payload) }));
  }

  public getEffects(runId: string): EffectRecord[] {
    return this.db.prepare("SELECT idempotency_key FROM effects WHERE run_id = ?").all(runId)
      .map((row: any) => this.getEffect(row.idempotency_key)!);
  }

  public recordS0Approval(projectId: string, reportDigest: string): void {
    const stmt = this.db.prepare(`
      INSERT OR REPLACE INTO project_gates (project_id, gate_name, status, report_digest, updated_at)
      VALUES (?, 'S0', 'S0_APPROVED', ?, ?)
    `);
    stmt.run(projectId, reportDigest, new Date().toISOString());
  }

  public hasS0Approval(projectId: string, configurationDigest?: string): boolean {
    const stmt = this.db.prepare(
      "SELECT status, report_digest FROM project_gates WHERE project_id = ? AND gate_name = 'S0'"
    );
    const row = stmt.get(projectId) as any;
    return Boolean(row && row.status === "S0_APPROVED" && (!configurationDigest || row.report_digest === configurationDigest));
  }

  public listRuns(limit = 20): RunRecord[] {
    const stmt = this.db.prepare(
      "SELECT run_id as runId, project_id as projectId, horizon_id as horizonId, objective, state, phase, termination_reason as terminationReason, parent_run_id as parentRunId, is_recovered as isRecovered, policy_json as policyJson, created_at as createdAt, updated_at as updatedAt FROM runs ORDER BY created_at DESC LIMIT ?"
    );
    const rows = stmt.all(limit) as any[];
    return rows.map((r) => ({
      ...r,
      isRecovered: Boolean(r.isRecovered),
    }));
  }

  public close(): void {
    this.db.close();
  }
}
