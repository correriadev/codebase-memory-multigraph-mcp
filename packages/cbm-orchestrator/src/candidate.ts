import { createHash } from "node:crypto";

export interface CandidateVersionInput {
  projectId: string;
  runId: string;
  horizonId: string;
  artifactContent: string;
  baselineDigest: string;
  graphGeneration: string | number;
  rulesRevision: string;
  coverageRevision: string;
  policyRevision: string;
  contractVersion: string;
}

export interface CandidateVersionRef {
  versionId: string;
  projectId: string;
  runId: string;
  horizonId: string;
  artifactDigest: string;
  baselineDigest: string;
  graphGeneration: string | number;
  rulesRevision: string;
  coverageRevision: string;
  policyRevision: string;
  contractVersion: string;
}

export type ProducerRole = "builder" | "adversarial" | "gatekeeper" | "meta";
export type EvidenceType = "test" | "review" | "coverage" | "effect" | "seam";
export type EvidenceStatus = "PASS" | "FAIL" | "INCONCLUSIVE" | "UNAVAILABLE";
export type VerdictStatus = "GREEN" | "RED" | "INCONCLUSIVE" | "UNAVAILABLE";

export interface EvidenceRecord {
  evidenceId: string;
  candidateVersionId: string;
  artifactDigest: string;
  origin: string;
  producerRole: ProducerRole;
  evidenceType: EvidenceType;
  status: EvidenceStatus;
  reference: string;
}

export interface IndependentVerdict {
  verdictId: string;
  candidateVersionId: string;
  verifierIdentity: string;
  status: VerdictStatus;
  evidenceRefs: string[];
}

function sha256(data: string): string {
  return "sha256:" + createHash("sha256").update(data, "utf8").digest("hex");
}

/**
 * Cria a referência imutável CandidateVersion com identificação determinística da tupla C01.
 */
export function createCandidateVersion(input: CandidateVersionInput): CandidateVersionRef {
  if (!input.projectId || !input.runId || !input.horizonId) {
    throw new Error("Identidade C01 incompleta: projectId, runId e horizonId são obrigatórios.");
  }

  const artifactDigest = sha256(input.artifactContent + ":" + input.baselineDigest);

  // Serialização canônica determinística para a tupla C01 (vinculação atômica da candidata)
  const tuplePayload = JSON.stringify({
    artifactDigest,
    baselineDigest: input.baselineDigest,
    contractVersion: input.contractVersion,
    coverageRevision: input.coverageRevision,
    graphGeneration: input.graphGeneration,
    horizonId: input.horizonId,
    policyRevision: input.policyRevision,
    projectId: input.projectId,
    rulesRevision: input.rulesRevision,
    runId: input.runId,
  });

  const versionId = sha256(tuplePayload);

  return {
    versionId,
    projectId: input.projectId,
    runId: input.runId,
    horizonId: input.horizonId,
    artifactDigest,
    baselineDigest: input.baselineDigest,
    graphGeneration: input.graphGeneration,
    rulesRevision: input.rulesRevision,
    coverageRevision: input.coverageRevision,
    policyRevision: input.policyRevision,
    contractVersion: input.contractVersion,
  };
}

/**
 * Valida a vinculação de um EvidenceRecord a uma versão candidata (C04).
 */
export function validateEvidenceAttachment(
  version: CandidateVersionRef,
  evidence: EvidenceRecord
): { valid: boolean; reason?: string } {
  if (!evidence.origin || evidence.origin.trim() === "") {
    return { valid: false, reason: "Evidence origin is missing or unverifiable" };
  }
  if (!evidence.reference || evidence.reference.trim() === "") {
    return { valid: false, reason: "Evidence reference is missing or empty" };
  }
  if (evidence.candidateVersionId !== version.versionId) {
    return {
      valid: false,
      reason: `Evidence candidateVersionId '${evidence.candidateVersionId}' does not match versionId '${version.versionId}'`,
    };
  }
  if (evidence.artifactDigest !== version.artifactDigest) {
    return {
      valid: false,
      reason: `Evidence artifactDigest '${evidence.artifactDigest}' does not match version '${version.artifactDigest}'`,
    };
  }

  return { valid: true };
}

/**
 * Valida um veredicto independente contra a versão, papel do builder e registros de evidência (C04 / INV-01 / INV-02).
 */
export function validateIndependentVerdict(
  version: CandidateVersionRef,
  verdict: IndependentVerdict,
  builderIdentity: string,
  evidenceRecords: EvidenceRecord[]
): { valid: boolean; canPromote: boolean; reason?: string } {
  if (verdict.candidateVersionId !== version.versionId) {
    return {
      valid: false,
      canPromote: false,
      reason: `Verdict candidateVersionId does not match versionId '${version.versionId}'`,
    };
  }
  if (verdict.verifierIdentity === builderIdentity) {
    return {
      valid: false,
      canPromote: false,
      reason: "Builder cannot issue independent verdict for its own candidate (INV-02)",
    };
  }
  if (!verdict.evidenceRefs || verdict.evidenceRefs.length === 0) {
    return {
      valid: false,
      canPromote: false,
      reason: "Independent verdict must reference at least one verified evidence record",
    };
  }
  if (!evidenceRecords || evidenceRecords.length === 0) {
    return {
      valid: false,
      canPromote: false,
      reason: "Evidence records cannot be empty and are mandatory for independent verdict verification",
    };
  }

  // Validação estrita de cada evidência referenciada contra a lista de evidências verificadas
  for (const ref of verdict.evidenceRefs) {
    const rec = evidenceRecords.find((e) => e.evidenceId === ref);
    if (!rec) {
      return {
        valid: false,
        canPromote: false,
        reason: `Referenced evidence '${ref}' was not found in verified evidence records`,
      };
    }
    const check = validateEvidenceAttachment(version, rec);
    if (rec.origin !== verdict.verifierIdentity || rec.producerRole !== "adversarial") {
      return { valid: false, canPromote: false, reason: "Evidence origin must be the independent adversarial verifier" };
    }
    if (!check.valid || rec.status !== "PASS") {
      return {
        valid: false,
        canPromote: false,
        reason: `Referenced evidence '${ref}' is invalid or did not PASS: ${check.reason || rec.status}`,
      };
    }
  }

  const canPromote = verdict.status === "GREEN";

  return {
    valid: true,
    canPromote,
    reason: canPromote ? undefined : `Verdict status '${verdict.status}' is not GREEN`,
  };
}
