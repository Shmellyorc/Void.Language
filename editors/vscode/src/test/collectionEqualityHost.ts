import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

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
        const text = (items: vscode.Hover[]) => (items ?? []).flatMap(item => item.contents).map(content => typeof content === 'string' ? content : 'value' in content ? content.value : '').join('\n').replaceAll('\\', '').replaceAll('&nbsp;', ' ');
        async function until<T>(read: () => Thenable<T>, accept: (value: T) => boolean): Promise<T> {
            const deadline = Date.now() + 15000; let last: unknown;
            do {
                const value = await read(); last = value;
                if (accept(value)) return value;
                await new Promise(resolve => setTimeout(resolve, 30));
            } while (Date.now() < deadline);
            throw new Error('Collection equality provider did not become current: ' + JSON.stringify(last));
        }
        const hover = (needle: string, delta: number) => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point(needle, delta));
        const result = await until(() => hover('var equal', 5), value => text(value).includes('bool'));
        assert(!text(result).includes('$collection'));
        observations.push('Content comparison result inferred as readable bool');
        assert(text(await hover('var different', 5)).includes('bool'));
        observations.push('Derived inequality result remains bool');
        const input = await hover('var left', 5);
        assert(text(input).includes('[i32]') && !text(input).includes('/@pkg/'));
        observations.push('Collection operand hover remains readable');
        const signature = await until(() => vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri, point('same(left, right)', 'same(left, ri'.length), ','), value => !!value?.signatures.length);
        assert.equal(signature.activeParameter, 1);
        assert(signature.signatures[0].label.includes('right: readonly [i32]') && JSON.stringify(signature).includes('second sequence'));
        editor.selection = new vscode.Selection(point('same(left, right)', 13), point('same(left, right)', 13));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        observations.push('Documented readonly sequence signature and active right argument');
        const operators = await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, point('return left == right', 'return left '.length)), value => !!value?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === '=='));
        assert(operators.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === '!='));
        assert(!JSON.stringify(operators).includes('$collection_equal'));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        observations.push('Builtin equality/inequality completion hides private helpers');
        const definition = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri, point('same(left, right)', 2));
        assert(definition?.some(item => item.range.start.line === 4));
        observations.push('Comparison helper source navigation');
        const refs = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider', document.uri, point('operator ==', 'operator '.length));
        assert(refs && refs.length >= 2);
        observations.push('Element custom equality retains source references');
        const tokens = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', document.uri);
        assert(tokens?.data.length);
        observations.push('Semantic coloring provider populated');
        async function replace(value: string): Promise<void> {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), value);
            assert(await vscode.workspace.applyEdit(edit));
        }
        await replace(original.replace('const right: [i32]', 'const right: [bool]'));
        await until(async () => vscode.languages.getDiagnostics(document.uri), value => value.some(item => item.severity === vscode.DiagnosticSeverity.Error && item.message.includes('matching collection and element types')));
        observations.push('Mismatched collection elements receive readable diagnostic');
        await replace(original);
        await until(async () => vscode.languages.getDiagnostics(document.uri), value => !value.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        await until(() => hover('var equal', 5), value => text(value).includes('bool'));
        observations.push('Valid unsaved recovery restores comparison metadata');
    } finally {
        await fs.writeFile(process.env.VOID_COLLECTION_EQUALITY_REPORT!, JSON.stringify({editor: vscode.version, observations}, null, 2));
        await api.deactivate();
    }
}
