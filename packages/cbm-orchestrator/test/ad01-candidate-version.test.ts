import test from "node:test";
import assert from "node:assert/strict";
import {
  createCandidateVersion,
  validateEvidenceAttachment,
  validateIndependentVerdict,
  type CandidateVersionInput,
  type EvidenceRecord,
  type IndependentVerdict,
} from "../src/candidate.ts";

const BASE_INPUT: CandidateVersionInput = {
  projectId: "cbm-project-test",
  runId: "run-001",
  horizonId: "horizon-h1",
  artifactContent: "function add(a: number, b: number) { return a + b; }",
  baselineDigest: "sha256-baseline-000000000000000000000000000000000000000000000000000000000000",
  graphGeneration: 1,
  rulesRevision: "rules-v1.0.0",
  coverageRevision: "cov-gen1-100",
  policyRevision: "policy-s0-v1",
  contractVersion: "1.0",
};

test("AD-01 / C01: criação determinística de CandidateVersion", () => {
  const v1 = createCandidateVersion(BASE_INPUT);
  const v2 = createCandidateVersion(BASE_INPUT);

  assert.equal(v1.versionId, v2.versionId);
  assert.equal(v1.artifactDigest, v2.artifactDigest);
  assert.ok(v1.versionId.startsWith("sha256:"));
  assert.ok(v1.artifactDigest.startsWith("sha256:"));
});

test("AD-01 / C01: qualquer alteração de artefato produz versão candidata distinta", () => {
  const v1 = createCandidateVersion(BASE_INPUT);
  const vModifiedArtifact = createCandidateVersion({
    ...BASE_INPUT,
    artifactContent: "function add(a: number, b: number) { return a + b + 1; }",
  });

  assert.notEqual(v1.artifactDigest, vModifiedArtifact.artifactDigest);
  assert.notEqual(v1.versionId, vModifiedArtifact.versionId);
});

test("AD-01 / C01: alteração de revisão de conhecimento (grafo/regras/cobertura) produz versão distinta", () => {
  const v1 = createCandidateVersion(BASE_INPUT);

  const vNewGraph = createCandidateVersion({ ...BASE_INPUT, graphGeneration: 2 });
  assert.notEqual(v1.versionId, vNewGraph.versionId);
  // O artefato em si não mudou, então o digest do artefato permanece o mesmo
  assert.equal(v1.artifactDigest, vNewGraph.artifactDigest);

  const vNewRules = createCandidateVersion({ ...BASE_INPUT, rulesRevision: "rules-v1.0.1" });
  assert.notEqual(v1.versionId, vNewRules.versionId);

  const vNewCoverage = createCandidateVersion({ ...BASE_INPUT, coverageRevision: "cov-gen1-101" });
  assert.notEqual(v1.versionId, vNewCoverage.versionId);
});

test("AD-01 / C04: vinculação válida de EvidenceRecord com CandidateVersion", () => {
  const version = createCandidateVersion(BASE_INPUT);

  const validEvidence: EvidenceRecord = {
    evidenceId: "ev-001",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "codex-test-runner",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/fixtures/report-001.json",
  };

  const validation = validateEvidenceAttachment(version, validEvidence);
  assert.equal(validation.valid, true);
  assert.equal(validation.reason, undefined);
});

test("AD-01 / C04: rejeição de evidência com versão ou artifactDigest incompatível", () => {
  const version = createCandidateVersion(BASE_INPUT);

  // Evidência com candidateVersionId diferente
  const mismatchedVersionEvidence: EvidenceRecord = {
    evidenceId: "ev-002",
    candidateVersionId: "sha256:wrongversion",
    artifactDigest: version.artifactDigest,
    origin: "codex-test-runner",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/fixtures/report-002.json",
  };
  const res1 = validateEvidenceAttachment(version, mismatchedVersionEvidence);
  assert.equal(res1.valid, false);
  assert.match(res1.reason!, /versionId/i);

  // Evidência com artifactDigest diferente
  const mismatchedDigestEvidence: EvidenceRecord = {
    evidenceId: "ev-003",
    candidateVersionId: version.versionId,
    artifactDigest: "sha256:wrongdigest",
    origin: "codex-test-runner",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/fixtures/report-003.json",
  };
  const res2 = validateEvidenceAttachment(version, mismatchedDigestEvidence);
  assert.equal(res2.valid, false);
  assert.match(res2.reason!, /artifactDigest/i);
});

test("AD-01 / AD-S04: rejeição de evidência malformada ou sem origem verificável", () => {
  const version = createCandidateVersion(BASE_INPUT);

  // Sem origem
  const noOrigin: EvidenceRecord = {
    evidenceId: "ev-004",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/report.json",
  };
  assert.equal(validateEvidenceAttachment(version, noOrigin).valid, false);

  // Sem referência consultável
  const noRef: EvidenceRecord = {
    evidenceId: "ev-005",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "runner-auth",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "",
  };
  assert.equal(validateEvidenceAttachment(version, noRef).valid, false);
});

test("AD-01 / C04: validação de veredicto independente contra auto-verificação do Builder", () => {
  const version = createCandidateVersion(BASE_INPUT);
  const builderId = "agent-builder-01";

  const passEvidence: EvidenceRecord = {
    evidenceId: "ev-001",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "agent-adversarial-02",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/reports/report-001.json",
  };

  // Veredicto onde o verificador é o próprio builder (deve ser rejeitado)
  const selfVerdict: IndependentVerdict = {
    verdictId: "verd-001",
    candidateVersionId: version.versionId,
    verifierIdentity: builderId,
    status: "GREEN",
    evidenceRefs: ["ev-001"],
  };
  const selfRes = validateIndependentVerdict(version, selfVerdict, builderId, [passEvidence]);
  assert.equal(selfRes.valid, false);
  assert.match(selfRes.reason!, /builder cannot issue independent verdict/i);

  // Veredicto emitido por verificador independente com evidência válida
  const validVerdict: IndependentVerdict = {
    verdictId: "verd-002",
    candidateVersionId: version.versionId,
    verifierIdentity: "agent-adversarial-02",
    status: "GREEN",
    evidenceRefs: ["ev-001"],
  };
  const validRes = validateIndependentVerdict(version, validVerdict, builderId, [passEvidence]);
  assert.equal(validRes.valid, true);
  assert.equal(validRes.canPromote, true);

  // Veredicto sem lista de evidências verificadas é recusado obrigatoriamente
  const noEvidenceRes = validateIndependentVerdict(version, validVerdict, builderId, []);
  assert.equal(noEvidenceRes.valid, false);
  assert.equal(noEvidenceRes.canPromote, false);
  assert.match(noEvidenceRes.reason!, /evidence records cannot be empty/i);

  // Veredicto não-verde (RED, INCONCLUSIVE, UNAVAILABLE) não autoriza promoção
  const redVerdict: IndependentVerdict = {
    ...validVerdict,
    verdictId: "verd-003",
    status: "RED",
  };
  const redRes = validateIndependentVerdict(version, redVerdict, builderId, [passEvidence]);
  assert.equal(redRes.valid, true);
  assert.equal(redRes.canPromote, false);
});
