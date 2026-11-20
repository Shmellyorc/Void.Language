import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

// G81 real editor providers, using an isolated profile and the candidate server.
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
        const hoverText = (items: vscode.Hover[]) => (items ?? []).flatMap(item => item.contents).map(content => typeof content === 'string' ? content : 'value' in content ? content.value : '').join('\n').replaceAll('\\', '').replaceAll('&nbsp;', ' ');
        async function until<T>(read: () => Thenable<T>, accept: (value: T) => boolean): Promise<T> {
            const deadline = Date.now() + 15000;
            let last: unknown;
            do {
                const value = await read(); last = value;
                if (accept(value)) return value;
                await new Promise(resolve => setTimeout(resolve, 30));
            } while (Date.now() < deadline);
            throw new Error('Bitwise editor result did not become current: ' + JSON.stringify(last));
        }
        const byte = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('print(result)', 7)), result => hoverText(result).includes('u8'));
        assert(!hoverText(byte).includes('.void::'));
        observations.push('Shift result remains readable u8');
        const complement = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('var complement', 5)), result => hoverText(result).includes('u8'));
        assert(!hoverText(complement).includes('.void::'));
        observations.push('Complement result retains exact width');
        const signature = await until(() => vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri, point('shift_byte(1, count)', 'shift_byte(1, co'.length), ','), result => !!result?.signatures.length);
        assert.equal(signature.activeParameter, 1);
        assert(signature.signatures[0].label.includes('count: i64') && signature.signatures[0].label.includes('-> u8'));
        assert(JSON.stringify(signature).includes('modulo eight'));
        editor.selection = new vscode.Selection(point('shift_byte(1, count)', 15), point('shift_byte(1, count)', 15));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        observations.push('Documented parameter help with active count parameter');
        const definitions = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri, point('shift_byte(1, count)', 2));
        assert(definitions?.some(item => item.range.start.line === 4));
        observations.push('Operator helper navigation');
        const tokens = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', document.uri);
        assert(tokens?.data.length);
        observations.push('Semantic coloring provider populated');
        async function replace(text: string): Promise<void> {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        }
        await replace(original.replace('print(result)', 'print(shift_)'));
        const completions = await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, point('print(shift_', 'print(shift_'.length)), result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'shift_byte'));
        assert(JSON.stringify(completions).includes('u8'));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        observations.push('Helper completion remains available during unsaved typing');
        await replace(original.replace('result >>= 8', 'result >>= true'));
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => result.some(item => item.severity === vscode.DiagnosticSeverity.Error && item.message.includes('Shifts require integer')));
        observations.push('Invalid count produces readable diagnostic');
        await replace(original);
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => !result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        await vscode.commands.executeCommand('editor.action.formatDocument');
        assert(document.getText().includes('<<') && document.getText().includes('>>='));
        const recovered = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('print(result)', 7)), result => hoverText(result).includes('u8'));
        assert(!hoverText(recovered).includes('.void::'));
        observations.push('Valid recovery and formatting preserve operators/types');
    } finally {
        await fs.writeFile(process.env.VOID_BITWISE_REPORT!, JSON.stringify({editor: vscode.version, observations}, null, 2));
        await api.deactivate();
    }
}
