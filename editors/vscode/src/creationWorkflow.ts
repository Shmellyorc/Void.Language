import * as vscode from 'vscode';
import {promises as fs} from 'node:fs';
import {basename} from 'node:path';
import {inside, resolveServer} from './config';
import {createReviewed, freshInventory, Inventory, Kind, probeCreation, promptKey, PromptState, reviewPlan} from './creation';

interface Scope { root: string; server: string; }
const offerAction = 'Create VOID project';
const standaloneAction = 'Keep standalone';
const publishAction = 'Create Manifest';

export class CreationWorkflow implements vscode.Disposable {
    private readonly pending = new Set<string>();
    private readonly missingAtSave = new Set<string>();
    private revision = 0;
    private timer?: NodeJS.Timeout;
    private busy = false;
    private disposed = false;
    private readonly subscriptions: vscode.Disposable[];

    constructor(private readonly state: vscode.Memento, private readonly bundled: string,
                private readonly selected: (root: string) => Promise<void>, private readonly report: (message: string) => void) {
        const watcher = vscode.workspace.createFileSystemWatcher('**/*.void', false, true, true);
        this.subscriptions = [
            watcher, watcher.onDidCreate(uri => this.enqueue(uri)),
            vscode.workspace.onDidCreateFiles(event => event.files.forEach(uri => this.enqueue(uri))),
            vscode.workspace.onWillSaveTextDocument(event => {
                const uri = event.document.uri;
                if (!this.accepts(uri)) return;
                event.waitUntil(fs.stat(uri.fsPath).then(() => [], (error: NodeJS.ErrnoException) => {
                    if (error.code === 'ENOENT') this.missingAtSave.add(uri.fsPath);
                    return [];
                }));
            }),
            vscode.workspace.onDidSaveTextDocument(document => {
                if (this.missingAtSave.delete(document.uri.fsPath)) this.enqueue(document.uri);
            }),
            vscode.workspace.onDidCloseTextDocument(document => this.missingAtSave.delete(document.uri.fsPath)),
            vscode.workspace.onDidChangeWorkspaceFolders(() => this.invalidate()),
            vscode.workspace.onDidChangeConfiguration(event => {
                if (event.affectsConfiguration('void')) this.invalidate();
            }),
        ];
    }
    private scope(ignoreSelectors = false): Scope | undefined {
        if (this.disposed || !vscode.workspace.isTrusted) return;
        const folders = vscode.workspace.workspaceFolders;
        if (folders?.length !== 1 || folders[0].uri.scheme !== 'file') return;
        const settings = vscode.workspace.getConfiguration('void');
        if (!ignoreSelectors && (settings.get('project.root', '') || settings.get('project.entry', ''))) return;
        return {root: folders[0].uri.fsPath, server: settings.get('server.path', '')};
    }
    private current(before: Scope, ignoreSelectors = false): boolean {
        const after = this.scope(ignoreSelectors);
        return after?.root === before.root && after.server === before.server;
    }
    private activeOutside(scope: Scope): boolean {
        const active = vscode.window.activeTextEditor?.document;
        return !!active && active.uri.scheme === 'file' && active.languageId === 'void' && !inside(scope.root, active.uri.fsPath);
    }
    private accepts(uri: vscode.Uri): boolean {
        const scope = this.scope();
        return !!scope && uri.scheme === 'file' && uri.fsPath.endsWith('.void') && inside(scope.root, uri.fsPath);
    }
    private enqueue(uri: vscode.Uri): void {
        if (!this.accepts(uri)) return;
        this.pending.add(uri.fsPath);
        ++this.revision;
        this.schedule();
    }
    private schedule(): void {
        if (this.timer) clearTimeout(this.timer);
        if (!this.pending.size || this.busy) return;
        this.timer = setTimeout(() => { this.timer = undefined; void this.automatic(); }, 180);
    }
    private invalidate(): void {
        ++this.revision;
        this.pending.clear();
        this.missingAtSave.clear();
        if (this.timer) clearTimeout(this.timer);
        this.timer = undefined;
    }
    private async automatic(): Promise<void> {
        const scope = this.scope();
        if (!scope || this.busy || this.activeOutside(scope)) { this.pending.clear(); return; }
        const revision = this.revision;
        const fresh = new Set(this.pending);
        this.busy = true;
        try {
            const server = await resolveServer(scope.server, scope.root, this.bundled);
            if (!this.current(scope) || revision !== this.revision) return;
            const root = await probeCreation(server, scope.root);
            if (!this.current(scope) || revision !== this.revision) return;
            this.pending.clear();
            if (this.activeOutside(scope)) return;
            const key = promptKey(root);
            if (this.state.get<PromptState>(key, 'unseen') !== 'unseen' || !freshInventory(root, fresh)) return;
            await this.state.update(key, 'offered' satisfies PromptState);
            if (!this.current(scope)) return;
            const choice = await vscode.window.showInformationMessage('Start a VOID project for these first sources?', offerAction, standaloneAction);
            if (this.disposed) return;
            await this.state.update(key, (choice === offerAction ? 'accepted' : choice === standaloneAction ? 'declined' : 'dismissed') satisfies PromptState);
            if (choice === offerAction && this.current(scope)) await this.review(scope, server, root, 'application');
        } catch (error) {
            if (revision === this.revision) this.pending.clear();
            if (this.current(scope)) this.report(String(error));
        } finally {
            this.busy = false;
            this.schedule();
        }
    }
    private async review(scope: Scope, server: string, root: Inventory, kind: Kind, name = '', packageName = ''): Promise<void> {
        if (!this.current(scope)) return;
        const plan = await reviewPlan(server, root, kind, name, packageName);
        if (!this.current(scope)) return;
        if (plan === 'collision') {
            await this.selectCollision(root.root);
            return;
        }
        const confirmed = await vscode.window.showQuickPick([
            {label: 'Reviewed metadata', kind: vscode.QuickPickItemKind.Separator},
            {label: plan.manifest, detail: `${plan.name} ${plan.version} (${plan.kind})`},
            {label: `Project ID: ${plan.projectId}`},
            {label: `${kind === 'application' ? 'Application' : 'Package'} ID: ${plan.roleId}`,
                description: plan.packageName},
            {label: 'Publish manifest', kind: vscode.QuickPickItemKind.Separator},
            {label: 'Cancel'}, {label: publishAction},
        ], {title: 'Review VOID project creation', placeHolder: 'Choose Create Manifest to publish the reviewed metadata'});
        if (confirmed?.label !== publishAction || !this.current(scope)) return;
        const result = await createReviewed(server, plan);
        if (!this.current(scope)) return;
        if (result === 'collision') await this.selectCollision(plan.root);
        else await this.selected(plan.root);
    }
    private async selectCollision(root: string): Promise<void> {
        void vscode.window.showWarningMessage('A manifest appeared during creation. VOID will select the existing project.');
        await this.selected(root);
    }
    async create(): Promise<void> {
        const scope = this.scope();
        if (!scope || this.busy) return;
        this.busy = true;
        try {
            const server = await resolveServer(scope.server, scope.root, this.bundled);
            if (!this.current(scope)) return;
            const root = await probeCreation(server, scope.root);
            if (!this.current(scope)) return;
            if (root.manifestPresent) { this.report('A VOID manifest already governs this workspace.'); return; }
            const kind = await vscode.window.showQuickPick(['application', 'library'], {title: 'VOID project kind'});
            if ((kind !== 'application' && kind !== 'library') || !this.current(scope)) return;
            const name = await vscode.window.showInputBox({title: 'VOID project name', value: basename(root.root)});
            if (name === undefined || !this.current(scope)) return;
            const packageName = kind === 'library' ? await vscode.window.showInputBox({title: 'VOID package name', value: basename(root.root)}) : '';
            if (packageName === undefined || !this.current(scope)) return;
            await this.state.update(promptKey(root), 'accepted' satisfies PromptState);
            await this.review(scope, server, root, kind, name, packageName);
        } catch (error) {
            if (this.current(scope)) this.report(String(error));
        } finally {
            this.busy = false;
            this.schedule();
        }
    }
    async reset(): Promise<void> {
        const scope = this.scope(true);
        if (!scope || this.busy) return;
        this.busy = true;
        try {
            const server = await resolveServer(scope.server, scope.root, this.bundled);
            if (!this.current(scope, true)) return;
            const root = await probeCreation(server, scope.root);
            if (this.current(scope, true)) await this.state.update(promptKey(root), 'unseen' satisfies PromptState);
        } catch (error) {
            if (this.current(scope, true)) this.report(String(error));
        } finally {
            this.busy = false;
            this.schedule();
        }
    }
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.invalidate();
        this.subscriptions.forEach(item => item.dispose());
    }
}
