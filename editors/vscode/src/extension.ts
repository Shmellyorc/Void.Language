import * as vscode from 'vscode';
import {basename} from 'node:path';
import {LanguageClient, CloseAction, ErrorAction, Trace, State, RevealOutputChannelOn, ServerOptions, LanguageClientOptions} from 'vscode-languageclient/node';
import {inside, Project, resolveProject, Settings} from './config';

let manager: ServerManager | undefined;
class ServerManager {
    private client?: LanguageClient;
    private project?: Project;
    private serial: Promise<void> = Promise.resolve();
    private disposed = false;
    private warned = new Set<string>();
    private fallback?: string;
    constructor(private readonly output: vscode.LogOutputChannel) {}
    private report(message: string, repeat = false): void {
        this.output.appendLine(message);
        if (repeat || !this.warned.has(message)) { this.warned.add(message); void vscode.window.showErrorMessage(message + ' See Output → VOID.'); }
    }
    private documents(): vscode.TextDocument[] {
        return vscode.workspace.textDocuments.filter(d => d.languageId === 'void' && d.uri.scheme === 'file');
    }
    refresh(restart = false): Promise<void> {
        this.serial = this.serial.then(() => this.update(restart)).catch(e => this.report(`VOID: ${String(e)}`));
        return this.serial;
    }
    private async stop(): Promise<void> {
        const client = this.client; this.client = undefined; this.project = undefined;
        if (client) { await client.stop(5000); await client.dispose(); }
    }
    private async update(restart: boolean): Promise<void> {
        if (this.disposed) return;
        if (!vscode.workspace.isTrusted) { await this.stop(); this.report('VOID native analysis requires a trusted workspace. Syntax highlighting remains available.'); return; }
        const folders = vscode.workspace.workspaceFolders ?? [];
        if (folders.some(f => f.uri.scheme !== 'file')) { await this.stop(); this.report('VOID requires a filesystem workspace on the extension host.'); return; }
        const config = vscode.workspace.getConfiguration('void');
        const settings: Settings = {server: config.get('server.path', ''), entry: config.get('project.entry', ''), trace: config.get('trace.server', 'off')};
        const docs = this.documents();
        const active = vscode.window.activeTextEditor?.document;
        if (restart) this.fallback = undefined;
        if (!settings.entry && !this.fallback) this.fallback = active?.languageId === 'void' && active.uri.scheme === 'file' ? active.uri.fsPath : docs[0]?.uri.fsPath;
        if (!docs.length && !this.client && !restart) return;
        let project: Project;
        try { project = await resolveProject(settings, folders.map(f => f.uri.fsPath), this.fallback); }
        catch (e) { await this.stop(); this.report(`VOID: ${e instanceof Error ? e.message : String(e)}`); return; }
        for (const doc of docs) if (!inside(project.root, doc.uri.fsPath) || (project.singleFile && doc.uri.fsPath !== vscode.Uri.file(project.entry).fsPath)) {
            this.report('VOID: This document is outside the selected project or single-file entry. Configure void.project.entry for a multi-file project; use a separate window for unrelated projects.');
        }
        if (!restart && this.client && this.project?.server === project.server && this.project.entry === project.entry && this.project.trace === project.trace) return;
        await this.stop();
        if (this.disposed) return;
        this.output.appendLine(`Starting ${project.server} --entry ${project.entry}`);
        const selector = {language: 'void', scheme: 'file', pattern: {baseUri: vscode.Uri.file(project.root).toString(),
            pattern: project.singleFile ? basename(project.entry).replace(/[*?()[\]{}!+@,]/g, c => '[' + c + ']') : '**/*.void'}};
        const serverOptions: ServerOptions = {command: project.server, args: ['--entry', project.entry],
            options: {cwd: project.root, shell: false}};
        let initialized = false;
        let failureReported = false;
        const reportUnexpected = (reason: string): void => {
            const intentional = this.disposed || this.client !== client;
            this.output.appendLine(`VOID connection ${reason}; initialized=${initialized}, intentional=${intentional}, state=${client.state}`);
            if (!initialized || intentional || failureReported) return;
            failureReported = true;
            // Deduplicate error/close callbacks for this session, not later crashes.
            this.report('VOID language server terminated unexpectedly. Use VOID: Restart Language Server after checking the log.', true);
        };
        const clientOptions: LanguageClientOptions = {
            documentSelector: [selector], outputChannel: this.output, traceOutputChannel: this.output,
            revealOutputChannelOn: RevealOutputChannelOn.Never, // Never steal focus; explicit errors include the channel location.
            errorHandler: {error: (error, _message, count) => {
                // Do not enter Stopping here: it skips closed() and its handled result.
                // A transport error alone is not proof of process termination.
                this.output.appendLine(`VOID connection error (${count}): ${error.message}; state=${client.state}`);
                return {action: ErrorAction.Continue, handled: true};
            }, closed: () => {
                reportUnexpected('closed');
                return {action: CloseAction.DoNotRestart, handled: true};
            }}
        };
        const client = new LanguageClient('void', 'VOID', serverOptions, clientOptions);
        this.client = client; this.project = project;
        try { await client.start(); initialized = true; await client.setTrace(project.trace === 'verbose' ? Trace.Verbose : project.trace === 'messages' ? Trace.Messages : Trace.Off); }
        catch (e) { this.report(`VOID language server failed to start: ${String(e)}`); await this.stop(); }
    }
    async dispose(): Promise<void> { this.disposed = true; await this.serial; await this.stop(); }
    status(): {state: string; entry?: string} {
        return {state: this.client?.state === State.Running ? 'running' : 'stopped', entry: this.project?.entry};
    }
}
export function activate(context: vscode.ExtensionContext): {ready: () => Promise<void>; status: () => {state: string; entry?: string}; deactivate: () => Promise<void>} {
    const output = vscode.window.createOutputChannel('VOID', {log: true});
    const current = manager = new ServerManager(output);
    context.subscriptions.push(output,
        vscode.commands.registerCommand('void.restartLanguageServer', () => current.refresh(true)),
        vscode.workspace.onDidOpenTextDocument(d => { if (d.languageId === 'void') void current.refresh(); }),
        vscode.window.onDidChangeActiveTextEditor(e => { if (e?.document.languageId === 'void') void current.refresh(); }),
        vscode.workspace.onDidChangeConfiguration(e => { if (e.affectsConfiguration('void')) void current.refresh(true); }),
        vscode.workspace.onDidChangeWorkspaceFolders(() => { void current.refresh(true); }),
        vscode.workspace.onDidGrantWorkspaceTrust(() => { void current.refresh(true); }));
    void current.refresh();
    return {ready: () => current.refresh(), status: () => current.status(), deactivate};
}
export async function deactivate(): Promise<void> { await manager?.dispose(); manager = undefined; }
