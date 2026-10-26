/**
 * CBM Terminal TUI Engine (PBB-TERMINAL-TUI v2.0)
 * Viewport delimitado com suporte a alternate screen buffer,
 * histórico rolável delimitado em memória,
 * toggle dinâmico de logs com estilo atenuado (dim)
 * e fallback linear para ambientes não-interativos.
 */

export type LogSubsystem = "engine" | "mcp" | "lock" | "token" | "seal" | "audit" | "general";
export type LogLevel = "info" | "warn" | "error";

export interface ViewportEntry {
  id: number;
  timestamp: number;
  type: "message" | "log";
  content: string;
  subsystem?: LogSubsystem | string;
  level?: LogLevel;
}

export interface TuiHeaderMetadata {
  project: string;
  workspace: string;
  provider: string;
  model: string;
  effort?: string;
  tokenCap: number;
  costCapUsd: number;
}

export interface TuiStatusTelemetry {
  phase: "IDLE" | "BUILDING" | "SEALING" | "VERIFYING" | "APPROVAL" | "COMPLETED" | "ERROR";
  tokensUsed: number;
  costUsd: number;
  isLocked: boolean;
}

export interface TerminalTuiOptions {
  stdout?: NodeJS.WritableStream & { columns?: number; rows?: number; isTTY?: boolean };
  stdin?: NodeJS.ReadableStream & { isTTY?: boolean };
  maxEntries?: number;
  maxBufferBytes?: number;
  noColor?: boolean;
}

export class TerminalTui {
  private stdout: NodeJS.WritableStream & { columns?: number; rows?: number; isTTY?: boolean };
  private stdin: NodeJS.ReadableStream & { isTTY?: boolean };
  private maxEntries: number;
  private maxBufferBytes: number;
  private noColor: boolean;

  private entries: ViewportEntry[] = [];
  private nextEntryId = 1;
  private currentBufferBytes = 0;
  private evictedCount = 0;

  private showLogs = false;
  private scrollOffset = 0;
  private isAlternateBufferActive = false;

  private header: TuiHeaderMetadata = {
    project: "CBM",
    workspace: "N/A",
    provider: "CODEX",
    model: "N/A",
    tokenCap: 0,
    costCapUsd: 0,
  };

  private telemetry: TuiStatusTelemetry = {
    phase: "IDLE",
    tokensUsed: 0,
    costUsd: 0,
    isLocked: false,
  };

  private currentInput = "";
  private promptPrefix = "❯ ";

  constructor(options: TerminalTuiOptions = {}) {
    this.stdout = options.stdout || process.stdout;
    this.stdin = options.stdin || process.stdin;
    this.maxEntries = options.maxEntries ?? 1000;
    this.maxBufferBytes = options.maxBufferBytes ?? 2 * 1024 * 1024; // 2 MB
    this.noColor = options.noColor ?? Boolean(process.env.NO_COLOR);
  }

  public isInteractive(): boolean {
    return Boolean(this.stdout.isTTY && this.stdin.isTTY);
  }

  public getShowLogs(): boolean {
    return this.showLogs;
  }

  public setShowLogs(show: boolean): void {
    this.showLogs = show;
  }

  public toggleLogs(): boolean {
    this.showLogs = !this.showLogs;
    return this.showLogs;
  }

  public setHeader(meta: Partial<TuiHeaderMetadata>): void {
    this.header = { ...this.header, ...meta };
  }

  public setTelemetry(status: Partial<TuiStatusTelemetry>): void {
    this.telemetry = { ...this.telemetry, ...status };
  }

  public setInput(input: string): void {
    this.currentInput = input;
  }

  public getScrollOffset(): number {
    return this.scrollOffset;
  }

  public scrollUp(lines = 5): void {
    this.scrollOffset = Math.min(this.scrollOffset + lines, Math.max(0, this.entries.length * 10));
  }

  public scrollDown(lines = 5): void {
    this.scrollOffset = Math.max(0, this.scrollOffset - lines);
  }

  public scrollToBottom(): void {
    this.scrollOffset = 0;
  }

  public addMessage(content: string): void {
    this.appendEntry({
      id: this.nextEntryId++,
      timestamp: Date.now(),
      type: "message",
      content,
    });
  }

  public addLog(subsystem: LogSubsystem | string, content: string, level: LogLevel = "info"): void {
    this.appendEntry({
      id: this.nextEntryId++,
      timestamp: Date.now(),
      type: "log",
      content,
      subsystem,
      level,
    });
  }

  private appendEntry(entry: ViewportEntry): void {
    // Truncate individual huge entries (> 10 KB) to prevent RAM explosion
    const MAX_ENTRY_LEN = 10 * 1024;
    if (entry.content.length > MAX_ENTRY_LEN) {
      entry.content = entry.content.slice(0, MAX_ENTRY_LEN) + " ... [truncado por limite de tamanho]";
    }

    const approxBytes = entry.content.length * 2 + 100;
    this.entries.push(entry);
    this.currentBufferBytes += approxBytes;

    // Evict old entries when exceeding limits
    while (
      this.entries.length > this.maxEntries ||
      (this.currentBufferBytes > this.maxBufferBytes && this.entries.length > 1)
    ) {
      const removed = this.entries.shift()!;
      this.currentBufferBytes -= removed.content.length * 2 + 100;
      this.evictedCount++;
    }

    // Auto-scroll to bottom if not actively scrolled back
    if (this.scrollOffset === 0) {
      this.scrollToBottom();
    }
  }

  public getEntries(): readonly ViewportEntry[] {
    return this.entries;
  }

  public getEvictedCount(): number {
    return this.evictedCount;
  }

  public enterAlternateBuffer(): void {
    if (!this.isInteractive() || this.isAlternateBufferActive) return;
    this.isAlternateBufferActive = true;
    this.stdout.write("\x1b[?1049h\x1b[H\x1b[?25h");
  }

  public exitAlternateBuffer(): void {
    if (!this.isAlternateBufferActive) return;
    this.isAlternateBufferActive = false;
    this.stdout.write("\x1b[0m\x1b[?1049l\x1b[?25h");
  }

  public getDimensions(): { cols: number; rows: number } {
    return {
      cols: this.stdout.columns && this.stdout.columns > 0 ? this.stdout.columns : 80,
      rows: this.stdout.rows && this.stdout.rows > 0 ? this.stdout.rows : 24,
    };
  }

  /**
   * Helper to truncate strings softly with ellipsis
   */
  public truncate(str: string, maxLen: number): string {
    if (str.length <= maxLen) return str;
    if (maxLen <= 3) return str.slice(0, maxLen);
    return str.slice(0, maxLen - 3) + "...";
  }

  /**
   * Render frame string
   */
  public renderFrame(): string {
    const { cols, rows } = this.getDimensions();

    // Enforce minimum geometry (60 cols x 15 rows)
    if (cols < 60 || rows < 15) {
      return this.renderGeometryWarning(cols, rows);
    }

    const lines: string[] = [];

    // 1. Header (5 lines)
    const headerLines = this.renderHeader(cols);
    lines.push(...headerLines);

    // 2. Status & telemetry (3 lines) + Input line (1 line) = 4 lines reserved at bottom
    const reservedBottom = 4;
    const reservedTotal = headerLines.length + reservedBottom;
    const viewportHeight = Math.max(1, rows - reservedTotal);

    // 3. Central Viewport lines
    const viewportLines = this.renderViewport(cols, viewportHeight);
    lines.push(...viewportLines);

    // Fill remaining vertical space if any
    while (lines.length < rows - reservedBottom) {
      lines.push(" ".repeat(cols));
    }

    // 4. Status Bar & Controls (3 lines)
    const statusLines = this.renderStatusBar(cols);
    lines.push(...statusLines);

    // 5. Input Prompt (1 line)
    const promptLine = this.truncate(this.promptPrefix + this.currentInput, cols);
    lines.push(promptLine.padEnd(cols, " "));

    return lines.slice(0, rows).join("\n");
  }

  private renderGeometryWarning(cols: number, rows: number): string {
    const msg = `Terminal insuficiente (${cols}x${rows}). Mínimo exigido: 60x15.`;
    return "\n".repeat(Math.max(0, Math.floor(rows / 2) - 1)) +
      msg.padStart(Math.floor((cols + msg.length) / 2), " ");
  }

  private renderHeader(cols: number): string[] {
    const border = "═".repeat(cols - 2);
    const top = `╔${border}╗`;
    const bottom = `╚${border}╝`;

    const title = " CBM SYSTEM INTELLIGENCE ENGINE · TUI v2.0 ";
    const paddedTitle = `║ ${title.padEnd(cols - 4, " ")} ║`;

    const wsTruncated = this.truncate(this.header.workspace, Math.max(15, cols - 35));
    const line1 = ` Projeto: ${this.header.project} │ Workspace: ${wsTruncated}`;
    const paddedLine1 = "║ " + this.truncate(line1, cols - 4).padEnd(cols - 4, " ") + " ║";

    const effortStr = this.header.effort ? ` [${this.header.effort}]` : "";
    const provStr = `${this.header.provider} · ${this.header.model}${effortStr}`;
    const capsStr = `Cap: ${(this.header.tokenCap ?? 0).toLocaleString()} tk │ Max: $${(this.header.costCapUsd ?? 0).toFixed(2)}`;
    const line2 = ` Provedor: ${provStr} │ ${capsStr}`;
    const paddedLine2 = `║ ${this.truncate(line2, cols - 4).padEnd(cols - 4, " ")} ║`;

    return [top, paddedTitle, paddedLine1, paddedLine2, bottom];
  }

  private renderViewport(cols: number, height: number): string[] {
    const outputLines: string[] = [];

    // Filter visible entries based on showLogs
    const visibleEntries = this.entries.filter((entry) => {
      if (entry.type === "message") return true;
      if (entry.level === "error") return true; // Critical errors always visible
      return this.showLogs;
    });

    if (this.showLogs && this.evictedCount > 0) {
      outputLines.push(this.dim(`[... ${this.evictedCount} eventos anteriores descartados por limite de memória ...]`));
    }

    for (const entry of visibleEntries) {
      if (entry.type === "log") {
        const prefix = `[cbm:${entry.subsystem || "engine"}]`;
        const wrapped = this.wrapText(`${prefix} ${entry.content}`, cols - 2);
        for (const w of wrapped) {
          outputLines.push(this.dim(w));
        }
      } else {
        const wrapped = this.wrapText(entry.content, cols - 2);
        outputLines.push(...wrapped);
      }
    }

    if (outputLines.length === 0) {
      outputLines.push(this.dim(" Nenhum evento registrado no histórico da sessão."));
    }

    // Scroll window calculation
    const totalLines = outputLines.length;
    let startIndex = totalLines - height - this.scrollOffset;
    if (startIndex < 0) startIndex = 0;
    const endIndex = Math.min(totalLines, startIndex + height);

    const slice = outputLines.slice(startIndex, endIndex);
    while (slice.length < height) {
      slice.push(" ".repeat(cols));
    }

    return slice.map((line) => line + "\x1b[K");
  }

  private renderStatusBar(cols: number): string[] {
    const divider = "─".repeat(cols);

    const phase = `[${this.telemetry.phase}]`;
    const tokens = `[Tk: ${this.telemetry.tokensUsed.toLocaleString()}]`;
    const cost = `[$${this.telemetry.costUsd.toFixed(3)}]`;
    const lock = this.telemetry.isLocked ? "[LOCK ATIVO]" : "[LIVRE]";
    const logsStatus = this.showLogs ? "[Logs: ON]" : "[Logs: OFF]";
    const scrollInfo = this.scrollOffset > 0 ? " [HISTÓRICO PAUSADO]" : "";

    const statusRow = `${phase} ${tokens} ${cost} ${lock} ${logsStatus}${scrollInfo}`;
    const paddedStatus = this.truncate(statusRow, cols).padEnd(cols, " ");

    const shortcuts = "[L/:logs] Toggle Logs │ [Ctrl+C] Abortar │ [0] Menu/Sair";
    const paddedShortcuts = this.dim(this.truncate(shortcuts, cols).padEnd(cols, " "));

    return [divider, paddedStatus, paddedShortcuts];
  }

  public draw(): void {
    if (!this.isInteractive()) return;
    const frame = this.renderFrame();
    this.stdout.write("\x1b[H" + frame);
  }

  private wrapText(text: string, maxWidth: number): string {
    if (maxWidth <= 0) return [text];
    const rawLines = text.split("\n");
    const result: string[] = [];

    for (const line of rawLines) {
      if (line.length <= maxWidth) {
        result.push(line);
      } else {
        let remaining = line;
        while (remaining.length > maxWidth) {
          result.push(remaining.slice(0, maxWidth));
          remaining = remaining.slice(maxWidth);
        }
        if (remaining.length > 0) {
          result.push(remaining);
        }
      }
    }

    return result;
  }

  private dim(text: string): string {
    if (this.noColor) return text;
    return `\x1b[2m${text}\x1b[0m`;
  }
}
