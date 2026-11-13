import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import * as os from 'node:os';

async function main():Promise<void> {
    const root = await fs.realpath(process.env.VOID_TEST_RAYLIB_PROJECT!);
    const server = await fs.realpath(process.env.VOIDLS_PATH!);
    const editor = await fs.realpath(process.env.VSCODE_EXECUTABLE_PATH!);
    const profile = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID raylib editor '));
    try {
        await fs.mkdir(path.join(root, '.vscode'), {recursive:true});
        await fs.writeFile(path.join(root, '.vscode/settings.json'), JSON.stringify({'void.server.path':server, 'void.project.root':'.', 'void.project.entry':''}));
        await runTests({vscodeExecutablePath:editor, extensionDevelopmentPath:path.resolve(__dirname,'../..'),
            extensionTestsPath:path.join(__dirname,'raylibHost.js'), extensionTestsEnv:{VOID_TEST_ROOT:root},
            launchArgs:[root, '--user-data-dir='+profile, '--extensions-dir='+path.join(profile,'extensions'), '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes']});
    } finally {await fs.rm(profile,{recursive:true,force:true,maxRetries:5,retryDelay:100});}
}
main().catch(error=>{console.error(error);process.exitCode=1;});
