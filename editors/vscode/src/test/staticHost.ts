import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

// G80 real providers in an isolated editor profile; no installed tool changes.
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
            throw new Error('Static editor result did not become current: ' + JSON.stringify(last));
        }
        const field = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('print(settings.count)', 'print(settings.'.length + 1)), result => hoverText(result).includes('count') && hoverText(result).includes('i32'));
        assert(hoverText(field).includes('static') && hoverText(field).includes('Current player limit') && !hoverText(field).includes('.void::'));
        observations.push('Readable documented static-field hover');
        const method = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('settings.add(2', 'settings.'.length + 1)), result => hoverText(result).includes('amount: i32'));
        assert(hoverText(method).includes('-> i32'));
        observations.push('Static method signature without a receiver parameter');
        const signature = await until(() => vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri, point('settings.add(2, reason)', 'settings.add(2, '.length + 2), ','), result => !!result?.signatures.length);
        assert.equal(signature.activeParameter, 1);
        assert(signature.signatures[0].label.includes('amount: i32') && JSON.stringify(signature).includes('Human-readable reason'));
        editor.selection = new vscode.Selection(point('settings.add(2, reason)', 'settings.add(2, '.length + 2), point('settings.add(2, reason)', 'settings.add(2, '.length + 2));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        observations.push('Parameter help, documentation and active parameter 1');
        const generic = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('bucket<i32>.read()', 'bucket<i32>.'.length + 1)), result => hoverText(result).includes('i32?'));
        assert(!hoverText(generic).includes('.void::'));
        observations.push('Concrete generic static-method hover');
        const definitions = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri, point('print(settings.count)', 'print(settings.'.length + 1));
        assert(definitions?.some(item => item.range.start.line === 2));
        const references = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider', document.uri, point('print(settings.count)', 'print(settings.'.length + 1));
        assert(references?.length >= 3);
        observations.push('Static-field definition and references');
        const tokens = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', document.uri);
        assert(tokens?.data.length);
        observations.push('Populated semantic-coloring provider');
        async function replace(text: string): Promise<void> {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        }
        await replace(original.replace('print(settings.count)', 'print(settings.)'));
        editor.selection = new vscode.Selection(point('print(settings.', 'print(settings.'.length), point('print(settings.', 'print(settings.'.length));
        const completion = await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, editor.selection.active, '.'), result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'count'));
        const labels = new Set(completion.items.map(item => typeof item.label === 'string' ? item.label : item.label.label));
        for (const name of ['count', 'label', 'add']) assert(labels.has(name));
        assert(!labels.has('hidden') && !labels.has('instance'));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        observations.push('Unsaved type completion excludes private and instance members');
        await replace(original.replace('print(settings.count)', 'print(bucket<i32>.)'));
        const generics = await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, point('print(bucket<i32>.', 'print(bucket<i32>.'.length), '.'), result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'item'));
        assert(generics.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'echo'));
        assert(!JSON.stringify(generics).includes('.void::'));
        observations.push('Readable generic static completion');
        await replace(original.replace('settings.count += amount', 'settings.count = "wrong"'));
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        await replace(original);
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => !result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        observations.push('Invalid-to-valid unsaved edit recovery');
    } finally {
        await fs.writeFile(process.env.VOID_STATIC_REPORT!, JSON.stringify({editor: vscode.version, observations}, null, 2));
        await api.deactivate();
    }
}
