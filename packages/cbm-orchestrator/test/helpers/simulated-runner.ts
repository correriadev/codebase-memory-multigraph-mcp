import type { AgentRunner, AgentInvocationOptions, AgentInvocationResult } from "../../src/orchestrator.ts";
import type { EvidenceRecord } from "../../src/candidate.ts";

/** Deterministic fixture only; never selected by production composition. */
export class SimulatedAgentRunner implements AgentRunner {
  private options: { simulateAlwaysRed?: boolean };
  constructor(options: { simulateAlwaysRed?: boolean } = {}) { this.options = options; }

  async invokeAgent(options: AgentInvocationOptions): Promise<AgentInvocationResult> {
    if (options.role === "builder") {
      return {
        artifactContent: `/* Output for prompt: ${options.prompt.slice(0, 50)} */\nexport function solve(): boolean { return true; }`,
        usage: { inputTokens: 12000, cachedInputTokens: 8000, outputTokens: 500, totalTokens: 12500 },
        costUsd: 0.125,
      };
    }
    const evidenceId = `ev-${options.candidateVersion?.versionId ?? "default"}`;
    const evidence: EvidenceRecord = {
      evidenceId, candidateVersionId: options.candidateVersion?.versionId ?? "unknown",
      artifactDigest: options.candidateVersion?.artifactDigest ?? "unknown", origin: options.agentId,
      producerRole: "adversarial", evidenceType: "test",
      status: this.options.simulateAlwaysRed ? "FAIL" : "PASS", reference: "fixture:simulated-review",
    };
    return {
      verdictStatus: this.options.simulateAlwaysRed ? "RED" : "GREEN",
      evidenceRefs: [evidenceId], evidenceRecords: [evidence],
      usage: { inputTokens: 4000, cachedInputTokens: 3000, outputTokens: 200, totalTokens: 4200 },
      costUsd: 0.042,
    };
  }
}
