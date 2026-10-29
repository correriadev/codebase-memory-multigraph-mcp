import { execFile } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync, renameSync, unlinkSync } from "node:fs";
import path from "node:path";
import os from "node:os";

export type ProviderId = "codex" | "antigravity" | "opencode" | "claude";

export type ProviderStatus =
  | "NOT_INSTALLED"
  | "INCOMPATIBLE"
  | "UNAUTHENTICATED"
  | "CATALOG_AVAILABLE"
  | "ACCESS_UNVERIFIED";

export type ModelSource = "live" | "cache" | "emergency" | "custom";

export interface ProviderModelOption {
  id: string;
  label: string;
  reasoningLevels?: string[];
  defaultReasoningLevel?: string;
  source: ModelSource;
}

export interface ProviderCatalog {
  provider: ProviderId;
  status: ProviderStatus;
  models: ProviderModelOption[];
  defaultModelId: string;
  executablePath?: string;
  version?: string;
  fetchedAt: string;
  ttlMs: number;
  error?: string;
}

export interface SelectedModelTuple {
  provider: ProviderId;
  command: string;
  model: string;
  effort: string;
}

export interface CacheStore {
  version: number;
  providers: Partial<Record<ProviderId, ProviderCatalog>>;
}

export const DEFAULT_TTL_MS = 24 * 60 * 60 * 1000; // 24 horas

export const EMERGENCY_CATALOGS: Record<ProviderId, { defaultModelId: string; models: ProviderModelOption[] }> = {
  codex: {
    defaultModelId: "gpt-6.1-sol",
    models: [
      { id: "gpt-6.1-sol", label: "GPT-6.1 Sol (Emergência)", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "low", source: "emergency" },
      { id: "gpt-6-luna", label: "GPT-6 Luna (Emergência)", reasoningLevels: ["low", "medium", "high", "xhigh"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "gpt-5.6-sol", label: "GPT-5.6 Sol (Emergência)", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "low", source: "emergency" },
      { id: "gpt-5-terra", label: "GPT-5 Terra (Emergência)", reasoningLevels: ["low", "medium"], defaultReasoningLevel: "low", source: "emergency" },
    ],
  },
  antigravity: {
    defaultModelId: "gemini-4-argon-medium",
    models: [
      { id: "gemini-4-argon-medium", label: "Gemini 4 Argon (Medium) (Emergência)", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "gemini-4-argon-high", label: "Gemini 4 Argon (High) (Emergência)", reasoningLevels: ["high"], defaultReasoningLevel: "high", source: "emergency" },
      { id: "gemini-3.8-flash-high", label: "Gemini 3.8 Flash (High) (Emergência)", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "high", source: "emergency" },
      { id: "claude-opus-5-5-medium", label: "Claude Opus 5.5 (Medium) (Emergência)", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "claude-sonnet-5-5-medium", label: "Claude Sonnet 5.5 (Medium) (Emergência)", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "medium", source: "emergency" },
    ],
  },
  opencode: {
    defaultModelId: "opencode/gemini-4-argon",
    models: [
      { id: "opencode/gemini-4-argon", label: "opencode/gemini-4-argon (Emergência)", reasoningLevels: ["low", "medium", "high", "xhigh"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "opencode/claude-fable-5-1", label: "opencode/claude-fable-5-1 (Emergência)", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "opencode/gpt-6.1-sol", label: "opencode/gpt-6.1-sol (Emergência)", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "low", source: "emergency" },
      { id: "opencode/big-pickle", label: "opencode/big-pickle (Emergência)", source: "emergency" },
    ],
  },
  claude: {
    defaultModelId: "sonnet",
    models: [
      { id: "sonnet", label: "Claude Sonnet (Alias Padrão)", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "opus", label: "Claude Opus (Alias Padrão)", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "haiku", label: "Claude Haiku (Alias Padrão)", reasoningLevels: ["low", "medium", "high"], defaultReasoningLevel: "low", source: "emergency" },
      { id: "fable", label: "Claude Fable (Alias Padrão)", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "claude-sonnet-4-5", label: "Claude Sonnet 4.5", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "claude-opus-4-5", label: "Claude Opus 4.5", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
      { id: "claude-sonnet-5", label: "Claude Sonnet 5", reasoningLevels: ["low", "medium", "high", "xhigh", "max"], defaultReasoningLevel: "medium", source: "emergency" },
    ],
  },
};

/**
 * Validação rigorosa de entrada livre de modelo para impedir injeção de shell.
 */
export function sanitizeCustomModel(id: string | null | undefined): string | null {
  if (typeof id !== "string") return null;
  const trimmed = id.trim();
  if (trimmed.length === 0 || trimmed.length > 200) return null;
  if (!/^[A-Za-z0-9][A-Za-z0-9._/:@-]*$/.test(trimmed)) return null;
  return trimmed;
}

/**
 * Normaliza o esforço de raciocínio.
 * Regra: NUNCA escala para um nível superior ao suportado ou pedido.
 */
export function normalizeReasoningEffort(
  modelOption: ProviderModelOption | undefined,
  requestedEffort: string
): { effort: string; wasClamped: boolean; warning?: string } {
  const req = (requestedEffort || "").trim().toLowerCase();
  if (!req || req === "default" || req === "none") {
    return { effort: req || "default", wasClamped: false };
  }

  const supported = modelOption?.reasoningLevels;
  if (!supported || supported.length === 0) {
    return {
      effort: "default",
      wasClamped: true,
      warning: `Modelo ${modelOption?.id || "desconhecido"} não suporta calibração de raciocínio estendido. Esforço mantido como padrão do CLI.`,
    };
  }

  if (supported.includes(req)) {
    return { effort: req, wasClamped: false };
  }

  // Se não suportado, reverte para o padrão do modelo (ou default), sem escalar para cima
  const fallback = modelOption.defaultReasoningLevel && supported.includes(modelOption.defaultReasoningLevel)
    ? modelOption.defaultReasoningLevel
    : supported[0] || "default";

  return {
    effort: fallback,
    wasClamped: true,
    warning: `Nível '${requestedEffort}' incompatível com modelo ${modelOption.id}. Revertido para '${fallback}' (sem escalonamento para cima).`,
  };
}

/**
 * Parser de saída JSON do `codex debug models`
 */
export function parseCodexDebugModels(stdout: string): ProviderModelOption[] {
  try {
    const data = JSON.parse(stdout);
    const list = Array.isArray(data) ? data : data?.models;
    if (!Array.isArray(list)) return [];

    const result: ProviderModelOption[] = [];
    const seen = new Set<string>();

    for (const item of list) {
      if (!item || typeof item !== "object") continue;
      const slug = typeof item.slug === "string" ? item.slug.trim() : typeof item.id === "string" ? item.id.trim() : "";
      if (!slug || seen.has(slug)) continue;
      seen.add(slug);

      const label = typeof item.display_name === "string" && item.display_name.trim()
        ? item.display_name.trim()
        : slug;

      const levels: string[] = [];
      if (Array.isArray(item.supported_reasoning_levels)) {
        for (const lvl of item.supported_reasoning_levels) {
          if (lvl && typeof lvl.effort === "string" && lvl.effort.trim()) {
            levels.push(lvl.effort.trim());
          }
        }
      }

      const defaultLevel = typeof item.default_reasoning_level === "string" && item.default_reasoning_level.trim()
        ? item.default_reasoning_level.trim()
        : undefined;

      result.push({
        id: slug,
        label,
        reasoningLevels: levels.length > 0 ? levels : undefined,
        defaultReasoningLevel: defaultLevel,
        source: "live",
      });
    }

    return result;
  } catch {
    return [];
  }
}

/**
 * Parser de saída TSV do `agy models`
 */
export function parseAgyModels(stdout: string): ProviderModelOption[] {
  const lines = String(stdout || "").split(/\r?\n/);
  const result: ProviderModelOption[] = [];
  const seen = new Set<string>();

  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!line || line.startsWith("Fetching") || line.startsWith("#")) continue;

    const parts = line.split("\t");
    const id = parts[0]?.trim();
    if (!id || seen.has(id)) continue;
    seen.add(id);

    const label = parts[1]?.trim() || id;

    // Detecta níveis baseados no slug ou label do Antigravity
    let defaultLevel: string | undefined;
    if (id.endsWith("-high")) defaultLevel = "high";
    else if (id.endsWith("-medium")) defaultLevel = "medium";
    else if (id.endsWith("-low")) defaultLevel = "low";

    result.push({
      id,
      label,
      reasoningLevels: ["low", "medium", "high", "xhigh", "max"],
      defaultReasoningLevel: defaultLevel || "medium",
      source: "live",
    });
  }

  return result;
}

/**
 * Parser multiline para `opencode models --verbose`
 */
export function parseOpenCodeModels(stdout: string): ProviderModelOption[] {
  const lines = String(stdout || "").split(/\r?\n/);
  const result: ProviderModelOption[] = [];
  const seen = new Set<string>();

  let currentId: string | null = null;
  let jsonBuffer = "";
  let insideJson = false;
  let braceCount = 0;

  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!line) continue;

    if (!insideJson) {
      if (line.includes("/") && !line.startsWith("{")) {
        currentId = line;
      } else if (line.startsWith("{") && currentId) {
        insideJson = true;
        jsonBuffer = line + "\n";
        braceCount = (line.match(/{/g) || []).length - (line.match(/}/g) || []).length;
        if (braceCount === 0) {
          processOpenCodeJson(currentId, jsonBuffer, seen, result);
          insideJson = false;
          currentId = null;
          jsonBuffer = "";
        }
      }
    } else {
      jsonBuffer += line + "\n";
      braceCount += (line.match(/{/g) || []).length - (line.match(/}/g) || []).length;
      if (braceCount <= 0 && currentId) {
        processOpenCodeJson(currentId, jsonBuffer, seen, result);
        insideJson = false;
        currentId = null;
        jsonBuffer = "";
      }
    }
  }

  // Fallback para modelos tabulares simples se nenhum JSON foi parseado
  if (result.length === 0) {
    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (line.includes("/") && !seen.has(line) && /^[A-Za-z0-9][A-Za-z0-9._/-]+$/.test(line)) {
        seen.add(line);
        result.push({
          id: line,
          label: line,
          source: "live",
        });
      }
    }
  }

  return result;
}

function processOpenCodeJson(
  id: string,
  rawJson: string,
  seen: Set<string>,
  result: ProviderModelOption[]
): void {
  if (seen.has(id)) return;
  seen.add(id);

  try {
    const parsed = JSON.parse(rawJson);
    const label = typeof parsed.name === "string" && parsed.name.trim() ? `${id} (${parsed.name.trim()})` : id;
    const variants = parsed.variants && typeof parsed.variants === "object" ? Object.keys(parsed.variants) : [];

    result.push({
      id,
      label,
      reasoningLevels: variants.length > 0 ? variants : undefined,
      defaultReasoningLevel: variants.includes("medium") ? "medium" : variants[0],
      source: "live",
    });
  } catch {
    result.push({
      id,
      label: id,
      source: "live",
    });
  }
}

/**
 * Catálogo canônico para Claude Code (sem invocar `claude models`)
 */
export function getClaudeCatalog(): ProviderModelOption[] {
  return EMERGENCY_CATALOGS.claude.models.map((m) => ({ ...m, source: "emergency" }));
}

/**
 * Resolução do caminho do executável do provedor no ambiente Windows / POSIX
 */
export async function resolveProviderExecutable(
  provider: ProviderId
): Promise<{ path?: string; version?: string; status: ProviderStatus }> {
  const home = os.homedir();
  const candidates: Record<ProviderId, string[]> = {
    codex: [
      "codex",
      path.join(home, "AppData", "Local", "Programs", "OpenAI", "Codex", "bin", "codex.exe"),
    ],
    antigravity: [
      "agy",
      path.join(home, "AppData", "Local", "agy", "bin", "agy.exe"),
    ],
    opencode: [
      "opencode",
      path.join(home, "AppData", "Roaming", "npm", "opencode.cmd"),
      path.join(home, "AppData", "Roaming", "npm", "opencode.ps1"),
    ],
    claude: [
      "claude",
      path.join(home, ".local", "bin", "claude.exe"),
      path.join(home, "AppData", "Roaming", "npm", "claude.cmd"),
    ],
  };

  const list = candidates[provider] || [];
  for (const cmd of list) {
    try {
      const { stdout } = await execFilePromise(cmd, ["--version"], 3000);
      const version = String(stdout || "").trim().split(/\r?\n/)[0];
      return {
        path: cmd,
        version: version || "disponível",
        status: provider === "claude" ? "ACCESS_UNVERIFIED" : "CATALOG_AVAILABLE",
      };
    } catch {
      // Tenta próximo candidato
    }
  }

  return { status: "NOT_INSTALLED" };
}

/**
 * Utilitário seguro para executar CLI com timeout rigoroso
 */
function execFilePromise(
  file: string,
  args: string[],
  timeoutMs: number
): Promise<{ stdout: string; stderr: string }> {
  return new Promise((resolve, reject) => {
    execFile(file, args, { timeout: timeoutMs, windowsHide: true }, (error, stdout, stderr) => {
      if (error) reject(error);
      else resolve({ stdout: String(stdout || ""), stderr: String(stderr || "") });
    });
  });
}

/**
 * Executa o probe de modelos nativo do CLI
 */
export async function probeProviderModels(
  provider: ProviderId,
  execPath: string,
  timeoutMs: number
): Promise<ProviderModelOption[]> {
  if (provider === "claude") {
    return getClaudeCatalog();
  }

  if (provider === "codex") {
    const { stdout } = await execFilePromise(execPath, ["debug", "models"], timeoutMs);
    const parsed = parseCodexDebugModels(stdout);
    if (parsed.length > 0) return parsed;
    throw new Error("Codex debug models retornou lista vazia ou formato inválido");
  }

  if (provider === "antigravity") {
    const { stdout } = await execFilePromise(execPath, ["models"], timeoutMs);
    const parsed = parseAgyModels(stdout);
    if (parsed.length > 0) return parsed;
    throw new Error("Antigravity models retornou lista vazia ou formato inválido");
  }

  if (provider === "opencode") {
    const { stdout } = await execFilePromise(execPath, ["models", "--verbose"], timeoutMs);
    const parsed = parseOpenCodeModels(stdout);
    if (parsed.length > 0) return parsed;
    throw new Error("OpenCode models retornou lista vazia ou formato inválido");
  }

  return [];
}

/**
 * Lê o cache de modelos a partir do diretório de estado
 */
export function readProviderCache(stateDir: string, provider: ProviderId): ProviderCatalog | null {
  const cacheFile = path.join(stateDir, "models-cache.json");
  if (!existsSync(cacheFile)) return null;

  try {
    const raw = readFileSync(cacheFile, "utf8");
    const store: CacheStore = JSON.parse(raw);
    const catalog = store.providers?.[provider];
    if (!catalog || !Array.isArray(catalog.models) || catalog.models.length === 0) return null;

    const age = Date.now() - new Date(catalog.fetchedAt).getTime();
    if (age > (catalog.ttlMs || DEFAULT_TTL_MS)) {
      // Retorna com flag de expirado para permitir uso degradado se necessário
      return { ...catalog, error: "CACHE_EXPIRED" };
    }

    return catalog;
  } catch {
    return null;
  }
}

/**
 * Escreve atomicamente o cache de modelos
 */
export function writeProviderCache(stateDir: string, catalog: ProviderCatalog): void {
  try {
    mkdirSync(stateDir, { recursive: true });
    const cacheFile = path.join(stateDir, "models-cache.json");
    let store: CacheStore = { version: 1, providers: {} };

    if (existsSync(cacheFile)) {
      try {
        store = JSON.parse(readFileSync(cacheFile, "utf8"));
      } catch {
        store = { version: 1, providers: {} };
      }
    }

    store.providers = store.providers || {};
    store.providers[catalog.provider] = catalog;

    const tempFile = `${cacheFile}.${process.pid}.${Date.now()}.tmp`;
    writeFileSync(tempFile, JSON.stringify(store, null, 2), "utf8");
    renameSync(tempFile, cacheFile);
  } catch {
    // Falha silenciosa de cache não pode interromper a aplicação
  }
}

/**
 * Orquestra a descoberta de modelos para um provedor seguindo a hierarquia de degradação:
 * 1. Descoberta Viva
 * 2. Cache Válido ou Expirado
 * 3. Catálogo de Emergência
 */
export async function discoverProvider(
  provider: ProviderId,
  options: { stateDir?: string; forceRefresh?: boolean; timeoutMs?: number } = {}
): Promise<ProviderCatalog> {
  const stateDir = options.stateDir || path.join(process.cwd(), ".cbm-orchestrator");
  const forceRefresh = Boolean(options.forceRefresh);
  const timeoutMs = options.timeoutMs ?? (provider === "opencode" ? 10000 : 5000);

  // 1. Tenta ler cache se refresh não foi forçado
  if (!forceRefresh) {
    const cached = readProviderCache(stateDir, provider);
    if (cached && cached.error !== "CACHE_EXPIRED") {
      return cached;
    }
  }

  // 2. Resolve executável
  const execInfo = await resolveProviderExecutable(provider);
  if (execInfo.status === "NOT_INSTALLED" || !execInfo.path) {
    const emergency = EMERGENCY_CATALOGS[provider];
    return {
      provider,
      status: "NOT_INSTALLED",
      models: emergency.models,
      defaultModelId: emergency.defaultModelId,
      fetchedAt: new Date().toISOString(),
      ttlMs: DEFAULT_TTL_MS,
      error: "Executável do provedor não localizado no PATH",
    };
  }

  // 3. Executa o probe de modelos
  try {
    const liveModels = await probeProviderModels(provider, execInfo.path, timeoutMs);
    const catalog: ProviderCatalog = {
      provider,
      status: provider === "claude" ? "ACCESS_UNVERIFIED" : "CATALOG_AVAILABLE",
      models: liveModels,
      defaultModelId: liveModels[0]?.id || EMERGENCY_CATALOGS[provider].defaultModelId,
      executablePath: execInfo.path,
      version: execInfo.version,
      fetchedAt: new Date().toISOString(),
      ttlMs: DEFAULT_TTL_MS,
    };

    writeProviderCache(stateDir, catalog);
    return catalog;
  } catch (err: any) {
    // 4. Degrada para cache anterior (mesmo expirado)
    const stale = readProviderCache(stateDir, provider);
    if (stale && stale.models.length > 0) {
      return {
        ...stale,
        status: "CATALOG_AVAILABLE",
        error: `Falha no probe vivo: ${err?.message || err}. Usando cache prévio.`,
      };
    }

    // 5. Degrada para catálogo de emergência
    const emergency = EMERGENCY_CATALOGS[provider];
    return {
      provider,
      status: "CATALOG_AVAILABLE",
      models: emergency.models,
      defaultModelId: emergency.defaultModelId,
      executablePath: execInfo.path,
      version: execInfo.version,
      fetchedAt: new Date().toISOString(),
      ttlMs: DEFAULT_TTL_MS,
      error: `Probe falhou (${err?.message || err}). Usando catálogo de emergência.`,
    };
  }
}

/**
 * Cria a tupla atômica inicial com os padrões do provedor
 */
export function createInitialTuple(provider: ProviderId, catalog?: ProviderCatalog): SelectedModelTuple {
  const cat = catalog || {
    provider,
    status: "CATALOG_AVAILABLE",
    models: EMERGENCY_CATALOGS[provider].models,
    defaultModelId: EMERGENCY_CATALOGS[provider].defaultModelId,
    fetchedAt: new Date().toISOString(),
    ttlMs: DEFAULT_TTL_MS,
  };

  const modelId = cat.defaultModelId;
  const modelOpt = cat.models.find((m) => m.id === modelId);
  const effort = modelOpt?.defaultReasoningLevel || (modelOpt?.reasoningLevels ? modelOpt.reasoningLevels[0] : "default") || "default";

  const commandMap: Record<ProviderId, string> = {
    codex: "codex",
    antigravity: "agy",
    opencode: "opencode",
    claude: "claude",
  };

  return {
    provider,
    command: commandMap[provider],
    model: modelId,
    effort,
  };
}

/**
 * Atualiza atomicamente a tupla ao trocar de provedor
 */
export function updateProviderTuple(
  currentTuple: SelectedModelTuple,
  newProvider: ProviderId,
  newCatalog?: ProviderCatalog
): SelectedModelTuple {
  if (currentTuple.provider === newProvider && newCatalog) {
    return currentTuple;
  }
  return createInitialTuple(newProvider, newCatalog);
}

export const SUPPORTED_PROVIDERS: ProviderId[] = ["codex", "antigravity", "opencode", "claude"];

export async function getAllProviderCatalogs(
  options: {
    stateDir?: string;
    forceRefresh?: boolean;
  } = {}
): Promise<ProviderCatalog[]> {
  const catalogs: ProviderCatalog[] = [];
  for (const p of SUPPORTED_PROVIDERS) {
    const cat = await discoverProvider(p, options);
    catalogs.push(cat);
  }
  return catalogs;
}

export function formatModelsTable(catalogs: ProviderCatalog[]): string {
  const lines: string[] = [];
  lines.push("┌─────────────┬────────────────────┬─────────────────────────────┬────────────────────────────────────────┐");
  lines.push("│ PROVEDOR    │ STATUS             │ MODELO                      │ ESFORÇOS                               │");
  lines.push("├─────────────┼────────────────────┼─────────────────────────────┼────────────────────────────────────────┤");

  for (const cat of catalogs) {
    const provName = cat.provider.toUpperCase().padEnd(11);
    const statusStr = cat.status.padEnd(18);

    if (cat.models && cat.models.length > 0) {
      for (const [idx, m] of cat.models.entries()) {
        const pCol = idx === 0 ? provName : " ".repeat(11);
        const sCol = idx === 0 ? statusStr : " ".repeat(18);
        const isDefault = m.id === cat.defaultModelId ? " *" : "";
        const modelStr = (m.id + isDefault).slice(0, 27).padEnd(27);
        const efforts =
          m.reasoningLevels && m.reasoningLevels.length > 0
            ? m.reasoningLevels.join(", ")
            : m.defaultReasoningLevel || "default";
        const effortStr = efforts.slice(0, 38).padEnd(38);
        lines.push(`│ ${pCol} │ ${sCol} │ ${modelStr} │ ${effortStr} │`);
      }
    } else {
      const modelStr = (cat.error || "-").slice(0, 27).padEnd(27);
      const effortStr = "-".padEnd(38);
      lines.push(`│ ${provName} │ ${statusStr} │ ${modelStr} │ ${effortStr} │`);
    }
  }

  lines.push("└─────────────┴────────────────────┴─────────────────────────────┴────────────────────────────────────────┘");
  lines.push("(* = modelo padrão do provedor)");
  return lines.join("\n");
}

