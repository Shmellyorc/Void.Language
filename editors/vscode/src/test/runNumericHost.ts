import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import * as os from 'node:os';

async function main() {
    const {VOIDLS_PATH, VSCODE_EXECUTABLE_PATH, VOID_NUMERIC_HOST_EVIDENCE} = process.env;
    if (!VOIDLS_PATH || !VSCODE_EXECUTABLE_PATH || !VOID_NUMERIC_HOST_EVIDENCE)
        throw new Error('Set VOIDLS_PATH, VSCODE_EXECUTABLE_PATH and VOID_NUMERIC_HOST_EVIDENCE.');
    const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID S6 editor '));
    try {
        const root = path.join(temp, 'project');
        const profile = path.join(temp, 'profile');
        await fs.mkdir(root);
        await fs.mkdir(path.join(profile, 'User'), {recursive: true});
        await fs.writeFile(path.join(root, 'void.project.toml'), `schema=1
[project]
id='org.test.numeric'
name='Numeric'
version='1.0.0'
kind='application'
[application]
id='org.test.numeric-app'
`);
        await fs.writeFile(path.join(root, 'modes.void'), 'public enum mode(i8):\n    ready = 7\n    gone = -128\n');
        await fs.writeFile(path.join(root, 'entry.void'), `use "./modes.void" as m
func main() -> i32:
    var raw: i32 = 7
    if raw as? m.mode is Some(var picked):
        if picked is m.mode.ready:
            return 7
    return 1
`);
        await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
            'void.server.path': await fs.realpath(VOIDLS_PATH),
            'security.workspace.trust.startupPrompt': 'never', 'workbench.startupEditor': 'none',
            'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'chat.disableAIFeatures': true
        }));
        await runTests({
            vscodeExecutablePath: VSCODE_EXECUTABLE_PATH,
            extensionDevelopmentPath: path.resolve(__dirname, '../..'),
            extensionTestsPath: path.resolve(__dirname, 'numericHost.js'),
            extensionTestsEnv: {VOID_NUMERIC_HOST_ROOT: root, VOID_NUMERIC_HOST_EVIDENCE},
            launchArgs: [root, '--user-data-dir='+profile, '--extensions-dir='+path.join(temp,'extensions'),
                '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes'],
        });
    } finally { await fs.rm(temp, {recursive: true, force: true}); }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
