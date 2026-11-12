import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';

async function main(): Promise<void> {
    const {VOIDLS_PATH, VSCODE_EXECUTABLE_PATH, VOID_LLDB_DAP, VOID_LLDB_EXTENSIONS} = process.env;
    if (!VOIDLS_PATH || !VSCODE_EXECUTABLE_PATH || !VOID_LLDB_DAP || !VOID_LLDB_EXTENSIONS)
        throw new Error('Set VOIDLS_PATH, VSCODE_EXECUTABLE_PATH, VOID_LLDB_DAP and VOID_LLDB_EXTENSIONS.');
    const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID native debugger '));
    try {
        for (const mode of ['missing', 'installed']) {
            const workspace = path.join(temp, mode, 'workspace');
            const root = path.join(workspace, 'selected project');
            const profile = path.join(temp, mode, 'profile');
            const extensions = mode === 'installed' ? VOID_LLDB_EXTENSIONS : path.join(temp, mode, 'extensions');
            await fs.mkdir(path.join(workspace, '.vscode'), {recursive: true});
            await fs.mkdir(path.join(root, 'src'), {recursive: true});
            await fs.mkdir(path.join(profile, 'User'), {recursive: true});
            await fs.writeFile(path.join(root, 'void.project.toml'),
                "schema=1\n[project]\nid='org.test.debug'\nname='Debug'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.debug.app'\n[sources]\nroots=['src']\n[build.debug]\ndebug_info='full'\n");
            await fs.writeFile(path.join(root, 'src/launcher with spaces.void'),
                'func compute(value: i32) -> i32:\n    var observed = value * 2\n    return observed + 3\nfunc main() -> i32:\n    return compute(7)\n');
            await fs.writeFile(path.join(workspace, '.vscode/settings.json'), JSON.stringify({
                'void.server.path': await fs.realpath(VOIDLS_PATH), 'void.project.root': 'selected project',
                'void.tasks.profile': 'debug', 'lldb-dap.executable-path': await fs.realpath(VOID_LLDB_DAP),
            }));
            await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
                'security.workspace.trust.startupPrompt': 'never', 'workbench.startupEditor': 'none',
                'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false,
                'chat.disableAIFeatures': true, 'files.hotExit': 'off',
            }));
            await runTests({
                vscodeExecutablePath: VSCODE_EXECUTABLE_PATH,
                extensionDevelopmentPath: path.resolve(__dirname, '../..'),
                extensionTestsPath: path.resolve(__dirname, 'debugHost.js'),
                extensionTestsEnv: {VOID_DEBUG_ROOT: root, VOID_DEBUG_NO_ADAPTER: mode === 'missing' ? '1' : '0'},
                launchArgs: [workspace, '--user-data-dir=' + profile, '--extensions-dir=' + extensions,
                    '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes'],
            });
        }
    } finally {
        await fs.rm(temp, {recursive: true, force: true, maxRetries: 5, retryDelay: 100});
    }
}
main().catch(error => {console.error(error); process.exitCode = 1;});
