import * as vscode from 'vscode';
import {execFile} from 'node:child_process';
import {isAbsolute} from 'node:path';
import {promisify} from 'node:util';
import {resolveCompiler, resolveServer} from './config';

interface Template {
    name: string;
    shortNames: string[];
    tags: string[];
    inPlace: boolean;
}

const exec = promisify(execFile);
const allStrings = (values: unknown): values is string[] =>
    Array.isArray(values) && values.every(value => typeof value === 'string');
const isTemplate = (value: unknown): value is Template => {
    if (!value || typeof value !== 'object') return false;
    const {name, shortNames, tags, inPlace} = value as Partial<Template>;
    return typeof name === 'string' && name.length > 0 && allStrings(shortNames) &&
        shortNames.length > 0 && shortNames.every(short => /^[a-z][a-z0-9-]*$/.test(short)) &&
        allStrings(tags) && typeof inPlace === 'boolean';
};

async function compilerJson(compiler: string, args: string[], cwd: string): Promise<Record<string, unknown>> {
    const {stdout} = await exec(compiler, args, {cwd, timeout: 60000, maxBuffer: 1024 * 1024});
    try {
        const result = JSON.parse(stdout);
        if (result?.schema === 1) return result;
    } catch { /* Report the protocol error consistently. */ }
    throw new Error('Set void.compiler.path to a voidc version that supports editor template creation.');
}

function workspaceScope() {
    if (!vscode.workspace.isTrusted) throw new Error('Trust this workspace before creating a VOID project.');
    const folders = vscode.workspace.workspaceFolders;
    if (folders?.length !== 1 || folders[0].uri.scheme !== 'file')
        throw new Error('Open one local workspace folder before creating a VOID project.');
    const uri = folders[0].uri;
    const settings = vscode.workspace.getConfiguration('void', uri);
    return {uri, root: uri.fsPath, compiler: settings.get('compiler.path', ''), server: settings.get('server.path', '')};
}

function reportCreationError(error: unknown): void {
    const {stderr, message} = error as Error & {stderr?: string};
    const diagnostic = stderr?.trim().split('\n').slice(0, 6).join('\n');
    void vscode.window.showErrorMessage(`VOID New Project: ${diagnostic || message}`);
}

export function newProjectCommand(bundledServer: string): () => Promise<void> {
    let active = false;
    return async () => {
        if (active) return;
        active = true;
        try {
            const scope = workspaceScope();
            const revalidate = () => {
                const current = workspaceScope();
                if (current.root !== scope.root || current.compiler !== scope.compiler || current.server !== scope.server)
                    throw new Error('The workspace or compiler setting changed. Run VOID: New Project again.');
            };
            const server = scope.compiler ? '' : await resolveServer(scope.server, scope.root, bundledServer);
            const compiler = await resolveCompiler(server, scope.compiler, scope.root);
            revalidate();
            const {templates} = await compilerJson(compiler, ['new', 'list', '--json'], scope.root);
            if (!Array.isArray(templates) || templates.length === 0 || !templates.every(isTemplate))
                throw new Error('The VOID compiler returned an invalid template list. Check void.compiler.path.');
            const choice = await vscode.window.showQuickPick(templates.map(template => ({
                label: template.name, description: template.shortNames.join(', '),
                detail: template.tags.join(', '), template,
            })), {title: 'Choose a VOID project template', matchOnDescription: true, matchOnDetail: true});
            if (!choice) return;
            const name = await vscode.window.showInputBox({title: 'VOID project name', prompt: 'Enter a portable project name, such as MyGame.'});
            if (name === undefined) return;
            let inPlace = false;
            if (choice.template.inPlace) {
                const location = await vscode.window.showQuickPick([
                    {label: 'Create a named project folder', inPlace: false},
                    {label: 'Use an existing empty folder', inPlace: true},
                ], {title: 'Choose how to place the project'});
                if (!location) return;
                inPlace = location.inPlace;
            }
            const selection = await vscode.window.showOpenDialog({
                defaultUri: scope.uri, canSelectFiles: false, canSelectFolders: true, canSelectMany: false,
                title: inPlace ? 'Select an empty folder for the VOID project' : 'Select the parent folder for the VOID project',
                openLabel: 'Select Folder',
            });
            if (!selection?.length) return;
            const folder = selection[0];
            if (folder.scheme !== 'file') throw new Error('Select a folder on the local filesystem.');
            const args = ['new', '--template', choice.template.shortNames[0], '--name', name];
            if (inPlace) args.push('--in-place');
            revalidate();
            const {root} = await compilerJson(compiler, args, folder.fsPath);
            if (typeof root !== 'string' || !isAbsolute(root) || root.includes('\0'))
                throw new Error('The compiler returned an invalid project location. Check the selected destination.');
            const action = 'Open Project';
            void vscode.window.showInformationMessage(`Created VOID project in ${root}.`, action).then(async selection => {
                if (selection === action) {
                    revalidate();
                    await vscode.commands.executeCommand('vscode.openFolder', vscode.Uri.file(root), {forceNewWindow: true});
                }
            }).then(undefined, reportCreationError);
        } catch (error) {
            reportCreationError(error);
        } finally {
            active = false;
        }
    };
}
