import * as vscode from 'vscode';
import {basename, isAbsolute, join, parse} from 'node:path';
import {promises as fs, constants} from 'node:fs';
import {LanguageClient, CloseAction, ErrorAction, State, RevealOutputChannelOn, ServerOptions, LanguageClientOptions} from 'vscode-languageclient/node';
import type {TextEdit as ProtocolTextEdit} from 'vscode-languageclient/node';
import {inside, Project, resolveCompiler, resolveProject, Settings} from './config';
import {CreationWorkflow} from './creationWorkflow';
import {newProjectCommand} from './newProject';
import {compilerExecution} from './projectTask';
import {libraryCommands} from './libraryManagement';
import {createProtocolTrace, DiagnosticLog} from './editorLog';

let manager: ServerManager | undefined;
let creation: CreationWorkflow | undefined;

type ProjectCommand = 'check' | 'build' | 'run' | 'package';
const debugBuildTask = 'VOID: Build Debug Project';
const debugType = process.arch !== 'x64' ? undefined :
    process.platform === 'linux' ? 'lldb-dap' : process.platform === 'win32' ? 'cppdbg' : undefined;

function requireDebugTaskErrorsStop(): void {
    const policy = vscode.workspace.getConfiguration('debug').get('onTaskErrors');
    if (policy !== 'abort' && policy !== 'showErrors')
        throw new Error('Set debug.onTaskErrors to abort or showErrors to prevent launching after a failed VOID build.');
}
function samePath(left: unknown, right: string): boolean {
    return typeof left === 'string' && isAbsolute(left) && inside(left, right) && inside(right, left);
}

interface QuickFixTicket {uri: string; version: number; revision: string; offset: number}

function applyIndentation(editor: vscode.TextEditor): void {
    if (editor.document.languageId !== 'void') return;
    const config = vscode.workspace.getConfiguration('void.editor', editor.document);
    const specified = config.inspect<boolean>('useTabs');
    const explicit = specified?.workspaceFolderLanguageValue ?? specified?.workspaceLanguageValue ??
        specified?.globalLanguageValue ?? specified?.workspaceFolderValue ?? specified?.workspaceValue ?? specified?.globalValue;
    let useTabs = explicit ?? true;
    if (explicit === undefined) {
        for (let line = 0; line < editor.document.lineCount; line++) {
            const prefix = /^( +|\t+)[^\s#]/.exec(editor.document.lineAt(line).text)?.[1];
            if (prefix) { useTabs = prefix[0] === '\t'; break; }
        }
    }
    editor.options = {insertSpaces: !useTabs, tabSize: 4, indentSize: 4};
}

class ServerManager {
    private client?: LanguageClient;
    private project?: Project;
    private watcher?: vscode.FileSystemWatcher;
    private serial: Promise<void> = Promise.resolve();
    private disposed = false;
    private epoch = 0;
    private requestSerial = 0;
    private restartRequested = false;
    private projectRevision = 0n;
    private warned = new Set<string>();
    private fallback?: string;
    private createdRoot?: string;
    private readonly diagnostics: DiagnosticLog;
    constructor(private readonly output: vscode.LogOutputChannel, private readonly trace: vscode.LogOutputChannel,
        private readonly bundled: string, private readonly statusBar: vscode.StatusBarItem) {
        this.diagnostics = new DiagnosticLog(output);
    }
    async refreshProject(): Promise<void> {
        await this.serial;
        try {
            if (!this.client?.isRunning()) throw new Error('Start the VOID language server first.');
            await this.client.sendRequest('void/refreshProject', {});
            this.output.info(`Refreshed VOID project ${this.project?.root ?? ''}`);
        } catch (error) { this.report(`Refresh VOID project: ${String(error)}`, true); }
    }
    async applyQuickFix(offeredEpoch: number, ticket: QuickFixTicket): Promise<boolean> {
        await this.serial;
        const client = this.client, project = this.project, serial = this.requestSerial;
        const editor = vscode.window.activeTextEditor, document = editor?.document;
        if (!vscode.workspace.isTrusted || !client?.isRunning() || !project?.manifestMode || !editor || !document ||
            offeredEpoch !== this.epoch || !ticket || document.languageId !== 'void' || document.uri.scheme !== 'file' ||
            ticket.uri !== document.uri.toString() || ticket.version !== document.version ||
            !inside(project.root, document.uri.fsPath)) return false;
        try {
            const proposal = await client.sendRequest<(ProtocolTextEdit & {expectedText: string}) | null>('void/resolveQuickFix', ticket);
            if (!proposal) return false;
            const root = await fs.realpath(project.requestedRoot ?? project.root);
            const file = await fs.realpath(document.uri.fsPath);
            if (!inside(root, project.root) || !inside(project.root, root) || !inside(root, file) ||
                !inside(file, document.uri.fsPath) || !inside(document.uri.fsPath, file) ||
                !vscode.workspace.isTrusted || this.client !== client || this.project !== project || this.requestSerial !== serial ||
                offeredEpoch !== this.epoch || document.isClosed || document.version !== ticket.version ||
                vscode.window.activeTextEditor !== editor) return false;
            const edit = client.protocol2CodeConverter.asTextEdit(proposal);
            if (document.getText(edit.range) !== proposal.expectedText) return false;
            // TextEditor.edit carries the editor model version through to application.
            return await editor.edit(builder => builder.replace(edit.range, edit.newText));
        } catch (error) {
            this.output.error(`Apply VOID quick fix for ${ticket.uri}: ${String(error)}`);
            return false;
        }
    }
    private async ensureProject(): Promise<void> {
        await this.serial;
        // An explicit task or launch may be the first action in a new folder.
        if (!this.project && vscode.workspace.workspaceFolders?.length === 1) await this.refresh(true);
    }
    async librarySelection() {
        if (!vscode.workspace.isTrusted) throw new Error('Trust this workspace before changing VOID libraries.');
        await this.ensureProject();
        const folder = vscode.workspace.workspaceFolders?.length === 1 ? vscode.workspace.workspaceFolders[0] : undefined;
        const project = this.project, client = this.client, selection = this.requestSerial;
        if (!folder || folder.uri.scheme !== 'file' || !project?.manifestMode || !client?.isRunning())
            throw new Error('Open one trusted local folder and select a VOID manifest project first.');
        const compilerSetting = () => vscode.workspace.getConfiguration('void', folder.uri).get('compiler.path', '');
        const setting = compilerSetting();
        const verify = () => {
            if (!vscode.workspace.isTrusted || this.disposed || this.project !== project || this.client !== client ||
                this.requestSerial !== selection || !client.isRunning() || compilerSetting() !== setting ||
                vscode.workspace.workspaceFolders?.length !== 1 ||
                vscode.workspace.workspaceFolders[0].uri.toString() !== folder.uri.toString())
                throw new Error('The editor or selected project changed. Invoke the command again.');
            const dirty = vscode.workspace.textDocuments.find(document => document.isDirty &&
                document.uri.scheme === 'file' && inside(project.root, document.uri.fsPath));
            if (dirty) throw new Error(`Save ${basename(dirty.uri.fsPath)} before changing VOID libraries. The compiler uses saved files.`);
        };
        const compiler = await resolveCompiler(project.server, setting, folder.uri.fsPath);
        if (!inside(await fs.realpath(folder.uri.fsPath), project.root))
            throw new Error('Select a VOID project inside the trusted workspace folder.');
        verify();
        const verifySelection = async () => {
            verify();
            const state = await client.sendRequest<{root: string; rootAvailable: boolean}>('void/projectState', {});
            verify();
            // Import errors can be resolved by adding a library. Directory
            // replacement, however, always requires a fresh project selection.
            if (typeof state.rootAvailable !== 'boolean')
                throw new Error('Use the matching voidls with support for editor library management.');
            if (!state.rootAvailable || !samePath(state.root, project.root))
                throw new Error('The saved project directory changed. Restart the VOID language server and try again.');
        };
        const refresh = async () => {
            await this.serial;
            if (!vscode.workspace.isTrusted || this.project !== project || this.client !== client) return;
            // A lock change intentionally changes the compiler's context key.
            // Accept that context in the same session, preserving its entry override.
            this.changed();
            await client.sendRequest('void/reselectProject', {entry: project.entryOverride ?? null});
        };
        return {compiler, root: project.root, requestedRoot: project.requestedRoot ?? project.root, verify, verifySelection, refresh};
    }
    private async projectInvocation(command: ProjectCommand, debugBuild = false) {
        await this.ensureProject();
        const folder = vscode.workspace.workspaceFolders?.length === 1 ? vscode.workspace.workspaceFolders[0] : undefined;
        const project = this.project;
        if (!vscode.workspace.isTrusted || !folder || folder.uri.scheme !== 'file' || !project?.manifestMode || !this.client?.isRunning())
            throw new Error('Open a trusted folder and select a VOID manifest project first.');
        const selection = this.requestSerial;
        const settings = vscode.workspace.getConfiguration('void', folder.uri);
        const profile = debugBuild ? 'debug' : settings.get('tasks.profile', 'debug');
        if (!['debug', 'release'].includes(profile)) throw new Error('Choose debug or release in void.tasks.profile.');
        const args = [command, '--project', project.requestedRoot ?? project.root, '--profile', profile];
        if (project.entryOverride) args.push('--entry', project.entryOverride);
        if (command === 'run' || command === 'build') args.push('--reuse-native-approval');
        if (command === 'run') {
            const runArguments = settings.get<unknown>('tasks.runArguments', []);
            if (!Array.isArray(runArguments) || runArguments.some(argument => typeof argument !== 'string' || argument.includes('\0')))
                throw new Error('Set void.tasks.runArguments to an array of literal strings.');
            if (runArguments.length) args.push('--', ...runArguments);
        }
        const compiler = await resolveCompiler(project.server, settings.get('compiler.path', ''), folder.uri.fsPath);
        // The compiler validates saved files itself. A file-watcher event
        // from saving a buffer or publishing an output is not a reselection.
        if (!vscode.workspace.isTrusted || this.project !== project || this.requestSerial !== selection)
            throw new Error('The editor or selected project changed. Invoke the command again.');
        const dirty = this.dirtyProjectDocument(project.root);
        if (dirty) throw new Error(`Save ${basename(dirty.uri.fsPath)} before running a project command. Project commands use saved files.`);
        return {folder, profile, compiler, args, cwd: project.root};
    }
    async projectTask(command: ProjectCommand): Promise<vscode.TaskExecution | undefined> {
        if (!vscode.workspace.isTrusted) return;
        try {
            const invocation = await this.projectInvocation(command);
            const {folder, profile, cwd} = invocation;
            const name = `${command[0].toUpperCase()}${command.slice(1)} Project (${profile})`;
            const task = new vscode.Task({type: 'void', command, profile, project: cwd}, folder, name, 'VOID',
                compilerExecution(invocation), '$void-compiler');
            task.presentationOptions = {panel: vscode.TaskPanelKind.Dedicated, clear: true};
            return await vscode.tasks.executeTask(task);
        } catch (error) { this.report(`VOID ${command}: ${error instanceof Error ? error.message : String(error)}`, true); }
    }
    resolveDebugBuild(task: vscode.Task): vscode.Task | undefined {
        const folder = vscode.workspace.workspaceFolders?.length === 1 ? vscode.workspace.workspaceFolders[0] : undefined;
        if (task.definition.command !== 'debugBuild' || !folder ||
            typeof task.scope !== 'object' || task.scope.uri.toString() !== folder.uri.toString()) return;
        const executable = new vscode.Task(task.definition, folder, task.name, task.source,
            compilerExecution(async () => {
                requireDebugTaskErrorsStop();
                return this.projectInvocation('build', true);
            }), '$void-compiler');
        executable.presentationOptions = {panel: vscode.TaskPanelKind.Dedicated, clear: true};
        return executable;
    }
    async debugProjectLocation(part: 'program' | 'root', beforeBuild = false): Promise<string> {
        if (!vscode.workspace.isTrusted) throw new Error('VOID debugging requires a trusted workspace.');
        if (!debugType) throw new Error('VOID’s generated launch supports Linux x64 and Windows x64.');
        if (debugType === 'lldb-dap' && !vscode.extensions.getExtension('llvm-vs-code-extensions.lldb-dap'))
            throw new Error('Install the LLVM LLDB DAP extension and configure its pinned LLVM 23.1.4 lldb-dap executable.');
        if (debugType === 'cppdbg' && !vscode.extensions.getExtension('ms-vscode.cpptools'))
            throw new Error('Install the Microsoft C/C++ extension and put native GDB 17.2 on PATH.');
        await this.ensureProject();
        if (beforeBuild) requireDebugTaskErrorsStop();
        const project = this.project, client = this.client;
        if (!project?.manifestMode || !client?.isRunning())
            throw new Error('Select a VOID manifest application before debugging.');
        // The build's own publication can race the workspace watcher. Retry
        // once against the same selected project; never use an older result.
        for (let attempt = 0; attempt < 2; ++attempt) {
            const epoch = this.epoch;
            const state = await client.sendRequest<{root: string; entry: string | null; status: string}>('void/projectState', {});
            if (state.status !== 'ready' || state.root !== project.root || !state.entry || !isAbsolute(state.entry) ||
                !inside(project.root, state.entry))
                throw new Error('Resolve project errors or reselect the application entry before debugging.');
            const program = join(project.root, 'build', parse(state.entry).name + (debugType === 'cppdbg' ? '.exe' : ''));
            // VS Code substitutes command variables before running preLaunchTask.
            if (part === 'program' && !beforeBuild) {
                const artifact = await fs.stat(program).catch(() => undefined);
                if (!artifact?.isFile()) throw new Error('Build the selected Debug project with debug_info = "full" or "line" first.');
                await fs.access(program, constants.X_OK);
            }
            if (!vscode.workspace.isTrusted || this.project !== project || this.client !== client) break;
            const dirty = this.dirtyProjectDocument(project.root);
            if (dirty) throw new Error(`Save ${basename(dirty.uri.fsPath)} and build the Debug project before debugging.`);
            if (this.epoch === epoch) return part === 'program' ? program : project.root;
        }
        throw new Error('The selected project changed. Start debugging again.');
    }
    async selectEntry(): Promise<void> {
        if (!vscode.workspace.isTrusted) return;
        const files = await vscode.window.showOpenDialog({canSelectMany: false, filters: {VOID: ['void']}, title: 'Select VOID project entry'});
        if (files?.[0]) {
            if (this.project?.manifestMode) {
                if (!inside(this.project.root, files[0].fsPath)) { this.report('VOID: Select an entry inside the pinned project root.'); return; }
                await vscode.workspace.getConfiguration('void').update('project.root', this.project.root, vscode.ConfigurationTarget.Workspace);
            }
            await vscode.workspace.getConfiguration('void').update('project.entry', files[0].fsPath, vscode.ConfigurationTarget.Workspace);
        }
    }
    report(message: string, repeat = false, severity: 'error' | 'warn' = 'error'): void {
        if (!repeat && this.warned.has(message)) return;
        this.warned.add(message);
        this.output[severity](message);
        const text = message + ' See Output → VOID.';
        if (severity === 'warn') void vscode.window.showWarningMessage(text);
        else void vscode.window.showErrorMessage(text);
    }
    private documents(): vscode.TextDocument[] {
        return vscode.workspace.textDocuments.filter(d => (d.languageId === 'void' || basename(d.uri.fsPath) === 'void.project.toml') && d.uri.scheme === 'file');
    }
    private dirtyProjectDocument(root: string): vscode.TextDocument | undefined {
        return this.documents().find(document => document.isDirty && inside(root, document.uri.fsPath));
    }
    refresh(restart = false): Promise<void> {
        this.restartRequested ||= restart;
        const serial = ++this.requestSerial; ++this.epoch;
        this.serial = this.serial.then(async () => {
            if (serial !== this.requestSerial) return;
            await this.update(this.restartRequested, serial);
            if (serial === this.requestSerial) { this.restartRequested = false; this.createdRoot = undefined; }
        }).catch(e => this.report(`VOID: ${String(e)}`));
        return this.serial;
    }
    selectCreatedRoot(root: string): Promise<void> {
        this.createdRoot = root;
        return this.refresh(true);
    }
    private async stop(): Promise<void> {
        ++this.epoch; this.projectRevision = 0n;
        this.watcher?.dispose(); this.watcher = undefined;
        const client = this.client; this.client = undefined; this.project = undefined;
        try {
            if (client?.isRunning()) await client.dispose(5000);
        } finally {
            this.diagnostics.flush();
            this.statusBar.text = 'VOID: stopped';
        }
    }
    private async update(restart: boolean, serial: number): Promise<void> {
        if (this.disposed || serial !== this.requestSerial) return;
        if (!vscode.workspace.isTrusted) { await this.stop(); this.report('VOID native analysis requires a trusted workspace. Syntax highlighting remains available.', false, 'warn'); return; }
        const folders = vscode.workspace.workspaceFolders ?? [];
        if (folders.some(f => f.uri.scheme !== 'file')) { await this.stop(); this.report('VOID requires a filesystem workspace on the extension host.', false, 'warn'); return; }
        const config = vscode.workspace.getConfiguration('void');
        const settings: Settings = {server: config.get('server.path', ''), entry: config.get('project.entry', ''), root: config.get('project.root', ''), trace: config.get('trace.server', 'off')};
        const creationRoot = !settings.root && !settings.entry && this.createdRoot && folders.length === 1 &&
            inside(folders[0].uri.fsPath, this.createdRoot) && inside(this.createdRoot, folders[0].uri.fsPath) ? this.createdRoot : undefined;
        const docs = this.documents();
        const active = vscode.window.activeTextEditor?.document;
        if (restart) this.fallback = undefined;
        if (!settings.entry && !this.fallback) this.fallback = active && (active.languageId === 'void' || basename(active.uri.fsPath) === 'void.project.toml') && active.uri.scheme === 'file' ? active.uri.fsPath : docs[0]?.uri.fsPath;
        if (!docs.length && !this.client && !restart && !settings.root) return;
        let project: Project;
        try { project = !restart && this.project ? this.project : await resolveProject(
            creationRoot ? {...settings, root: creationRoot} : settings,
            folders.map(f => f.uri.fsPath), creationRoot ? undefined : this.fallback, this.bundled); }
        catch (e) { await this.stop(); this.report(`VOID: ${e instanceof Error ? e.message : String(e)}`); return; }
        for (const doc of docs) if (!inside(project.root, doc.uri.fsPath) || (project.singleFile && doc.uri.fsPath !== vscode.Uri.file(project.entry!).fsPath)) {
            this.report('VOID: This document is outside the selected project or single-file entry. Configure void.project.entry for a multi-file project; use a separate window for unrelated projects.', false, 'warn');
        }
        if (!restart && this.client && this.project?.server === project.server && this.project.entry === project.entry && this.project.root === project.root && this.project.manifestMode === project.manifestMode) return;
        if (serial !== this.requestSerial) return;
        await this.stop();
        if (this.disposed || serial !== this.requestSerial || !vscode.workspace.isTrusted) return;
        const args = project.manifestMode ? ['--project', project.requestedRoot ?? project.root, ...(project.entryOverride ? ['--entry', project.entryOverride] : [])] : ['--entry', project.entry!];
        this.output.info(`Starting ${project.server} ${JSON.stringify(args)}`);
        const selector = {language: 'void', scheme: 'file', pattern: {baseUri: vscode.Uri.file(project.root).toString(),
            pattern: project.singleFile ? basename(project.entry!).replace(/[*?()[\]{}!+@,]/g, c => '[' + c + ']') : '**/*.void'}};
        const serverOptions: ServerOptions = {command: project.server, args,
            options: {cwd: project.root, shell: false}};
        let initialized = false;
        let failureReported = false;
        const reportUnexpected = (): void => {
            if (this.disposed || this.client !== client || failureReported) return;
            failureReported = true;
            this.diagnostics.flush();
            this.output.error(`CRASH voidls connection closed ${initialized ? 'while running' : 'during initialization'} for ${project.root} (${project.server}). Check the process exit and stderr in Output → VOID, then run VOID: Restart Language Server.`);
            this.statusBar.text = 'VOID: server stopped';
            void vscode.window.showErrorMessage('VOID language server terminated unexpectedly. Check Output → VOID, then run VOID: Restart Language Server.');
        };
        this.watcher = vscode.workspace.createFileSystemWatcher(new vscode.RelativePattern(project.root, project.manifestMode ? '**/*' : '**/*.void'));
        const clientOptions: LanguageClientOptions = {
            initializationOptions: {voidGuardedFixes: true, voidInteractivePresentation: true},
            documentSelector: [selector, ...(project.manifestMode ? [{scheme: 'file', pattern: {baseUri: vscode.Uri.file(project.root).toString(), pattern: '**/void.project.toml'}}] : [])],
            synchronize: {fileEvents: this.watcher},
            middleware: {handleDiagnostics: (uri, diagnostics, next) => {
                if (this.client !== client) return;
                next(uri, diagnostics);
                this.diagnostics.update(uri, diagnostics);
            }, provideDefinition: async (document, position, token, next) => {
                const epoch = this.epoch; const result = await next(document, position, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, provideHover: async (document, position, token, next) => {
                const epoch = this.epoch; const result = await next(document, position, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, provideCompletionItem: async (document, position, context, token, next) => {
                const epoch = this.epoch, version = document.version;
                const result = await next(document, position, context, token);
                return !token.isCancellationRequested && !document.isClosed && document.version === version &&
                    this.client === client && epoch === this.epoch ? result : null;
            }, provideReferences: async (document, position, context, token, next) => {
                const epoch = this.epoch; const result = await next(document, position, context, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, provideSignatureHelp: async (document, position, context, token, next) => {
                const epoch = this.epoch; const result = await next(document, position, context, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, provideDocumentSymbols: async (document, token, next) => {
                const epoch = this.epoch; const result = await next(document, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, provideDocumentSemanticTokens: async (document, token, next) => {
                const epoch = this.epoch;
                const version = document.version;
                const result = await next(document, token);
                return !token.isCancellationRequested && !document.isClosed && document.version === version &&
                    this.client === client && epoch === this.epoch ? result : null;
            }, provideWorkspaceSymbols: async (query, token, next) => {
                const epoch = this.epoch; const result = await next(query, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, prepareRename: async (document, position, token, next) => {
                const epoch = this.epoch; const result = await next(document, position, token);
                return !token.isCancellationRequested && this.client === client && epoch === this.epoch ? result : null;
            }, provideRenameEdits: async (document, position, newName, token, next) => {
                const epoch = this.epoch;
                const before = new Map(vscode.workspace.textDocuments.map(d => [d.uri.toString(), {document:d, version:d.version}]));
                const result = await next(document, position, newName, token);
                if (!result || token.isCancellationRequested || this.client !== client || epoch !== this.epoch) return null;
                for (const [uri] of result.entries()) {
                    const previous = before.get(uri.toString());
                    const current = vscode.workspace.textDocuments.find(d => d.uri.toString() === uri.toString());
                    if (previous ? current !== previous.document || current.isClosed || current.version !== previous.version : !!current) return null;
                }
                return result;
            }, provideDocumentFormattingEdits: async (document, options, token, next) => {
                const version = document.version; const epoch = this.epoch;
                const result = await next(document, options, token);
                return !token.isCancellationRequested && !document.isClosed && document.version === version && epoch === this.epoch && this.client === client ? result : [];
            }, provideCodeActions: async (document, range, context, token, next) => {
                const version = document.version, epoch = this.epoch, serial = this.requestSerial;
                if (!vscode.workspace.isTrusted || !project.manifestMode) return [];
                const result = await next(document, range, context, token);
                if (!vscode.workspace.isTrusted || token.isCancellationRequested || document.isClosed ||
                    document.version !== version || this.epoch !== epoch || this.requestSerial !== serial ||
                    this.client !== client) return [];
                for (const item of result ?? []) {
                    if (item instanceof vscode.CodeAction && item.command?.command === 'void.applyQuickFix' &&
                        item.command.arguments?.length === 1)
                        item.command.arguments = [epoch, item.command.arguments[0]];
                }
                return result;
            }},
            outputChannel: this.output, traceOutputChannel: this.trace,
            revealOutputChannelOn: RevealOutputChannelOn.Never, // Never steal focus; explicit errors include the channel location.
            errorHandler: {error: (error, _message, count) => {
                // Do not enter Stopping here: it skips closed() and its handled result.
                // A transport error alone is not proof of process termination.
                if (this.client === client && !this.disposed)
                    this.output.warn(`voidls transport error (${count}) for ${project.root} (${project.server}): ${error.message}`);
                return {action: ErrorAction.Continue, handled: true};
            }, closed: () => {
                reportUnexpected();
                return {action: CloseAction.DoNotRestart, handled: true};
            }}
        };
        const client = new LanguageClient('void', 'VOID', serverOptions, clientOptions);
        this.client = client; this.project = project;
        const changed = () => { ++this.epoch; };
        this.watcher.onDidCreate(changed); this.watcher.onDidChange(changed); this.watcher.onDidDelete(changed);
        if (project.manifestMode) client.onNotification('void/projectState', (state: {schema?: number; mode?: string; root?: string; selectionRevision?: string; entry?: string|null; status?: string; kind?: string}) => {
            if (this.client !== client || state.schema !== 2 || state.mode !== 'project' || state.root !== project.root || !/^[0-9]{1,20}$/.test(state.selectionRevision ?? '')) return;
            const revision = BigInt(state.selectionRevision!); if (revision <= this.projectRevision) return;
            this.projectRevision = revision;
            if (state.entry !== null && typeof state.entry !== 'string') return;
            project.entry = state.entry;
            this.statusBar.text = 'VOID: ' + (state.status === 'error' ? 'project errors' : state.status === 'reselection-required' ? 'reselect required' : state.kind === 'library' ? 'library' : state.entry ? basename(state.entry) : 'project (no entry)');
        });
        try { await client.start();
            const capabilities = client.initializeResult?.capabilities as {experimental?: {voidToolingProtocol?: number; voidProjectProtocol?: number}};
            if (capabilities?.experimental?.voidToolingProtocol !== 1) throw new Error('Incompatible voidls. Install the matching server or configure its executable in void.server.path.');
            if (project.manifestMode && capabilities?.experimental?.voidProjectProtocol !== 1) throw new Error('Incompatible voidls: selected manifest projects require voidProjectProtocol 1. No legacy downgrade is allowed.');
            initialized = true;
            this.statusBar.text = 'VOID: ' + (project.singleFile ? 'standalone ' : '') + (project.entry ? basename(project.entry) : 'project (no entry)');
            this.statusBar.tooltip = project.server + '\n' + project.root + '\n' + (project.entry ?? 'No executable entry');
            this.statusBar.show();
            this.output.info(`voidls ready for ${project.root} (${project.server}, tooling protocol ${capabilities.experimental.voidToolingProtocol})`);
        }
        catch (e) { this.report(`VOID language server failed to start for ${project.root} (${project.server}): ${String(e)}`, true); await this.stop(); }
    }
    async dispose(): Promise<void> { this.disposed = true; await this.serial; await this.stop(); }
    changed(): void { ++this.epoch; }
    status(): {state: string; entry?: string} {
        return {state: this.client?.state === State.Running ? 'running' : 'stopped', entry: this.project?.entry ?? undefined};
    }
}
export function activate(context: vscode.ExtensionContext): {ready: () => Promise<void>; status: () => {state: string; entry?: string}; deactivate: () => Promise<void>} {
    const output = vscode.window.createOutputChannel('VOID', {log: true});
    const trace = createProtocolTrace();
    output.info(`Activating VOID ${context.extension.packageJSON.version} from ${context.extensionPath}`);
    const statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 10);
    statusBar.command = 'void.selectEntry'; statusBar.text = 'VOID: stopped'; statusBar.show();
    const bundled = join(context.extensionPath, 'server', process.platform === 'win32' ? 'voidls.exe' : 'voidls');
    const current = manager = new ServerManager(output, trace, bundled, statusBar);
    const workflow = creation = new CreationWorkflow(context.workspaceState, bundled, root => current.selectCreatedRoot(root),
        message => current.report(`VOID project creation: ${message}`));
    const libraries = libraryCommands(() => current.librarySelection(), output);
    context.subscriptions.push(output, trace, statusBar, workflow,
        vscode.commands.registerCommand('void.addLibrary', () => libraries('add')),
        vscode.commands.registerCommand('void.removeLibrary', () => libraries('remove')),
        vscode.commands.registerCommand('void.newProject', newProjectCommand(bundled)),
        vscode.commands.registerCommand('void.createProject', () => workflow.create()),
        vscode.commands.registerCommand('void.resetCreationPrompt', () => workflow.reset()),
        vscode.commands.registerCommand('void.selectEntry', () => current.selectEntry()),
        vscode.commands.registerCommand('void.refreshProject', () => current.refreshProject()),
        vscode.commands.registerCommand('void.applyQuickFix', (epoch, ticket) => current.applyQuickFix(epoch, ticket)),
        vscode.commands.registerCommand('void.checkProject', () => current.projectTask('check')),
        vscode.commands.registerCommand('void.buildProject', () => current.projectTask('build')),
        vscode.commands.registerCommand('void.runProject', () => current.projectTask('run')),
        vscode.commands.registerCommand('void.packageProject', () => current.projectTask('package')),
        vscode.commands.registerCommand('void.debugProjectProgram', config => current.debugProjectLocation('program', config?.preLaunchTask === debugBuildTask)),
        vscode.commands.registerCommand('void.debugProjectRoot', () => current.debugProjectLocation('root')),
        vscode.tasks.registerTaskProvider('void', {provideTasks: () => [], resolveTask: task => current.resolveDebugBuild(task)}),
        vscode.commands.registerCommand('void.restartLanguageServer', () => current.refresh(true)),
        vscode.tasks.onDidEndTaskProcess(event => {
            const {definition} = event.execution.task;
            if (definition.type !== 'void' || typeof definition.command !== 'string') return;
            const message = `VOID ${definition.command} for ${definition.project ?? 'selected project'}`;
            if (event.exitCode === 0) output.info(`${message} completed`);
            else if (event.exitCode === undefined) output.warn(`${message} stopped without an exit status`);
            else output.error(`${message} failed with exit code ${event.exitCode}. See the task terminal and Problems.`);
        }),
        vscode.workspace.onDidOpenTextDocument(d => { if (d.languageId === 'void' || basename(d.uri.fsPath) === 'void.project.toml') void current.refresh(); }),
        vscode.window.onDidChangeActiveTextEditor(e => { if (e && (e.document.languageId === 'void' || basename(e.document.uri.fsPath) === 'void.project.toml')) void current.refresh(); }),
        vscode.workspace.onDidChangeTextDocument(e => { if (e.document.languageId === 'void' || basename(e.document.uri.fsPath) === 'void.project.toml') current.changed(); }),
        vscode.window.onDidChangeVisibleTextEditors(editors => editors.forEach(applyIndentation)),
        vscode.workspace.onDidOpenTextDocument(document => {
            for (const editor of vscode.window.visibleTextEditors) if (editor.document === document) applyIndentation(editor);
        }),
        vscode.workspace.onDidChangeConfiguration(e => {
            for (const editor of vscode.window.visibleTextEditors) {
                if (e.affectsConfiguration('void.editor.useTabs', editor.document)) applyIndentation(editor);
            }
            if (e.affectsConfiguration('void.trace.server')) trace.update();
            if (['void.server', 'void.project'].some(setting => e.affectsConfiguration(setting))) void current.refresh(true);
        }),
        vscode.workspace.onDidChangeWorkspaceFolders(() => { void current.refresh(true); }),
        vscode.workspace.onDidGrantWorkspaceTrust(() => { void current.refresh(true); }));
    if (debugType) context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(debugType, {
        async resolveDebugConfigurationWithSubstitutedVariables(_folder, config) {
            if (config.request === 'launch' && config.preLaunchTask === debugBuildTask) {
                requireDebugTaskErrorsStop();
                if (!samePath(config.program, await current.debugProjectLocation('program')) ||
                    !samePath(config.cwd, await current.debugProjectLocation('root')))
                    throw new Error('The selected VOID entry changed during the Debug build. Start debugging again.');
            }
            return config;
        }
    }));
    vscode.window.visibleTextEditors.forEach(applyIndentation);
    void current.refresh();
    return {ready: () => current.refresh(), status: () => current.status(), deactivate};
}
export async function deactivate(): Promise<void> { creation?.dispose(); creation = undefined; await manager?.dispose(); manager = undefined; }
