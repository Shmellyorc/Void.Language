import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import {execFile} from 'node:child_process';
import * as path from 'node:path';
import {promisify} from 'node:util';
import {until} from './creationWorkbench';

export async function run(): Promise<void> {
    const folder = vscode.workspace.workspaceFolders![0];
    const root = folder.uri.fsPath;
    const windows = process.platform === 'win32';
    const debuggerType = windows ? 'cppdbg' : 'lldb-dap';
    const normalized = (file: string) => windows ? path.normalize(file).toLowerCase() : file;
    const program = path.join(root, 'build', windows ? 'app.exe' : 'app');
    const source = vscode.Uri.file(path.join(root, 'src', 'app.void'));
    const name = 'VOID Debug project';
    const extension = vscode.extensions.getExtension('void-language.void-language');
    const adapter = vscode.extensions.getExtension(windows ? 'ms-vscode.cpptools' : 'llvm-vs-code-extensions.lldb-dap');
    assert(extension && adapter && vscode.workspace.isTrusted);
    const api = await extension.activate() as {deactivate(): Promise<void>};
    const builds: Array<number | undefined> = [];
    const exits: number[] = [];
    const launches: string[] = [];
    const stops: Array<{session: vscode.DebugSession; threadId: number}> = [];
    let ended = 0;
    const owned: vscode.Breakpoint[] = [];
    const events = [
        vscode.tasks.onDidEndTaskProcess(event => {
            if (event.execution.task.definition.command === 'debugBuild') builds.push(event.exitCode);
        }),
        vscode.debug.onDidTerminateDebugSession(session => { if (session.name === name) ++ended; }),
        vscode.debug.registerDebugAdapterTrackerFactory(debuggerType, {
            createDebugAdapterTracker(session) {
                if (session.name !== name) return;
                return {
                    onWillReceiveMessage(message) {
                        if (message.command === 'launch') launches.push(normalized(message.arguments.program));
                    },
                    onDidSendMessage(message) {
                        if (message.event === 'exited') exits.push(message.body.exitCode);
                        if (message.event === 'stopped') stops.push({session, threadId: message.body.threadId});
                    }
                };
            }
        })
    ];
    const start = () => vscode.debug.startDebugging(folder, name);
    try {
        // Opening the folder with no editor and no previous build is the normal first F5.
        assert(!vscode.window.activeTextEditor);
        await assert.rejects(fs.stat(program), {code: 'ENOENT'});
        assert(await start());
        await until(() => ended, count => count === 1, 'first F5 termination');
        assert.deepEqual(builds, [0]);
        assert.deepEqual(exits, [0]);
        assert.deepEqual(launches, [normalized(program)]);
        const first = await fs.readFile(program);

        // The old executable still exists. On Linux deny publishing its
        // replacement; on Windows force the same task to fail during setup.
        const compiler = vscode.workspace.getConfiguration('void', folder.uri);
        const priorCompiler = compiler.inspect<string>('compiler.path')?.workspaceValue;
        if (windows) await compiler.update('compiler.path', path.join(root, 'missing-voidc.exe'), vscode.ConfigurationTarget.Workspace);
        else await fs.chmod(path.dirname(program), 0o555);
        try {
            assert.equal(await start(), false);
            assert.equal(builds.length, 2);
            assert.notEqual(builds[1], 0);
            assert.deepEqual(launches, [normalized(program)]);
            assert.deepEqual(await fs.readFile(program), first);
        } finally {
            if (windows) await compiler.update('compiler.path', priorCompiler, vscode.ConfigurationTarget.Workspace);
            else await fs.chmod(path.dirname(program), 0o755);
        }

        // Reject the VS Code override before it can run a stale binary.
        const settings = vscode.workspace.getConfiguration('debug', folder.uri);
        await settings.update('onTaskErrors', 'debugAnyway', vscode.ConfigurationTarget.Workspace);
        try {
            assert.equal(await start().then(result => result, () => false), false);
            assert.equal(builds.length, 2);
            assert.deepEqual(launches, [normalized(program)]);
        } finally {
            await settings.update('onTaskErrors', 'abort', vscode.ConfigurationTarget.Workspace);
        }

        await fs.writeFile(source.fsPath,
            'func compute(value: i32) -> i32:\n    var observed = value * 2\n    return observed + 3\nfunc main() -> i32:\n    return compute(7)\n');
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(source));
        owned.push(new vscode.SourceBreakpoint(new vscode.Location(source, new vscode.Position(1, 0))));
        vscode.debug.addBreakpoints(owned);
        assert(await start());
        const frame = async (stop: typeof stops[number]) => {
            const trace = await stop.session.customRequest('stackTrace', {threadId: stop.threadId});
            return trace.stackFrames[0] as {id: number; line: number; source: {path: string}};
        };
        const stop = await until(() => stops[0], Boolean, 'starter source breakpoint');
        assert.equal(normalized((await frame(stop)).source.path), normalized(source.fsPath));
        assert.equal((await frame(stop)).line, 2);
        await stop.session.customRequest('next', {threadId: stop.threadId});
        const step = await until(() => stops[1], Boolean, 'starter source step');
        const stepped = await frame(step);
        assert.equal(stepped.line, 3);
        const {scopes} = await step.session.customRequest('scopes', {frameId: stepped.id});
        const variables = (await Promise.all(scopes.map(async (scope: {variablesReference: number}) =>
            (await step.session.customRequest('variables', {variablesReference: scope.variablesReference})).variables))).flat();
        assert(variables.some(variable => variable.name === 'observed' && variable.value === '14'));
        await step.session.customRequest('continue', {threadId: step.threadId});
        await until(() => ended, count => count === 2, 'edited starter termination');
        assert.equal(exits.length, 2);
        assert.equal(exits[0], 0);
        if (!windows) assert.equal(exits[1], 17);
        // cppdbg does not reliably propagate the Windows target's status.
        await assert.rejects(promisify(execFile)(program, [], {cwd: root}),
            (error: NodeJS.ErrnoException) => Number(error.code) === 17);
        assert.deepEqual(launches, [normalized(program), normalized(program)]);
        assert.equal(builds[2], 0);
        assert.notDeepEqual(await fs.readFile(program), first);
        console.log(`starter_launch_host: VS Code ${vscode.version}, ${debuggerType} ${adapter.packageJSON.version}; builds ${JSON.stringify(builds)}, exits ${JSON.stringify(exits)}, source breakpoint, step and observed=14, standalone exit 17`);
    } finally {
        if (vscode.debug.activeDebugSession) await vscode.debug.stopDebugging();
        vscode.debug.removeBreakpoints(owned);
        for (const event of events) event.dispose();
        await api.deactivate();
    }
}
