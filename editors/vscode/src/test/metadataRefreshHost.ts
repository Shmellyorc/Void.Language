import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import * as path from 'node:path';
import {until} from './creationWorkbench';

// Runs in the persistent repository fixture, with the real installed PATH
// tools. Never copy a compiler/server or point settings at another executable.
export async function run(): Promise<void> {
    const root = vscode.workspace.workspaceFolders![0].uri.fsPath;
    assert.equal(path.basename(path.dirname(root)), 'editor_metadata');
    const manifest = path.join(root, 'void.project.toml');
    const lock = path.join(root, 'void.lock');
    const source = vscode.Uri.file(path.join(root, 'src/app.void'));
    const original = await fs.readFile(source.fsPath, 'utf8');
    const library = path.join(root, '../library');
    const cli = (...args: string[]) => promisify(execFile)('voidc', args, {cwd: root, timeout: 30000});
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    let document: vscode.TextDocument | undefined;
    const events: string[] = [];
    try {
        assert(vscode.workspace.isTrusted);
        // The fixture intentionally starts as a standalone source below the
        // folder where its manifest will appear later.
        await assert.rejects(fs.stat(manifest), {code: 'ENOENT'});
        document = await vscode.workspace.openTextDocument(source);
        await vscode.window.showTextDocument(document);
        await api.ready();
        const replace = async (text: string) => {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(source, new vscode.Range(document!.positionAt(0), document!.positionAt(document!.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        };
        const completion = async () => {
            try { return await vscode.commands.executeCommand<vscode.CompletionList>(
                'vscode.executeCompletionItemProvider', source, new vscode.Position(3, 11)); }
            catch { return undefined; } // A request overtaken by a lock update is retired.
        };
        const hasReady = (list: vscode.CompletionList | undefined) =>
            !!list?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'is_ready');
        const expectReady = async (label: string) => {
            await until(completion, hasReady, label);
            events.push(label);
            console.log('metadata_refresh_host: ' + label);
        };
        await fs.writeFile(manifest, 'schema=1\n[project]\nid="org.test.editor.metadata"\nname="Metadata"\nversion="0.1.0"\nkind="application"\n[application]\nid="org.test.editor.metadata.app"\n[sources]\nroots=["src"]\n');
        await cli('add', 'example', '--dir', library, '--project', root);
        const text = 'use pkg/example as rl\n\nfunc main():\n    for rl.\n';
        await replace(text);
        await expectReady('manifest creation and CLI add refresh the live standalone session');
        const manifestDocument = await vscode.workspace.openTextDocument(vscode.Uri.file(manifest));
        assert(!manifestDocument.isDirty);
        await cli('remove', 'example', '--project', root);
        await until(async () => manifestDocument.getText(),
            value => !value.includes('[dependencies.example]'), 'open manifest reloads after CLI removal');
        await until(async () => vscode.languages.getDiagnostics(source),
            diagnostics => diagnostics.some(d => d.message.includes('Undeclared direct package import')),
            'removed dependency diagnostic reaches the open source');
        await until(completion, list => !hasReady(list), 'removed package retires its completion catalog');
        events.push('removed package retires its completion catalog');
        // Remove deliberately preserves developer-owned local sources. Re-add
        // that existing in-project package rather than overwriting its copy.
        await cli('add', 'example', '--dir', path.join(root, 'libs/example'), '--project', root);
        await expectReady('CLI re-add restores the alias without restarting');
        // Source edits inside a package cannot become trusted merely because
        // a watcher fired. Its locked source identity still has to match.
        const installed = path.join(root, 'libs/example/src/example.void');
        const bytes = await fs.readFile(installed);
        try {
            await fs.appendFile(installed, '\n# differs from the locked tree\n');
            await until(completion, list => !hasReady(list), 'changed locked source rejects stale suggestions');
            events.push('changed locked source rejects stale suggestions');
        } finally { await fs.writeFile(installed, bytes); }
        await expectReady('restoring locked inputs restores completion');
        await vscode.commands.executeCommand('void.restartLanguageServer');
        await api.ready();
        await expectReady('cold project startup retains incomplete loop completion');
        assert.equal(await fs.readFile(source.fsPath, 'utf8'), original);
        console.log('metadata_refresh_host: ' + events.length + ' checks passed: ' + events.join('; '));
    } finally {
        await api.deactivate();
        if (document && !document.isClosed) await vscode.commands.executeCommand('workbench.action.revertAndCloseActiveEditor');
        for (const file of [manifest, lock]) await fs.rm(file, {force: true});
        for (const folder of ['libs', '.void']) await fs.rm(path.join(root, folder), {recursive: true, force: true});
    }
}
