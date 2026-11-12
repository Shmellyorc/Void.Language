import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

const manifest = `schema=1
[project]
id="org.example.fixes"
name="Fixes"
version="1.0.0"
kind="application"
entry="src/main.void"
[application]
id="org.example.fixes.app"
[sources]
roots=["src"]
`;
const source = 'func main() -> i32:\n    while false:\n        pass\n    return 0\n';
const span = new vscode.Range(1, 4, 1, 9);

async function until<T>(read: () => PromiseLike<T> | T, accept: (value: T) => boolean, description: string): Promise<T> {
    const deadline = Date.now() + 25000;
    while (Date.now() < deadline) {
        const result = await read();
        if (accept(result)) return result;
        await new Promise(resolve => setTimeout(resolve, 75));
    }
    throw new Error('Timed out waiting for ' + description);
}

async function project(root: string, text: string): Promise<vscode.Uri> {
    const main = path.join(root, 'src', 'main.void');
    await fs.mkdir(path.dirname(main), {recursive: true});
    await fs.writeFile(path.join(root, 'void.project.toml'), manifest);
    await fs.writeFile(main, text);
    return vscode.Uri.file(main);
}

export async function run(): Promise<void> {
    assert(vscode.workspace.isTrusted);
    const root = process.env.VOID_SEMANTIC_ROOT!;
    const uri = await project(root, source);
    const child = path.join(root, 'other');
    const next = await project(child, 'func main() -> i32:\n    return 1\n');
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api: {ready(): Promise<void>; status(): {entry?: string}; deactivate(): Promise<void>} = await extension.activate();
    try {
        const document = await vscode.workspace.openTextDocument(uri);
        const editor = await vscode.window.showTextDocument(document);
        await api.ready();
        const diagnose = () => vscode.languages.getDiagnostics(uri).some(diagnostic => diagnostic.code === 'VOID1104');
        const actions = () => vscode.commands.executeCommand<vscode.CodeAction[]>('vscode.executeCodeActionProvider', uri, span, 'quickfix');
        const offer = () => until(actions, values => values?.length === 1, 'compiler quick fix');
        const execute = (action: vscode.CodeAction) => {
            assert.equal(action.title, "Replace 'while' with 'for'");
            assert(!action.edit && action.command?.command === 'void.applyQuickFix');
            return vscode.commands.executeCommand<boolean>(action.command.command, ...action.command.arguments ?? []);
        };
        await until(diagnose, Boolean, 'obsolete loop diagnostic');
        const [oldVersion] = await offer();
        assert(await editor.edit(builder => builder.insert(new vscode.Position(3, 0), '    # unsaved\n')));
        assert.equal(await execute(oldVersion), false);
        assert(document.getText().includes('while false'));
        const [current] = await offer();
        assert.equal(await execute(current), true);
        assert(document.isDirty && document.getText().includes('for false'));
        await until(diagnose, value => !value, 'cleared diagnostic');
        assert.equal(await fs.readFile(uri.fsPath, 'utf8'), source);
        assert.deepEqual(await actions(), []);

        assert(await editor.edit(builder => builder.replace(new vscode.Range(1, 4, 1, 7), 'while')));
        const [oldProject] = await offer();
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(next));
        await vscode.workspace.getConfiguration('void').update('project.root', child, vscode.ConfigurationTarget.Workspace);
        await api.ready();
        await until(() => api.status().entry, entry => !!entry && vscode.Uri.file(entry).fsPath === next.fsPath, 'selected project');
        await vscode.window.showTextDocument(document);
        assert.equal(await execute(oldProject), false);
        assert(document.getText().includes('while false'));
        assert.equal(await fs.readFile(uri.fsPath, 'utf8'), source);
        console.log('quick_fix_editor_host: compiler action, stale version, project switch and disk isolation passed');
    } finally {
        await api.deactivate();
    }
}
