import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

// G82 provider observations run in existing editors with isolated task profiles.
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
            throw new Error('Generic interface provider did not become current: ' + JSON.stringify(last));
        }
        const declaration = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('interface channel', 'interface ch'.length)), result => hoverText(result).includes('interface channel<t>'));
        assert(!hoverText(declaration).includes('channel<i32>'));
        observations.push('Generic declaration retains symbolic interface channel<t>');
        const requirement = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('func read() -> t', 'func re'.length));
        assert(hoverText(requirement).includes('-> t') && hoverText(requirement).includes('latest value'));
        observations.push('Symbolic requirement hover and source documentation');
        const concrete = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('return current.read()', 'return current.re'.length));
        assert(hoverText(concrete).includes('i32') && !hoverText(concrete).includes('-> t'));
        observations.push('Concrete dispatch hover retains i32');
        const imported = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('const words:', 'const wo'.length));
        assert(hoverText(imported).includes('reader<string>') && !hoverText(imported).includes('/@pkg/'));
        observations.push('Imported specialized type has readable presentation');
        const signature = await until(() => vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri, point('current.write(replacement)', 'current.write(repl'.length), '('), result => !!result?.signatures.length);
        assert.equal(signature.activeParameter, 0);
        assert(signature.signatures[0].label.includes('value: i32') && JSON.stringify(signature).includes('new value'));
        editor.selection = new vscode.Selection(point('current.write(replacement)', 14), point('current.write(replacement)', 14));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        observations.push('Concrete parameter help, documentation and active argument');
        const definitions = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri, point('return current.read()', 'return current.re'.length));
        assert(definitions?.some(item => item.range.start.line === 6));
        const references = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider', document.uri, point('func read() -> t', 'func re'.length));
        assert(references?.some(item => item.range.start.line > 20));
        observations.push('Requirement navigation and source references');
        const symbols = await vscode.commands.executeCommand<vscode.DocumentSymbol[]>('vscode.executeDocumentSymbolProvider', document.uri);
        assert(symbols?.some(item => item.name.startsWith('channel') && item.kind === vscode.SymbolKind.Interface));
        const tokens = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', document.uri);
        assert(tokens?.data.length);
        observations.push('Interface document symbol and semantic tokens available');
        async function replace(text: string): Promise<void> {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        }
        await replace(original.replace('return current.read()', 'return current.'));
        const completions = await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, point('return current.', 'return current.'.length)), result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'read'));
        assert(completions.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'write'));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        observations.push('Specialized member completion during unsaved incomplete typing');
        await replace(original.replace('current.write(replacement)', 'current.write("wrong")'));
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => result.some(item => item.severity === vscode.DiagnosticSeverity.Error && item.message.includes('Type mismatch')));
        observations.push('Wrong specialized argument produces ordinary diagnostic');
        await replace(original);
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => !result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        const recovered = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('return current.read()', 'return current.re'.length)), result => hoverText(result).includes('i32'));
        assert(!hoverText(recovered).includes('-> t'));
        observations.push('Valid unsaved recovery restores concrete types');
    } finally {
        await fs.writeFile(process.env.VOID_GENERIC_INTERFACES_REPORT!, JSON.stringify({editor: vscode.version, observations}, null, 2));
        await api.deactivate();
    }
}
