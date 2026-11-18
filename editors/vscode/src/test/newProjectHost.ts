import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {execFile} from 'node:child_process';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {promisify} from 'node:util';
import {until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root = vscode.workspace.workspaceFolders![0].uri.fsPath;
    const compiler = process.env.VOID_G66_COMPILER!;
    const fixture = process.env.VOID_G66_FIXTURE!;
    const receipt = process.env.VOID_G66_RECEIPT!;
    const restricted = process.env.VOID_G66_RESTRICTED === '1';
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {deactivate(): Promise<void>};
    const ui = await workbench(process.env.VOID_G66_PORT!);
    const dialog = '.quick-input-widget';
    const toast = '.notification-toast';
    const cli = (cwd: string, ...args: string[]) => promisify(execFile)(compiler, args, {cwd, timeout: 60000});
    const begin = () => vscode.commands.executeCommand('void.newProject');
    const choose = (label: string) => ui.click(label, dialog);
    const waitFor = (label: string, region = dialog) => until(() => ui.visible(label, region), Boolean, label);
    const dismiss = () => vscode.commands.executeCommand('notifications.clearAll');
    const events: string[] = [];
    const press = (key: 'Enter' | 'Escape') => ui.key(key, key, key === 'Enter' ? 13 : 27);
    const enterName = async (name: string) => {
        await waitFor('VOID project name');
        await ui.type(name);
        press('Enter');
    };
    const chooseFolder = async (folder: string, inPlace: boolean) => {
        const title = inPlace ? 'Select an empty folder for the VOID project' : 'Select the parent folder for the VOID project';
        await waitFor(title);
        if (folder !== root) {
            ui.key('a', 'KeyA', 65, process.platform === 'darwin' ? 4 : 2);
            await ui.type(folder + path.sep);
            await waitFor('1 Results');
        }
        await choose('Select Folder');
    };
    const manifest = (name: string) => fs.readFile(path.join(root, name, 'void.project.toml'), 'utf8');
    const create = async (template: string, name: string, result: string,
                          folder = root, mode: 'named' | 'inPlace' | 'custom' = 'named', keepNotice = false) => {
        const active = begin();
        await waitFor('Choose a VOID project template');
        await choose(template);
        await enterName(name);
        if (mode !== 'custom') await choose(mode === 'named' ? 'Create a named project folder' : 'Use an existing empty folder');
        await chooseFolder(folder, mode === 'inPlace');
        if (result === 'Created VOID project') {
            await waitFor(`${mode === 'inPlace' ? path.basename(folder) : name}.`, toast);
            await waitFor('Open Project', toast);
        } else await waitFor(result, toast);
        await active;
        if (!keepNotice) await dismiss();
    };
    try {
        assert.equal(vscode.workspace.isTrusted, !restricted);
        if (restricted) {
            const before = await fs.readdir(root);
            const invocation = begin();
            await waitFor('Trust this workspace before creating a VOID project', toast);
            assert.equal(await ui.visible('Choose a VOID project template', dialog), 0);
            await dismiss();
            await invocation;
            assert.deepEqual(await fs.readdir(root), before);
            const marker = process.env.VOID_G66_COMPILER_PROBE!;
            await assert.rejects(fs.stat(marker), {code: 'ENOENT'});
            events.push('Restricted Mode blocked the command before compiler launch or any workspace writes');
        } else {
            assert.match((await cli(root, 'native', 'approvals')).stdout, /No native approvals/);
            await cli(root, 'new', 'install', '--dir', fixture);
            const before = await fs.readdir(root);
            const cancelled = begin();
            await waitFor('Console Application');
            for (const label of ['app', 'Common/Console', 'VOID Library', 'classlib, lib', 'VOID 2D Game', 'voidgame, game-starter', 'Game, 2D'])
                await waitFor(label);
            await choose('Console Application');
            await enterName('CancelledGame');
            await choose('Create a named project folder');
            await waitFor('Select the parent folder for the VOID project');
            press('Escape');
            await cancelled;
            assert.deepEqual(await fs.readdir(root), before);
            events.push('built-ins and installed metadata visible in the real picker; destination cancellation wrote nothing');

            await create('Console Application', 'MyGame', 'Created VOID project', root, 'named', true);
            const application = await manifest('MyGame');
            assert.match(application, /name = "MyGame"[\s\S]*kind = "application"/);
            assert.match(application, /\[application\]/);
            assert.match(await fs.readFile(path.join(root,'MyGame/src/app.void'),'utf8'), /func main\(\)/);
            const launch = JSON.parse(await fs.readFile(path.join(root,'MyGame/.vscode/launch.json'),'utf8'));
            assert.equal(launch.configurations[0].preLaunchTask, 'VOID: Build Debug Project');
            assert.equal(launch.configurations[0].type, process.platform === 'win32' ? 'cppdbg' : 'lldb-dap');
            for (const command of ['run', 'build']) await cli(path.join(root, 'MyGame'), command, '.');
            const exe = path.join(root, 'MyGame/build', process.platform === 'win32' ? 'app.exe' : 'app');
            await promisify(execFile)(await fs.realpath(exe), []);
            events.push('built-in app manifest, source, launch configuration, JIT and independent executable');

            const existing = path.join(root, 'existing');
            await fs.mkdir(existing);
            // The previous Open Project action is still available while another creation starts.
            await waitFor('MyGame.', toast);
            await create('VOID Library', 'MyMath', 'Created VOID project', existing, 'inPlace');
            const library = await manifest('existing');
            assert.match(library, /name = "MyMath"[\s\S]*kind = "library"/);
            assert.match(library, /\[package\]/);
            assert.match(await fs.readFile(path.join(existing,'src/existing.void'),'utf8'), /public func answer/);
            await assert.rejects(fs.stat(path.join(existing, '.vscode')), {code: 'ENOENT'});
            await cli(existing, 'check', '.');
            events.push('built-in classlib created without debug files in an empty folder while the earlier Open Project offer was pending');

            await create('VOID 2D Game', 'CustomGame', 'Created VOID project', root, 'custom');
            const custom = await manifest('CustomGame');
            assert.match(custom, /name = "CustomGame"[\s\S]*entry = "src\/CustomGame.void"/);
            assert.deepEqual(await fs.readFile(path.join(root, 'CustomGame/assets/CustomGame.bin')),
                await fs.readFile(path.join(fixture,'assets/TemplateGame.bin')));
            assert.equal((await cli(path.join(root,'CustomGame'), 'run', '.')).stdout.replaceAll('\r\n','\n'), 'CustomGame\n42\n');
            assert.equal(new Set([application,library,custom].flatMap(text => text.match(/local\.[pal][a-f0-9]{32}/g) ?? [])).size, 6);
            events.push('installed template substitution, distinct IDs, binary preservation and execution');

            await create('Console Application', 'MyGame', 'already exists');
            assert.equal(await manifest('MyGame'), application);
            await create('Console Application', '../UnsafeName', 'portable identifier');
            await assert.rejects(fs.stat(path.join(root,'..','UnsafeName')), {code: 'ENOENT'});
            assert.equal(vscode.workspace.workspaceFolders![0].uri.fsPath, root);
            events.push('invalid names and existing destinations fail without overwrites or workspace switches');

            await fs.appendFile(process.env.VOID_G66_TAMPER!, '\n# changed\n');
            const tampered = begin();
            await waitFor('Template registration changed', toast);
            assert.equal(await ui.visible('Choose a VOID project template', dialog), 0);
            await dismiss();
            await tampered;
            assert.match((await cli(root, 'native', 'approvals')).stdout, /No native approvals/);
            events.push('tampered registration failed before showing choices; creation conferred no native consent');
        }
        await fs.writeFile(receipt, JSON.stringify({vscode: vscode.version, events}, null, 2) + '\n');
        console.log('g66_new_project_host:', JSON.stringify(events));
    } catch (error) {
        console.error('G66 workbench:', await ui.evaluate<string>('document.body.innerText'));
        throw error;
    } finally {
        ui.close();
        await api.deactivate();
    }
}
