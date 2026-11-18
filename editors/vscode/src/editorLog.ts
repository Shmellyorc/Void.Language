import * as vscode from 'vscode';

export function createProtocolTrace(): vscode.LogOutputChannel & {update(): void} {
    const output = vscode.window.createOutputChannel('VOID LSP Trace');
    const changed = new vscode.EventEmitter<vscode.LogLevel>();
    const configuredLevel = () => vscode.workspace.getConfiguration('void').get<string>('trace.server', 'off') === 'off'
        ? vscode.LogLevel.Off : vscode.LogLevel.Trace;
    let level = configuredLevel();
    const write = (message: string, ...args: unknown[]) => output.appendLine([message, ...args].join(' '));
    return {
        name: output.name,
        get logLevel() { return level; },
        onDidChangeLogLevel: changed.event,
        update() {
            const next = configuredLevel();
            if (next !== level) { level = next; changed.fire(level); }
        },
        trace(message, ...args) { if (level === vscode.LogLevel.Trace) write(message, ...args); },
        debug: write, info: write, warn: write, error: write,
        append: output.append.bind(output), appendLine: output.appendLine.bind(output),
        replace: output.replace.bind(output), clear: output.clear.bind(output),
        show: output.show.bind(output), hide: output.hide.bind(output),
        dispose() { changed.dispose(); output.dispose(); }
    };
}

export class DiagnosticLog {
    private readonly seen = new Map<string, Set<string>>();
    private readonly pending = new Map<string, {timer: NodeJS.Timeout; write: () => void}>();

    constructor(private readonly output: vscode.LogOutputChannel) {}

    update(uri: vscode.Uri, diagnostics: vscode.Diagnostic[]): void {
        const file = uri.toString();
        const timer = this.pending.get(file);
        if (timer) clearTimeout(timer.timer);
        this.pending.delete(file);
        if (!diagnostics.length) {
            this.seen.delete(file);
            return;
        }
        // Write persistent problems once per location and code, even as the
        // compiler's wording changes while the user types the same identifier.
        const write = () => {
            this.pending.delete(file);
            const previous = this.seen.get(file) ?? new Set<string>();
            const keys = new Set<string>();
            let count = 0;
            let omitted = 0;
            for (const diagnostic of diagnostics) {
                const {severity, range, message} = diagnostic;
                if (severity !== vscode.DiagnosticSeverity.Error && severity !== vscode.DiagnosticSeverity.Warning) continue;
                const code = typeof diagnostic.code === 'object' ? diagnostic.code.value : diagnostic.code ?? 'diagnostic';
                const position = `${range.start.line + 1}:${range.start.character + 1}`;
                const key = `${severity} ${position} ${code}`;
                if (keys.has(key)) continue;
                keys.add(key);
                if (previous.has(key)) continue;
                if (++count > 4) { ++omitted; continue; }
                const entry = `voidls analysis ${uri.fsPath}:${position} [${code}] ${message.replace(/\s*\r?\n\s*/g, ' ')}`;
                if (severity === vscode.DiagnosticSeverity.Error) this.output.error(entry);
                else this.output.warn(entry);
            }
            this.seen.set(file, keys);
            if (omitted) this.output.info(`voidls analysis ${uri.fsPath}: ${omitted} more new diagnostics in Problems.`);
        };
        this.pending.set(file, {timer: setTimeout(write, 500), write});
    }

    flush(): void {
        for (const {timer, write} of this.pending.values()) {
            clearTimeout(timer);
            write();
        }
        this.pending.clear();
        this.seen.clear();
    }
}
