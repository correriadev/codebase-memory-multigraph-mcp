import test from "node:test";
import assert from "node:assert/strict";
import {
  McpProxy,
  type AgentSessionContext,
  type McpToolCallRequest,
  type CbmClientMock,
} from "../src/mcp-proxy.ts";
import { createCandidateVersion, type IndependentVerdict } from "../src/candidate.ts";

test("MCP Proxy / PBI-16: bloqueio estrito de promoção direta para o Builder", async () => {
  let cbmPromoteCalled = false;
  const mockCbm: CbmClientMock = {
    async callTool(tool, args) {
      if (tool === "promote_horizon") {
        cbmPromoteCalled = true;
        return { success: true };
      }
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const builderSession: AgentSessionContext = {
    sessionId: "sess-builder-01",
    role: "builder",
    agentId: "agent-builder-01",
    intentScope: [{ path: "src/math.c", operation: "write" }],
  };

  const req: McpToolCallRequest = {
    toolName: "promote_horizon",
    arguments: { horizon_id: "horizon-h1" },
  };

  const result = await proxy.dispatchToolCall(builderSession, req);

  assert.equal(result.allowed, false);
  assert.equal(result.status, 403);
  assert.match(result.error!, /promotion.*prohibited/i);
  assert.equal(cbmPromoteCalled, false, "CBM nunca deve ser chamado quando proxy recusa promoção");
});

test("MCP Proxy / ADR-005 (Critério 5): recusa estrita de mutações fora do intent_scope", async () => {
  let cbmMutateCalled = false;
  const mockCbm: CbmClientMock = {
    async callTool(tool, args) {
      cbmMutateCalled = true;
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const builderSession: AgentSessionContext = {
    sessionId: "sess-builder-02",
    role: "builder",
    agentId: "agent-builder-02",
    intentScope: [{ path: "src/calculator.c", operation: "write" }],
  };

  // 1. Operação dentro do intent scope (permitida)
  const allowedReq: McpToolCallRequest = {
    toolName: "write_file",
    arguments: { path: "src/calculator.c", content: "// updated code" },
  };
  const resAllowed = await proxy.dispatchToolCall(builderSession, allowedReq);
  assert.equal(resAllowed.allowed, true);
  assert.equal(cbmMutateCalled, true);

  // 2. Operação fora do intent scope (ex: tentar escrever em src/secret.c)
  cbmMutateCalled = false;
  const forbiddenReq: McpToolCallRequest = {
    toolName: "write_file",
    arguments: { path: "src/secret.c", content: "// malicious code" },
  };
  const resForbidden = await proxy.dispatchToolCall(builderSession, forbiddenReq);
  assert.equal(resForbidden.allowed, false);
  assert.equal(resForbidden.status, 403);
  assert.match(resForbidden.error!, /outside intent scope/i);
  assert.equal(cbmMutateCalled, false, "Operação fora do intent scope deve ser recusada no proxy");
});

test("MCP Proxy / C05: Adversarial é restrito a leitura e evidências", async () => {
  let cbmMutateCalled = false;
  const mockCbm: CbmClientMock = {
    async callTool(tool, args) {
      if (tool.includes("write") || tool.includes("promote")) {
        cbmMutateCalled = true;
      }
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const adversarialSession: AgentSessionContext = {
    sessionId: "sess-adv-01",
    role: "adversarial",
    agentId: "agent-adv-01",
    intentScope: [], // Adversarial não tem mutação de código
  };

  // Tentativa de escrita por Adversarial deve ser negada
  const writeReq: McpToolCallRequest = {
    toolName: "write_file",
    arguments: { path: "src/calculator.c", content: "// hack" },
  };
  const resWrite = await proxy.dispatchToolCall(adversarialSession, writeReq);
  assert.equal(resWrite.allowed, false);
  assert.match(resWrite.error!, /Adversarial role is read-only/i);
  assert.equal(cbmMutateCalled, false);

  // Consulta de grafo (permitida para Adversarial)
  const readReq: McpToolCallRequest = {
    toolName: "search_graph",
    arguments: { name_pattern: ".*calc.*" },
  };
  const resRead = await proxy.dispatchToolCall(adversarialSession, readReq);
  assert.equal(resRead.allowed, true);
});

test("MCP Proxy / Gatekeeper: promoção permitida somente com evidência independente verde", async () => {
  let cbmPromoteCalled = false;
  const mockCbm: CbmClientMock = {
    async callTool(tool, args) {
      if (tool === "index_status") {
        return { graphGeneration: 1, baselineDigest: "base-0" };
      }
      if (tool === "promote_horizon") {
        cbmPromoteCalled = true;
        return { success: true, promoted: true, promotedHorizon: args.horizon_id };
      }
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const gatekeeperSession: AgentSessionContext = {
    sessionId: "sess-gate-01",
    role: "gatekeeper",
    agentId: "gatekeeper-core",
    intentScope: [],
  };

  const version = createCandidateVersion({
    projectId: "proj-1",
    runId: "run-1",
    horizonId: "horizon-h1",
    artifactContent: "let x = 1;",
    baselineDigest: "base-0",
    graphGeneration: 1,
    rulesRevision: "r1",
    coverageRevision: "c1",
    policyRevision: "p1",
    contractVersion: "1.0",
  });

  // 1. Gatekeeper tenta promover SEM veredicto verde independente
  const redEvidence: EvidenceRecord = {
    evidenceId: "ev-fail",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "agent-adv-01",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "FAIL",
    reference: "test/reports/report-fail.json",
  };
  const redVerdict: IndependentVerdict = {
    verdictId: "v-red",
    candidateVersionId: version.versionId,
    verifierIdentity: "agent-adv-01",
    status: "RED",
    evidenceRefs: ["ev-fail"],
  };

  const resRed = await proxy.promoteCandidate(gatekeeperSession, version, redVerdict, "builder-01", {
    evidenceRecords: [redEvidence],
  });
  assert.equal(resRed.promoted, false);
  assert.equal(cbmPromoteCalled, false);
  assert.match(resRed.reason!, /(not GREEN|did not PASS)/i);

  // 2. Gatekeeper promove COM veredicto verde independente válido
  const passEvidence: EvidenceRecord = {
    evidenceId: "ev-pass",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "agent-adv-01",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/reports/report-pass.json",
  };
  const greenVerdict: IndependentVerdict = {
    verdictId: "v-green",
    candidateVersionId: version.versionId,
    verifierIdentity: "agent-adv-01",
    status: "GREEN",
    evidenceRefs: ["ev-pass"],
  };

  // Tentativa de promoção sem evidências deve falhar
  const resNoEv = await proxy.promoteCandidate(gatekeeperSession, version, greenVerdict, "builder-01", {
    evidenceRecords: [],
  });
  assert.equal(resNoEv.promoted, false);
  assert.match(resNoEv.reason!, /evidence records/i);

  const resGreen = await proxy.promoteCandidate(gatekeeperSession, version, greenVerdict, "builder-01", {
    evidenceRecords: [passEvidence],
  });
  assert.equal(resGreen.promoted, true);
  assert.equal(cbmPromoteCalled, true);
});

test("MCP Proxy / Segurança: bloqueio estrito de Path Traversal e paths vazios em mutações", async () => {
  const mockCbm: CbmClientMock = {
    async callTool() {
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const builderSession: AgentSessionContext = {
    sessionId: "sess-traversal",
    role: "builder",
    agentId: "agent-builder-01",
    intentScope: [{ path: "src/math.c", operation: "write" }],
  };

  // 1. Path traversal relativo
  const traversalReq: McpToolCallRequest = {
    toolName: "write_file",
    arguments: { path: "../outside.c", content: "// exploit" },
  };
  const res1 = await proxy.dispatchToolCall(builderSession, traversalReq);
  assert.equal(res1.allowed, false);
  assert.match(res1.error!, /path traversal/i);

  // 2. Path vazio ou ausente
  const emptyPathReq: McpToolCallRequest = {
    toolName: "write_file",
    arguments: { path: "", content: "// empty" },
  };
  const res2 = await proxy.dispatchToolCall(builderSession, emptyPathReq);
  assert.equal(res2.allowed, false);
  assert.match(res2.error!, /explicit target path/i);

  // 3. Builder tenta invocar delete_project sem autorização no intent_scope (Point 1)
  const deleteProjReq: McpToolCallRequest = {
    toolName: "delete_project",
    arguments: { project_id: "unallocated-project" },
  };
  const res3 = await proxy.dispatchToolCall(builderSession, deleteProjReq);
  assert.equal(res3.allowed, false);
  assert.equal(res3.status, 403);
  assert.match(res3.error!, /outside intent scope/i);
});

test("MCP Proxy / Segurança: dispatchToolCall genérico NUNCA pode promover, mesmo para gatekeeper", async () => {
  const mockCbm: CbmClientMock = {
    async callTool() {
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const gateSession: AgentSessionContext = {
    sessionId: "sess-gate-direct",
    role: "gatekeeper",
    agentId: "gatekeeper-01",
    intentScope: [],
  };

  const directPromoReq: McpToolCallRequest = {
    toolName: "promote_horizon",
    arguments: { horizon_id: "h1" },
  };

  const res = await proxy.dispatchToolCall(gateSession, directPromoReq);
  assert.equal(res.allowed, false);
  assert.equal(res.status, 403);
  assert.match(res.error!, /Direct promotion via generic tool call is prohibited/i);
});

test("MCP Proxy / Segurança: bloqueio estrito de ferramentas de mutação CBM e tools não-aprovadas para Adversarial (Point 4)", async () => {
  const mockCbm: CbmClientMock = {
    async callTool() {
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);

  const adversarialSession: AgentSessionContext = {
    sessionId: "sess-adv-cbm-mutation",
    role: "adversarial",
    agentId: "adv-01",
    intentScope: [],
  };

  // 1. Adversarial tenta chamar create_horizon
  const res1 = await proxy.dispatchToolCall(adversarialSession, {
    toolName: "create_horizon",
    arguments: { horizon_id: "h2" },
  });
  assert.equal(res1.allowed, false);
  assert.equal(res1.status, 403);
  assert.match(res1.error!, /read-only/i);

  // 2. Adversarial tenta chamar union_session_close
  const res2 = await proxy.dispatchToolCall(adversarialSession, {
    toolName: "union_session_close",
    arguments: { session_id: "s1" },
  });
  assert.equal(res2.allowed, false);
  assert.equal(res2.status, 403);
  assert.match(res2.error!, /read-only/i);

  // 3. Adversarial tenta chamar ferramenta desconhecida fora da whitelist
  const res3 = await proxy.dispatchToolCall(adversarialSession, {
    toolName: "unknown_arbitrary_tool",
    arguments: {},
  });
  assert.equal(res3.allowed, false);
  assert.equal(res3.status, 403);
  assert.match(res3.error!, /read-only/i);
});

test("MCP Proxy / Frescor e Transporte: rejeição por versão obsoleta e preservação de UNKNOWN em falha de rede (Points 3 e 5)", async () => {
  let backendGen = 2; // Geração no backend é mais recente que a versão candidata
  const mockCbm: CbmClientMock = {
    async callTool(tool, args) {
      if (tool === "index_status") {
        return { graphGeneration: backendGen, baselineDigest: "sha256-base-0" };
      }
      if (tool === "promote_horizon") {
        if (args.throwNetworkError) {
          throw new Error("ETIMEDOUT / connection reset by peer");
        }
        return { success: true };
      }
      return { success: true };
    },
  };

  const proxy = new McpProxy(mockCbm);
  const gateSession: AgentSessionContext = {
    sessionId: "sess-gate-freshness",
    role: "gatekeeper",
    agentId: "gate-01",
    intentScope: [],
  };

  const version = createCandidateVersion({
    projectId: "proj-fresh",
    runId: "run-fresh",
    horizonId: "horizon-h1",
    artifactContent: "let y = 2;",
    baselineDigest: "sha256-base-0",
    graphGeneration: 1, // Candidato gerado na geração 1
    rulesRevision: "r1",
    coverageRevision: "c1",
    policyRevision: "p1",
    contractVersion: "1.0",
  });

  const passEvidence: EvidenceRecord = {
    evidenceId: "ev-fresh",
    candidateVersionId: version.versionId,
    artifactDigest: version.artifactDigest,
    origin: "adv-01",
    producerRole: "adversarial",
    evidenceType: "test",
    status: "PASS",
    reference: "test/reports/report-fresh.json",
  };

  const verdict: IndependentVerdict = {
    verdictId: "v-fresh",
    candidateVersionId: version.versionId,
    verifierIdentity: "adv-01",
    status: "GREEN",
    evidenceRefs: ["ev-fresh"],
  };

  // 1. Promoção deve ser rejeitada se o grafo mudou após verificação (Point 3)
  const stalePromo = await proxy.promoteCandidate(gateSession, version, verdict, "builder-01", {
    evidenceRecords: [passEvidence],
  });
  assert.equal(stalePromo.promoted, false);
  assert.equal(stalePromo.status, "FAILED");
  assert.match(stalePromo.reason!, /stale candidate version.*generation/i);

  // 2. Erro de transporte durante promote_horizon deve preservar status UNKNOWN (Point 5)
  backendGen = 1; // Alinha a geração
  const transportErrorPromo = await proxy.promoteCandidate(
    gateSession,
    version,
    verdict,
    "builder-01",
    {
      evidenceRecords: [passEvidence],
      idempotencyKey: "promo-key-net-err",
    }
  );
  // Simula erro de transporte passando flag no client mock
  const netErrClient: CbmClientMock = {
    async callTool(tool) {
      if (tool === "index_status") return { graphGeneration: 1, baselineDigest: "sha256-base-0" };
      if (tool === "promote_horizon") throw new Error("ECONNRESET transport error");
      return { success: true };
    },
  };
  const proxyNetErr = new McpProxy(netErrClient);
  const resNetErr = await proxyNetErr.promoteCandidate(
    gateSession,
    version,
    verdict,
    "builder-01",
    {
      evidenceRecords: [passEvidence],
    }
  );
  assert.equal(resNetErr.promoted, false);
  assert.equal(resNetErr.status, "UNKNOWN");
  assert.match(resNetErr.reason!, /ambiguous/i);
});
