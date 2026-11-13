import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

export async function run(): Promise<void> {
    const uri = vscode.Uri.file(path.join(process.env.VOID_TEST_ROOT!, 'src/main.void'));
    const document = await vscode.workspace.openTextDocument(uri);
    await vscode.window.showTextDocument(document);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready():Promise<void>; status():unknown; deactivate():Promise<void>};
    const events: string[] = [];
    let lastHover: unknown;
    async function until<T>(read:()=>T|Thenable<T>, valid:(value:T)=>boolean, label:string):Promise<T> {
        const deadline = Date.now() + 30000;
        while (Date.now() < deadline) {
            const value = await read();
            if (valid(value)) { events.push(label); return value; }
            await new Promise(resolve => setTimeout(resolve, 100));
        }
        throw new Error('Timed out: ' + label);
    }
    try {
        assert.equal(vscode.version, '1.141.0');
        await api.ready();
        if (process.env.VOID_TEST_RAYLIB_MISSING_SNAPSHOT) {
            const manifest = vscode.Uri.file(path.join(process.env.VOID_TEST_ROOT!, 'void.project.toml'));
            await until(() => vscode.languages.getDiagnostics(manifest), diagnostics => diagnostics.some(item =>
                item.message.includes('Missing Git dependency snapshot') || item.message.includes('Missing void.lock')),
                'missing public source snapshot or lock diagnostic');
            const original = document.getText();
            const editor = vscode.window.activeTextEditor!;
            assert(await editor.edit(edit => edit.insert(document.positionAt(original.length), '\nfunc package_completion_probe():\n    rl.get_ray_\n')));
            const result = await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', uri,
                new vscode.Position(document.lineCount - 2, 15));
            const candidates = result?.items.filter(item =>
                (typeof item.label === 'string' ? item.label : item.label.label) === 'get_ray_collision_box');
            events.push('completion response captured with unresolved public package');
            assert(await editor.edit(edit => edit.delete(new vscode.Range(document.positionAt(original.length), document.positionAt(document.getText().length)))));
            assert(await document.save());
            for (const command of ['check', 'package']) {
                const exits = new Map<vscode.TaskExecution, number|undefined>();
                const listener = vscode.tasks.onDidEndTaskProcess(event => exits.set(event.execution, event.exitCode));
                try {
                    const task = await vscode.commands.executeCommand<vscode.TaskExecution>(`void.${command}Project`);
                    assert(task);
                    const exit = await until(() => exits.get(task), () => exits.has(task), `${command} rejects missing public snapshot`);
                    assert.equal(exit, 1);
                } finally { listener.dispose(); }
            }
            await fs.writeFile(process.env.VOID_RAYLIB_EDITOR_EVIDENCE!, JSON.stringify({status:'BLOCKED_BY_PUBLIC_FETCH',
                editor:vscode.version, platform:process.platform, packageCompletionQualified:false, candidates,
                diagnostics:vscode.languages.getDiagnostics(manifest).map(item => ({code:item.code, message:item.message})), events}, null, 2));
            return;
        }
        const offset = document.getText().indexOf('rl.fade') + 3;
        assert(offset >= 3);
        const position = document.positionAt(offset);
        await until(async () => {
            const result = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', uri, position.translate(0, 1));
            lastHover = result?.map(item => item.contents.map(content => typeof content === 'string' ? content : content.value));
            return result;
        },
            result => !!result?.some(item => item.contents.some(content =>
                (typeof content === 'string' ? content : content.value).includes('color'))), 'raylib native record signature hover');
        if (process.env.VOID_TEST_RAYLIB_PHYSICAL) {
            const declarations = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', uri, position.translate(0, 1));
            const definition = declarations?.find(item => item.uri.fsPath.endsWith(path.join('src','raylib.void')));
            assert(definition);
            const publicSource = await vscode.workspace.openTextDocument(definition.uri);
            const text = publicSource.getText();
            const call = text.indexOf('native.fade',text.indexOf('func fade('));
            assert(call >= 0);
            const nativeDefinitions = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',
                publicSource.uri, publicSource.positionAt(call + 8));
            const native = nativeDefinitions?.find(item => item.uri.fsPath.endsWith(path.join('src','raylib_native.void')));
            assert(native);
            assert((await fs.readFile(native.uri.fsPath,'utf8')).includes('native "org.voidlang.raylib"'));
            events.push('public wrapper and native declarations resolve to real source files');
        }
        const callbacks = await vscode.workspace.openTextDocument(vscode.Uri.file(path.join(process.env.VOID_TEST_ROOT!, 'src/callbacks.void')));
        const callbackPosition = callbacks.positionAt(callbacks.getText().indexOf('rl.set_trace_log_callback') + 5);
        await until(() => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', callbacks.uri, callbackPosition),
            result => !!result?.some(item => item.contents.some(content =>
                (typeof content === 'string' ? content : content.value).replaceAll('\\_', '_').includes('callback_registration'))), 'final retained callback signature hover');
        const original = document.getText();
        const editor = vscode.window.activeTextEditor!;
        assert(await editor.edit(edit => edit.insert(document.positionAt(original.length), '\nfunc package_completion_probe():\n    rl.get_ray_\n')));
        const completion = new vscode.Position(document.lineCount - 2, 15);
        await until(() => vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', uri, completion),
            result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'get_ray_collision_box'),
            'pinned pkg/raylib typed import completion');
        assert(await editor.edit(edit => edit.delete(new vscode.Range(document.positionAt(original.length), document.positionAt(document.getText().length)))));
        assert(await document.save());
        await until(() => vscode.languages.getDiagnostics(uri), result => !result.some(item => item.severity === vscode.DiagnosticSeverity.Error),
            'consumer source has no editor errors');
        for (const command of process.env.VOID_TEST_RAYLIB_PHYSICAL ? ['check', 'package'] : ['check']) {
            const exits = new Map<vscode.TaskExecution, number|undefined>();
            const listener = vscode.tasks.onDidEndTaskProcess(event => exits.set(event.execution, event.exitCode));
            try {
                const task = await vscode.commands.executeCommand<vscode.TaskExecution>(`void.${command}Project`);
                assert(task);
                const label = command === 'check' ? 'consumer Check task completes' :
                    'consumer Package task preserves explicit --allow-native boundary';
                const exit = await until(() => exits.get(task), () => exits.has(task), label);
                assert.equal(exit, command === 'check' ? 0 : 2);
            } finally { listener.dispose(); }
        }
        await fs.writeFile(process.env.VOID_RAYLIB_EDITOR_EVIDENCE!, JSON.stringify({status:'PASS', editor:vscode.version, platform:process.platform, events}, null, 2));
    } finally {
        await fs.writeFile(process.env.VOID_RAYLIB_EDITOR_EVIDENCE! + '.probe.json', JSON.stringify({state:api.status(), language:document.languageId, lastHover, events}, null, 2));
        await api.deactivate();
    }
}
