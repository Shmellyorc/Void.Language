import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root = process.env.VOID_TEST_ROOT!, report = process.env.VOID_MODERNIZATION_REPORT!;
    const file = path.join(root, 'src', 'app.void'), original = await fs.readFile(file, 'utf8');
    const document = await vscode.workspace.openTextDocument(file);
    const editor = await vscode.window.showTextDocument(document);
    const extension = vscode.extensions.getExtension('void-language.void-language'); assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; status(): {state: string}; deactivate(): Promise<void>};
    const browser = await workbench(process.env.VOID_TEST_CDP_PORT!);
    const observations: string[] = [];
    let lastHover = "";
    const point = (needle: string, delta = 0) => {
        const at = document.getText().indexOf(needle); assert(at >= 0, needle);
        return document.positionAt(at + delta);
    };
    const hoverText = (items: vscode.Hover[]) => (items ?? []).flatMap(h => h.contents)
        .map(c => typeof c === 'string' ? c : 'value' in c ? c.value : '').join('\n').replaceAll('&nbsp;', ' ');
    const replace = async (value: string) => {
        const edit = new vscode.WorkspaceEdit();
        edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), value);
        assert(await vscode.workspace.applyEdit(edit));
    };
    try {
        await api.ready(); assert.equal(api.status().state, 'running');
        await until(async () => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri,
            point('rl.config_flags.window_hidden', 'rl.config_flags.'.length + 2)),
            v => { lastHover = hoverText(v); return lastHover.includes('hide window'); }, 'raylib flag source documentation');
        const definitions = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri,
            point('rl.keyboard_key.escape', 'rl.keyboard_key.'.length + 2));
        assert(definitions.some(d => d.uri.fsPath.endsWith('/libs/raylib/src/raylib.void')));
        const refs = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider', document.uri,
            point('rl.keyboard_key.escape', 'rl.keyboard_key.'.length + 2)); assert(refs.length >= 2);
        observations.push('Raylib enum source docs, definition and references resolve to the editable package');
        const signature = await vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri,
            point('rl.set_exit_key(', 'rl.set_exit_key('.length));
        assert(signature?.signatures.some(s => s.label.includes('key: keyboard_key')));
        const standard = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri,
            point('io.open_mode.create_new', 'io.open_mode.'.length + 2));
        assert(hoverText(standard).includes('Exclusively create'));
        observations.push('Typed raylib signature help and standard-library member docs remain readable');
        const rawSignature = await vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri,
            point('rl.is_key_down_code(', 'rl.is_key_down_code('.length));
        assert(rawSignature?.signatures.some(s => s.label.includes('key: i32')));
        const rawHover = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri,
            point('rl.is_key_down_code(', 'rl.'.length + 4));
        assert(hoverText(rawHover).includes('raw provider key code') && hoverText(rawHover).includes('512'));
        const rawCompletion = await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri,
            point('rl.is_key_down_code(', 'rl.'.length));
        assert(rawCompletion.items.some(i => (typeof i.label === 'string' ? i.label : i.label.label) === 'is_key_down_code'));
        observations.push('Raw provider keyboard completion, hover and i32 signature coexist with named-key APIs');

        // Prove the actual editor renders compiler enum metadata.
        await vscode.workspace.getConfiguration('editor').update('semanticTokenColorCustomizations',
            {enabled: true, rules: {enumMember: '#778899'}}, vscode.ConfigurationTarget.Workspace);
        editor.revealRange(new vscode.Range(point('rl.config_flags.window_hidden'), point('rl.config_flags.window_hidden')),
            vscode.TextEditorRevealType.InCenter);
        await until(() => browser.evaluate<boolean>("[...document.querySelectorAll('.view-line span')].some(e => e.textContent.includes('window_hidden') && getComputedStyle(e).color === 'rgb(119, 136, 153)')"),
            Boolean, 'visible enum-member semantic color');
        observations.push('Actual editor applies enumMember semantic highlighting');
        await replace(original.replace('rl.config_flags.window_hidden', 'rl.config_flags.'));
        const cursor = point('rl.config_flags.\n', 'rl.config_flags.'.length);
        editor.selection = new vscode.Selection(cursor, cursor);
        await until(async () => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, cursor),
            v => !!v?.items.some(i => (typeof i.label === 'string' ? i.label : i.label.label) === 'window_hidden'), 'typed flag completion');
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(() => browser.evaluate<string[]>("[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row .label-name')].map(e => e.textContent.trim())"),
            v => v.includes('window_hidden'), 'visible typed enum suggestion');
        observations.push('Typed flag members appear in the actual suggestion popup');
        await vscode.commands.executeCommand('hideSuggestWidget');
        await replace(original.replace('rl.set_exit_key(rl.keyboard_key.escape)', 'rl.set_exit_key(7)'));
        await until(() => vscode.languages.getDiagnostics(document.uri),
            v => v.some(d => d.severity === vscode.DiagnosticSeverity.Error && d.message.includes('keyboard_key')), 'nominal argument diagnostic');
        await replace(original);
        await until(() => vscode.languages.getDiagnostics(document.uri),
            v => !v.some(d => d.severity === vscode.DiagnosticSeverity.Error), 'repaired typed argument');
        assert.equal(api.status().state, 'running');
        assert.equal(await fs.readFile(file, 'utf8'), original);
        observations.push('Wrong numeric arguments diagnose and recover without losing the live server');
    } finally {
        browser.close();
        await fs.writeFile(report, JSON.stringify({editor: vscode.env.appName, version: vscode.version, observations, lastHover, diagnostics: vscode.languages.getDiagnostics().map(([uri, diagnostics]) => ({uri: uri.toString(), diagnostics: diagnostics.map(d => ({message: d.message, code: d.code}))}))}, null, 2) + '\n');
        await api.deactivate();
    }
}
