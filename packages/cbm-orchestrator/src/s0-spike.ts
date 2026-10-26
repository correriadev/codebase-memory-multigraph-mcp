import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";

export const S0_CRITERIA = ["endToEnd", "crashRestart", "cancellation", "costPersisted", "intentScopeRefusal"] as const;
export interface S0SpikeReport {
  source: "live";
  configurationDigest: string;
  timestamp: string;
  criteria: Record<typeof S0_CRITERIA[number], "PASS" | "FAIL">;
  invariantsPassed: boolean;
  evidenceRefs: string[];
}

export function configurationDigest(config: unknown): string {
  const hash = createHash("sha256");
  hash.update(JSON.stringify(config));
  const backendCmd = (config as any)?.backend?.command;
  if (typeof backendCmd === "string") {
    try {
      const content = readFileSync(backendCmd);
      hash.update(":binary:");
      hash.update(createHash("sha256").update(content).digest("hex"));
    } catch {
      // If command is not directly a file path or unreadable, preserve config string hash
    }
  }
  return "sha256:" + hash.digest("hex");
}

export function validateS0Report(report: S0SpikeReport, digest: string): void {
  if (!report || report.source !== "live" || report.configurationDigest !== digest ||
    S0_CRITERIA.some((criterion) => report.criteria?.[criterion] !== "PASS") ||
    report.invariantsPassed !== true || !Array.isArray(report.evidenceRefs) || !report.evidenceRefs.length ||
    report.evidenceRefs.some((reference) => typeof reference !== "string" || !reference)) {
    throw new Error("S0_APPROVED requires live passing evidence for the current configuration");
  }
  for (const reference of report.evidenceRefs) readFileSync(reference);
}

/** Operator-only CLI step; this operation is intentionally absent from MCP tools. */
export function approveS0(reportPath: string, approvalPath: string, digest: string): void {
  const report = JSON.parse(readFileSync(reportPath, "utf8"));
  validateS0Report(report, digest);
  writeFileSync(approvalPath, JSON.stringify({ status: "S0_APPROVED", adr005Status: "ACCEPTED",
    configurationDigest: digest, reportPath, reportDigest: configurationDigest(report), approvedAt: new Date().toISOString() }, null, 2));
}

export function loadS0Approval(approvalPath: string, digest: string): void {
  const approval = JSON.parse(readFileSync(approvalPath, "utf8"));
  if (approval.status !== "S0_APPROVED" || approval.adr005Status !== "ACCEPTED" || approval.configurationDigest !== digest) {
    throw new Error("S0 approval is missing or belongs to another configuration");
  }
  const report = JSON.parse(readFileSync(approval.reportPath, "utf8"));
  validateS0Report(report, digest);
  if (configurationDigest(report) !== approval.reportDigest) throw new Error("S0 evidence changed after operator approval");
}
