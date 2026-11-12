import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import {workbench} from './creationWorkbench';

const manifest = `schema=1
[project]
id='org.test.constructor'
name='Constructor'
version='1.0.0'
kind='application'
[application]
id='org.test.constructor'
[sources]
roots=['src']
`;
const game = `struct game:
    name: string
    running: bool

func (g game) init(name: string):
    g.name = name

func main():
    var test = game(name = "player")
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

const signature = (uri: vscode.Uri, position: vscode.Position) =>
    vscode.commands.executeCommand<vscode.SignatureHelp | undefined>('vscode.executeSignatureHelpProvider', uri, position);

async function verifyConstructor(document: vscode.TextDocument): Promise<vscode.Position> {
    await replace(document, game);
    const editor = await vscode.window.showTextDocument(document);
    const name = document.positionAt(game.indexOf('game(name'));
    await until(() => vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', document.uri, name),
        locations => !!locations?.some(location => location.uri.fsPath === document.uri.fsPath && location.range.start.line === 4),
        'constructor navigation to init');
    const beforeParen = name.translate(0, 'game'.length);
    await replace(document, game.replace('game(name = "player")', 'game'));
    await vscode.commands.executeCommand('hideSuggestWidget');
    editor.selection = new vscode.Selection(beforeParen, beforeParen);
    const port = process.env.VOID_TEST_CDP_PORT;
    const ui = port ? await workbench(port) : undefined;
    try {
        await vscode.commands.executeCommand('type', {text: '('});
        assert.equal(document.lineAt(name.line).text, '    var test = game()');
        if (ui) await until(() => ui.visible('game(name: string)', '.parameter-hints-widget'), value => value > 0,
            'automatic constructor parameter hints');
        const info = await until(() => signature(document.uri, editor.selection.active), Boolean, 'game signature');
        assert(info);
        assert.equal(info.signatures[0].label, 'game(name: string)');
        assert.equal(info.activeParameter, 0);
        assert.deepEqual(info.signatures[0].parameters[0].label, 'name: string');
        assert.equal(info.signatures[0].parameters[0].documentation, undefined);

        await vscode.commands.executeCommand('type', {text: 'na'});
        const list = await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',
            document.uri, editor.selection.active);
        const candidate = list?.items.find(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'name =');
        assert(candidate?.textEdit);
        if (ui) {
            await vscode.commands.executeCommand('editor.action.triggerSuggest');
            await until(() => ui.visible('name =', '.suggest-widget.visible .monaco-list-row.focused'), value => value > 0,
                'constructor named argument suggestion');
            await vscode.commands.executeCommand('acceptSelectedSuggestion');
        } else {
            const edit = new vscode.WorkspaceEdit();
            edit.set(document.uri, [candidate.textEdit]);
            assert(await vscode.workspace.applyEdit(edit));
        }
        assert.equal(document.lineAt(name.line).text, '    var test = game(name = )');
        assert(document.isDirty);
    } finally { ui?.close(); }
    await replace(document, game);
    return beforeParen.translate(0, 3);
}

export async function run(): Promise<void> {
    assert(vscode.workspace.isTrusted);
    const root = process.env.VOID_SEMANTIC_ROOT!;
    const file = path.join(root, 'src/main.void');
    await fs.mkdir(path.dirname(file), {recursive: true});
    await fs.writeFile(path.join(root, 'void.project.toml'), manifest);
    await fs.writeFile(file, game);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api: {ready(): Promise<void>; status(): {entry?: string}; deactivate(): Promise<void>} = await extension.activate();
    try {
        const document = await vscode.workspace.openTextDocument(file);
        await vscode.window.showTextDocument(document);
        await api.ready();
        const parameter = await verifyConstructor(document);
        await until(() => signature(document.uri, parameter), value => value?.signatures[0].label === 'game(name: string)',
            'constructor signature before project switch');
        assert.equal(await fs.readFile(file, 'utf8'), game, 'dirty editing must leave the source file intact');

        const child = path.join(root, 'child');
        const next = vscode.Uri.file(path.join(child, 'src/main.void'));
        await fs.mkdir(path.dirname(next.fsPath), {recursive: true});
        await fs.writeFile(path.join(child, 'void.project.toml'), manifest.replaceAll('org.test.constructor', 'org.test.child'));
        await fs.writeFile(next.fsPath, game.replaceAll('game', 'other'));
        const nextDocument = await vscode.workspace.openTextDocument(next);
        await vscode.window.showTextDocument(nextDocument);
        await vscode.workspace.getConfiguration('void').update('project.root', child, vscode.ConfigurationTarget.Workspace);
        await api.ready();
        await until(() => api.status().entry, entry => !!entry && vscode.Uri.file(entry).fsPath === next.fsPath, 'the selected project');
        const nextParameter = nextDocument.positionAt(nextDocument.getText().indexOf('other(name') + 'other(na'.length);
        await until(() => signature(next, nextParameter), value => value?.signatures[0].label === 'other(name: string)',
            'new project constructor signature');
        assert.equal(await signature(document.uri, parameter), undefined, 'Previous project must not expose constructor assistance');
        console.log('constructor_editor_host: native typing, signature, named completion, init navigation and project switch passed');
    } finally { await api.deactivate(); }
}
