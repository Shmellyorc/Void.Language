// Real VS Code/VSCodium completion; isolated development host, no installation.
const fs = require('node:fs/promises');
const path = require('node:path');
const net = require('node:net');
const assert = require('node:assert/strict');
const extension = path.resolve(__dirname, '../../editors/vscode');
exports.launch = async function () {
    for (const key of ['VOID_G92_EDITOR_OUTPUT', 'VOIDLS_PATH', 'VOIDC_PATH', 'VSCODE_EXECUTABLE_PATH']) assert(process.env[key], key);
    const output = path.resolve(process.env.VOID_G92_EDITOR_OUTPUT);
    await fs.mkdir(output);
    const existing = process.env.VOID_G92_EXISTING_PROJECT;
    const project = existing ? path.resolve(existing) : path.join(output, 'project');
    const documentPath = existing ? path.resolve(process.env.VOID_G92_EXISTING_DOCUMENT) : path.join(project, 'main.void');
    if (!existing) {
        await fs.mkdir(project);
        await fs.copyFile(path.join(__dirname, 'fixtures/member_completion.void'), documentPath);
    }
    const settings = {
        'void.server.path': path.resolve(process.env.VOIDLS_PATH), 'void.compiler.path': path.resolve(process.env.VOIDC_PATH),
        ...(existing ? {'void.project.root': project} : {'void.project.entry': 'main.void'}), 'void.trace.server': 'verbose'};
    const profile = path.join(output, 'profile'); await fs.mkdir(path.join(profile, 'User'), {recursive: true});
    await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
        'security.workspace.trust.enabled': false, 'workbench.startupEditor': 'none',
        'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false,
        'files.hotExit': 'off', 'editor.wordBasedSuggestions': 'off', ...settings}));
    const port = await new Promise(resolve => { const s = net.createServer(); s.listen(0, '127.0.0.1', () => { const p = s.address().port; s.close(() => resolve(p)); }); });
    const {runTests} = require(path.join(extension, 'node_modules/@vscode/test-electron'));
    await runTests({vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH, extensionDevelopmentPath: extension,
        extensionTestsPath: __filename, extensionTestsEnv: {VOID_G92_PROJECT: project, VOID_G92_DOCUMENT: documentPath, VOID_G92_PORT: String(port)},
        launchArgs: [project, '--user-data-dir=' + profile, '--extensions-dir=' + path.join(output, 'extensions'),
            '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes', '--remote-debugging-port=' + port]});
};
exports.run = async function () {
    const vscode = require('vscode');
    const {until, workbench} = require(path.join(extension, 'out/test/creationWorkbench.js'));
    const output = process.env.VOID_G92_EDITOR_OUTPUT;
    const uri = vscode.Uri.file(process.env.VOID_G92_DOCUMENT);
    const doc = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(doc);
    const api = await vscode.extensions.getExtension('void-language.void-language').activate();
    const browser = await workbench(process.env.VOID_G92_PORT);
    const original = doc.getText();
    const good = await fs.readFile(path.join(__dirname, 'fixtures/member_completion.void'), 'utf8');
    const report = {version: vscode.version, server: process.env.VOIDLS_PATH, compiler: process.env.VOIDC_PATH, cases: []};
    const save = () => fs.writeFile(path.join(output, 'results.json'), JSON.stringify(report, null, 2));
    const replace = async text => { const e = new vscode.WorkspaceEdit(); e.replace(uri, new vscode.Range(doc.positionAt(0), doc.positionAt(doc.getText().length)), text); assert(await vscode.workspace.applyEdit(e)); };
    const at = needle => doc.positionAt(doc.getText().lastIndexOf(needle) + needle.length);
    const complete = needle => vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri, at(needle));
    const label = i => typeof i.label === 'string' ? i.label : i.label.label;
    try {
        await api.ready();
        await replace(good);
        report.language = doc.languageId; report.client = api.status(); await save();
        await until(() => complete('if s.'), r => r?.items.some(i => label(i) === 'scenes'), 'initial receiver');
        for (const [expression, needle, wanted] of [
            ['s.', 's.', 'scenes'], ['s.scenes.', 's.scenes.', 'count'],
            ['if s.', 's.', 'scenes'], ['if s.scenes.', 's.scenes.', 'count'],
            ['if s.scenes.c', 's.scenes.c', 'count']]) {
            await replace(good.slice(0, good.indexOf('    if ')) + '    ' + expression + (expression.startsWith('if ') ? '\n        return\n' : '\n'));
            const result = await until(() => complete(needle), r => r?.items.some(i => label(i) === wanted), expression);
            report.cases.push({expression, items: result.items.map(i => ({label: label(i), detail: i.detail, kind: i.kind, documentation: i.documentation}))}); await save();
            const p = at(needle); editor.selection = new vscode.Selection(p, p); editor.revealRange(new vscode.Range(p, p));
            await vscode.commands.executeCommand('workbench.action.focusActiveEditorGroup');
            await vscode.commands.executeCommand('editor.action.triggerSuggest');
            await until(() => browser.visible(wanted, '.suggest-widget.visible .monaco-list-row'), n => n > 0, expression + ' rendered popup');
            await vscode.commands.executeCommand('hideSuggestWidget');
        }
        // Type through the editor command path, exercising automatic dot triggers.
        await replace(good.slice(0, good.indexOf('    if ')) + '    ');
        let p = doc.positionAt(doc.getText().length); editor.selection = new vscode.Selection(p, p);
        for (const text of ['s', '.']) await vscode.commands.executeCommand('type', {text});
        await until(() => browser.visible('scenes', '.suggest-widget.visible .monaco-list-row'), n => n > 0, 'typed s. popup');
        await vscode.commands.executeCommand('hideSuggestWidget');
        for (const text of ['scenes', '.']) await vscode.commands.executeCommand('type', {text});
        await until(() => browser.visible('count', '.suggest-widget.visible .monaco-list-row'), n => n > 0, 'typed chain popup');
        report.automatic_dot_popups = true;
        const targets = await (await fetch('http://127.0.0.1:' + process.env.VOID_G92_PORT + '/json/list')).json();
        const socket = new WebSocket(targets.find(t => t.url.includes('workbench.html')).webSocketDebuggerUrl);
        await new Promise(resolve => socket.addEventListener('open', resolve, {once: true}));
        const shot = new Promise(resolve => socket.addEventListener('message', e => resolve(JSON.parse(String(e.data)).result), {once: true}));
        socket.send(JSON.stringify({id: 1, method: 'Page.captureScreenshot', params: {format: 'png'}}));
        const capture = await shot; socket.close();
        await fs.writeFile(path.join(output, 'member-completion.png'), Buffer.from(capture.data, 'base64'));
        await until(() => vscode.languages.getDiagnostics(uri), ds => ds.some(d => d.severity === vscode.DiagnosticSeverity.Error), 'incomplete diagnostic');
        const tokens = await vscode.commands.executeCommand('vscode.provideDocumentSemanticTokens', uri);
        assert(tokens?.data.length); report.incomplete_semantic_tokens = tokens.data.length / 5;
        await replace(good);
        await until(() => vscode.languages.getDiagnostics(uri), ds => ds.length === 0, 'diagnostic recovery');
        assert((await vscode.commands.executeCommand('vscode.executeHoverProvider', uri, at('s.scenes.cou')))?.length);
        assert(await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, at('s.sce')));
        const node = '\nstruct g92_node:\n    value: i32\n    children: [g92_node]\n' +
            'func (n g92_node) read() -> i32:\n    return n.value\n' +
            'func (n g92_node) change():\n    n.value += 1\n' +
            'func g92_freeze(const n: g92_node) -> g92_node:\n    return n\n';
        const readonly = good + node + 'func g92_inspect(n: g92_node):\n    var alias = g92_freeze(n)\n    alias.\n';
        report.readonly_cases = [];
        const verifyReadonly = async (text, needle, wanted, excluded) => {
            await replace(text);
            const result = await until(() => complete(needle), r => r?.items.some(i => label(i) === wanted) &&
                !r.items.some(i => excluded.includes(label(i))), 'readonly ' + needle);
            report.readonly_cases.push({needle, items: result.items.map(i => ({label: label(i), detail: i.detail}))});
            const p = at(needle); editor.selection = new vscode.Selection(p, p); editor.revealRange(new vscode.Range(p, p));
            await vscode.commands.executeCommand('workbench.action.focusActiveEditorGroup');
            await vscode.commands.executeCommand('editor.action.triggerSuggest');
            await until(() => browser.visible(wanted, '.suggest-widget.visible .monaco-list-row'), n => n > 0, 'readonly rendered ' + needle);
            for (const name of excluded) assert.equal(await browser.visible(name, '.suggest-widget.visible .monaco-list-row'), 0);
            await vscode.commands.executeCommand('hideSuggestWidget');
            await save();
        };
        await verifyReadonly(readonly, '    alias.', 'read', ['change']);
        await verifyReadonly(readonly.replace('    alias.\n', '    alias.children.\n'), '    alias.children.', 'count', ['push', 'pop', 'clear']);
        for (const binding of ['const', 'var']) {
            const caller = good + node + 'func g92_inspect(const n: g92_node) -> i32:\n    return n.\n' +
                'func g92_caller():\n    ' + binding + ' item = g92_node()\n    print(g92_inspect(item))\n';
            await verifyReadonly(caller, 'return n.', 'read', ['change']);
        }
        await replace(good);
        await until(() => vscode.languages.getDiagnostics(uri), ds => ds.length === 0, 'readonly edit repair');
        assert.equal(await fs.readFile(uri.fsPath, 'utf8'), original, 'Editor verification must leave disk unchanged');
        report.target = uri.fsPath; report.disk_unchanged = true;
        report.status = 'PASS'; await save();
        console.log('G92 real editor: receiver/chains, readonly aliases, const/mutable callers, rendered popups, coloring and recovery passed');
    } catch (error) {
        report.status = 'FAIL'; report.error = String(error.stack ?? error);
        report.buffer = doc.getText(); report.client = api.status();
        report.diagnostics = vscode.languages.getDiagnostics(uri);
        await save(); throw error;
    }
    finally { await replace(original); browser.close(); await api.deactivate(); }
};
