const vscode = require('vscode');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
async function until(read, valid, label) {
    const end = Date.now() + 20000;
    while (Date.now() < end) {
        const result = await read();
        if (valid(result)) return result;
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    throw new Error('Timed out: ' + label);
}
exports.run = async function () {
    const root = process.env.VOID_TEST_ROOT;
    const uri = vscode.Uri.file(path.join(root, 'main.void'));
    const document = await vscode.workspace.openTextDocument(uri);
    await vscode.window.showTextDocument(document);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate();
    await until(() => api.status().state, state => state === 'running', 'native language server');
    const hover = await until(() => vscode.commands.executeCommand('vscode.executeHoverProvider', uri, new vscode.Position(5, 10)), value => value && value.length, 'global hover');
    assert(hover.some(item => item.contents.some(content => content.value.replaceAll('&nbsp;', ' ').includes('var score: i32'))), JSON.stringify(hover.map(item => item.contents.map(content => content.value))));
    const definition = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, new vscode.Position(5, 23));
    assert(definition.length === 1 && definition[0].uri.fsPath.endsWith('game.void'));
    const references = await vscode.commands.executeCommand('vscode.executeReferenceProvider', uri, new vscode.Position(2, 5));
    assert.equal(references.length, 3);
    const rename = await vscode.commands.executeCommand('vscode.executeDocumentRenameProvider', uri, new vscode.Position(2, 5), 'points');
    assert(rename && rename.entries().length === 2);
    assert.equal(rename.entries().reduce((count, item) => count + item[1].length, 0), 3);
    const original = document.getText();
    const replace = async text => {
        const edits = new vscode.WorkspaceEdit();
        edits.replace(uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
        assert(await vscode.workspace.applyEdit(edits));
    };
    await replace(original.replace('print(score, game.defeated)', 'print(game.)'));
    const completion = await until(() => vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri, new vscode.Position(5, 15), '.'),
        value => value && value.items.some(item => item.label === 'defeated'), 'unsaved alias completion');
    assert(!completion.items.some(item => item.label === 'secret'));
    await replace(original.replace('var score = 0', 'var score = missing'));
    await until(() => vscode.languages.getDiagnostics(uri), value => value.length > 0, 'invalid global diagnostic');
    await replace(original);
    await until(() => vscode.languages.getDiagnostics(uri), value => value.length === 0, 'unsaved global repair');
    assert.equal(fs.readFileSync(uri.fsPath, 'utf8'), original);
    await api.deactivate();
    console.log('PASS real VS Code Stage 11 globals: hover, aliases, definition, references, rename, unsaved completion and diagnostic repair');
};
