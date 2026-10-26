import net from "node:net";
import { createInterface } from "node:readline";
import { randomUUID } from "node:crypto";
import { tmpdir } from "node:os";
import path from "node:path";
import { rmSync } from "node:fs";
import type { ToolDefinition } from "./mcp-transport.ts";

/** Per-process capability channel. It never exposes the privileged CBM transport. */
export async function openAgentChannel(
  tools: ToolDefinition[],
  dispatch: (name: string, args: Record<string, unknown>) => Promise<unknown>,
): Promise<{ address: string; token: string; close: () => Promise<void> }> {
  const id = randomUUID();
  const address = process.platform === "win32" ? `\\\\.\\pipe\\cbm-agent-${id}` : path.join(tmpdir(), `cbm-${id}.sock`);
  const token = randomUUID();
  const sockets = new Set<net.Socket>();
  const server = net.createServer((socket) => {
    sockets.add(socket);
    socket.on("close", () => sockets.delete(socket));
    const lines = createInterface({ input: socket });
    let bytes = 0;
    socket.on("data", (data) => { bytes += data.length; if (bytes > 8 * 1024 * 1024) socket.destroy(); });
    lines.on("line", (line) => {
      void (async () => {
        let request: any;
        try {
          request = JSON.parse(line);
          if (request.token !== token) throw new Error("Unauthorized agent channel");
          const result = request.method === "tools/list" ? tools
            : await dispatch(request.name, request.arguments ?? {});
          socket.write(JSON.stringify({ id: request.id, result }) + "\n");
        } catch (error) {
          socket.write(JSON.stringify({ id: request?.id, error: (error as Error).message }) + "\n");
        }
      })().catch(() => socket.destroy());
    });
  });
  await new Promise<void>((resolve, reject) => { server.once("error", reject); server.listen(address, resolve); });
  return {
    address, token,
    close: async () => {
      for (const socket of sockets) socket.destroy();
      await new Promise<void>((resolve) => server.close(() => resolve()));
      if (process.platform !== "win32") rmSync(address, { force: true });
    },
  };
}
