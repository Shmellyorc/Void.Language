import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {until} from './creationWorkbench';

export async function run(): Promise<void> {
    const root = process.env.VOID_DEBUG_ROOT!;
    const source = vscode.Uri.file(path.join(root, 'src/launcher with spaces.void'));
    const document = await vscode.workspace.openTextDocument(source);
    await vscode.window.showTextDocument(document);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const program = async () => vscode.commands.executeCommand<string>('void.debugProjectProgram');
    const adapter = vscode.extensions.getExtension('llvm-vs-code-extensions.lldb-dap');
    const owned: vscode.Breakpoint[] = [];
    const disposables: vscode.Disposable[] = [];
    try {
        assert(vscode.workspace.isTrusted);
        await api.ready();
        if (process.env.VOID_DEBUG_NO_ADAPTER === '1') {
            assert(!adapter);
            await assert.rejects(program(), /Install the LLVM LLDB DAP extension/);
            console.log('debug_editor_host: missing adapter rejected');
            return;
        }
        assert(adapter);
        await assert.rejects(program(), /Build the selected Debug project/);
        assert.equal(await vscode.commands.executeCommand('void.debugProjectRoot'), root);

        const exits = new Map<vscode.TaskExecution, number | undefined>();
        disposables.push(vscode.tasks.onDidEndTaskProcess(event => exits.set(event.execution, event.exitCode)));
        const build = await vscode.commands.executeCommand<vscode.TaskExecution>('void.buildProject');
        assert(build);
        assert.equal(await until(() => exits.get(build), () => exits.has(build), 'Debug task exit'), 0);
        assert.equal(await program(), path.join(root, 'build/launcher with spaces'));

        const end = document.positionAt(document.getText().length);
        const edit = new vscode.WorkspaceEdit();
        edit.insert(source, end, '# Unsaved change\n');
        assert(await vscode.workspace.applyEdit(edit));
        await assert.rejects(program(), /Save launcher with spaces\.void/);
        const repair = new vscode.WorkspaceEdit();
        repair.delete(source, new vscode.Range(end, document.positionAt(document.getText().length)));
        assert(await vscode.workspace.applyEdit(repair));
        assert(await document.save());

        const stops: Array<{session: vscode.DebugSession; threadId: number}> = [];
        let exit: number | undefined;
        let terminated = false;
        disposables.push(vscode.debug.registerDebugAdapterTrackerFactory('lldb-dap', {
            createDebugAdapterTracker(session) {
                return {onDidSendMessage(message) {
                    if (message.event === 'stopped') stops.push({session, threadId: message.body.threadId});
                    if (message.event === 'exited') exit = message.body.exitCode;
                }};
            }
        }));
        disposables.push(vscode.debug.onDidTerminateDebugSession(() => {terminated = true;}));
        owned.push(new vscode.SourceBreakpoint(new vscode.Location(source, new vscode.Position(1, 0))));
        vscode.debug.addBreakpoints(owned);
        assert(await vscode.debug.startDebugging(vscode.workspace.workspaceFolders![0], {
            type: 'lldb-dap', request: 'launch', name: 'VOID Debug project test',
            program: '${command:void.debugProjectProgram}', cwd: '${command:void.debugProjectRoot}',
            disableASLR: false,
        }));
        const frame = async (stop: typeof stops[number]) => {
            const trace = await stop.session.customRequest('stackTrace', {threadId: stop.threadId});
            return trace.stackFrames[0] as {id: number; line: number; source: {path: string}};
        };
        await until(() => stops.length, count => count >= 1, 'source breakpoint');
        const breakpoint = await frame(stops[0]);
        assert.equal(breakpoint.source.path, source.fsPath);
        assert.equal(breakpoint.line, 2);
        await stops[0].session.customRequest('next', {threadId: stops[0].threadId});
        await until(() => stops.length, count => count >= 2, 'source step');
        const stepped = await frame(stops[1]);
        assert.equal(stepped.line, 3);
        const {scopes} = await stops[1].session.customRequest('scopes', {frameId: stepped.id});
        const variables = await Promise.all(scopes.map(async (scope: {variablesReference: number}) =>
            (await stops[1].session.customRequest('variables', {variablesReference: scope.variablesReference})).variables));
        assert(variables.flat().some(variable => variable.name === 'observed' && variable.value === '14'), JSON.stringify(variables));
        await stops[1].session.customRequest('continue', {threadId: stops[1].threadId});
        await until(() => terminated, Boolean, 'debug session shutdown');
        assert.equal(exit, 17);
        console.log(`debug_editor_host: LLVM LLDB DAP ${adapter.packageJSON.version} hit, stepped, inspected observed = 14 and exited 17`);
    } finally {
        if (vscode.debug.activeDebugSession) await vscode.debug.stopDebugging();
        vscode.debug.removeBreakpoints(owned);
        for (const disposable of disposables) disposable.dispose();
        await api.deactivate();
    }
}
