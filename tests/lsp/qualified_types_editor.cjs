// Real VS Code qualification using an installed VSIX and isolated profile.
const fs = require('node:fs/promises');
const path = require('node:path');
const net = require('node:net');
const {execFileSync} = require('node:child_process');
const assert = require('node:assert/strict');
const extension = path.resolve(__dirname, '../../editors/vscode');
const fixture = path.join(__dirname, 'fixtures/qualified_types.void');

exports.launch = async function () {
    for (const key of ['VOID_QUALIFIED_EDITOR_OUTPUT', 'VOID_QUALIFIED_PROJECT', 'VOID_QUALIFIED_VSIX',
        'VOIDLS_PATH', 'VOIDC_PATH', 'VSCODE_EXECUTABLE_PATH']) assert(process.env[key], key);
    const output = path.resolve(process.env.VOID_QUALIFIED_EDITOR_OUTPUT);
    await fs.mkdir(output);
    const project = path.join(output, 'project');
    await fs.cp(process.env.VOID_QUALIFIED_PROJECT, project, {recursive: true});
    await fs.copyFile(fixture, path.join(project, 'app.void'));
    const profile = path.join(output, 'profile');
    await fs.mkdir(path.join(profile, 'User'), {recursive: true});
    await fs.writeFile(path.join(profile, 'User/settings.json'), JSON.stringify({
        'security.workspace.trust.enabled': false, 'workbench.startupEditor': 'none',
        'telemetry.telemetryLevel': 'off', 'update.mode': 'none', 'extensions.autoCheckUpdates': false,
        'files.hotExit': 'off', 'editor.wordBasedSuggestions': 'off', 'editor.semanticHighlighting.enabled': true,
        'void.server.path': path.resolve(process.env.VOIDLS_PATH), 'void.compiler.path': path.resolve(process.env.VOIDC_PATH),
        'void.project.root': project, 'void.trace.server': 'verbose'}));
    const extensions = path.join(output, 'extensions');
    execFileSync(process.env.VSCODE_CLI_PATH || 'code', ['--install-extension', path.resolve(process.env.VOID_QUALIFIED_VSIX),
        '--force', '--user-data-dir=' + profile, '--extensions-dir=' + extensions], {stdio: 'inherit'});
    const harness = path.join(output, 'harness'); await fs.mkdir(harness);
    await fs.writeFile(path.join(harness, 'package.json'), JSON.stringify({name: 'qualified-types-harness', publisher: 'void-language',
        version: '0.0.1', engines: {vscode: '^1.141.0'}, main: './index.js'}));
    await fs.writeFile(path.join(harness, 'index.js'), 'exports.activate = () => {};');
    const port = await new Promise(resolve => { const s = net.createServer(); s.listen(0, '127.0.0.1', () => {
        const p = s.address().port; s.close(() => resolve(p)); }); });
    const {runTests} = require(path.join(extension, 'node_modules/@vscode/test-electron'));
    await runTests({vscodeExecutablePath: process.env.VSCODE_EXECUTABLE_PATH, extensionDevelopmentPath: harness,
        extensionTestsPath: __filename, extensionTestsEnv: {VOID_QUALIFIED_PROJECT: project, VOID_QUALIFIED_PORT: String(port)},
        launchArgs: [project, '--user-data-dir=' + profile, '--extensions-dir=' + extensions, '--disable-updates',
            '--disable-gpu', '--skip-welcome', '--skip-release-notes', '--remote-debugging-port=' + port]});
};

exports.run = async function () {
    const vscode = require('vscode');
    const {until, workbench} = require(path.join(extension, 'out/test/creationWorkbench.js'));
    const output = process.env.VOID_QUALIFIED_EDITOR_OUTPUT;
    const uri = vscode.Uri.file(path.join(process.env.VOID_QUALIFIED_PROJECT, 'app.void'));
    const doc = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(doc);
    const installed = vscode.extensions.getExtension('void-language.void-language');
    const api = await installed.activate();
    const browser = await workbench(process.env.VOID_QUALIFIED_PORT);
    const good = doc.getText();
    const report = {version: vscode.version, extension: installed.extensionPath, server: process.env.VOIDLS_PATH,
        compiler: process.env.VOIDC_PATH, cases: []};
    const save = () => fs.writeFile(path.join(output, 'results.json'), JSON.stringify(report, null, 2));
    const replace = async text => { const edit = new vscode.WorkspaceEdit(); edit.replace(uri,
        new vscode.Range(doc.positionAt(0), doc.positionAt(doc.getText().length)), text); assert(await vscode.workspace.applyEdit(edit)); };
    const at = needle => doc.positionAt(doc.getText().lastIndexOf(needle) + needle.length);
    const label = item => typeof item.label === 'string' ? item.label : item.label.label;
    const complete = (needle, first = false) => vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri,
        first ? doc.positionAt(doc.getText().indexOf(needle) + needle.length) : at(needle));
    const rendered = () => browser.evaluate(`[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row')]
        .filter(e => e.checkVisibility({visibilityProperty:true,opacityProperty:true})).map(e => e.innerText)`);
    const decode = (tokens, legend) => {
        const result = []; let line = 0, column = 0;
        for (let i = 0; tokens && i < tokens.data.length; i += 5) {
            line += tokens.data[i]; column = tokens.data[i] ? tokens.data[i + 1] : column + tokens.data[i + 1];
            result.push({word: doc.lineAt(line).text.slice(column, column + tokens.data[i + 2]), role: legend.tokenTypes[tokens.data[i + 3]]});
        }
        return result;
    };
    try {
        await api.ready();
        await until(() => vscode.commands.executeCommand('vscode.executeHoverProvider', uri, at('value: rl.vector')),
            result => result?.length, 'typed return/parameter hover');
        assert.equal(vscode.languages.getDiagnostics(uri).length, 0);
        const definitions = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, at('value: rl.vector'));
        assert(definitions.some(d => d.uri.fsPath.endsWith('/raylib_native.void')), definitions);
        report.definition = definitions.map(d => ({path: d.uri.fsPath, line: d.range.start.line + 1}));
        const legend = await vscode.commands.executeCommand('vscode.provideDocumentSemanticTokensLegend', uri);
        const getTokens = () => vscode.commands.executeCommand('vscode.provideDocumentSemanticTokens', uri);
        const prefix = good.slice(0, good.indexOf('func main'));
        for (const expression of ['rl.', 'if rl.', 'for not rl.', 'rl.vec', 'point.']) {
            const wanted = expression === 'point.' ? 'x' : 'vector2';
            await replace(prefix + 'func main():\n    var point: rl.vector2 = rl.vector2(3, 4)\n    ' + expression + '\n');
            const result = await until(() => complete(expression), r => r?.items.some(i => label(i) === wanted), expression);
            const tokens = await until(getTokens, t => decode(t, legend).some(t => t.word === 'point' && t.role === 'variable'), expression + ' coloring');
            const decoded = decode(tokens, legend);
            assert(decoded.some(t => t.word === 'vector2' && t.role === 'struct'));
            const p = at(expression); editor.selection = new vscode.Selection(p, p); editor.revealRange(new vscode.Range(p, p));
            await vscode.commands.executeCommand('workbench.action.focusActiveEditorGroup');
            await vscode.commands.executeCommand('editor.action.triggerSuggest');
            const visible = await until(rendered, rows => rows.some(row => result.items.some(item => row.includes(label(item)))), expression + ' rendered suggestions');
            if (expression === 'for not rl.') {
                const capture = await browser.screenshot();
                await fs.writeFile(path.join(output, 'qualified-types.png'), Buffer.from(capture.data, 'base64'));
            }
            await vscode.commands.executeCommand('hideSuggestWidget');
            assert.equal(api.status().state, 'running');
            report.cases.push({expression, completions: result.items.map(label), rendered: visible, tokens: decoded}); await save();
        }
        for (const [from, needle] of [['value: rl.vector2', 'value: rl.'], ['-> rl.vector2', '-> rl.'], ['local: rl.vector2', 'local: rl.']]) {
            await replace(good.replace(from, needle));
            const result = await until(() => complete(needle, true), r => ['vector2', 'rectangle', 'color'].every(name => r?.items.some(i => label(i) === name)), needle);
            report.cases.push({annotation: needle, completions: result.items.map(label)}); await save();
        }
        await replace(prefix + 'func main():\n    ');
        const end = doc.positionAt(doc.getText().length); editor.selection = new vscode.Selection(end, end);
        for (const text of ['rl', '.']) await vscode.commands.executeCommand('type', {text});
        await until(rendered, rows => rows.length > 0, 'automatic dot popup');
        report.automatic_dot_popup = true;
        await vscode.commands.executeCommand('hideSuggestWidget');
        await replace(good);
        await until(() => vscode.languages.getDiagnostics(uri), ds => ds.length === 0, 'repaired diagnostics');
        assert.equal(api.status().state, 'running');
        assert.equal(await fs.readFile(uri.fsPath, 'utf8'), good);
        report.status = 'PASS'; report.disk_unchanged = true; await save();
        console.log('Qualified types real VS Code: installed VSIX, native definitions, coloring, rendered/automatic completion and repair PASS');
    } catch (error) {
        report.status = 'FAIL'; report.error = String(error.stack || error); report.buffer = doc.getText();
        report.client = api.status(); report.diagnostics = vscode.languages.getDiagnostics(uri); await save(); throw error;
    } finally { await replace(good); browser.close(); await api.deactivate(); }
};
