import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { createInterface } from "node:readline";
import { killProcessTree } from "./runner.ts";

export interface ProcessCommand {
  command: string;
  args?: string[];
  cwd?: string;
  env?: NodeJS.ProcessEnv;
}

export interface ToolDefinition {
  name: string;
  description?: string;
  inputSchema: Record<string, unknown>;
}

export class McpRemoteError extends Error {
  code?: number | string;
  data?: unknown;
  constructor(message: string, code?: number | string, data?: unknown) {
    super(message);
    this.name = "McpRemoteError";
    this.code = code;
    this.data = data;
  }
}

/** A single owned MCP stdio connection; timed-out effects are never retried. */
export class StdioMcpClient {
  private child: ChildProcessWithoutNullStreams;
  private nextId = 0;
  private pending = new Map<number, { resolve: (value: any) => void; reject: (error: Error) => void }>();
  private stderr = "";
  private closed = false;
  serverInfo?: { name: string; version: string };
  private timeoutMs: number;

  constructor(command: ProcessCommand, timeoutMs = 15_000) {
    this.timeoutMs = timeoutMs;
    this.child = spawn(command.command, command.args ?? [], {
      cwd: command.cwd,
      env: command.env ? { ...process.env, ...command.env } : process.env,
      windowsHide: true,
      detached: process.platform !== "win32",
      stdio: ["pipe", "pipe", "pipe"],
    });
    this.child.stderr.on("data", (data) => { this.stderr = (this.stderr + data.toString()).slice(-8000); });
    this.child.stdin.on("error", (error) => this.fail(error));
    this.child.on("error", (error) => this.fail(error));
    this.child.on("close", (code) => this.fail(new Error(`MCP process closed (${code}): ${this.stderr}`)));
    const lines = createInterface({ input: this.child.stdout });
    lines.on("line", (line) => {
      try {
        const message = JSON.parse(line);
        const pending = this.pending.get(message.id);
        if (!pending) return;
        this.pending.delete(message.id);
        if (message.error) {
          pending.reject(new McpRemoteError(message.error.message, message.error.code, message.error.data));
        } else if ("result" in message) {
          pending.resolve(message.result);
        } else {
          pending.reject(new Error("Malformed MCP response: result or error required"));
        }
      } catch (error) {
        this.fail(new Error(`Invalid MCP JSON-RPC stdout: ${(error as Error).message}`));
      }
    });
  }

  private fail(error: Error): void {
    this.closed = true;
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
  }

  async initialize(): Promise<void> {
    const result = await this.request("initialize", {
      protocolVersion: "2024-11-05", capabilities: {},
      clientInfo: { name: "cbm-orchestrator", version: "0.1.0" },
    });
    if (!result?.protocolVersion || !result?.capabilities?.tools || !result.serverInfo) {
      throw new Error("MCP server did not negotiate tools capability");
    }
    this.serverInfo = result.serverInfo;
    this.notify("notifications/initialized");
  }

  notify(method: string, params: Record<string, unknown> = {}): void {
    if (this.closed) throw new Error("MCP connection is closed");
    this.child.stdin.write(JSON.stringify({ jsonrpc: "2.0", method, params }) + "\n");
  }

  async request(method: string, params: Record<string, unknown> = {}, signal?: AbortSignal): Promise<any> {
    if (this.closed) throw new Error("MCP connection is closed");
    signal?.throwIfAborted();
    const id = ++this.nextId;
    return new Promise((resolve, reject) => {
      const finish = (error?: Error, value?: unknown) => {
        clearTimeout(timer);
        signal?.removeEventListener("abort", abort);
        this.pending.delete(id);
        if (error) reject(error); else resolve(value);
      };
      const cancel = (error: Error) => {
        try { this.notify("notifications/cancelled", { requestId: id, reason: error.message }); } catch {}
        finish(error);
      };
      const abort = () => cancel(new Error("MCP request cancelled"));
      const timer = setTimeout(() => cancel(new Error(`MCP ${method} timed out after ${this.timeoutMs}ms; outcome unknown`)), this.timeoutMs);
      this.pending.set(id, { resolve: (value) => finish(undefined, value), reject: (error) => finish(error) });
      signal?.addEventListener("abort", abort, { once: true });
      this.child.stdin.write(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n", (error) => {
        if (error) finish(error);
      });
    });
  }

  async listTools(): Promise<ToolDefinition[]> {
    const tools: ToolDefinition[] = [];
    let cursor: string | undefined;
    const seen = new Set<string>();
    do {
      const page = await this.request("tools/list", cursor ? { cursor } : {});
      if (!Array.isArray(page.tools)) throw new Error("MCP tools/list did not return tools");
      tools.push(...page.tools);
      cursor = page.nextCursor;
      if (cursor && seen.has(cursor)) throw new Error("MCP tools/list repeated cursor");
      if (cursor) seen.add(cursor);
    } while (cursor);
    return tools;
  }

  async callTool(name: string, args: Record<string, unknown>, signal?: AbortSignal): Promise<any> {
    return decodeToolResult(await this.request("tools/call", { name, arguments: args }, signal));
  }

  async close(): Promise<void> {
    this.fail(new Error("MCP client closed"));
    this.child.stdin.end();
    if (this.child.exitCode === null && this.child.pid) {
      const result = await killProcessTree(this.child.pid);
      if (!result.killed) throw new Error(result.reason ?? "MCP process did not stop");
    }
  }
}

export function decodeToolResult(result: any): any {
  if (!result || !Array.isArray(result.content)) throw new Error("Malformed MCP tool result");
  const text = result.content.filter((item: any) => item.type === "text").map((item: any) => item.text).join("\n");
  let payload: any = result.structuredContent;
  if (payload === undefined) {
    try { payload = JSON.parse(text); } catch { payload = text; }
  }
  if (result.isError || payload?.isError || payload?.error) {
    throw new McpRemoteError(payload?.message ?? payload?.error ?? text ?? "MCP tool failed", payload?.code, payload);
  }
  return payload;
}

export function serveMcp(
  tools: ToolDefinition[],
  callTool: (name: string, args: Record<string, any>, signal: AbortSignal) => Promise<unknown>,
  input: NodeJS.ReadableStream = process.stdin,
  output: NodeJS.WritableStream = process.stdout,
): Promise<void> {
  const active = new Map<string | number, AbortController>();
  const send = (message: unknown) => output.write(JSON.stringify(message) + "\n");
  const lines = createInterface({ input });
  lines.on("line", (line) => {
    void (async () => {
      let message: any;
      try { message = JSON.parse(line); } catch {
        send({ jsonrpc: "2.0", id: null, error: { code: -32700, message: "Parse error" } });
        return;
      }
      if (message.method === "notifications/cancelled") {
        active.get(message.params?.requestId)?.abort();
        return;
      }
      if (message.id === undefined) return;
      const response = (result: unknown) => send({ jsonrpc: "2.0", id: message.id, result });
      if (message.method === "initialize") {
        response({ protocolVersion: "2024-11-05", capabilities: { tools: {} }, serverInfo: { name: "cbm-orchestrator", version: "0.1.0" } });
      } else if (message.method === "ping") {
        response({});
      } else if (message.method === "tools/list") {
        response({ tools });
      } else if (message.method === "tools/call") {
        const controller = new AbortController();
        active.set(message.id, controller);
        try {
          const name = message.params?.name;
          if (!tools.some((tool) => tool.name === name)) throw new Error(`Unknown tool: ${name}`);
          const result = await callTool(name, message.params?.arguments ?? {}, controller.signal);
          response({ content: [{ type: "text", text: JSON.stringify(result) }], structuredContent: result });
        } catch (error) {
          response({ isError: true, content: [{ type: "text", text: JSON.stringify({ error: (error as Error).message }) }] });
        } finally {
          active.delete(message.id);
        }
      } else {
        send({ jsonrpc: "2.0", id: message.id, error: { code: -32601, message: "Method not found" } });
      }
    })().catch((error) => process.stderr.write(String(error) + "\n"));
  });
  return new Promise((resolve) => lines.on("close", () => {
    for (const controller of active.values()) controller.abort();
    resolve();
  }));
}
