import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import {dirname, join} from 'node:path';
import {createHash} from 'node:crypto';
import {until} from './creationWorkbench';

// Runs against the actual installed extension in the normal Code user profile.
export async function run(): Promise<void> {
    const root = vscode.workspace.workspaceFolders![0].uri.fsPath;
    assert.equal(root.split('/').at(-1), 'default-profile-live');
    const extension = vscode.extensions.getExtension('void-language.void-language')!;
    assert(extension.extensionPath.includes('/.vscode/extensions/void-language.void-language-0.1.2'));
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const source = vscode.Uri.file(join(root, 'src/app.void'));
    await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(source));
    const stops: Array<{session: vscode.DebugSession; threadId: number}> = [];
    const tracker = vscode.debug.registerDebugAdapterTrackerFactory('lldb-dap', {createDebugAdapterTracker(session) {
        return {onDidSendMessage(message) {if (message.event === 'stopped') stops.push({session, threadId: message.body.threadId});}};
    }});
    const bp = new vscode.SourceBreakpoint(new vscode.Location(source, new vscode.Position(4, 0)));
    vscode.debug.addBreakpoints([bp]);
    try {
        await api.ready();
        assert(await vscode.debug.startDebugging(vscode.workspace.workspaceFolders![0], 'VOID Debug project'));
        const stop = await until(() => stops[0], Boolean, 'normal-profile source breakpoint');
        const {stackFrames} = await stop.session.customRequest('stackTrace', {threadId: stop.threadId});
        const frame = stackFrames[0];assert.equal(frame.line, 5);assert(frame.column <= 1);
        const scopes = await stop.session.customRequest('scopes', {frameId: frame.id});
        const locals = scopes.scopes.find((s: {name: string}) => s.name === 'Locals');
        const variables = await stop.session.customRequest('variables', {variablesReference: locals.variablesReference});
        assert(variables.variables.some((v: {name: string; value: string}) => v.name === 'dt' && v.value === '0.25'), JSON.stringify(variables));
        const hover = await stop.session.customRequest('evaluate', {frameId: frame.id, expression: 'dt', context: 'hover'});
        assert.equal(hover.result, '0.25');
        const hash = createHash('sha256').update(await fs.readFile(join(extension.extensionPath, 'dist/extension.js'))).digest('hex');
        await fs.writeFile(join(dirname(root), 'default-profile-editor.json'), JSON.stringify({status:'PASS',vscode:vscode.version,
            extension:extension.extensionPath,sha256:hash,frame,variables,hover,
            serverSetting:vscode.workspace.getConfiguration('void').get('server.path'),
            compilerSetting:vscode.workspace.getConfiguration('void').get('compiler.path')},null,2));
        console.log('PASS normal user profile: installed extension, F5 build, nested scalar local, hover evaluation and line breakpoint');
    } finally {
        if (vscode.debug.activeDebugSession) await vscode.debug.stopDebugging();
        vscode.debug.removeBreakpoints([bp]);tracker.dispose();await api.deactivate();
    }
}
