import {crashEnvironment} from './crashDiagnostics';
import * as vscode from 'vscode';
import {execFile} from 'node:child_process';
import {basename} from 'node:path';
import {promisify} from 'node:util';
import {inside} from './config';

interface Selection {
    compiler: string;
    root: string;
    requestedRoot: string;
    verify(): void;
    verifySelection(): Promise<void>;
    refresh(): Promise<void>;
}
interface Library extends vscode.QuickPickItem {
    args: string[];
    verifySource?(): void;
}

const exec = promisify(execFile);
const digest = (value: unknown): value is string => typeof value === 'string' && /^[0-9a-f]{64}$/.test(value);
const operand = (value: unknown): value is string =>
    typeof value === 'string' && value.length > 0 && !value.startsWith('-') && !value.includes('\0');

async function invoke(project: Selection, args: string[], readOnly: boolean): Promise<string> {
    project.verify();
    const argv = args[0] === 'package' ? args : [...args, '--project', project.requestedRoot];
    const running = exec(project.compiler, argv,
        {cwd: project.root, env: crashEnvironment(), shell: false, windowsHide: true, timeout: readOnly ? 60000 : undefined, maxBuffer: 4 * 1024 * 1024});
    running.child.stdin?.end();
    try { return (await running).stdout; }
    catch (error) {
        const failure = error as Error & {stdout?: string; stderr?: string};
        let detail: string | undefined;
        try {
            const json = JSON.parse(failure.stdout ?? '');
            if (json?.schema === 1 && json.status === 'error' && typeof json.message === 'string') detail = json.message;
        } catch { /* CLI maintenance diagnostics normally arrive on stderr. */ }
        throw new Error(detail || failure.stderr?.trim() || failure.message);
    }
}

async function discover(project: Selection, args: string[]): Promise<Record<string, unknown>> {
    const stdout = await invoke(project, args, true);
    try {
        const result = JSON.parse(stdout);
        if (result?.schema === 1) return result;
    } catch { /* Report an incompatible protocol consistently. */ }
    throw new Error('Use the matching voidc with support for editor library management.');
}

function catalog(packages: unknown): Library[] {
    if (!Array.isArray(packages)) throw new Error('Invalid library catalog from voidc.');
    return packages.map(item => {
        if (!operand(item?.alias) || typeof item.id !== 'string' || typeof item.version !== 'string')
            throw new Error('Invalid library catalog from voidc.');
        if (item.kind === 'bundled') return {
            label: item.alias, description: 'Bundled', detail: `${item.id} ${item.version}`, args: [item.alias, '--bundled'],
        };
        if (item.kind === 'registered' && digest(item.identity)) return {
            label: item.alias, description: 'Registered', detail: `${item.id} ${item.version}`,
            args: [item.alias, '--registered', item.identity],
        };
        throw new Error('Invalid library source in the voidc catalog.');
    });
}

async function askAlias(title: string): Promise<string | undefined> {
    return vscode.window.showInputBox({title, prompt: 'Enter the name to use in imports, for example: pkg/my_library.',
        validateInput: value => operand(value) ? undefined : 'Enter a library alias.'});
}

async function addLibrary(project: Selection): Promise<Library | undefined> {
    const {packages} = await discover(project, ['package', 'list', '--json']);
    project.verify();
    const local = {label: 'From local folder', args: []};
    const github = {label: 'From GitHub', args: []};
    const choice = await vscode.window.showQuickPick([...catalog(packages), local, github], {
        title: 'Add VOID library', placeHolder: `Choose a library for ${basename(project.root)}`, matchOnDescription: true, matchOnDetail: true,
    });
    if (!choice) return;
    project.verify();
    if (choice === local) {
        const folders = await vscode.window.showOpenDialog({
            canSelectFiles: false, canSelectFolders: true, canSelectMany: false,
            defaultUri: vscode.Uri.file(project.root), title: 'Select a VOID library folder', openLabel: 'Select Folder',
        });
        if (!folders?.length) return;
        const folder = folders[0];
        if (folder.scheme !== 'file') throw new Error('Select a folder on the local filesystem.');
        const verifySource = () => {
            const dirty = vscode.workspace.textDocuments.find(d => d.isDirty && d.uri.scheme === 'file' && inside(folder.fsPath, d.uri.fsPath));
            if (dirty) throw new Error(`Save ${basename(dirty.uri.fsPath)} before adding files from this library.`);
        };
        verifySource();
        const metadata = await discover(project, ['add', '--suggest-alias', '--dir', folder.fsPath]);
        if (metadata.alias !== null && !operand(metadata.alias)) throw new Error('Invalid library alias from voidc.');
        const alias = metadata.alias ?? await askAlias('Local library alias');
        if (alias === undefined) return;
        if (!operand(alias)) throw new Error('Invalid library alias.');
        return {label: alias, args: [alias, '--dir', folder.fsPath, '--copy'], verifySource};
    }
    if (choice === github) {
        const url = await vscode.window.showInputBox({
            title: 'GitHub library URL', prompt: 'Paste an HTTPS repository or GitHub subtree URL.',
            placeHolder: 'https://github.com/owner/repository',
            validateInput: value => value.trim().startsWith('https://github.com/') ? undefined : 'Enter an HTTPS GitHub URL.',
        });
        if (!url) return;
        // A URL or directory basename is not compiler-owned package metadata.
        const alias = await askAlias('GitHub library alias');
        if (alias === undefined) return;
        if (!operand(alias)) throw new Error('Invalid library alias.');
        return {label: alias, args: [alias, '--git', url.trim()]};
    }
    return choice;
}

async function removeLibrary(project: Selection, relationships: unknown): Promise<Library | undefined> {
    if (!Array.isArray(relationships)) throw new Error('Invalid dependency list from voidc.');
    const libraries: Library[] = [];
    for (const relation of relationships) {
        if (relation?.direct === false) continue;
        if (relation?.direct !== true || !operand(relation.alias) || typeof relation.target !== 'string')
            throw new Error('Invalid direct dependency from voidc.');
        libraries.push({label: relation.alias, description: relation.target, args: [relation.alias]});
    }
    if (!libraries.length) {
        void vscode.window.showInformationMessage('This VOID project has no direct dependencies.');
        return;
    }
    const choice = await vscode.window.showQuickPick(libraries, {title: 'Remove VOID library',
        placeHolder: `Choose a direct dependency of ${basename(project.root)}`});
    if (!choice) return;
    project.verify();
    const confirmed = await vscode.window.showQuickPick(['Cancel', 'Remove Library'], {
        title: `Remove "${choice.label}" from ${basename(project.root)}?`,
    });
    return confirmed === 'Remove Library' ? choice : undefined;
}

export function libraryCommands(selected: () => Promise<Selection>, output: vscode.LogOutputChannel) {
    let active = false;
    return async (command: 'add' | 'remove'): Promise<void> => {
        if (active) return;
        active = true;
        const title = command === 'add' ? 'Add Library' : 'Remove Library';
        try {
            const project = await selected();
            const state = await discover(project, ['deps', 'explain', '--json', '--for-edit']);
            const token = state.edit_token;
            if (state.status !== 'ok' || !digest(token)) throw new Error('Invalid saved project state from voidc.');
            project.verify();
            const library = await (command === 'add' ? addLibrary(project) : removeLibrary(project, state.relationships));
            if (!library) return;
            // The compiler revalidates the saved root, manifest and lock before
            // acquisition and publication. No task, shell or consent input is involved.
            const result = await vscode.window.withProgress({location: vscode.ProgressLocation.Notification,
                title: `VOID: ${command === 'add' ? 'Adding' : 'Removing'} ${library.label}…`},
                async () => {
                    await project.verifySelection();
                    library.verifySource?.();
                    return invoke(project, [command, ...library.args, '--edit-token', token], false);
                });
            if (result.trim()) output.info(result.trim());
            const message = `VOID: ${command === 'add' ? 'Added' : 'Removed'} ${library.label} ${command === 'add' ? 'to' : 'from'} ${basename(project.root)}.`;
            let advice: string | undefined;
            try { await project.refresh(); }
            catch (error) {
                output.warn(`Refresh VOID libraries for ${project.root}: ${String(error)}`);
                advice = 'Use VOID: Refresh Project to update editor diagnostics.';
            }
            try {
                const checked = await invoke(project, ['tidy', '--check'], true);
                if (checked.trim()) output.info(checked.trim());
            } catch (error) {
                output.warn(`VOID tidy --check for ${project.root}: ${error instanceof Error ? error.message : String(error)}`);
                advice ??= 'Review the dependency check in Output → VOID.';
            }
            if (advice) void vscode.window.showWarningMessage(`${message} ${advice}`);
            else void vscode.window.showInformationMessage(message);
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            output.error(`VOID ${title}: ${message}`);
            void vscode.window.showErrorMessage(`VOID ${title}: ${message.split(/\r?\n/).slice(0, 6).join('\n')}`);
        } finally {
            active = false;
        }
    };
}
