import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

// G79 real editor providers, isolated profile and source extension only.
export async function run(): Promise<void> {
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const observations: string[] = [];
    try {
        await api.ready();
        const document = await vscode.workspace.openTextDocument(path.join(process.env.VOID_TEST_ROOT!, 'main.void'));
        const editor = await vscode.window.showTextDocument(document);
        const original = document.getText();
        const point = (needle: string, delta = 0) => document.positionAt(document.getText().indexOf(needle) + delta);
        const hoverText = (items: vscode.Hover[]) => items.flatMap(item => item.contents).map(content => typeof content === 'string' ? content : 'value' in content ? content.value : '').join('\n').replaceAll('\\', '').replaceAll('&nbsp;', ' ');
        async function until<T>(read: () => Thenable<T>, accept: (value: T) => boolean): Promise<T> {
            const deadline = Date.now() + 15000;
            let last: unknown;
            do {
                const value = await read(); last = value;
                if (accept(value)) return value;
                await new Promise(resolve => setTimeout(resolve, 30));
            } while (Date.now() < deadline);
            throw new Error('Nullable editor result did not become current: ' + JSON.stringify(last));
        }
        for (const [needle, expected] of [['var health:', 'i32?'], ['var nickname:', 'string?'], ['var items:', '[string]?'], ['var actor:', 'player?']]) {
            const hover = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point(needle, 5)), result => hoverText(result).includes(expected));
            assert(!hoverText(hover).includes('$nullable') && !hoverText(hover).includes('/@pkg/'));
            observations.push('Readable ' + expected + ' hover');
        }
        const signature = await until(() => vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri, point('update(health, nickname', 'update(health, '.length + 2), ','), result => !!result?.signatures.length);
        assert.equal(signature.activeParameter, 1);
        for (const type of ['health: i32?', 'label: string?', 'items: [string]?', 'owner: player?', '-> i32?']) assert(signature.signatures[0].label.includes(type), signature.signatures[0].label);
        assert(JSON.stringify(signature).includes('Optional label'));
        editor.selection = new vscode.Selection(point('update(health, nickname', 'update(health, '.length + 2), point('update(health, nickname', 'update(health, '.length + 2));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        observations.push('Nullable signature, documentation and active parameter 1');
        const generic = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('echo(health)', 1)), result => hoverText(result).includes('value: i32?'));
        assert(hoverText(generic).includes('-> i32?'));
        observations.push('Concrete nullable generic call hover');
        async function replace(text: string): Promise<void> {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        }
        await replace(original.replace('return present.health - 7', 'return present.'));
        editor.selection = new vscode.Selection(point('return present.', 'return present.'.length), point('return present.', 'return present.'.length));
        const completions = await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, editor.selection.active, '.'), result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'health'));
        assert(completions.items.length);
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        observations.push('Unsaved present-payload member completion');
        await replace(original.replace('var health: i32? = null', 'var health: i32 = null'));
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        await replace(original);
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => !result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        observations.push('Non-nullable rejection and unsaved invalid-to-valid recovery');
        const tokens = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', document.uri);
        assert(tokens?.data.length);
        observations.push('Semantic token provider remains populated');
    } finally {
        await fs.writeFile(process.env.VOID_NULLABLE_REPORT!, JSON.stringify({editor: vscode.version, observations}, null, 2));
        await api.deactivate();
    }
}
