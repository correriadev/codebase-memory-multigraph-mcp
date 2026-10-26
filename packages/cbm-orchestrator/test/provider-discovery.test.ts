import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, existsSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import {
  parseCodexDebugModels,
  parseAgyModels,
  parseOpenCodeModels,
  getClaudeCatalog,
  sanitizeCustomModel,
  normalizeReasoningEffort,
  readProviderCache,
  writeProviderCache,
  createInitialTuple,
  updateProviderTuple,
  type ProviderCatalog,
  type ProviderModelOption,
} from "../src/provider-discovery.ts";

test("Provider Discovery: parseCodexDebugModels extrai modelos e níveis de raciocínio", () => {
  const jsonFixture = JSON.stringify({
    models: [
      {
        slug: "gpt-6.1-sol",
        display_name: "GPT-6.1 Sol",
        default_reasoning_level: "low",
        supported_reasoning_levels: [{ effort: "low" }, { effort: "medium" }, { effort: "high" }, { effort: "xhigh" }],
      },
      {
        slug: "gpt-6-luna",
        display_name: "GPT-6 Luna",
        default_reasoning_level: "medium",
        supported_reasoning_levels: [{ effort: "low" }, { effort: "medium" }, { effort: "high" }],
      },
    ],
  });

  const parsed = parseCodexDebugModels(jsonFixture);
  assert.equal(parsed.length, 2);
  assert.equal(parsed[0].id, "gpt-6.1-sol");
  assert.equal(parsed[0].label, "GPT-6.1 Sol");
  assert.equal(parsed[0].defaultReasoningLevel, "low");
  assert.deepEqual(parsed[0].reasoningLevels, ["low", "medium", "high", "xhigh"]);
  assert.equal(parsed[0].source, "live");
});

test("Provider Discovery: parseCodexDebugModels tolera JSON inválido ou vazio", () => {
  assert.deepEqual(parseCodexDebugModels("invalid-json"), []);
  assert.deepEqual(parseCodexDebugModels("{}"), []);
  assert.deepEqual(parseCodexDebugModels(JSON.stringify({ models: [] })), []);
});

test("Provider Discovery: parseAgyModels extrai pares TSV e detecta níveis default", () => {
  const tsvFixture = `Fetching available models...
gemini-4-argon-medium\tGemini 4 Argon (Medium)
gemini-4-argon-high\tGemini 4 Argon (High)
claude-opus-5-5-low\tClaude Opus 5.5 (Low)
gpt-oss-120b-medium\tGPT-OSS 120B (Medium)`;

  const parsed = parseAgyModels(tsvFixture);
  assert.equal(parsed.length, 4);
  assert.equal(parsed[0].id, "gemini-4-argon-medium");
  assert.equal(parsed[0].label, "Gemini 4 Argon (Medium)");
  assert.equal(parsed[0].defaultReasoningLevel, "medium");
  assert.equal(parsed[1].defaultReasoningLevel, "high");
  assert.equal(parsed[2].defaultReasoningLevel, "low");
  assert.equal(parsed[0].source, "live");
});

test("Provider Discovery: parseOpenCodeModels extrai formato multiline JSON com variantes", () => {
  const multilineFixture = `opencode/beam-free
{
  "id": "beam-free",
  "name": "Beam Free",
  "variants": {
    "low": { "effort": "low" },
    "medium": { "effort": "medium" },
    "high": { "effort": "high" }
  }
}
opencode/big-pickle
{
  "id": "big-pickle",
  "name": "Big Pickle",
  "variants": {}
}`;

  const parsed = parseOpenCodeModels(multilineFixture);
  assert.equal(parsed.length, 2);
  assert.equal(parsed[0].id, "opencode/beam-free");
  assert.match(parsed[0].label, /Beam Free/);
  assert.deepEqual(parsed[0].reasoningLevels, ["low", "medium", "high"]);
  assert.equal(parsed[1].id, "opencode/big-pickle");
  assert.equal(parsed[1].reasoningLevels, undefined);
});

test("Provider Discovery: getClaudeCatalog entrega catálogo curado canônico", () => {
  const catalog = getClaudeCatalog();
  assert.ok(catalog.length >= 6);
  assert.ok(catalog.some((m) => m.id === "sonnet"));
  assert.ok(catalog.some((m) => m.id === "opus"));
  assert.ok(catalog.some((m) => m.id === "claude-sonnet-4-5"));
  assert.equal(catalog[0].source, "emergency");
});

test("Provider Discovery: sanitizeCustomModel bloqueia injeções de shell e strings perigosas", () => {
  assert.equal(sanitizeCustomModel("gpt-6.1-sol"), "gpt-6.1-sol");
  assert.equal(sanitizeCustomModel("opencode/claude-fable-5-1"), "opencode/claude-fable-5-1");
  assert.equal(sanitizeCustomModel("custom_model:v2.1@preview"), "custom_model:v2.1@preview");

  // Rejeições de segurança
  assert.equal(sanitizeCustomModel("model; rm -rf /"), null);
  assert.equal(sanitizeCustomModel("model && echo hack"), null);
  assert.equal(sanitizeCustomModel("model | cat"), null);
  assert.equal(sanitizeCustomModel("$(evil)"), null);
  assert.equal(sanitizeCustomModel("`evil`"), null);
  assert.equal(sanitizeCustomModel(""), null);
  assert.equal(sanitizeCustomModel(null), null);
  assert.equal(sanitizeCustomModel("a".repeat(201)), null);
});

test("Provider Discovery: normalizeReasoningEffort nunca escala esforço para cima", () => {
  const modelWithLevels: ProviderModelOption = {
    id: "gpt-6.1-sol",
    label: "GPT-6.1 Sol",
    reasoningLevels: ["low", "medium", "high"],
    defaultReasoningLevel: "low",
    source: "live",
  };

  const modelWithoutLevels: ProviderModelOption = {
    id: "basic-model",
    label: "Basic Model",
    source: "live",
  };

  // Nível suportado
  assert.deepEqual(normalizeReasoningEffort(modelWithLevels, "medium"), { effort: "medium", wasClamped: false });
  assert.deepEqual(normalizeReasoningEffort(modelWithLevels, "low"), { effort: "low", wasClamped: false });

  // Modelo sem raciocínio -> reverte para default sem erro fatal
  const clampedNone = normalizeReasoningEffort(modelWithoutLevels, "high");
  assert.equal(clampedNone.effort, "default");
  assert.equal(clampedNone.wasClamped, true);
  assert.ok(clampedNone.warning?.includes("não suporta"));

  // Nível inexistente ('xhigh' quando só suporta até 'high') -> reverte para default do modelo, NUNCA escala
  const clampedOutOfRange = normalizeReasoningEffort(modelWithLevels, "xhigh");
  assert.equal(clampedOutOfRange.effort, "low"); // defaultReasoningLevel do modelo
  assert.equal(clampedOutOfRange.wasClamped, true);
  assert.ok(clampedOutOfRange.warning?.includes("Revertido"));
});

test("Provider Discovery: escrita atômica e leitura particionada de cache com TTL", () => {
  const tempDir = mkdtempSync(path.join(tmpdir(), "cbm-cache-test-"));
  try {
    const catalog: ProviderCatalog = {
      provider: "codex",
      status: "CATALOG_AVAILABLE",
      models: [
        { id: "gpt-6.1-sol", label: "GPT-6.1 Sol", source: "live" },
      ],
      defaultModelId: "gpt-6.1-sol",
      fetchedAt: new Date().toISOString(),
      ttlMs: 3600000, // 1h
    };

    writeProviderCache(tempDir, catalog);
    const read = readProviderCache(tempDir, "codex");
    assert.ok(read);
    assert.equal(read.provider, "codex");
    assert.equal(read.models[0].id, "gpt-6.1-sol");

    // Cache expirado
    const expiredCatalog: ProviderCatalog = {
      ...catalog,
      fetchedAt: new Date(Date.now() - 7200000).toISOString(), // 2h atrás
      ttlMs: 3600000, // TTL 1h
    };
    writeProviderCache(tempDir, expiredCatalog);
    const expiredRead = readProviderCache(tempDir, "codex");
    assert.ok(expiredRead);
    assert.equal(expiredRead.error, "CACHE_EXPIRED");
  } finally {
    rmSync(tempDir, { recursive: true, force: true });
  }
});

test("Provider Discovery: tupla atômica é reinicializada ao trocar de provedor", () => {
  const codexTuple = createInitialTuple("codex");
  assert.equal(codexTuple.provider, "codex");
  assert.equal(codexTuple.command, "codex");
  assert.equal(codexTuple.model, "gpt-6.1-sol");
  assert.equal(codexTuple.effort, "low");

  // Troca para Antigravity reatribui modelo e esforço para o padrão de Antigravity
  const agyTuple = updateProviderTuple(codexTuple, "antigravity");
  assert.equal(agyTuple.provider, "antigravity");
  assert.equal(agyTuple.command, "agy");
  assert.equal(agyTuple.model, "gemini-4-argon-medium");
  assert.equal(agyTuple.effort, "medium");

  // Troca para Claude reatribui modelo para sonnet e comando para claude
  const claudeTuple = updateProviderTuple(agyTuple, "claude");
  assert.equal(claudeTuple.provider, "claude");
  assert.equal(claudeTuple.command, "claude");
  assert.equal(claudeTuple.model, "sonnet");
  assert.equal(claudeTuple.effort, "medium");
});
