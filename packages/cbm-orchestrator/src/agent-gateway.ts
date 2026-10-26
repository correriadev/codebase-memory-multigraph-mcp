import net from "node:net";
import { createInterface } from "node:readline";
import { serveMcp } from "./mcp-transport.ts";

const address = process.env.CBM_AGENT_CHANNEL;
const token = process.env.CBM_AGENT_TOKEN;
if (!address || !token) throw new Error("Agent capability channel is required");
const socket = net.createConnection(address);
const pending = new Map<number, { resolve: (value: any) => void; reject: (error: Error) => void }>();
let id = 0;
socket.on("error", (error) => { for (const p of pending.values()) p.reject(error); pending.clear(); });
socket.on("close", () => { for (const p of pending.values()) p.reject(new Error("Agent channel closed")); pending.clear(); });
createInterface({ input: socket }).on("line", (line) => {
  try {
    const response = JSON.parse(line);
    const p = pending.get(response.id);
    pending.delete(response.id);
    if (response.error) p?.reject(new Error(response.error)); else p?.resolve(response.result);
  } catch { socket.destroy(); }
});
function request(method: string, name?: string, args?: Record<string, unknown>): Promise<any> {
  return new Promise((resolve, reject) => {
    const requestId = ++id;
    pending.set(requestId, { resolve, reject });
    socket.write(JSON.stringify({ id: requestId, token, method, name, arguments: args }) + "\n");
  });
}
try {
  const tools = await request("tools/list");
  await serveMcp(tools, (name, args) => request("tools/call", name, args));
} finally {
  socket.destroy();
}
