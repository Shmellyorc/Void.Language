import {runTests} from '@vscode/test-electron';
import {spawn} from 'node:child_process';
import {promises as fs} from 'node:fs';
import {createServer} from 'node:net';
import * as path from 'node:path';
import * as os from 'node:os';
import {offer, pause, until, workbench} from './creationWorkbench';

async function freePort(): Promise<number> {
    return new Promise((resolve, reject) => {
        const server = createServer().listen(0, '127.0.0.1');
        server.once('error', reject);
        server.once('listening', () => {
            const address = server.address();
            if (!address || typeof address === 'string') {server.close(); reject(new Error('No CDP port')); return;}
            server.close(error => error ? reject(error) : resolve(address.port));
        });
    });
}
async function verifyPersistence(executable: string, server: string, root: string, evidence: string): Promise<void> {
    const workspace = path.join(root, 'persistence');
    const profile = path.join(root, 'persistent-profile');
    await fs.mkdir(workspace);
    await fs.mkdir(path.join(profile, 'User'), {recursive: true});
    await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
        'void.server.path': server, 'security.workspace.trust.enabled': false, 'workbench.startupEditor': 'none',
        'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'chat.disableAIFeatures': true,
        'window.confirmBeforeClose': 'never',
    }));
    const events: string[] = [];
    let failure: string | undefined;
    try {
        for (const phase of ['offer', 'reopen']) {
            const port = await freePort();
            // The VS Code extension test launch uses ephemeral storage. The two
            // windows below use one real user-data directory and graceful exit.
            const child = spawn(executable, [workspace, '--user-data-dir=' + profile, '--shared-data-dir=' + path.join(root, 'shared-data'), '--disable-extensions',
                '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes',
                '--extensionDevelopmentPath=' + path.resolve(__dirname, '../..'), '--remote-debugging-port=' + port],
                {stdio: 'inherit'});
            const stopped = new Promise<void>((resolve, reject) => {
                child.once('error', reject);
                child.once('exit', status => status === 0 ? resolve() : reject(new Error('Editor exited ' + status)));
            });
            void stopped.catch(() => {});
            let ui: Awaited<ReturnType<typeof workbench>> | undefined;
            try {
                ui = await workbench(String(port));
                await until(() => ui!.visible('VOID: stopped', '.statusbar'), Boolean, 'startup of normal VS Code');
                if (phase === 'reopen') await fs.unlink(path.join(workspace, 'offer.void'));
                await fs.writeFile(path.join(workspace, phase + '.void'), 'func main():\n    return\n');
                if (phase === 'offer') {
                    await until(() => ui!.visible(offer, '.notification-toast'), Boolean, 'offer in a normal editor window');
                    events.push('normal editor closes while the prompt remains offered');
                } else {
                    await pause(1500);
                    if (await ui.visible(offer, '.notification-toast')) throw new Error('Offer repeated after normal editor restart');
                    events.push('same physical root and persisted workspace storage suppress the next qualifying event');
                }
                ui.key('q', 'KeyQ', 81, 2);
                let timer: NodeJS.Timeout | undefined;
                try {
                    await Promise.race([stopped, new Promise<never>((_, reject) => {
                        timer = setTimeout(() => reject(new Error('Editor quit timed out')), 12000);
                    })]);
                } finally { clearTimeout(timer); }
            } finally {
                ui?.close();
                if (child.exitCode === null && child.signalCode === null) {
                    child.kill();
                    await stopped.catch(() => {});
                }
            }
        }
    } catch (error) {
        failure = String(error);
        throw error;
    } finally {
        await fs.writeFile(evidence, JSON.stringify({events, failure, status: failure ? 'FAIL' : 'PASS'}, null, 2) + '\n');
    }
}

async function main(): Promise<void> {
    const {VOIDLS_PATH, VSCODE_EXECUTABLE_PATH, VOID_CREATION_EVIDENCE_DIR} = process.env;
    if (!VOIDLS_PATH || !VSCODE_EXECUTABLE_PATH || !VOID_CREATION_EVIDENCE_DIR)
        throw new Error('Set VOIDLS_PATH, VSCODE_EXECUTABLE_PATH, and VOID_CREATION_EVIDENCE_DIR.');
    const older = process.env.VOID_OLDER_VOIDLS_PATH;
    if (!older) throw new Error('Set VOID_OLDER_VOIDLS_PATH to the retained legacy server.');
    const temporary = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID S5 editor '));
    try {
        for (const mode of ['trusted', 'old-tree', 'offered-close', 'manual', 'collision', 'outside', 'old-server', 'multi-root', 'untrusted']) {
            const group = mode.startsWith('offered-') ? 'offered' : mode;
            const root = path.join(temporary, group);
            const profile = path.join(temporary, group + '-profile');
            await fs.mkdir(root, {recursive: true});
            await fs.mkdir(path.join(profile, 'User'), {recursive: true});
            const marker = path.join(temporary, 'native-launch');
            const wrapper = path.join(temporary, 'voidls-launch-guard');
            const restricted = mode === 'untrusted' || mode === 'multi-root';
            if (restricted) {
                await fs.writeFile(wrapper, `#!/bin/sh\ntouch "${marker}"\n`, {mode: 0o700});
            } else if (mode === 'old-tree' || mode === 'manual') {
                await fs.writeFile(path.join(root, 'existing.void'), 'func main() -> i32:\n    return 7\n');
                if (mode === 'manual') await fs.writeFile(path.join(root, 'helper.void'), '');
            }
            await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
                'void.server.path': restricted ? wrapper : await fs.realpath(mode === 'old-server' ? older : VOIDLS_PATH),
                'void.trace.server': 'verbose',
                'security.workspace.trust.startupPrompt': 'never', 'workbench.startupEditor': 'none',
                'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false,
                'files.hotExit': 'off', 'files.enableTrash': false, 'chat.disableAIFeatures': true
            }));
            const port = await freePort();
            let workspace = root;
            if (mode === 'multi-root') {
                const second = path.join(temporary, 'second-root');
                await fs.mkdir(second);
                workspace = path.join(temporary, 'multi.code-workspace');
                await fs.writeFile(workspace, JSON.stringify({folders: [{path: root}, {path: second}]}));
            }
            const args = [workspace, '--user-data-dir=' + profile, '--extensions-dir=' + path.join(temporary, 'extensions'),
                '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes',
                '--remote-debugging-port=' + port];
            const env = {VOID_CREATION_ROOT: root, VOID_CREATION_MODE: mode, VOID_CREATION_CDP: String(port),
                VOID_CREATION_SPAWN: marker, VOID_CREATION_EVIDENCE: path.join(VOID_CREATION_EVIDENCE_DIR, mode + '-host.json')};
            const extension = path.resolve(__dirname, '../..');
            const test = path.resolve(__dirname, 'creationHost.js');
            if (mode !== 'untrusted') {
                try { await runTests({vscodeExecutablePath: VSCODE_EXECUTABLE_PATH, extensionDevelopmentPath: extension,
                    extensionTestsPath: test, extensionTestsEnv: env, launchArgs: args}); }
                catch (error) {
                    await fs.cp(path.join(profile, 'logs'), path.join(VOID_CREATION_EVIDENCE_DIR, 'failed-' + mode + '-logs'), {recursive: true});
                    throw error;
                }
            } else {
                await new Promise<void>((resolve, reject) => {
                    const child = spawn(VSCODE_EXECUTABLE_PATH, [...args, '--no-sandbox', '--disable-gpu-sandbox',
                        '--extensionDevelopmentPath=' + extension, '--extensionTestsPath=' + test], {
                        env: {...process.env, ...env}, stdio: 'inherit'});
                    const timeout = setTimeout(() => {child.kill(); reject(new Error('Restricted workspace test timed out'));}, 60000);
                    child.once('error', error => {clearTimeout(timeout); reject(error);});
                    child.once('exit', code => {clearTimeout(timeout); code === 0 ? resolve() : reject(new Error('Restricted workspace test exited ' + code));});
                });
            }
        }
        await verifyPersistence(VSCODE_EXECUTABLE_PATH, await fs.realpath(VOIDLS_PATH), temporary,
            path.join(VOID_CREATION_EVIDENCE_DIR, 'persistence-host.json'));
    } finally {
        await fs.rm(temporary, {recursive: true, force: true});
    }
}
main().catch(error => {console.error(error); process.exitCode = 1;});
