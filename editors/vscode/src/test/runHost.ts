import {runTests, downloadAndUnzipVSCode} from '@vscode/test-electron';
import {spawn} from 'node:child_process';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import * as os from 'node:os';

async function main(): Promise<void> {
    if (!process.env.VOIDLS_PATH) throw new Error('Set VOIDLS_PATH to the built real voidls executable.');
    const server = await fs.realpath(process.env.VOIDLS_PATH);
    const executable = process.env.VSCODE_EXECUTABLE_PATH ?? await downloadAndUnzipVSCode('1.141.0');
    const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID editor é😀 '));
    try {
        const project = path.join(temp, 'project space # %'); await fs.mkdir(project);
        const root = await fs.realpath(project);
        const installed = process.env.VOID_TEST_VSIX;
        let development = path.resolve(__dirname, '../../..');
        if (installed) {
            await new Promise<void>((resolve, reject) => {
                const child = spawn(process.env.VSCODE_CLI_PATH ?? 'code', ['--install-extension', path.resolve(installed), '--force', '--user-data-dir=' + path.join(temp, 'install'), '--extensions-dir=' + path.join(temp, 'extensions')], {stdio: 'inherit'});
                child.once('error', reject); child.once('exit', code => code === 0 ? resolve() : reject(new Error('VSIX installation failed: ' + code)));
            });
            development = path.join(temp, 'harness'); await fs.mkdir(development);
            await fs.writeFile(path.join(development, 'package.json'), JSON.stringify({name:'void-acceptance-harness',publisher:'void-language',version:'0.0.1',engines:{vscode:'^1.141.0'},main:'./index.js'}));
            await fs.writeFile(path.join(development, 'index.js'), 'exports.activate = () => {};');
        }
        const main = '# UTF-8 é😀\nuse "/worker space.void" as worker\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x)\n';
        const worker = 'func calculate(x: i32) -> i32:\n    return x + 42\n';
        await fs.writeFile(path.join(root, 'main.void'), main);
        await fs.writeFile(path.join(root, 'worker space.void'), worker);
        await fs.writeFile(path.join(root, 'second.void'), 'func entry(x: i32) -> i32:\n    return x\n');
        await fs.mkdir(path.join(root, '.vscode'));
        await fs.writeFile(path.join(root, '.vscode/settings.json'), JSON.stringify({
            'void.server.path': installed ? '' : server, 'void.project.entry': 'main.void', 'void.trace.server': 'verbose'}));
        const other = path.join(temp, 'unrelated'); await fs.mkdir(other);
        const multi = path.join(temp, 'multiple.code-workspace');
        await fs.writeFile(multi, JSON.stringify({folders:[{path:root},{path:other}], settings:{'void.server.path':server, 'void.project.entry':path.join(root,'main.void')}}));
        for (const mode of ['trusted', 'untrusted', 'multi-root']) {
            const profile = path.join(temp, mode); await fs.mkdir(path.join(profile, 'User'), {recursive:true});
            await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
                'security.workspace.trust.startupPrompt': 'never', 'workbench.startupEditor': 'none',
                'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false}));
            const env = {VOID_TEST_PROFILE: profile, VOID_TEST_ROOT: root, VOID_TEST_MODE: mode, VOIDLS_PATH: server};
            const launchArgs = [mode === 'multi-root' ? multi : root, '--user-data-dir=' + profile, '--extensions-dir=' + path.join(temp, 'extensions'),
                '--skip-welcome', '--skip-release-notes', ...(installed ? [] : ['--disable-extensions']), '--disable-updates', '--disable-gpu'];
            try {
            if (mode === 'untrusted') {
                // test-electron.runTests unconditionally disables Workspace Trust.
                // Use VS Code's documented test CLI against the same official editor.
                await new Promise<void>((resolve, reject) => {
                    const child = spawn(executable, [...launchArgs, '--no-sandbox', '--disable-gpu-sandbox', '--no-cached-data',
                        '--extensionDevelopmentPath=' + development,
                        '--extensionTestsPath=' + path.resolve(__dirname, 'host.js')], {env: {...process.env, ...env}, stdio:'inherit'});
                    const timeout = setTimeout(() => { child.kill(); reject(new Error('Restricted editor test timed out')); }, 90000);
                    child.once('error', error => { clearTimeout(timeout); reject(error); });
                    child.once('exit', code => { clearTimeout(timeout); code === 0 ? resolve() : reject(new Error('Restricted editor exited ' + code)); });
                });
            } else await runTests({version: '1.141.0',
                vscodeExecutablePath: executable,
                extensionDevelopmentPath: development,
                extensionTestsPath: path.resolve(__dirname, 'host.js'),
                extensionTestsEnv: env, launchArgs
            }); } catch(error) {
                const evidence = path.resolve(__dirname, '../../../../build/phase11-verification/editor-profile-' + mode);
                await fs.mkdir(path.dirname(evidence), {recursive:true});
                await fs.cp(profile, evidence, {recursive:true}); throw error;
            }
        }
    } finally { await fs.rm(temp, {recursive:true, force:true}); }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
