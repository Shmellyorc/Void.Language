import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import {verifyEditorBasics} from './editorBasics';

const manifest = `schema=1
[project]
id='org.test.semantic'
name='Semantic'
version='1.0.0'
kind='application'
[application]
id='org.test.semantic'
[sources]
roots=['src']
`;
const source = `use console
use "./helper.void" as lib
value struct point:
    const x: i32
func (p point) move(delta: i32) -> i32:
    var offset = delta
    return p.x + offset
func main() -> i32:
    var p: point = point(x = 2)
    var total, text = lib.split(p.x)
    var valid, note = lib.status()
    print("雪😀", p.move(lib.print(3)), p.x, total, text, valid, note)
    return 0
`;
const helper = `public func print(n: i32) -> i32:
    return n
public func split(input: i32) -> (total: i32, label: string):
    return input, "ready"
public func status() -> (bool, string):
    return true, "ready"
`;

async function until<T>(read: () => T | Thenable<T>, accept: (value: T) => boolean, label: string): Promise<T> {
    const deadline = Date.now() + 25000;
    while (Date.now() < deadline) {
        const value = await read();
        if (accept(value)) return value;
        await new Promise(resolve => setTimeout(resolve, 75));
    }
    throw new Error('Timed out waiting for ' + label);
}

async function replace(document: vscode.TextDocument, text: string): Promise<void> {
    const edit = new vscode.WorkspaceEdit();
    edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
    assert(await vscode.workspace.applyEdit(edit));
}

function tokenAt(data: Uint32Array, legend: vscode.SemanticTokensLegend, document: vscode.TextDocument,
                 prefix: string, fragment: string, word: string): [string, string, string[]] | undefined {
    const lines = document.getText().split(/\r?\n/);
    const expectedLine = lines.findIndex(line => line.startsWith(prefix));
    assert(expectedLine >= 0 && lines[expectedLine].includes(fragment) && fragment.includes(word));
    const expectedStart = lines[expectedLine].indexOf(fragment) + fragment.indexOf(word);
    let line = 0, start = 0;
    assert.equal(data.length % 5, 0);
    for (let i = 0; i < data.length; i += 5) {
        line += data[i];
        start = data[i] === 0 ? start + data[i + 1] : data[i + 1];
        if (line === expectedLine && start === expectedStart) return [lines[line].slice(start, start + data[i + 2]),
            legend.tokenTypes[data[i + 3]], legend.tokenModifiers.filter((_, bit) => (data[i + 4] & (1 << bit)) !== 0)];
    }
}

const getTokens = (uri: vscode.Uri) => vscode.commands.executeCommand<vscode.SemanticTokens | undefined>('vscode.provideDocumentSemanticTokens', uri);
const getLegend = (uri: vscode.Uri) => vscode.commands.executeCommand<vscode.SemanticTokensLegend | undefined>('vscode.provideDocumentSemanticTokensLegend', uri);

export async function run(): Promise<void> {
    assert(vscode.workspace.isTrusted);
    const root = process.env.VOID_SEMANTIC_ROOT!;
    const app = vscode.Uri.file(path.join(root, 'src/main.void'));
    const lib = vscode.Uri.file(path.join(root, 'src/helper.void'));
    await fs.mkdir(path.dirname(app.fsPath), {recursive: true});
    await fs.writeFile(path.join(root, 'void.project.toml'), manifest);
    await fs.writeFile(app.fsPath, source);
    await fs.writeFile(lib.fsPath, helper);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api: {ready(): Promise<void>; status(): {entry?: string}; deactivate(): Promise<void>} = await extension.activate();
    try {
        await verifyEditorBasics(root);
        const document = await vscode.workspace.openTextDocument(app);
        await vscode.window.showTextDocument(document);
        await api.ready();
        const legend = await until(() => getLegend(app), Boolean, 'the real language-client semantic provider');
        assert(legend);
        const initial = await until(() => getTokens(app), tokens => !!tokens?.data.length, 'initial semantic data');
        assert(initial);
        const expect = (prefix: string, fragment: string, word: string, kind: string, ...flags: string[]) =>
            assert.deepEqual(tokenAt(initial.data, legend, document, prefix, fragment, word), [word, kind, flags]);
        expect('value struct', 'point', 'point', 'struct', 'declaration', 'definition');
        expect('func (p', 'move', 'move', 'method', 'declaration', 'definition');
        expect('func (p', 'delta', 'delta', 'parameter', 'declaration');
        expect('    var offset', 'offset', 'offset', 'variable', 'declaration', 'definition');
        expect('    print(', 'print', 'print', 'function', 'defaultLibrary');
        expect('    print(', 'p.move', 'move', 'method');
        expect('    print(', 'lib.print', 'lib', 'namespace');
        expect('    print(', 'lib.print', 'print', 'function');
        expect('    print(', 'p.x', 'x', 'property', 'readonly');
        expect('    var total', 'total', 'total', 'variable', 'declaration', 'definition');
        expect('    print(', ', text,', 'text', 'variable');

        const dependency = await vscode.workspace.openTextDocument(lib);
        const outputTokens = await until(() => getTokens(lib), tokens => !!tokens?.data.length, 'return label tokens');
        assert(outputTokens);
        assert.deepEqual(tokenAt(outputTokens.data, legend, dependency, 'public func split', 'label:', 'label'),
            ['label', 'parameter', ['declaration']]);
        const position = (fragment: string, delta = 0) => {
            const offset = document.getText().indexOf(fragment);
            assert(offset >= 0, fragment);
            return document.positionAt(offset + delta);
        };
        const signature = (opening: string) => vscode.commands.executeCommand<vscode.SignatureHelp | undefined>(
            'vscode.executeSignatureHelpProvider', app, position(opening, opening.length), '(');
        const named = await until(() => signature('lib.split('), s => !!s?.signatures.length, 'named return signature');
        assert.equal(named?.signatures[0].label, 'split(input: i32) -> (total: i32, label: string)');
        const unnamed = await until(() => signature('lib.status('), s => !!s?.signatures.length, 'unnamed return signature');
        assert.equal(unnamed?.signatures[0].label, 'status() -> (bool, string)');
        const hover = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', app, position('split('));
        const hoverText = hover.flatMap(item => item.contents.map(part => typeof part === 'string' ? part : part.value));
        assert(hoverText.some(part => part.replaceAll('&nbsp;', ' ').replace(/\\([():,])/g, '$1').includes('(total: i32, label: string)')),
            JSON.stringify(hoverText));
        const local = await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',
            app, position(', text,', ', te'.length));
        assert(local.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'text' &&
            item.detail === 'string'));
        const dirtyHelper = helper.replace('label: string', 'dirty_label: string');
        await replace(dependency, dirtyHelper);
        await until(() => signature('lib.split('), s => s?.signatures[0]?.label.includes('dirty_label: string') ?? false,
            'dirty return signature');
        assert(dependency.isDirty && await fs.readFile(lib.fsPath, 'utf8') === helper);

        const format = (insertSpaces: boolean) => vscode.commands.executeCommand<vscode.TextEdit[]>('vscode.executeFormatDocumentProvider', app, {tabSize: 4, insertSpaces});
        const applyFormat = async (edits: vscode.TextEdit[]) => {
            const edit = new vscode.WorkspaceEdit();
            edit.set(document.uri, edits);
            assert(await vscode.workspace.applyEdit(edit));
        };
        const tabEdits = await until(() => format(false), edits => !!edits?.length, 'formatter accepts saved source');
        await applyFormat(tabEdits);
        const tabbed = source.replace(/^ +/gm, indent => '\t'.repeat(indent.length / 4));
        assert.equal(document.getText(), tabbed);
        const spaceEdits = await until(() => format(true), edits => !!edits?.length, 'formatter accepts tabbed dirty source');
        await applyFormat(spaceEdits);
        assert.equal(document.getText(), source);

        const dirty = source.replaceAll('move', 'translate');
        await replace(document, dirty);
        await until(() => getTokens(app), tokens => !!tokens && tokenAt(tokens.data, legend, document,
            '    print(', 'p.translate', 'translate')?.[1] === 'method', 'dirty method spelling');
        assert(document.isDirty && await fs.readFile(app.fsPath, 'utf8') === source);

        await replace(dependency, 'public func print(');
        await until(() => vscode.languages.getDiagnostics(lib), ds => ds.some(d => d.severity === vscode.DiagnosticSeverity.Error), 'dependency diagnostic');
        await until(() => getTokens(app), tokens => !!tokens && !tokens.data.length, 'empty semantic data for failed graph');
        await replace(dependency, helper);
        await until(() => getTokens(app), tokens => !!tokens?.data.length, 'semantic recovery after dependency repair');

        const child = path.join(root, 'child');
        const next = vscode.Uri.file(path.join(child, 'src/other.void'));
        await fs.mkdir(path.dirname(next.fsPath), {recursive: true});
        await fs.writeFile(path.join(child, 'void.project.toml'), manifest.replaceAll('org.test.semantic', 'org.test.child'));
        await fs.writeFile(next.fsPath, 'func main() -> i32:\n    var selected = 7\n    return selected\n');
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(next));
        await vscode.workspace.getConfiguration('void').update('project.root', child, vscode.ConfigurationTarget.Workspace);
        await api.ready();
        await until(() => api.status().entry, entry => !!entry && vscode.Uri.file(entry).fsPath === next.fsPath, 'the newly selected project');
        await until(() => getTokens(next), tokens => !!tokens?.data.length, 'new project semantic data');
        assert(!(await getTokens(app))?.data.length, 'Previous project must not expose semantic tokens');
        console.log('semantic_editor_host: return signatures, destructured completion, resolved roles, UTF-16, dirty changes, failed graph, recovery and project switch passed');
    } finally {
        await api.deactivate();
    }
}
