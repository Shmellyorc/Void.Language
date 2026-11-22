import {runTests} from '@vscode/test-electron';
import {execFileSync} from 'node:child_process';
import {promises as fs} from 'node:fs';
import {join, resolve} from 'node:path';

async function main(): Promise<void> {
    const evidence = resolve(process.env.VOID_F5_EVIDENCE!);
    const compiler = resolve(process.env.VOIDC_PATH!);
    await fs.mkdir(evidence, {recursive: true});
    const root = join(evidence, 'Raylib F5');
    if (!process.env.VOID_F5_EXISTING) {
        execFileSync(compiler, ['new', 'app', root]);
        execFileSync(compiler, ['add', 'raylib', '--project', root]);
        for (const name of ['launch.json', 'tasks.json']) await fs.rm(join(root, '.vscode', name));
        await fs.writeFile(join(root, 'src/app.void'), 'use pkg/raylib as rl\nfunc main() -> i32:\n    var window = rl.init_window(320, 240, "VOID F5 qualification") as rl.window\n    defer rl.close_window(window)\n    rl.set_target_fps(60)\n    var count = 0\n    for count < 2:\n        var dt = rl.get_frame_time()\n        var observed = count + 7\n        rl.begin_drawing()\n        rl.clear_background(rl.skyblue())\n        rl.end_drawing()\n        count += 1\n    return count\n');
    }
    await fs.writeFile(join(root, '.vscode/settings.json'), JSON.stringify({'lldb-dap.executable-path': resolve(process.env.VOID_LLDB_DAP!),
        'void.server.path': resolve(process.env.VOIDLS_PATH!), 'debug.onTaskErrors': 'abort'}));
    const profile = join(evidence, 'editor-profile');
    await fs.mkdir(join(profile, 'User'), {recursive: true});
    await fs.writeFile(join(profile, 'User/settings.json'), JSON.stringify({'workbench.startupEditor': 'none',
        'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false, 'extensions.autoUpdate': false, 'chat.disableAIFeatures': true}));
    const harness = join(evidence, 'harness');await fs.mkdir(harness, {recursive: true});
    await fs.writeFile(join(harness, 'package.json'), JSON.stringify({name:'f5-qualification-harness',publisher:'void-language',version:'0.0.1',engines:{vscode:'^1.141.0'},main:'index.js'}));
    await fs.writeFile(join(harness, 'index.js'), 'exports.activate=()=>{};');
    await runTests({vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH!,
        extensionDevelopmentPath: process.env.VOID_F5_INSTALLED === '1' ? harness : resolve(__dirname, '../..'),
        extensionTestsPath: resolve(__dirname, 'f5ExperienceHost.js'),
        extensionTestsEnv: {VOID_F5_EVIDENCE: evidence, VOID_TEST_CDP_PORT: '9557',
            XDG_DATA_HOME: join(evidence, 'native-user-data')},
        launchArgs: [root, '--user-data-dir='+profile, '--extensions-dir='+process.env.VOID_LLDB_EXTENSIONS!,
            '--remote-debugging-port=9557', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes']});
}
main().catch(error => {console.error(error); process.exitCode=1;});
