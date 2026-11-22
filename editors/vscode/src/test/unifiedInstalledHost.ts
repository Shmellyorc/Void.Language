import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import * as path from 'node:path';
import {homedir} from 'node:os';
import {createHash} from 'node:crypto';
import {until} from './creationWorkbench';

// Exercises artifacts at the existing installation, in the actual user profile.
// The runner owns its generated project. Ninja is queried without any edits.
export async function run(): Promise<void> {
    const root = vscode.workspace.workspaceFolders![0].uri.fsPath;
    const evidence = process.env.VOID_DX_EVIDENCE;
    const phase = process.env.VOID_DX_PHASE;
    assert(evidence && phase && ['live', 'reopen', 'ninja'].includes(phase));
    const ninja = phase === 'ninja';
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    assert.equal(extension.extensionPath, path.join(homedir(), '.vscode/extensions/void-language.void-language-0.1.2'));
    const compiler = path.join(homedir(), '.local/bin/voidc');
    const api = await extension.activate() as {ready(): Promise<void>; status(): {state: string}};
    const events: unknown[] = [];
    const cli = async (...args: string[]) => {
        const result = await promisify(execFile)(compiler, args, {cwd: root, timeout: 180000});
        events.push({cli: args, stdout: result.stdout, stderr: result.stderr});
        return result;
    };
    const source = vscode.Uri.file(path.join(root, ninja ? 'app.void' : 'src/app.void'));
    const original = await fs.readFile(source.fsPath, 'utf8');
    const modes = new Map<string, number>();
    for (const p of [root, path.join(root, '.void')]) {
        try { modes.set(p, (await fs.stat(p)).mode); } catch { /* created by init */ }
    }
    const protectedFiles = ninja ? await Promise.all(['app.void', 'void.project.toml', 'void.lock'].map(async name => {
        const p = path.join(root, name);
        return {p, mode: (await fs.stat(p)).mode, bytes: await fs.readFile(p)};
    })) : [];
    const document = await vscode.workspace.openTextDocument(source);
    await vscode.window.showTextDocument(document);
    await api.ready();
    const complete = async (needle: string, expected: string) => {
        const index = document.getText().indexOf(needle);
        assert(index >= 0, needle);
        const position = document.positionAt(index + needle.length);
        const list = await until(async () => {
            try { return await vscode.commands.executeCommand<vscode.CompletionList>(
                'vscode.executeCompletionItemProvider', source, position); }
            catch { return undefined; }
        }, result => !!result?.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === expected), needle);
        events.push({completion: needle, count: list!.items.length, expected});
    };
    const valid = 'use pkg/raylib as rl\n\nfunc main() -> i32:\n' +
        '    var image = rl.gen_image_color(2, 2, rl.orange()) as rl.image\n' +
        '    defer rl.unload_image(image)\n' +
        '    if rl.get_image_color(image, 0, 0).r != 255:\n        return 9\n' +
        '    print("unified raylib passed")\n    return 0\n';
    const replace = async (text: string) => {
        const edit = new vscode.WorkspaceEdit();
        edit.replace(source, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
        assert(await vscode.workspace.applyEdit(edit));
    };
    try {
        assert(vscode.workspace.isTrusted);
        if (ninja) {
            await complete('rl.', 'init_window');
            await complete('as rl.', 'window');
            await complete(original.includes('for not rl.') ? 'for not rl.' : 'for rl.', 'window_should_close');
        } else {
            if (phase === 'live') {
                await assert.rejects(fs.stat(path.join(root, 'void.project.toml')), {code: 'ENOENT'});
                await cli('init');
                await cli('add', 'raylib');
            }
            await replace('use pkg/raylib as rl\n\nfunc main():\n    rl.\n');
            await complete('rl.', 'init_window');
            await replace('use pkg/raylib as rl\n\nfunc main():\n    for rl.\n');
            await complete('for rl.', 'window_should_close');
            await until(() => vscode.languages.getDiagnostics(source), d => d.length > 0, 'ordinary incomplete-code diagnostics');
            await replace('use pkg/raylib as rl\n\nfunc main():\n    var image = rl.gen_image_color(2, 2, rl.orange()) as rl.\n');
            await complete('as rl.', 'image');
            await replace(valid);
            assert(await document.save());
            await until(() => vscode.languages.getDiagnostics(source), d => d.length === 0, 'valid source clears diagnostics');
            const tokens = await until(async () => vscode.commands.executeCommand<vscode.SemanticTokens>(
                'vscode.provideDocumentSemanticTokens', source), value => !!value?.data.length, 'semantic tokens');
            events.push({semanticTokenIntegers: tokens!.data.length});
            await cli('check', '.');
            for (const suffix of ['cold', 'warm']) {
                const timings = path.join(evidence, phase + '-' + Date.now() + '-' + suffix + '.json');
                await cli('run', '.', '--allow-native', '--timings', '--timings-format', 'json', '--timings-output', timings);
                const status = JSON.parse(await fs.readFile(timings, 'utf8')).cache.status;
                if (suffix === 'warm') assert.equal(status, 'hit');
                events.push({cache: suffix, status});
            }
            await cli('build', '.', '--allow-native');
            const executable = path.join(root, '.void/build/app');
            const output = await promisify(execFile)(executable, [], {cwd: root, timeout: 30000});
            assert(output.stdout.includes('unified raylib passed'));
            events.push({aotExecution: output.stdout});
            if (phase === 'live') {
                const manifest = await vscode.workspace.openTextDocument(vscode.Uri.file(path.join(root, 'void.project.toml')));
                assert(!manifest.isDirty);
                await cli('remove', 'raylib');
                await until(() => manifest.getText(), text => !text.includes('[dependencies.raylib]'), 'open manifest refresh');
                await until(() => vscode.languages.getDiagnostics(source), d => d.some(x => x.message.includes('Undeclared direct package import')), 'removal diagnostics');
                // Removal preserves developer-owned sources. A plain local
                // source reference grants no native-provider authority. Keep
                // that copy, then select the bundled provider again.
                const retained = path.join(evidence, 'retained-raylib-' + Date.now());
                await fs.rename(path.join(root, 'libs/raylib'), retained);
                try { await cli('add', 'raylib', '--bundled'); }
                finally { await fs.rm(retained, {recursive: true, force: true}); }
                await replace('use pkg/raylib as rl\n\nfunc main():\n    for rl.\n');
                await complete('for rl.', 'window_should_close');
                await replace(valid); assert(await document.save());
            }
        }
        assert.equal(api.status().state, 'running');
        for (const [p, mode] of modes) assert.equal((await fs.stat(p)).mode, mode, p);
        for (const file of protectedFiles) {
            assert.equal((await fs.stat(file.p)).mode, file.mode);
            assert.deepEqual(await fs.readFile(file.p), file.bytes);
        }
        const identity = createHash('sha256').update(await fs.readFile(path.join(extension.extensionPath, 'dist/extension.js'))).digest('hex');
        await fs.writeFile(path.join(evidence, 'installed-editor-' + phase + '.json'), JSON.stringify({
            phase, root, vscode: vscode.version, executable: process.execPath,
            extensionPath: extension.extensionPath, extensionSHA256: identity,
            compiler, server: path.join(homedir(), '.local/bin/voidls'), events, status: 'PASS'
        }, null, 2));
        console.log('unified_installed_host: ' + phase + ' PASS');
    } finally {
        if (!ninja && document.getText() !== valid) { await replace(valid); await document.save(); }
        if (ninja) assert.equal(await fs.readFile(source.fsPath, 'utf8'), original);
    }
}
