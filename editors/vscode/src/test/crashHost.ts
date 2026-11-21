import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import cp = require('node:child_process');
import {promises as fs} from 'node:fs';
async function until<T>(read: () => T | PromiseLike<T>, valid: (value: T) => boolean, label: string): Promise<T> {
    const end = Date.now() + 20000;
    while (Date.now() < end) {const result = await read(); if (valid(result)) return result; await new Promise(resolve => setTimeout(resolve, 25));}
    throw new Error('Timed out: ' + label);
}
export async function run(): Promise<void> {
    assert.equal(process.platform, 'linux', 'This qualification injects native Linux worker signals');
    const root = process.env.VOID_G90_ROOT!, profile = process.env.VOID_G90_PROFILE!;
    const directory = path.join(root, 'reports');
    const children: {child: cp.ChildProcess; enabled?: string}[] = [];
    const spawn = cp.spawn;
    (cp as {spawn: typeof cp.spawn}).spawn = ((...args: Parameters<typeof cp.spawn>) => {
        const child = spawn(...args);
        if (String(args[0]) === process.env.VOIDLS_PATH)
            children.push({child, enabled: (args[2] as cp.SpawnOptions | undefined)?.env?.VOID_CRASH_DIAGNOSTICS});
        return child;
    }) as typeof cp.spawn;
    const messages: string[] = [], showError = vscode.window.showErrorMessage;
    (vscode.window as {showErrorMessage: typeof showError}).showErrorMessage = ((message: string) => {
        messages.push(message); return Promise.resolve(undefined);
    }) as typeof showError;
    const extension = vscode.extensions.getExtension('void-language.void-language'); assert(extension);
    const document = await vscode.workspace.openTextDocument(vscode.Uri.file(path.join(root, 'main.void')));
    await vscode.window.showTextDocument(document);
    const api = await extension.activate() as {ready(): Promise<void>; status(): {state: string}; deactivate(): Promise<void>};
    const config = vscode.workspace.getConfiguration('void');
    const files = async () => (await fs.readdir(directory).catch(() => [] as string[])).filter(name => name.endsWith('.json'));
    const output = async () => {
        const logs = path.join(path.dirname(root), 'logs');
        const paths = (await fs.readdir(logs, {recursive: true})).filter(file => path.basename(file).endsWith('VOID.log'));
        return (await Promise.all(paths.map(file => fs.readFile(path.join(logs, file), 'utf8')))).join('\n');
    };
    try {
        await api.ready(); assert.equal(api.status().state, 'running');
        assert.equal(children.at(-1)?.enabled, '0', 'editor default overrides inherited enablement');
        const edit = new vscode.WorkspaceEdit();
        edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), 'func main():\n    unknown_name()\n');
        assert(await vscode.workspace.applyEdit(edit));
        await until(() => vscode.languages.getDiagnostics(document.uri), ds => ds.some(d => d.severity === vscode.DiagnosticSeverity.Error), 'ordinary source diagnostic');
        assert.equal(api.status().state, 'running'); assert.equal((await files()).length, 0);
        await config.update('crashDiagnostics.directory', directory, vscode.ConfigurationTarget.Global);
        await config.update('crashDiagnostics.enabled', true, vscode.ConfigurationTarget.Global); await api.ready();
        await until(() => children.at(-1)?.enabled, v => v === '1', 'enabled native launch');
        assert.equal(api.status().state, 'running');
        const active = children.at(-1)!.child;
        const worker = await until(async () => Number((await fs.readFile(`/proc/${active.pid}/task/${active.pid}/children`, 'utf8')).trim().split(' ')[0]), pid => pid > 0, 'native voidls worker');
        process.kill(worker, 'SIGABRT');
        await until(() => api.status().state, s => s === 'stopped', 'crashed language server');
        await until(files, paths => paths.length === 1, 'local crash artifact');
        const report = path.join(directory, (await files())[0]);
        const data = JSON.parse(await fs.readFile(report, 'utf8'));
        assert.equal(data.process, 'voidls'); assert.equal(data.termination_code, 6);
        await until(output, text => text.includes(report) && text.includes('signal=SIGABRT'), 'report path and native exit in VOID output');
        assert.equal(messages.filter(m => m.includes('terminated unexpectedly')).length, 1);
        const count = children.length;
        await new Promise(resolve => setTimeout(resolve, 250));
        assert.equal(children.length, count, 'no automatic restart loop');
        await vscode.commands.executeCommand('void.restartLanguageServer'); await api.ready();
        assert.equal(api.status().state, 'running'); assert.equal(children.length, count + 1);
        await config.update('crashDiagnostics.enabled', false, vscode.ConfigurationTarget.Global); await api.ready();
        assert.equal(children.at(-1)?.enabled, '0'); assert.equal(api.status().state, 'running');
        assert.equal((await files()).length, 1);
        console.log('G90 real editor: default off, source diagnostics, native crash, report/path, exact signal, one notification, no restart loop, explicit recovery and disabling passed');
    } finally {
        await api.deactivate(); (cp as {spawn: typeof cp.spawn}).spawn = spawn;
        (vscode.window as {showErrorMessage: typeof showError}).showErrorMessage = showError;
    }
}
