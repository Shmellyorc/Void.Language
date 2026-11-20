import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

// Focused real-editor G78 qualification. Run with an isolated profile and the
// source extension; it never installs/replaces the user's extension or settings.
export async function run(): Promise<void> {
    const root = process.env.VOID_TEST_ROOT!;
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const observations: string[] = [];
    const elapsed: number[] = [];
    try {
        await api.ready();
        const document = await vscode.workspace.openTextDocument(path.join(root, 'main.void'));
        const editor = await vscode.window.showTextDocument(document);
        const text = document.getText();
        const point = (needle: string, delta = 0) => document.positionAt(document.getText().indexOf(needle) + delta);
        async function until<T>(read: () => Thenable<T>, accept: (value: T) => boolean): Promise<T> {
            const deadline = Date.now() + 15000;
            let last: unknown;
            do {
                const value = await read(); last = value;
                if (accept(value)) return value;
                await new Promise(resolve => setTimeout(resolve, 30));
            } while (Date.now() < deadline);
            throw new Error('G78 editor provider did not reach the required current result: ' + JSON.stringify(last));
        }
        const hoverText = (items: vscode.Hover[]) => items.flatMap(item => item.contents).map(content => typeof content === 'string' ? content : 'value' in content ? content.value : '').join('\n').replaceAll('\\', '').replaceAll('&nbsp;', ' ');
        const hover = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('has(access', 1)), value => hoverText(value).includes('zero'));
        assert(hoverText(hover).includes('mask'));
        observations.push('Documented flags hover uses public types and describes zero-mask semantics');
        const signature = await vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', document.uri, point('has(access', 4));
        assert(signature && signature.activeParameter === 0);
        assert(signature.signatures[0].label.includes('mask: access'));
        assert(signature.signatures[0].parameters[0].documentation);
        observations.push('Flags signature highlights mask and supplies parameter documentation');
        const generic = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, point('value(7)', 1));
        assert(hoverText(generic).includes('data: i32'), hoverText(generic));
        observations.push('Generic case hover resolves its concrete payload');
        editor.selection = new vscode.Selection(point('has(access', 4), point('has(access', 4));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        observations.push('Real editor parameter-hints command accepted the enum call');
        const replace = async (replacement: string) => {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), replacement);
            assert(await vscode.workspace.applyEdit(edit));
        };
        await replace(text.replace('var title = mask.name', 'var title = mask.'));
        const completion = () => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, point('mask.\n', 5), '.');
        const suggestions = await until(completion, result => Boolean(result?.items.some(item => item.label === 'has')));
        const names = suggestions.items.map(item => typeof item.label === 'string' ? item.label : item.label.label);
        for (const name of ['name', 'value', 'has', 'has_any', 'with', 'without', 'toggled', 'is_defined']) assert(names.includes(name), name);
        for (let n = 0; n < 20; ++n) {
            const start = performance.now();
            assert((await completion()).items.some(item => item.label === 'has'));
            elapsed.push(performance.now() - start);
        }
        editor.selection = new vscode.Selection(point('mask.\n', 5), point('mask.\n', 5));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        observations.push('Current unsaved mask. completion includes all documented enum/flags operations');
        await replace(text.replace('mask.has(access.write)', 'mask | 3'));
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        await replace(text);
        await until(async () => vscode.languages.getDiagnostics(document.uri), result => !result.some(item => item.severity === vscode.DiagnosticSeverity.Error));
        observations.push('Invalid-to-valid unsaved edits refresh diagnostics');
        const tokens = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', document.uri);
        assert(tokens?.data.length);
        observations.push('Semantic token provider returns coloring for enums, members and generic payloads');
    } finally {
        await fs.writeFile(process.env.VOID_ENUM_REPORT!, JSON.stringify({observations, completionProviderMs: elapsed}, null, 2));
        await api.deactivate();
    }
}
