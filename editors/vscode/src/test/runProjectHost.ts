import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import * as os from 'node:os';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';

async function installVsix(editor: string, profileArgs: string[], vsix: string): Promise<void> {
    const root = path.dirname(editor);
    const relative = 'resources/app/out/cli.js';
    let cli = path.join(root, relative);
    if (process.platform === 'win32' && !await fs.stat(cli).then(stat => stat.isFile(), () => false)) {
        // The Windows archive's official launcher uses a commit-prefixed resource directory.
        const shim = await fs.readFile(path.join(root, 'bin/code.cmd'), 'utf8');
        const layout = shim.match(/%~dp0\.\.\\([0-9a-f]+)\\resources\\app\\out\\cli\.js"/i);
        if (!layout) throw new Error('Cannot locate the VS Code CLI from its Windows launcher.');
        cli = path.join(root, layout[1], relative);
    }
    const {stdout} = await promisify(execFile)(editor, [cli, ...profileArgs, '--install-extension', vsix], {
        env: {...process.env, ELECTRON_RUN_AS_NODE: '1'}, timeout: 60000,
    });
    console.log(stdout.trim());
}

async function main(): Promise<void> {
    if (!process.env.VOIDLS_PATH || !process.env.VSCODE_EXECUTABLE_PATH) throw new Error('Set VOIDLS_PATH and VSCODE_EXECUTABLE_PATH to the matching built server and existing qualified editor.');
    const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID S3 editor 雪 '));
    try {
        const root = path.join(temp,'project');await fs.mkdir(path.join(root,'src'),{recursive:true});await fs.mkdir(path.join(root,'.vscode'));
        const manifest = "schema=1\n[project]\nid='org.test.sources'\nname='Sources'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n[sources]\nroots=['src']\n";
        await fs.writeFile(path.join(root,'void.project.toml'),manifest);
        await fs.writeFile(path.join(root,'src/helper.void'),'public func value() -> i32:\n    return 23\n');
        await fs.writeFile(path.join(root,'.vscode/settings.json'),JSON.stringify({'void.server.path':await fs.realpath(process.env.VOIDLS_PATH),'void.project.root':'.','void.project.entry':'','void.trace.server':'verbose'}));
        const profile=path.join(temp,'profile');await fs.mkdir(path.join(profile,'User'),{recursive:true});
        await fs.writeFile(path.join(profile,'User/settings.json'),JSON.stringify({'security.workspace.trust.startupPrompt':'never','workbench.startupEditor':'none','telemetry.telemetryLevel':'off','update.mode':'none','extensions.autoCheckUpdates':false,'files.enableTrash':false}));
        const editor=await fs.realpath(process.env.VSCODE_EXECUTABLE_PATH);
        const extensions=path.join(temp,'extensions');
        const profileArgs=['--user-data-dir='+profile,'--extensions-dir='+extensions];
        let development=path.resolve(__dirname,'../..');
        if (process.env.VOID_TEST_VSIX) {
            const vsix=await fs.realpath(process.env.VOID_TEST_VSIX);
            await installVsix(editor,profileArgs,vsix);
            // Give the test runner a development extension without shadowing the installed VOID extension.
            development=path.join(temp,'harness');
            await fs.mkdir(development);
            await fs.writeFile(path.join(development,'package.json'),JSON.stringify({
                name:'void-project-host-fixture',publisher:'void-test',version:'0.0.0',engines:{vscode:'^1.141.0'}
            }));
        }
        await runTests({vscodeExecutablePath:editor,extensionDevelopmentPath:development,extensionTestsPath:path.resolve(__dirname,'projectHost.js'),extensionTestsEnv:{VOID_TEST_ROOT:root,VOID_S3_MANIFEST:manifest,VOID_S3_HOST_EVIDENCE:process.env.VOID_S3_HOST_EVIDENCE ?? '',VOID_TEST_PROJECT_TASKS:process.env.VOID_TEST_PROJECT_TASKS ?? '',VOIDLS_PATH:process.env.VOIDLS_PATH,VOID_TEST_EXTENSIONS:process.env.VOID_TEST_VSIX ? extensions : ''},launchArgs:[root,...profileArgs,...(process.env.VOID_TEST_VSIX ? [] : ['--disable-extensions']),'--disable-updates','--disable-gpu','--skip-welcome','--skip-release-notes']});
    } catch(error) {
        if (process.env.VOID_S3_PROFILE_EVIDENCE) await fs.cp(temp,process.env.VOID_S3_PROFILE_EVIDENCE,{recursive:true});
        throw error;
    } finally {await fs.rm(temp,{recursive:true,force:true,maxRetries:5,retryDelay:100});}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
