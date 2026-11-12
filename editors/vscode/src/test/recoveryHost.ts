import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import {until, workbench} from './creationWorkbench';

const diagnosticCode = (d: vscode.Diagnostic) => typeof d.code === 'object' ? d.code.value : d.code;

export async function run(): Promise<void> {
    const root = process.env.VOID_SEMANTIC_ROOT!;
    const port = process.env.VOID_TEST_CDP_PORT;
    assert(port, 'Set VOID_TEST_CDP_PORT for the real completion and color checks');
    const fixtures = path.resolve(__dirname, '../../../../tests/lsp/fixtures/editor_recovery');
    const main = vscode.Uri.file(path.join(root, 'main.void'));
    const app = vscode.Uri.file(path.join(root, 'app.void'));
    const broken = await fs.readFile(path.join(fixtures, 'main.void'), 'utf8');
    const good = broken.replace('if game.\n', 'if game.is_running:\n');
    assert.notEqual(broken, good);
    await fs.copyFile(path.join(fixtures, 'void.project.toml'), path.join(root, 'void.project.toml'));
    await fs.copyFile(path.join(fixtures, 'app.void'), app.fsPath);
    await fs.writeFile(main.fsPath, good);
    const configuration = vscode.workspace.getConfiguration('editor');
    await configuration.update('wordBasedSuggestions', 'off', vscode.ConfigurationTarget.Workspace);
    await configuration.update('semanticTokenColorCustomizations', {enabled:true, rules:{'variable':'#12ABCD'}}, vscode.ConfigurationTarget.Workspace);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension && vscode.workspace.isTrusted);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const browser = await workbench(port);
    try {
        const document = await vscode.workspace.openTextDocument(main);
        const editor = await vscode.window.showTextDocument(document);
        await api.ready();
        const replace = async (text: string) => {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(main, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        };
        const color = (name: string) => browser.evaluate<boolean>(`[...document.querySelectorAll('.view-line span')].some(e =>
            e.children.length === 0 && e.textContent === ${JSON.stringify(name)} && getComputedStyle(e).color === 'rgb(18, 171, 205)')`);
        const tokens = async () => vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', main);
        const diagnostics = () => vscode.languages.getDiagnostics(main);
        const constructor = document.positionAt(good.indexOf('app()'));
        const definition = () => vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', main, constructor);
        const first = await until(tokens, value => !!value?.data.length, 'baseline semantic provider');
        await until(() => color('game'), Boolean, 'rendered semantic variable color');
        await replace(broken);
        await until(diagnostics, errors => errors.some(d => diagnosticCode(d) === 'VOID1102'), 'real incomplete member diagnostic');
        const cursor = document.positionAt(broken.indexOf('if game.\n') + 'if game.'.length);
        editor.selection = new vscode.Selection(cursor, cursor);
        editor.revealRange(new vscode.Range(cursor, cursor));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(() => browser.visible('is_running', '.suggest-widget.visible .monaco-list-row'), count => count > 0, 'rendered compiler property suggestion');
        await vscode.commands.executeCommand('hideSuggestWidget');
        await until(() => color('game'), Boolean, 'semantic color retained on an invalid document');
        assert((await tokens())?.data.length);
        assert((await definition())?.some(item => item.uri.fsPath === app.fsPath));
        assert(document.isDirty && await fs.readFile(main.fsPath, 'utf8') === good);
        await replace(broken.replace(/\bgame\b/g, 'player').replace('if player.\n', 'if player.is_\n'));
        await until(() => color('player'), Boolean, 'rendered semantic color uses the new invalid revision');
        await replace(good);
        await until(diagnostics, errors => !errors.length, 'error clears in the same editor session');
        await until(tokens, value => !!value && Buffer.from(value.data.buffer).equals(Buffer.from(first.data.buffer)), 'original semantic token bytes restored');
        console.log('editor_recovery_host: rendered popup, semantic color, navigation and same-session diagnostics passed');
    } finally {
        browser.close();
        await api.deactivate();
    }
}
