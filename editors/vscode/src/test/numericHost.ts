import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {until} from './creationWorkbench';

const checks: string[] = [];
async function replace(document: vscode.TextDocument, source: string) {
    const edit = new vscode.WorkspaceEdit();
    edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), source);
    assert(await vscode.workspace.applyEdit(edit));
}
export async function run(): Promise<void> {
    const root = process.env.VOID_NUMERIC_HOST_ROOT!;
    const output = process.env.VOID_NUMERIC_HOST_EVIDENCE!;
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    let api: {ready(): Promise<void>; deactivate(): Promise<void>} | undefined;
    let status = 'FAIL';
    try {
        assert(vscode.workspace.isTrusted);
        const helper = await vscode.workspace.openTextDocument(path.join(root, 'modes.void'));
        const document = await vscode.workspace.openTextDocument(path.join(root, 'entry.void'));
        await vscode.window.showTextDocument(document);
        api = await extension.activate();
        await api!.ready();
        const member = document.positionAt(document.getText().indexOf('.ready') + 2);
        const definition = async () => vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri, member);
        await until(definition, definitions => !!definitions?.some(item => item.uri.fsPath === helper.uri.fsPath), 'imported numeric enum definition');
        checks.push('imported definition');
        const hover = async () => {
            const values = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', document.uri, member);
            return values?.flatMap(value => value.contents.map(content => typeof content === 'string' ? content : content.value));
        };
        await until(hover, values => !!values?.some(value => value.replaceAll('&nbsp;', ' ').replaceAll('\\', '').includes('mode.ready = 7 (i8)')), 'exact enum hover');
        checks.push('declared value in hover');
        const source = helper.getText();
        await vscode.window.showTextDocument(helper, vscode.ViewColumn.Beside);
        await replace(helper, source.replace('gone = -128', 'gone = 7'));
        await until(() => vscode.languages.getDiagnostics(helper.uri),
            values => values.some(value => value.message.includes('Duplicate numeric enum discriminant')), 'dirty duplicate diagnostic');
        assert(helper.isDirty);
        await until(definition, values => !values?.length, 'no stale navigation');
        await replace(helper, source);
        await until(definition, values => !!values?.some(item => item.uri.fsPath === helper.uri.fsPath), 'recovered navigation');
        checks.push('dirty import invalidates and recovers');
        const original = document.getText();
        await replace(document, original.replace('.ready', '.'));
        const cursor = document.positionAt(document.getText().indexOf('m.mode.') + 'm.mode.'.length);
        await until(async () => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', document.uri, cursor),
            value => !!value?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'gone' &&
                item.detail?.includes('-128')), 'dirty enum completion');
        checks.push('dirty completion retains signed backing value');
        await replace(document, original.replace('raw as?', '"🎮" as?'));
        const bad = document.positionAt(document.getText().indexOf('as?'));
        await until(() => vscode.languages.getDiagnostics(document.uri),
            values => values.some(value => value.message.includes('as? requires an integer') &&
                value.range.isEqual(new vscode.Range(bad, bad.translate(0, 3)))), 'UTF-16 as? range');
        checks.push('UTF-16 conversion error');
        await replace(document, original.replace('raw as? m.mode', 'raw   as?    m.mode'));
        const edits = await until(async () => vscode.commands.executeCommand<vscode.TextEdit[]>('vscode.executeFormatDocumentProvider', document.uri,
            {insertSpaces: true, tabSize: 4}), value => !!value?.length, 'formatted as?');
        let formatted = document.getText();
        for (const edit of edits.sort((a, b) => b.range.start.compareTo(a.range.start))) {
            formatted = formatted.slice(0, document.offsetAt(edit.range.start)) + edit.newText +
                formatted.slice(document.offsetAt(edit.range.end));
        }
        assert.equal(formatted, original);
        await replace(document, original);
        await document.save();
        await helper.save();
        checks.push('formatting preserves the cast token and values');
        status = 'PASS';
        console.log(`numeric_enum_editor: ${checks.length} checks passed`);
    } finally {
        await api?.deactivate();
        await fs.writeFile(output, JSON.stringify({status, editor: vscode.version, checks}, null, 2) + '\n');
    }
}
