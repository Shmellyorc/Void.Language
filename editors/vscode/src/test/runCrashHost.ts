import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
async function main(): Promise<void> {
    const server = process.env.VOIDLS_PATH, executable = process.env.VSCODE_EXECUTABLE_PATH, evidence = process.env.VOID_G90_EDITOR_EVIDENCE;
    if (!server || !executable || !evidence) throw new Error('Set VOIDLS_PATH, VSCODE_EXECUTABLE_PATH and VOID_G90_EDITOR_EVIDENCE');
    const root = path.join(evidence, 'project'), profile = path.join(evidence, 'profile');
    await fs.mkdir(path.join(root, '.vscode'), {recursive: true}); await fs.mkdir(path.join(profile, 'User'), {recursive: true});
    await fs.writeFile(path.join(root, 'main.void'), 'func main():\n    print("hello")\n');
    await fs.writeFile(path.join(root, '.vscode/settings.json'), JSON.stringify({'void.server.path': server, 'void.project.entry': 'main.void', 'void.trace.server': 'off'}));
    await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
        'security.workspace.trust.startupPrompt': 'never', 'workbench.startupEditor': 'none', 'telemetry.telemetryLevel': 'off',
        'update.mode': 'none', 'extensions.autoCheckUpdates': false, 'chat.disableAIFeatures': true, 'files.hotExit': 'off'}));
    await runTests({vscodeExecutablePath: executable, extensionDevelopmentPath: path.resolve(__dirname, '../..'),
        extensionTestsPath: path.resolve(__dirname, 'crashHost.js'),
        extensionTestsEnv: {VOID_G90_ROOT: root, VOID_G90_PROFILE: profile, VOIDLS_PATH: server},
        launchArgs: [root, '--user-data-dir=' + profile, '--extensions-dir=' + path.join(evidence, 'extensions'),
            '--logsPath=' + path.join(evidence, 'logs'), '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes']});
}
main().catch(error => {console.error(error); process.exitCode = 1;});
