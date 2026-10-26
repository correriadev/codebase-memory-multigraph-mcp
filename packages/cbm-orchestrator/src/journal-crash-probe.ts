import { Journal } from "./journal.ts";

const journal = new Journal(process.argv[2]);
journal.createRun({ runId: "crash-probe", projectId: "s0", horizonId: "s0-crash", objective: "Crash durability probe",
  policy: { cycleTokenCap: 100, builderReservation: 50, adversarialReservation: 25, maxIterations: 1,
    agentTimeoutSeconds: 10, cycleTimeoutSeconds: 30, cycleMonetaryCapUsd: 1 } });
journal.reserveBudget("crash-probe", "builder", 50);
journal.recordEffectIntent({ runId: "crash-probe", idempotencyKey: "s0-crash-effect", toolName: "probe", argsDigest: "s0" });
process.stdout.write("READY\n");
setInterval(() => {}, 1000);
