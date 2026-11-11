import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import {until} from './creationWorkbench';

export async function verifyEditorBasics(root: string): Promise<void> {
    const file = vscode.Uri.file(path.join(root, 'editor-input.void'));
    await fs.writeFile(file.fsPath, '');
    const document = await vscode.workspace.openTextDocument(file);
    const editor = await vscode.window.showTextDocument(document);
    const setting = vscode.workspace.getConfiguration('void.editor', document);
    const scope = vscode.ConfigurationTarget.Workspace;
    const initialSetting = setting.inspect<boolean>('useTabs')?.workspaceValue;
    const initialLanguageSetting = setting.inspect<boolean>('useTabs')?.workspaceLanguageValue;
    const options = (spaces: boolean) => until(() => editor.options,
        current => current.insertSpaces === spaces && current.tabSize === 4 && current.indentSize === 4, 'VOID indentation options');

    async function fixture(text: string, start = text.length, end = start): Promise<void> {
        assert(await editor.edit(edit => edit.replace(new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text)));
        editor.selection = new vscode.Selection(document.positionAt(start), document.positionAt(end));
    }
    async function type(text: string, expected: string, cursor?: number): Promise<void> {
        await vscode.commands.executeCommand('type', {text});
        assert.equal(document.getText(), expected, `Typing ${JSON.stringify(text)}`);
        if (cursor !== undefined) {
            assert(editor.selection.isEmpty);
            assert.equal(document.offsetAt(editor.selection.active), cursor, 'cursor after native type command');
        }
    }
    async function enter(line: string, indentation: string): Promise<void> {
        await fixture(line);
        const expected = line + '\n' + indentation;
        await type('\n', expected, expected.length);
    }
    try {
        await options(false);
        for (const [open, close] of [['"', '"'], ['(', ')'], ['[', ']'], ['{', '}']]) {
            await fixture('');
            await type(open, open + close, 1);
            await type(close, open + close, 2);
            await fixture('value', 0, 5);
            await type(open, open + 'value' + close);
        }
        await fixture('# quote ');
        await type('"', '# quote "', 9);
        await fixture('var text = "a\\ b"', 14);
        await type('"', 'var text = "a\\" b"', 15);
        await fixture('');
        await type('"', '""', 1);
        await type('"', '""', 2);
        await type('"', '"""', 3);
        await type('content', '"""content');
        await type('"', '"""content"');
        await type('"', '"""content""');
        await type('"', '"""content"""');

        for (const header of ['func main():', 'func (p point) move():', 'for item in items:', 'if ready:',
            'else if ready:', 'else:', 'value struct point:', 'interface movable:', 'enum direction:',
            'property (p point) x: i32:', 'get:', 'set(value):', 'match value:', 'Ok(var n):', '_:',
            '"stop":', 'direction.north:', 'if text == "# x:": # comment', 'var result = match value:',
            'with resource as handle:', 'var callback = func():', 'defer {']) await enter(header, '\t');
        await enter('func main():', '\t');
        await type('if ready:', 'func main():\n\tif ready:');
        const nested = 'func main():\n\tif ready:\n\t\t';
        await type('\n', nested, nested.length);
        await enter('func main():\n\tif ready:\n\t\tpass', '\t\t');
        await vscode.commands.executeCommand('outdent');
        assert(document.getText().endsWith('\n\t'), 'dedent from nested statement');
        await enter('func main():\n\t', '\t');
        await enter('func main():\n\tvar item: i32 # :', '\t');
        await enter('func main():\n\tvar item:', '\t');
        await enter('func main():\n\tvar text = "if ready:"', '\t');
        await enter('func main():\n\t# if ready:', '\t');
        await enter('func main():\n\tvar text = """\n\ttext\n\tif ready:', '\t');

        await enter('func main():', '\t');
        await vscode.commands.executeCommand('paste', {text: 'pass'});
        assert.equal(document.getText(), 'func main():\n\tpass');
        assert(await document.save());
        assert.equal(await fs.readFile(file.fsPath, 'utf8'), 'func main():\n\tpass');

        const plain = await vscode.window.showTextDocument(await vscode.workspace.openTextDocument({language: 'plaintext', content: 'plain'}), vscode.ViewColumn.Beside);
        const plainOptions = {...plain.options};
        await setting.update('useTabs', false, scope);
        await options(true);
        assert.equal(document.getText(), 'func main():\n\tpass', 'settings must preserve existing bytes');
        assert.equal(plain.options.insertSpaces, plainOptions.insertSpaces);
        assert.equal(plain.options.tabSize, plainOptions.tabSize);
        await vscode.window.showTextDocument(document, editor.viewColumn);
        await enter('func main():', '    ');
        await vscode.commands.executeCommand('paste', {text: 'pass'});
        assert(await document.save());
        assert.equal(await fs.readFile(file.fsPath, 'utf8'), 'func main():\n    pass');
        await setting.update('useTabs', true, scope, true);
        await options(false);
        assert.equal(document.getText(), 'func main():\n    pass', 'language override must preserve existing bytes');
        await setting.update('useTabs', initialLanguageSetting, scope, true);
        await options(true);
        await setting.update('useTabs', initialSetting, scope);
        await options(false);
        await vscode.commands.executeCommand('workbench.action.closeActiveEditor');
        await fs.unlink(file.fsPath);
        console.log('editor_basics_host: pairing, overtype, surrounding, native indentation, setting changes, paste and save passed');
    } finally {
        await setting.update('useTabs', initialLanguageSetting, scope, true);
        await setting.update('useTabs', initialSetting, scope);
    }
}
