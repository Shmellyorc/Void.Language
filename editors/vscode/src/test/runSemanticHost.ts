import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';

async function main(): Promise<void> {
    const {VOIDLS_PATH, VSCODE_EXECUTABLE_PATH} = process.env;
    if (!VOIDLS_PATH || !VSCODE_EXECUTABLE_PATH) throw new Error('Set VOIDLS_PATH and VSCODE_EXECUTABLE_PATH.');
    const base = process.platform === 'darwin' ? await fs.realpath('/tmp') : os.tmpdir();
    const temporary = await fs.mkdtemp(path.join(base, 'VOID semantic '));
    try {
        const root = path.join(temporary, 'project');
        const settings = path.join(root, '.vscode');
        const profile = path.join(temporary, 'profile');
        await fs.mkdir(settings, {recursive: true});
        await fs.mkdir(path.join(profile, 'User'), {recursive: true});
        await fs.writeFile(path.join(settings, 'settings.json'), JSON.stringify({
            'void.server.path': await fs.realpath(VOIDLS_PATH), 'void.project.root': '.', 'void.trace.server': 'off',
            'editor.semanticHighlighting.enabled': true, 'files.eol': '\n',
        }));
        await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
            'security.workspace.trust.startupPrompt': 'never', 'workbench.startupEditor': 'none',
            'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false,
            'chat.disableAIFeatures': true, 'files.hotExit': 'off',
        }));
        await runTests({vscodeExecutablePath: VSCODE_EXECUTABLE_PATH,
            extensionDevelopmentPath: path.resolve(__dirname, '../..'), extensionTestsPath: path.resolve(__dirname, 'semanticHost.js'),
            extensionTestsEnv: {VOID_SEMANTIC_ROOT: root},
            launchArgs: [root, '--user-data-dir=' + profile, '--extensions-dir=' + path.join(temporary, 'extensions'),
                '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes']});
    } finally {
        await fs.rm(temporary, {recursive: true, force: true, maxRetries: 5, retryDelay: 100});
    }
}
main().catch(error => {console.error(error); process.exitCode = 1;});
