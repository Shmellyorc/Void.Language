import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {execFile} from 'node:child_process';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {promisify} from 'node:util';
import {pause, until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root = vscode.workspace.workspaceFolders![0].uri.fsPath;
    const compiler = process.env.VOID_G67_COMPILER!;
    const receipt = process.env.VOID_G67_RECEIPT!;
    const github = process.env.VOID_G67_GITHUB;
    const restricted = process.env.VOID_G67_RESTRICTED === '1';
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    const api = await extension.activate() as {ready(): Promise<void>; deactivate(): Promise<void>};
    const ui = await workbench(process.env.VOID_G67_PORT!);
    const picker = '.quick-input-widget', toast = '.notification-toast';
    const wait = async (label: string, region = picker) => {
        try { return await until(() => ui.visible(label, region), Boolean, label); }
        catch (error) {
            const uiText = await ui.evaluate<string[]>(`[...document.querySelectorAll('.quick-input-widget, .notification-toast, .monaco-dialog-box')]
                .filter(element => element.getBoundingClientRect().width).map(element => element.innerText.slice(0, 1500))`);
            throw new Error(`${String(error)}. Visible UI: ${JSON.stringify(uiText)}`);
        }
    };
    const clear = () => vscode.commands.executeCommand('notifications.clearAll');
    const press = (key: 'Enter' | 'Escape') => ui.key(key, key, key === 'Enter' ? 13 : 27);
    const focusInput = async () => {
        // The workbench restores focus when a previous Quick Pick closes.
        // Wait for the new input to receive it before sending keyboard input.
        await until(() => ui.evaluate<boolean>(`!!document.activeElement?.closest('.quick-input-widget')
            && document.activeElement?.tagName === 'INPUT'`), Boolean, 'focused Quick Pick input');
        await pause(200);
    };
    const choose = async (label: string) => {
        await focusInput();
        await ui.type(label);
        await wait('1 Results');
        press('Enter');
    };
    const cli = async (cwd: string, ...args: string[]) =>
        (await promisify(execFile)(compiler, args, {cwd, timeout: 90000})).stdout;
    const start = (action: 'add' | 'remove' = 'add') => vscode.commands.executeCommand(`void.${action}Library`);
    const add = async (label: string, result = `Added ${label}`) => {
        const pending = start();
        await wait('Add VOID library');
        await choose(label);
        await pending;
        await wait(result, toast);
        await clear();
    };
    const remove = async (alias: string, confirm = true) => {
        const pending = start('remove');
        await wait('Remove VOID library');
        await choose(alias);
        await wait(`Remove "${alias}" from`);
        await choose(confirm ? 'Remove Library' : 'Cancel');
        await pending;
        if (confirm) await wait(`Removed ${alias}`, toast);
        await clear();
    };
    const enter = async (title: string, value: string) => {
        await wait(title);
        await focusInput();
        await ui.type(value);
        press('Enter');
    };
    const gitAdd = async (url: string, alias: string, result: string) => {
        const pending = start();
        await wait('Add VOID library');
        await choose('From GitHub');
        await enter('GitHub library URL', url);
        await enter('GitHub library alias', alias);
        await pending;
        await wait(result, toast);
        await clear();
    };
    const view = (...parts: string[]) => path.join(root, 'libs', ...parts);
    const missing = (file: string) => assert.rejects(fs.stat(file), {code: 'ENOENT'});
    const manifest = path.join(root, 'void.project.toml'), lock = path.join(root, 'void.lock');
    const read = (file: string) => fs.readFile(file, 'utf8');
    const state = async () => ({
        manifest: await read(manifest),
        lock: await read(lock).catch((error: NodeJS.ErrnoException) => {
            if (error.code === 'ENOENT') return '';
            throw error;
        }),
        views: (await fs.readdir(view()).catch((error: NodeJS.ErrnoException) => {
            if (error.code === 'ENOENT') return [];
            throw error;
        })).sort(),
    });
    const verifyLock = () => cli(root, 'resolve', '--project', root, '--locked');
    const events: string[] = [];
    try {
        assert.equal(vscode.workspace.isTrusted, !restricted);
        if (restricted) {
            const before = await fs.readdir(root);
            for (const command of ['add', 'remove'] as const) {
                await start(command);
                await wait('Trust this workspace before changing VOID libraries', toast);
                assert.equal(await ui.visible('VOID library', picker), 0);
                await clear();
            }
            await missing(process.env.VOID_G67_COMPILER_PROBE!);
            assert.deepEqual(await fs.readdir(root), before);
            events.push('Restricted Mode blocks both commands before compiler launch or writes');
        } else {
            const file = path.join(root, 'src/app.void'), uri = vscode.Uri.file(file);
            const original = await read(file);
            assert.match(original, /func main\(\)/);
            const registered = path.join(path.dirname(root), 'registered fixture');
            const local = path.join(root, 'local fixture');
            for (const [folder, name] of [[registered, 'arithmetic'], [local, 'localpkg']]) {
                await cli(root, 'new', 'lib', folder, '--nested', '--package-name', name);
                const libraryManifest = await read(path.join(folder, 'void.project.toml'));
                const entry = /^entry = "([^"]+)"/m.exec(libraryManifest)?.[1];
                assert(entry);
                await fs.writeFile(path.join(folder, entry), 'public func answer() -> i32:\n    return 17\n');
            }
            await cli(root, 'package', 'install', 'registered', '--dir', registered);
            const source = 'use pkg/registered\nfunc main():\n    print(registered.answer())\n';
            await fs.writeFile(file, source);
            const document = await vscode.workspace.openTextDocument(uri);
            const editor = await vscode.window.showTextDocument(document);
            await api.ready();
            const importError = (present: boolean) => until(() => vscode.languages.getDiagnostics(uri),
                diagnostics => diagnostics.some(d => d.message.includes('pkg/registered')) === present,
                present ? 'unresolved registered import' : 'registered import resolved');
            await importError(true);
            const before = await state();
            const cancelled = start();
            for (const label of ['Add VOID library', 'raylib', 'Registered', 'From local folder', 'From GitHub']) await wait(label);
            press('Escape');
            await cancelled;
            assert.deepEqual(await state(), before);
            await pause(300);
            const folderCancelled = start();
            await wait('Add VOID library');
            await choose('From local folder');
            await wait('Select a VOID library folder');
            press('Escape');
            await folderCancelled;
            assert.deepEqual(await state(), before);

            assert(await editor.edit(edit => edit.insert(document.positionAt(source.length), '\n')));
            try {
                await start();
                await wait('Save app.void before changing VOID libraries', toast);
                assert.deepEqual(await state(), before);
            } finally {
                await vscode.commands.executeCommand('undo');
                await clear();
            }
            assert.equal(document.isDirty, false);
            events.push('Picker and folder cancellation preserve project; dirty source blocks compiler action');

            const changed = start();
            await wait('Add VOID library');
            await fs.appendFile(manifest, '\n# Changed while picker was open\n');
            const edited = await state();
            await choose('registered');
            await changed;
            await wait("saved manifest, lock or directory changed", toast);
            assert.deepEqual(await state(), edited);
            await clear();
            await fs.writeFile(manifest, before.manifest);
            events.push('Compiler rejects a saved manifest changed while the picker is open');

            await add('registered');
            await verifyLock();
            await importError(false);
            assert.match((await state()).lock, /libs\/registered/);
            assert.equal(await read(file), source);
            const symbol = document.positionAt(source.indexOf('registered.answer') + 'registered.'.length);
            await until(async () => vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', uri, symbol.translate(0, 2)),
                result => !!result?.some(item => item.contents.some(value =>
                    (typeof value === 'string' ? value : value.value).includes('answer'))), 'package hover');
            await until(async () => vscode.commands.executeCommand<(vscode.Location | vscode.LocationLink)[]>(
                'vscode.executeDefinitionProvider', uri, symbol.translate(0, 2)), result => !!result?.some(item =>
                ('targetUri' in item ? item.targetUri : item.uri).fsPath.startsWith(vscode.Uri.file(view('registered')).fsPath + path.sep)),
                'verified package definition');
            const probe = '\nfunc probe():\n    registered.ans\n';
            assert(await editor.edit(edit => edit.insert(document.positionAt(source.length), probe)));
            try {
                const cursor = document.positionAt(source.length + probe.length - 1);
                await until(async () => vscode.commands.executeCommand<vscode.CompletionList>(
                    'vscode.executeCompletionItemProvider', uri, cursor),
                    result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'answer'),
                    'package completion of a partial qualified name');
            } finally {
                await vscode.commands.executeCommand('undo');
            }
            assert.equal(document.isDirty, false);
            assert.equal(document.getText(), source);

            const taskExits = new Map<vscode.TaskExecution, number | undefined>();
            const listener = vscode.tasks.onDidEndTaskProcess(event => taskExits.set(event.execution, event.exitCode));
            try {
                for (const command of ['run', 'build']) {
                    const task = await vscode.commands.executeCommand<vscode.TaskExecution>(`void.${command}Project`);
                    assert(task);
                    assert.equal(await until(() => taskExits.get(task), () => taskExits.has(task), command + ' task exit'), 0);
                }
            } finally { listener.dispose(); }
            await vscode.window.showTextDocument(document);
            const added = await state();
            await remove('registered', false);
            assert.deepEqual(await state(), added);
            await remove('registered');
            await missing(view('registered'));
            await missing(lock);
            await importError(true);
            await cli(root, 'package', 'search', 'registered');
            events.push('Registered add/remove update lock, installed view, diagnostics, completion, hover and definition; Run and Build succeeded');

            const localPending = start();
            await wait('Add VOID library');
            await choose('From local folder');
            await wait('Select a VOID library folder');
            ui.key('a', 'KeyA', 65, 2);
            await ui.type(local + path.sep);
            // Wait until the folder dialog shows just the parent and src.
            await wait('2 Results');
            await ui.click('Select Folder', picker);
            await localPending;
            await wait('Added localpkg', toast);
            await clear();
            assert.match((await state()).manifest, /path = "libs\/localpkg"/);
            assert.equal(await read(view('localpkg', 'void.project.toml')), await read(path.join(local, 'void.project.toml')));
            await verifyLock();
            events.push('Local source copied under the compiler-suggested alias from a folder with a different basename');

            const gitBefore = await state();
            const url = github ?? 'https://github.com/g67-fixture/libraries/blob/main/invalid';
            await gitAdd(url, '../invalid', 'valid alias');
            assert.deepEqual(await state(), gitBefore);
            await gitAdd(github ? github.replace('/tree/', '/blob/') : url, 'network', 'Use a GitHub /tree/');
            assert.deepEqual(await state(), gitBefore);
            if (github) {
                await gitAdd(github, 'network', 'Added network');
                await verifyLock();
                assert.match((await state()).lock, /https:\/\/github\.com\/g67-fixture\/libraries\.git/);
                assert.match((await state()).lock, /subdir = "packages\/units"/);
                assert((await state()).lock.includes(`rev = "${process.env.VOID_G67_GITHUB_REV}"`));
                assert(await read(view('network', 'src', 'api.void')));
                await remove('network');
                await missing(view('network'));
                await verifyLock();
                const after = await state();
                await gitAdd(github.replace('/topic/slash/', '/missing-ref/'), 'network', 'GitHub tree ref not found');
                assert.deepEqual(await state(), after);
                events.push('GitHub slash-ref subtree acquired through the real HTTPS fixture; valid lock/view, removal and failed acquisition checked');
            }
            events.push('Invalid Git alias and malformed subtree rejected with manifest, lock and views unchanged');

            await fs.writeFile(file, original);
            await until(() => document.getText(), text => text === original, 'original starter source reloaded');
            const approvals = await cli(root, 'native', 'approvals');
            assert.match(approvals, /No native approvals/);
            await add('raylib');
            await verifyLock();
            assert.match((await state()).lock, /org\.voidlang\.raylib/);
            assert(await read(view('raylib', '.void', 'native', 'void.native.toml')));
            assert.equal(await cli(root, 'native', 'approvals'), approvals);
            await assert.rejects(cli(root, 'run', '.', '--reuse-native-approval'), /No saved native approval/);
            assert.equal(await read(file), original);
            events.push('Bundled raylib installed and verified; source and native approvals stayed unchanged');
        }
        await fs.writeFile(receipt, JSON.stringify({platform: process.platform, vscode: vscode.version, events}, null, 2));
        console.log('g67_library_host:', events.join('; '));
    } finally {
        ui.close();
        await api.deactivate();
        await pause(100);
    }
}
