import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {execFile} from 'node:child_process';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {promisify} from 'node:util';
import {until} from './creationWorkbench';

export async function run(): Promise<void> {
    const folder = vscode.workspace.workspaceFolders![0];
    const root = folder.uri.fsPath;
    const windows = process.platform === 'win32';
    const adapterType = windows ? 'cppdbg' : 'lldb-dap';
    const adapter = vscode.extensions.getExtension(windows ? 'ms-vscode.cpptools' : 'llvm-vs-code-extensions.lldb-dap');
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(adapter && extension && vscode.workspace.isTrusted);
    const api = await extension.activate() as {deactivate(): Promise<void>};
    const cli = process.env.VOID_G65_COMPILER!;
    const python = process.env.VOID_G65_TEST_PYTHON!;
    const exec = promisify(execFile);
    const invoke = (args: string[]) => exec(cli, args, {cwd: root});
    const receipt = process.env.VOID_G65_EDITOR_EVIDENCE!;
    const program = path.join(root, 'build', windows ? 'app.exe' : 'app');
    const nativeResult = path.join(root, '..', 'g65-pixel-result.txt');
    const catalog = path.join(root, 'libs', 'raylib', '.void', 'native', 'void.native.toml');
    const name = 'VOID Debug project';
    const statuses: Record<string, Array<number | undefined>> = {run: [], build: [], debugBuild: []};
    const launchPrograms: string[] = [];
    const adapterExits: number[] = [];
    let ended = 0;
    const events = [
        vscode.tasks.onDidEndTaskProcess(event => {
            const command = event.execution.task.definition.command as string;
            statuses[command]?.push(event.exitCode);
        }),
        vscode.debug.onDidTerminateDebugSession(session => { if (session.name === name) ++ended; }),
        vscode.debug.registerDebugAdapterTrackerFactory(adapterType, {
            createDebugAdapterTracker(session) {
                if (session.name !== name) return;
                return {
                    onWillReceiveMessage(message) {
                        if (message.command === 'launch') launchPrograms.push(message.arguments.program);
                    },
                    onDidSendMessage(message) { if (message.event === 'exited') adapterExits.push(message.body.exitCode); }
                };
            }
        })
    ];
    const start = () => vscode.debug.startDebugging(folder, name);
    const task = async (command: 'run' | 'build', code: number) => {
        const prior = statuses[command].length;
        const execution = await vscode.commands.executeCommand<vscode.TaskExecution>(
            command === 'run' ? 'void.runProject' : 'void.buildProject');
        assert(execution);
        await until(() => statuses[command].length, count => count > prior, command + ' task exit');
        assert.equal(statuses[command][prior], code, command);
    };
    const rejectAll = async () => {
        const prior = statuses.debugBuild.length;
        const previousLaunches = launchPrograms.length;
        await task('run', 2);
        await task('build', 2);
        assert.equal(await start(), false);
        assert.equal(statuses.debugBuild[prior], 2);
        assert.equal(launchPrograms.length, previousLaunches);
        await assert.rejects(fs.stat(nativeResult), {code: 'ENOENT'});
    };
    try {
        assert.match((await invoke(['native', 'approvals'])).stdout, /No native approvals/);
        await rejectAll();
        await assert.rejects(fs.stat(program), {code: 'ENOENT'});
        assert.match((await invoke(['native', 'approvals'])).stdout, /No native approvals/);

        await exec(python, [process.env.VOID_G65_CONSENT_DRIVER!, cli, root],
            {cwd: root, timeout: 120000});
        const approved = (await invoke(['native', 'approvals'])).stdout;
        const id = approved.match(/\b[a-f0-9]{64}\b/)?.[0];
        assert(id);
        assert.equal(await fs.readFile(nativeResult, 'utf8'), '241');
        await fs.unlink(nativeResult);
        assert(await start(), JSON.stringify(statuses));
        await until(() => ended, count => count === 1, 'approved F5 termination');
        assert.deepEqual(statuses.debugBuild, [2, 0]);
        assert.equal(launchPrograms.length, 1);
        assert.equal(adapterExits.length, 1);
        if (!windows) assert.equal(adapterExits[0], 17);
        // The fixture writes the pixel receipt only after native execution.
        // This also verifies the debuggee on adapters that misreport its exit.
        assert.equal(await fs.readFile(nativeResult, 'utf8'), '241');
        await fs.unlink(nativeResult);
        await exec(python, ['-c', [
            'import subprocess, sys',
            'app = subprocess.run([sys.argv[1]], capture_output=True, timeout=30)',
            'assert app.returncode == 17 and b"G65 native fixture: 241" in app.stdout, app',
        ].join('\n'), await fs.realpath(program)], {cwd: root});
        assert.equal(await fs.readFile(nativeResult, 'utf8'), '241');
        await fs.unlink(nativeResult);
        await task('run', 17);
        await task('build', 0);
        assert.equal(await fs.readFile(nativeResult, 'utf8'), '241');
        await fs.unlink(nativeResult);
        assert.equal((await invoke(['native', 'approvals'])).stdout, approved);
        const executable = await fs.readFile(program);

        const previous = await fs.readFile(catalog);
        try {
            // A valid descriptor with changed bytes reaches exact approval matching.
            await fs.appendFile(catalog, '\n# Qualification identity change\n');
            await rejectAll();
            assert.deepEqual(await fs.readFile(program), executable);
        } finally {
            await fs.writeFile(catalog, previous);
        }

        await invoke(['native', 'revoke', id]);
        assert.match((await invoke(['native', 'approvals'])).stdout, /No native approvals/);
        await rejectAll();
        assert.deepEqual(await fs.readFile(program), executable);
        assert.match((await invoke(['native', 'approvals'])).stdout, /No native approvals/);
        const summary = {vscode: vscode.version, adapter: adapter.id, adapterVersion: adapter.packageJSON.version,
            statuses, launchPrograms, ended, adapterExits, independentExit: 17, raylibPixel: 241,
            debuggeeWrotePixelResult: true,
            sequence: ['missing', 'approved', 'catalog changed', 'revoked']};
        await fs.writeFile(receipt, JSON.stringify(summary, null, 2) + '\n');
        console.log('native_approval_host:', JSON.stringify(summary));
    } finally {
        if (vscode.debug.activeDebugSession) await vscode.debug.stopDebugging();
        for (const event of events) event.dispose();
        await api.deactivate();
    }
}
