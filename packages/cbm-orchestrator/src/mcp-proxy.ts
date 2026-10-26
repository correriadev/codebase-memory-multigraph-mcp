import path from "node:path";
import { McpRemoteError } from "./mcp-transport.ts";
import {
  validateIndependentVerdict,
  type CandidateVersionRef,
  type IndependentVerdict,
  type EvidenceRecord,
  type ProducerRole,
} from "./candidate.ts";

export interface IntentScopePair {
  path: string;
  operation: "read" | "write" | "delete";
}

export interface AgentSessionContext {
  sessionId: string;
  role: ProducerRole;
  agentId: string;
  intentScope: IntentScopePair[];
}

export interface McpToolCallRequest {
  toolName: string;
  arguments: Record<string, any>;
}

export interface McpToolCallResult {
  allowed: boolean;
  status?: number;
  error?: string;
  data?: any;
}

export interface CbmClient {
  callTool(tool: string, args: Record<string, any>): Promise<any>;
}

/** @deprecated Use CbmClient. Kept for existing test adapters. */
export type CbmClientMock = CbmClient;

export const READ_ONLY_TOOLS = new Set([
  "read_file",
  "search_graph",
  "query_graph",
  "trace_path",
  "get_code_snippet",
  "get_file_outline",
  "get_graph_schema",
  "compare_graphs",
  "get_architecture",
  "search_code",
  "list_projects",
  "index_status",
  "check_index_coverage",
  "detect_changes",
  "list_horizons",
  "union_session_get",
  "classify_activity",
  "validate_provenance",
  "theme_lookup",
  "theme_search",
  "theme_list",
  "theme_graph_search",
  "theme_graph_query",
  "graph.query",
  "graph.impact",
  "presence.who",
  "presence.focus",
  "workflow.read",
]);

export const MUTATION_TOOLS_OPS: Record<string, "write" | "delete"> = {
  write_file: "write",
  edit_file: "write",
  delete_file: "delete",
  delete_project: "delete",
  "node.edit": "write",
  "changeset.commit": "write",
  "changeset.open": "write",
  "changeset.claim": "write",
  "changeset.abort": "write",
  "authority.flip": "write",
  create_horizon: "write",
  sync_horizon_spec: "write",
  union_session_open: "write",
  union_session_close: "write",
  union_record_action: "write",
  binding_claim: "write",
  founding_propose: "write",
  founding_decide: "write",
  contest_verify: "write",
  tradition_propose: "write",
  record_admission_manifest: "write",
};

function normalizePath(p: string): string {
  return path.normalize(p).replace(/\\/g, "/");
}

export class McpProxy {
    private cbmClient: CbmClient;

  constructor(cbmClient: CbmClient) {
    this.cbmClient = cbmClient;
  }

  /**
   * Intercepta chamadas de tools de agentes aplicando autoridade por perfil (C05 / PBI-16).
   */
  public async dispatchToolCall(
    session: AgentSessionContext,
    request: McpToolCallRequest
  ): Promise<McpToolCallResult> {
    const { role, intentScope } = session;
    const { toolName, arguments: args } = request;

    // Regra 1: Promoção NUNCA pode ser acionada via dispatchToolCall genérico (por nenhum perfil)
    if (toolName === "promote_horizon" || toolName === "admit_fast_path") {
      return {
        allowed: false,
        status: 403,
        error: "Forbidden: Direct promotion via generic tool call is prohibited. Promotion requires verified workflow via promoteCandidate() (INV-01, INV-02)",
      };
    }

    const requiredMutationOp = MUTATION_TOOLS_OPS[toolName];

    // Regra 2: Adversarial é estritamente somente-leitura (C05 / ADR-005)
    if (role === "adversarial") {
      if (requiredMutationOp || !READ_ONLY_TOOLS.has(toolName)) {
        return {
          allowed: false,
          status: 403,
          error: `Forbidden: Adversarial role is read-only and cannot perform mutations or invoke unapproved tool '${toolName}' (C05)`,
        };
      }
    }

    // Regra 3: Validação de Intent Scope para mutações do Builder (ADR-005 Critério 5 / Deny-by-default)
    const isMutation = Boolean(requiredMutationOp || !READ_ONLY_TOOLS.has(toolName));
    if (role === "builder" && isMutation) {
      const mutationOp = requiredMutationOp || "write";
      const rawPath = (args.path || args.file_path || args.target || args.project_id || args.name || args.horizon_id) as
        | string
        | undefined;
      if (!rawPath || typeof rawPath !== "string" || rawPath.trim() === "") {
        return {
          allowed: false,
          status: 403,
          error: `Forbidden: Mutation tool '${toolName}' requires an explicit target path or identifier`,
        };
      }

      const normalizedTargetPath = normalizePath(rawPath);
      // Impede Directory Traversal escapando da raiz
      if (rawPath.split(/[\\/]/).includes("..") || normalizedTargetPath.startsWith("../") || normalizedTargetPath.includes("/../")) {
        return {
          allowed: false,
          status: 403,
          error: `Forbidden: Path traversal not allowed: '${rawPath}'`,
        };
      }

      // Valida se o targetPath e a operação exata estão autorizados
      const inScope = intentScope.some((scope) => {
        const normalizedScopePath = normalizePath(scope.path);
        return (
          normalizedScopePath === normalizedTargetPath &&
          scope.operation === mutationOp
        );
      });

      if (!inScope) {
        return {
          allowed: false,
          status: 403,
          error: `Forbidden: Target path '${rawPath}' with operation '${mutationOp}' is outside intent scope for session ${session.sessionId}`,
        };
      }
    }

    // Encaminha para o backend CBM após aprovação de autoridade
    try {
      const result = await this.cbmClient.callTool(toolName, args);
      if (result && (result.isError === true || result.error)) {
        return {
          allowed: true,
          status: 500,
          error: result.error || "Backend returned error status",
          data: result,
        };
      }
      return {
        allowed: true,
        data: result,
      };
    } catch (err: any) {
      return {
        allowed: false,
        status: 500,
        error: err.message,
      };
    }
  }

  /**
   * Coordenação privilegiada do Gatekeeper para promoção atômica (C04 / INV-01).
   */
  public async promoteCandidate(
    session: AgentSessionContext,
    version: CandidateVersionRef,
    verdict: IndependentVerdict,
    builderIdentity: string,
    options: { idempotencyKey?: string; evidenceRecords: EvidenceRecord[] }
  ): Promise<{ promoted: boolean; status?: "CONFIRMED" | "FAILED" | "UNKNOWN"; reason?: string }> {
    if (session.role !== "gatekeeper") {
      return {
        promoted: false,
        status: "FAILED",
        reason: "Only Gatekeeper can execute candidate promotion",
      };
    }

    if (!options?.evidenceRecords || options.evidenceRecords.length === 0) {
      return {
        promoted: false,
        status: "FAILED",
        reason: "Evidence records are required and cannot be empty",
      };
    }

    // Valida o veredicto independente contra a versão, contra o builder real do ciclo e evidências
    const verdictCheck = validateIndependentVerdict(
      version,
      verdict,
      builderIdentity,
      options.evidenceRecords
    );
    if (!verdictCheck.valid || !verdictCheck.canPromote) {
      return {
        promoted: false,
        status: "FAILED",
        reason: verdictCheck.reason || "Veredicto independente não autoriza promoção",
      };
    }

    // Verificação de frescor contra mutações concorrentes no backend pós-verificação (AD-09 / C01 / C04)
    try {
      const stateCheck = await this.cbmClient.callTool("index_status", {
        project: version.projectId,
        horizon_id: version.horizonId,
      });
      if (stateCheck && (stateCheck.isError === true || stateCheck.error)) {
        return {
          promoted: false,
          status: "FAILED",
          reason: `Backend state freshness verification returned error: ${stateCheck.error || "isError"} (AD-09)`,
        };
      }
      if (stateCheck) {
        if (!stateCheck.graphGeneration || !stateCheck.baselineDigest) {
          return { promoted: false, status: "FAILED", reason: "Backend freshness revisions are unavailable" };
        }
        if (stateCheck.graphGeneration !== undefined && stateCheck.graphGeneration !== version.graphGeneration) {
          return {
            promoted: false,
            status: "FAILED",
            reason: `Stale candidate version: backend generation ${stateCheck.graphGeneration} does not match candidate generation ${version.graphGeneration} (AD-09)`,
          };
        }
        if (stateCheck.baselineDigest && stateCheck.baselineDigest !== version.baselineDigest) {
          return {
            promoted: false,
            status: "FAILED",
            reason: `Stale candidate version: backend baseline ${stateCheck.baselineDigest} does not match candidate baseline ${version.baselineDigest} (AD-09)`,
          };
        }
      }
    } catch (err: any) {
      return {
        promoted: false,
        status: "FAILED",
        reason: `Backend state freshness verification failed: ${err.message} (AD-09)`,
      };
    }

    // Executa a chamada privilegiada no CBM transmitindo tupla e chave idempotente
    try {
      const result = await this.cbmClient.callTool("promote_horizon", {
        horizon_id: version.horizonId,
        candidate_version_id: version.versionId,
        expected_graph_generation: version.graphGeneration,
        expected_baseline_digest: version.baselineDigest,
        idempotency_key: options?.idempotencyKey,
      });

      if (
        !result ||
        result.isError === true ||
        result.error ||
        (result.promoted === false && result.success === false)
      ) {
        return {
          promoted: false,
          status: "FAILED",
          reason: `CBM rejected promotion: ${result?.error || "backend returned unconfirmed or error response"}`,
        };
      }

      const isConfirmed = Boolean(result.success === true || result.promoted === true);
      if (!isConfirmed) {
        return {
          promoted: false,
          status: "FAILED",
          reason: "CBM backend did not explicitly confirm promotion success",
        };
      }

      return { promoted: true, status: "CONFIRMED" };
    } catch (err: any) {
      // Falha de transporte de rede: desfecho incerto deve ser registrado como UNKNOWN (C04/C07)
      if (err instanceof McpRemoteError) {
        return { promoted: false, status: "FAILED", reason: `CBM refused promotion: ${err.message}` };
      }
      return {
        promoted: false,
        status: "UNKNOWN",
        reason: `CBM transport error during promotion (outcome ambiguous): ${err.message}`,
      };
    }
  }
}
