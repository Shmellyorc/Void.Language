import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {execFile} from 'node:child_process';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {promisify} from 'node:util';

export async function run(): Promise<void> {
    const root = process.env.VOID_TEST_ROOT!;
    const uri = vscode.Uri.file(path.join(root, 'src/app.void'));
    const document = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(document);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const events: string[] = [];
    const original = document.getText();
    const callOffset = original.indexOf('rl.fade');
    assert(callOffset >= 0);
    const call = document.positionAt(callOffset + 4);
    async function until<T>(read: () => T | Thenable<T>, valid: (result: T) => boolean, event: string) {
        const deadline = Date.now() + 45000;
        while (Date.now() < deadline) {
            const result = await read();
            if (valid(result)) { events.push(event); return result; }
            await new Promise(resolve => setTimeout(resolve, 150));
        }
        throw new Error('Timed out: ' + event);
    }
    try {
        await api.ready();
        await until(() => vscode.languages.getDiagnostics(uri),
            items => items.some(item => item.message.includes('pkg/raylib')), 'uninstalled package diagnostic');
        const installation = await promisify(execFile)(process.env.VOID_TEST_COMPILER!, ['add', 'raylib'], {cwd: root});
        assert(installation.stdout.includes('Installed and locked bundled raylib'));
        const definitions = await until(
            () => vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', uri, call),
            locations => !!locations?.some(item => item.uri.fsPath === path.join(root, 'libs/raylib/src/raylib.void')),
            'installation refresh and physical package definition');
        const definition = definitions!.find(item => item.uri.fsPath.endsWith(path.join('src', 'raylib.void')))!;
        const declarations = await vscode.workspace.openTextDocument(definition.uri);
        const nativeOffset = declarations.getText().indexOf('native.fade', declarations.offsetAt(definition.range.start));
        assert(nativeOffset >= 0);
        await until(() => vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',
            definition.uri, declarations.positionAt(nativeOffset + 8)),
            locations => !!locations?.some(item => item.uri.fsPath === path.join(root, 'libs/raylib/src/raylib_native.void')),
            'definition reaches the physical native declarations');

        const hover = await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', uri, call),
            results => !!results?.some(item => item.contents.some(content =>
                (typeof content === 'string' ? content : content.value).includes('adjustment'))), 'typed raylib hover documentation');
        const legend = await vscode.commands.executeCommand<vscode.SemanticTokensLegend>(
            'vscode.provideDocumentSemanticTokensLegend', uri);
        assert(legend);
        await until(() => vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', uri),
            result => {
                let line = 0, column = 0;
                for (let i = 0; result && i < result.data.length; i += 5) {
                    line += result.data[i];
                    column = result.data[i] ? result.data[i + 1] : column + result.data[i + 1];
                    if (line === call.line && column === call.character - 1) {
                        return legend.tokenTypes[result.data[i + 3]] === 'function';
                    }
                }
                return false;
            }, 'semantic token identifies the raylib function');

        const end = () => document.positionAt(document.getText().length);
        const originalEnd = document.positionAt(original.length);
        const suffix = '\nfunc package_probe():\n    rl.';
        assert(await editor.edit(edit => edit.insert(originalEnd, suffix + 'fade(')));
        const signature = await until(() => vscode.commands.executeCommand<vscode.SignatureHelp>(
            'vscode.executeSignatureHelpProvider', uri, end(), '('),
            result => !!result?.signatures.some(item => item.label.includes('fade') && item.label.includes('f32')),
            'callable raylib signature and parameter hint');
        assert(await editor.edit(edit => edit.replace(new vscode.Range(originalEnd, end()), suffix)));
        const completion = await until(() => vscode.commands.executeCommand<vscode.CompletionList>(
            'vscode.executeCompletionItemProvider', uri, end(), '.'),
            result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'fade'),
            'rl. completion in an unsaved edit');
        assert(await editor.edit(edit => edit.replace(new vscode.Range(document.positionAt(original.length),
            document.positionAt(document.getText().length)), '\nfunc invalid_call():\n    rl.fade()\n')));
        await until(() => vscode.languages.getDiagnostics(uri),
            items => items.some(item => item.severity === vscode.DiagnosticSeverity.Error &&
                item.range.start.line >= document.lineCount - 3), 'invalid native wrapper call diagnostic');
        assert(await editor.edit(edit => edit.delete(new vscode.Range(document.positionAt(original.length),
            document.positionAt(document.getText().length)))));
        assert(await document.save());
        await until(() => vscode.languages.getDiagnostics(uri), items => !items.some(item =>
            item.severity === vscode.DiagnosticSeverity.Error), 'valid unsaved-source recovery');
        await fs.writeFile(process.env.VOID_RAYLIB_EDITOR_EVIDENCE!, JSON.stringify({
            status: 'PASS', editor: vscode.version, events, hover, signature, completionCount: completion?.items.length
        }, null, 2));
    } catch (error) {
        await fs.writeFile(process.env.VOID_RAYLIB_EDITOR_EVIDENCE!, JSON.stringify({
            status: 'FAIL', events, error: String(error)
        }, null, 2));
        throw error;
    } finally {
        await api.deactivate();
    }
}
