import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';

import {offer, pause, until, workbench} from './creationWorkbench';
const review = 'Review VOID project creation';

export async function run(): Promise<void> {
    const root = process.env.VOID_CREATION_ROOT!;
    const mode = process.env.VOID_CREATION_MODE!;
    const evidence = process.env.VOID_CREATION_EVIDENCE!;
    const manifest = path.join(root, 'void.project.toml');
    const events: string[] = [];
    const pass = (label: string) => {events.push(label); console.log('PASS ' + label);};
    const uri = (name: string) => vscode.Uri.file(path.join(root, name));
    const exists = (file: string) => fs.access(file).then(() => true, () => false);
    const extension = vscode.extensions.getExtension('void-language.void-language');
    assert(extension);
    let api: {ready(): Promise<void>; status(): {state: string; entry?: string}; deactivate(): Promise<void>} | undefined;
    let ui: Awaited<ReturnType<typeof workbench>> | undefined;
    let failure: string | undefined;
    try {
        assert.equal(vscode.workspace.isTrusted, mode !== 'untrusted');
        api = await extension.activate();
        await api!.ready();
        if (mode === 'untrusted' || mode === 'multi-root') {
            const file = uri('restricted.void');
            await vscode.workspace.fs.writeFile(file, Buffer.from('func main():\n    return\n'));
            const document = await vscode.workspace.openTextDocument(file);
            await vscode.window.showTextDocument(document);
            await vscode.commands.executeCommand('void.createProject');
            await vscode.commands.executeCommand('void.resetCreationPrompt');
            await pause(1300);
            assert.equal(document.languageId, 'void');
            assert.equal(api!.status().state, 'stopped');
            assert(!await exists(process.env.VOID_CREATION_SPAWN!));
            assert(!await exists(manifest));
            pass(mode + ' workspace stays syntax-only with zero child launches or writes');
            return;
        }
        ui = await workbench(process.env.VOID_CREATION_CDP!);
        const toast = '.notification-toast';
        const dialog = '.quick-input-widget';
        if (mode === 'offered-close') {
            const named = await vscode.workspace.openTextDocument(uri('first.void').with({scheme: 'untitled'}));
            await vscode.window.showTextDocument(named);
            const text = new vscode.WorkspaceEdit();
            text.insert(named.uri, new vscode.Position(0, 0), 'func main():\n    return\n');
            assert(await vscode.workspace.applyEdit(text));
            await pause(400);
            assert.equal(await ui.visible(offer, toast), 0);
            assert(await named.save());
            await until(() => ui!.visible(offer, toast), n => n === 1, 'an outstanding offer before host close');
            assert(!await exists(manifest));
            pass('first save of a named buffer offers, and host closes while offered');
            return;
        }
        const noOffer = async () => {
            await pause(950);
            assert.equal(await ui!.visible(offer, toast), 0);
            assert(!await exists(manifest));
        };
        async function add(name: string, bytes = '') {
            const file = uri(name);
            await vscode.workspace.fs.writeFile(file, Buffer.from(bytes));
            return file;
        }
        const main = 'func main() -> i32:\n    return 27\n';
        if (mode === 'old-tree') {
            const document = await vscode.workspace.openTextDocument(uri('existing.void'));
            await vscode.window.showTextDocument(document);
            await noOffer();
            pass('activation and opening a pre-existing source do not offer');
            await add('later.void', main);
            await noOffer();
            pass('an unrelated pre-existing source suppresses a later-file offer');
            return;
        }
        if (mode === 'outside') {
            const outside = vscode.Uri.file(path.join(path.dirname(root), 'outside.void'));
            await fs.writeFile(outside.fsPath, main);
            await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(outside));
            await add('first.void', main);
            await noOffer();
            pass('an active document outside the workspace suppresses the automatic offer');
            return;
        }
        if (mode === 'old-server') {
            await add('first.void', main);
            await until(() => ui!.visible('Project creation requires a matching voidls', toast), Boolean, 'older server rejection');
            await noOffer();
            pass('an older server fails visibly with no standalone creation fallback');
            return;
        }
        if (mode === 'manual') {
            const source = uri('existing.void');
            const create = vscode.commands.executeCommand('void.createProject');
            await until(() => ui!.visible('VOID project kind', dialog), Boolean, 'kind selection');
            await ui.click('application', dialog);
            await until(() => ui!.visible('VOID project name', dialog), Boolean, 'name selection');
            ui.key('Enter', 'Enter', 13);
            await until(() => ui!.visible(review, dialog), Boolean, 'deliberate metadata review');
            assert(!await exists(manifest));
            await ui.click('Create Manifest', dialog);
            await create;
            await until(() => api!.status().entry, entry => entry === source.fsPath, 'manual project entry');
            assert(await exists(manifest));
            pass('deliberate creation works around existing multiple sources');
            return;
        }
        if (mode === 'collision') {
            const source = await add('first.void', main);
            await until(() => ui!.visible(offer, toast), n => n === 1, 'offer before collision');
            await ui.click('Create VOID project', toast);
            await until(() => ui!.visible(review, dialog), n => n === 1, 'review before collision');
            const incoming = "schema=1\n[project]\nid='org.test.external'\nname='External'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.external-app'\n";
            await fs.writeFile(manifest, incoming, {flag: 'wx'});
            await ui.click('Create Manifest', dialog);
            await until(() => api!.status().entry, entry => entry === source.fsPath, 'same-root session after collision');
            assert.equal(await fs.readFile(manifest, 'utf8'), incoming);
            pass('a manifest arriving during review is preserved and selected in the same root');
            return;
        }

        const first = await add('first.void', main);
        await until(() => ui!.visible(offer, toast), n => n === 1, 'one initial offer');
        await ui.click('Keep standalone', toast);
        await until(() => ui!.visible(offer, toast), n => n === 0, 'standalone choice recorded');
        await fs.unlink(first.fsPath);
        const suppressed = await add('declined.void', main);
        await noOffer();
        pass('decline persists and prevents a repeat');
        await fs.unlink(suppressed.fsPath);

        await vscode.commands.executeCommand('void.resetCreationPrompt');
        const dismissed = await add('dismissed.void', main);
        await until(() => ui!.visible(offer, toast), n => n === 1, 'offer after deliberate reset');
        await vscode.commands.executeCommand('notifications.clearAll');
        await until(() => ui!.visible(offer, toast), n => n === 0, 'notification dismissed');
        await fs.unlink(dismissed.fsPath);
        const afterDismissal = await add('after-dismissal.void', main);
        await noOffer();
        pass('dismissal persists and reset is deliberate');
        await fs.unlink(afterDismissal.fsPath);

        await vscode.commands.executeCommand('void.resetCreationPrompt');
        const cancel = await add('cancelled.void', main);
        await until(() => ui!.visible(offer, toast), n => n === 1, 'offer before cancelled review');
        await ui.click('Create VOID project', toast);
        await until(() => ui!.visible(review, dialog), n => n === 1, 'review before cancellation');
        await ui.click('Cancel', dialog);
        await until(() => ui!.visible(review, dialog), n => n === 0, 'cancelled review');
        await fs.unlink(cancel.fsPath);
        const afterCancel = await add('after-cancel.void', main);
        await noOffer();
        await fs.unlink(afterCancel.fsPath);
        pass('cancelled review writes nothing and acceptance suppresses another automatic offer');

        await vscode.commands.executeCommand('void.resetCreationPrompt');
        const source = uri('game.void');
        const helper = uri('helper.void');
        const edit = new vscode.WorkspaceEdit();
        edit.createFile(source);
        edit.createFile(helper);
        assert(await vscode.workspace.applyEdit(edit));
        await until(() => ui!.visible(offer, toast), n => n === 1, 'coalesced first-file offer');
        assert(!await exists(manifest));
        const dirty = await vscode.workspace.openTextDocument(source);
        await vscode.window.showTextDocument(dirty);
        const patch = new vscode.WorkspaceEdit();
        patch.insert(source, new vscode.Position(0, 0), main);
        assert(await vscode.workspace.applyEdit(patch));
        assert(dirty.isDirty);
        await ui.click('Create VOID project', toast);
        await until(() => ui!.visible(review, dialog), n => n === 1, 'metadata review');
        const detail = await ui.evaluate<string>("document.querySelector('.quick-input-widget').innerText");
        assert(detail.includes(manifest) && /local\.p[0-9a-f]{32}/.test(detail) && /local\.a[0-9a-f]{32}/.test(detail));
        assert(!await exists(manifest));
        await ui.click('Create Manifest', dialog);
        await until(() => exists(manifest), Boolean, 'exclusive manifest publication');
        const manifestBytes = await fs.readFile(manifest, 'utf8');
        for (const id of detail.match(/local\.[pa][0-9a-f]{32}/g)!) assert(manifestBytes.includes(id));
        await until(() => api!.status().entry, entry => entry === source.fsPath, 'dirty main in a compiler project session');
        assert(dirty.isDirty && dirty.getText() === main);
        assert.equal(await fs.readFile(source.fsPath, 'utf8'), '');
        assert.equal(await fs.readFile(helper.fsPath, 'utf8'), '');
        pass('one real offer for a two-file batch, reviewed IDs, dirty source preserved in the project session');
        assert(await dirty.save());
        await vscode.commands.executeCommand('void.createProject');
        assert.equal(await fs.readFile(manifest, 'utf8'), manifestBytes);
        pass('deliberate creation preserves the existing project');
    } catch (error) {
        failure = String(error instanceof Error ? error.stack : error);
        const body = await ui?.evaluate<string>('document.body.innerText').catch(() => undefined);
        if (body) events.push(body);
        events.push(JSON.stringify({status: api?.status(), open: vscode.workspace.textDocuments.map(d =>
            ({file: d.uri.fsPath, dirty: d.isDirty, version: d.version})), diagnostics: vscode.languages.getDiagnostics().map(([file, ds]) =>
            [file.fsPath, ds.map(d => ({message: d.message, code: d.code}))])}));
        throw error;
    } finally {
        ui?.close();
        await api?.deactivate();
        await fs.writeFile(evidence, JSON.stringify({mode, editor: vscode.version, platform: process.platform, events, failure, status: failure ? 'FAIL' : 'PASS'}, null, 2) + '\n');
    }
}
