import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {pause, until, workbench} from './creationWorkbench';

const diagnosticCode = (d: vscode.Diagnostic) => typeof d.code === 'object' ? d.code.value : d.code;
const members = ['incorrect', 'none', 'unknown'];

export async function run(): Promise<void> {
    const root = process.env.VOID_SEMANTIC_ROOT!;
    const port = process.env.VOID_TEST_CDP_PORT;
    const baseline = process.env.VOID_ENUM_BASELINE === '1';
    assert(port, 'Set VOID_TEST_CDP_PORT to inspect the native workbench');
    const fixtures = process.env.VOID_ENUM_FIXTURE ?? path.resolve(__dirname, '../../../../tests/lsp/fixtures/enum_recovery');
    for (const name of ['void.project.toml', 'main.void', 'app.void'])
        await fs.copyFile(path.join(fixtures, name), path.join(root, name));
    const uri = vscode.Uri.file(path.join(root, 'main.void'));
    const original = await fs.readFile(uri.fsPath, 'utf8');
    const completeArm = 'process_type.none:';
    const document = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(document);
    await vscode.workspace.getConfiguration('editor').update('wordBasedSuggestions', 'off', vscode.ConfigurationTarget.Workspace);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const browser = await workbench(port);
    try {
        await api.ready();
        const reveal = (needle: string, caret = needle.length) => {
            const start = document.getText().indexOf(needle);
            assert(start >= 0, needle);
            const position = document.positionAt(start + caret);
            editor.selection = new vscode.Selection(position, position);
            editor.revealRange(new vscode.Range(position, position), vscode.TextEditorRevealType.InCenter);
            return position;
        };
        const legend = await until(
            async () => vscode.commands.executeCommand<vscode.SemanticTokensLegend>('vscode.provideDocumentSemanticTokensLegend', uri),
            value => !!value, 'semantic token legend');
        assert(legend);
        const token = async (position: vscode.Position) => {
            const result = await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens', uri);
            if (!result) return undefined;
            let line = 0, start = 0;
            for (let i = 0; i < result.data.length; i += 5) {
                const [dl, ds, width, kind] = result.data.subarray(i, i + 5);
                line += dl;
                start = dl ? ds : start + ds;
                if (position.line === line && position.character >= start && position.character < start + width)
                    return legend.tokenTypes[kind];
            }
        };
        const colors = (fragment: string, names: string[]) => browser.evaluate<(string | null)[] | null>(`(() => {
            const row = [...document.querySelectorAll('.view-line')].find(e => e.textContent.includes(${JSON.stringify(fragment)}));
            if (!row) return null;
            return ${JSON.stringify(names)}.map(name => {
                const walker = document.createTreeWalker(row, NodeFilter.SHOW_TEXT);
                for (let node = walker.nextNode(); node; node = walker.nextNode()) {
                    if (node.textContent.includes(name)) return getComputedStyle(node.parentElement).color;
                }
                return null;
            });
        })()`);
        const typePosition = reveal(completeArm, 0);
        const memberPosition = typePosition.translate(0, 'process_type.'.length);
        await until(() => token(memberPosition), role => role === 'enumMember', 'completed pattern member token');
        const validRoles = [await token(typePosition), await token(memberPosition)];
        const rendered = await until(() => colors(completeArm, ['process_type', 'none']),
            value => !!value?.every(Boolean), 'rendered enum pattern');
        const theme = vscode.workspace.getConfiguration('workbench').get<string>('colorTheme');
        if (!baseline) assert.deepEqual(validRoles, ['enum', 'enumMember']);
        console.log('enum_recovery_host: complete', JSON.stringify({theme, roles: validRoles, colors: rendered}));

        const displayed = () => browser.evaluate<string[]>(
            "[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row .label-name')].map(e => e.textContent.trim()).sort()");
        const query = async (position: vscode.Position) => {
            const result = await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', uri, position);
            return (result?.items ?? []).filter(item => item.kind === vscode.CompletionItemKind.EnumMember)
                .map(item => typeof item.label === 'string' ? item.label : item.label.label).sort();
        };
        const diagnostics = () => vscode.languages.getDiagnostics(uri).map(diagnosticCode);
        const end = typePosition.translate(0, completeArm.length);
        assert(await editor.edit(builder => builder.replace(new vscode.Range(typePosition, end), 'process_type')));
        reveal('process_type\n', 'process_type'.length);
        await vscode.commands.executeCommand('type', {text: '.'});
        await until(diagnostics, codes => codes.includes('VOID1102'), 'incomplete pattern diagnostic');
        const cursor = editor.selection.active;
        assert(document.lineAt(cursor.line).text.trim() === 'process_type.');
        const suggestions = await query(cursor);
        if (baseline) {
            await pause(1000);
            console.log('enum_recovery_host: baseline dot', JSON.stringify({
                cursor, suggestions, popup: await displayed(), diagnostics: diagnostics(), typeRole: await token(typePosition),
            }));
            assert.deepEqual(suggestions, []);
            assert(!(await displayed()).some(label => members.includes(label)));
        } else {
            assert.deepEqual(suggestions, members);
            await until(displayed, values => members.every(label => values.includes(label)), 'dot-triggered enum suggestion popup');
            await until(() => token(typePosition), role => role === 'enum', 'incomplete pattern enum type token');
            for (const character of ['i', 'n']) {
                await vscode.commands.executeCommand('type', {text: character});
                const prefix = document.lineAt(cursor.line).text.trim();
                assert(prefix === 'process_type.i' || prefix === 'process_type.in', prefix);
                assert.deepEqual(await query(editor.selection.active), ['incorrect']);
                await until(displayed, values => values.includes('incorrect') && !values.includes('unknown'), 'prefix-filtered enum popup');
                assert.equal(await token(typePosition), 'enum');
                assert.equal(await token(memberPosition), undefined);
            }
            await vscode.commands.executeCommand('hideSuggestWidget');
            assert(await editor.edit(builder => builder.insert(editor.selection.active, 'correct:')));
            await until(diagnostics, codes => codes.length === 0, 'completed pattern clears diagnostics');
            await until(() => token(memberPosition), role => role === 'enumMember', 'completed member semantic token');
            assert.equal(await token(typePosition), 'enum');
            console.log('enum_recovery_host: auto-triggered enum popup, partial prefixes, and current semantic roles passed');
        }
        assert(document.isDirty);
        assert.equal(await fs.readFile(uri.fsPath, 'utf8'), original);
    } finally {
        browser.close();
        await api.deactivate();
    }
}
