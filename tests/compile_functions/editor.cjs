// Real editor regression, using an isolated development extension host/profile.
const fs = require('node:fs/promises');
const path = require('node:path');
const net = require('node:net');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const extension = path.resolve(__dirname, '../../editors/vscode');
exports.launch = async function () {
    for (const name of ['VOID_G93_EDITOR_OUTPUT', 'VOIDLS_PATH', 'VOIDC_PATH', 'VSCODE_EXECUTABLE_PATH']) assert(process.env[name], name);
    const output = path.resolve(process.env.VOID_G93_EDITOR_OUTPUT); await fs.mkdir(output);
    const project = path.join(output, 'project'); await fs.mkdir(project);
    await fs.copyFile(path.join(__dirname, '../../examples/compile_functions/main.void'), path.join(project, 'main.void'));
    const profile = path.join(output, 'profile'); await fs.mkdir(path.join(profile, 'User'), {recursive: true});
    await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
        'security.workspace.trust.enabled': false, 'workbench.startupEditor': 'none',
        'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false,
        'files.hotExit': 'off', 'editor.wordBasedSuggestions': 'off',
        'void.server.path': path.resolve(process.env.VOIDLS_PATH), 'void.compiler.path': path.resolve(process.env.VOIDC_PATH),
        'void.project.entry': 'main.void', 'void.trace.server': 'verbose'}));
    const port = await new Promise(resolve => {const s = net.createServer(); s.listen(0, '127.0.0.1', () => {const p = s.address().port; s.close(() => resolve(p));});});
    const {runTests} = require(path.join(extension, 'node_modules/@vscode/test-electron'));
    await runTests({vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH, extensionDevelopmentPath: extension,
        extensionTestsPath: __filename, extensionTestsEnv: {VOID_G93_PROJECT: project, VOID_G93_PORT: String(port)},
        launchArgs: [project, '--user-data-dir=' + profile, '--extensions-dir=' + path.join(output, 'extensions'),
            '--disable-extensions', '--disable-updates', '--disable-gpu', '--skip-welcome', '--skip-release-notes', '--remote-debugging-port=' + port]});
};
exports.run = async function () {
    const vscode = require('vscode');
    const {until, workbench} = require(path.join(extension, 'out/test/creationWorkbench.js'));
    const output = process.env.VOID_G93_EDITOR_OUTPUT;
    const uri = vscode.Uri.file(path.join(process.env.VOID_G93_PROJECT, 'main.void'));
    const doc = await vscode.workspace.openTextDocument(uri); const editor = await vscode.window.showTextDocument(doc);
    const original = doc.getText();
    const good = original + '\ncompile func identity<t>(value: t) -> t:\n    return value\nconst generic_value = identity<i64>(42)\n';
    const api = await vscode.extensions.getExtension('void-language.void-language').activate();
    const browser = await workbench(process.env.VOID_G93_PORT);
    const report = {version: vscode.version, compiler: process.env.VOIDC_PATH, server: process.env.VOIDLS_PATH, cases: []};
    report.compiler_sha256 = crypto.createHash('sha256').update(await fs.readFile(process.env.VOIDC_PATH)).digest('hex');
    report.server_sha256 = crypto.createHash('sha256').update(await fs.readFile(process.env.VOIDLS_PATH)).digest('hex');
    const save = () => fs.writeFile(path.join(output, 'results.json'), JSON.stringify(report, null, 2));
    const replace = async text => {const e = new vscode.WorkspaceEdit();e.replace(uri, new vscode.Range(doc.positionAt(0), doc.positionAt(doc.getText().length)), text);assert(await vscode.workspace.applyEdit(e));};
    const at = (needle, back = 0) => doc.positionAt(doc.getText().lastIndexOf(needle) + needle.length - back);
    const hover = (needle, back = 0) => vscode.commands.executeCommand('vscode.executeHoverProvider', uri, at(needle, back));
    const hoverText = value => value?.flatMap(h => h.contents).map(c => typeof c === 'string' ? c : c.value).join('\n').replace(/&nbsp;/g, ' ').replace(/\\ /g, ' ');
    const diagnostics = () => vscode.languages.getDiagnostics(uri);
    try {
        await api.ready(); await replace(good);
        await until(diagnostics, ds => ds.length === 0, 'compile function diagnostics');
        report.initial_hover = await hover('square(wid', 5); await save();
        const result = await until(() => hover('square(wid', 5), h => hoverText(h)?.includes('compile func'), 'compile function hover');
        report.cases.push({name: 'hover', text: hoverText(result)});
        const folded = await until(() => hover('print(answe'), h => hoverText(h)?.includes('64'), 'folded constant hover');
        report.cases.push({name: 'constant', text: hoverText(folded)});
        const generic = await until(() => hover('identity<i6', 4), h => hoverText(h)?.includes('i64'), 'generic specialization');
        report.cases.push({name: 'generic', text: hoverText(generic)});
        const definition = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, at('square(wid', 5));
        assert(definition?.length); report.cases.push({name: 'definition', result: definition});
        await replace(good.replace('square(width)', 'square()'));
        const signature = await until(() => vscode.commands.executeCommand('vscode.executeSignatureHelpProvider', uri, at('square('), '(', 0),
            h => h?.signatures.some(s => s.label.includes('value: i32')), 'compile function signature');
        report.cases.push({name: 'signature', result: signature});
        const prefix = good.slice(0, good.indexOf('func main()')) + 'func main():\n    var rectangle = dimensions()\n    rectangle';
        await replace(prefix);
        const p = doc.positionAt(doc.getText().length); editor.selection = new vscode.Selection(p, p); editor.revealRange(new vscode.Range(p, p));
        // Observe focus before the real editor type command. A background
        // Electron window can otherwise drop the command without editing bytes.
        const focusTargets = await (await fetch('http://127.0.0.1:' + process.env.VOID_G93_PORT + '/json/list')).json();
        const focusSocket = new WebSocket(focusTargets.find(t => t.url.includes('workbench.html')).webSocketDebuggerUrl);
        await new Promise(resolve => focusSocket.addEventListener('open', resolve, {once: true}));
        const focused = new Promise(resolve => focusSocket.addEventListener('message', resolve, {once: true}));
        focusSocket.send(JSON.stringify({id: 1, method: 'Page.bringToFront'})); await focused; focusSocket.close();
        await vscode.commands.executeCommand('workbench.action.focusActiveEditorGroup');
        await until(() => browser.evaluate('document.hasFocus() && !!document.activeElement?.closest(".monaco-editor")'), Boolean, 'focused editor input');
        await vscode.commands.executeCommand('type', {text: '.'});
        await until(() => doc.getText().endsWith('rectangle.'), Boolean, 'typed method dot');
        await until(() => browser.visible('area', '.suggest-widget.visible .monaco-list-row'), n => n > 0, 'automatic compile method popup');
        const completion = await vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri, doc.positionAt(doc.getText().length));
        assert(completion.items.some(i => (typeof i.label === 'string' ? i.label : i.label.label) === 'area'));
        report.cases.push({name: 'automatic method completion', items: completion.items.map(i => ({label: i.label, detail: i.detail}))});
        const targets = await (await fetch('http://127.0.0.1:' + process.env.VOID_G93_PORT + '/json/list')).json();
        const socket = new WebSocket(targets.find(t => t.url.includes('workbench.html')).webSocketDebuggerUrl);
        await new Promise(resolve => socket.addEventListener('open', resolve, {once: true}));
        const shot = new Promise(resolve => socket.addEventListener('message', e => resolve(JSON.parse(String(e.data)).result), {once: true}));
        socket.send(JSON.stringify({id: 1, method: 'Page.captureScreenshot'}));
        await fs.writeFile(path.join(output, 'compile-functions.png'), Buffer.from((await shot).data, 'base64')); socket.close();
        await vscode.commands.executeCommand('hideSuggestWidget');
        await replace('compile func divide(value: i32) -> i32:\n    return 8 / value\nconst answer = divide(0)\n');
        const errors = await until(diagnostics, ds => ds.some(d => d.range.start.line === 1 && /zero/i.test(d.message)), 'constant division diagnostic');
        report.cases.push({name: 'division diagnostic', diagnostics: errors});
        await replace(good + '\ncompile func unfinished(\n');
        await until(diagnostics, ds => ds.length > 0, 'incomplete declaration diagnostic');
        await replace(good); await until(diagnostics, ds => ds.length === 0, 'incomplete declaration repair');
        const tokens = await vscode.commands.executeCommand('vscode.provideDocumentSemanticTokens', uri);
        assert(tokens?.data.length); report.semantic_tokens = tokens.data.length;
        await replace('compile func spin() -> i32:\n    for:\n        pass\n    return 0\nconst busy = spin()\n');
        await replace(good);
        await until(() => hover('print(answe'), h => hoverText(h)?.includes('64'), 'fresh repaired hover');
        await until(diagnostics, ds => ds.length === 0, 'superseded constant evaluation');
        assert.equal(await fs.readFile(uri.fsPath, 'utf8'), original);
        report.disk_unchanged = true; report.status = 'PASS'; await save();
        console.log('G93 real editor: constants, methods, signature, generics, diagnostics, semantic tokens and recovery passed');
    } catch (error) {
        report.status = 'FAIL'; report.error = String(error.stack ?? error);report.buffer = doc.getText();report.diagnostics = diagnostics();await save();throw error;
    } finally {await replace(original);browser.close();await api.deactivate();}
};
