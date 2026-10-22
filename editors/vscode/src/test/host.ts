import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import cp = require('node:child_process');
import {promises as fs, appendFileSync, unlinkSync} from 'node:fs';

async function until<T>(read: () => T | PromiseLike<T>, valid: (value:T) => boolean, label: string): Promise<T> {
    const end = Date.now() + 20000;
    while (Date.now() < end) { const value = await read(); if(valid(value)) return value; await new Promise(r => setTimeout(r, 50)); }
    throw new Error('Timed out: ' + label);
}
function code(d: vscode.Diagnostic): unknown { return typeof d.code === 'object' ? d.code.value : d.code; }
export async function run(): Promise<void> {
    const root = process.env.VOID_TEST_ROOT!;
    // Real editor URI conversion normalizes only the Windows drive here; it
    // must not erase differences in directories, basenames or other drives.
    const driveUri = (drive:string, folder='CaseSensitive é😀', file='worker space.void') =>
        vscode.Uri.from({scheme:'file', path:`/${drive}:/${folder}/${file}`});
    assert.equal(driveUri('C').fsPath, driveUri('c').fsPath);
    assert.notEqual(driveUri('C').fsPath, driveUri('D').fsPath);
    assert.notEqual(driveUri('C').fsPath, driveUri('C', 'casesensitive é😀').fsPath);
    assert.notEqual(driveUri('C').fsPath, driveUri('C', undefined, 'other.void').fsPath);
    // runHost supplies the actual filesystem-canonical fixture root. Native
    // status paths use that root; editor URI.fsPath may spell its drive differently.
    const workerEntry = path.join(root, 'worker space.void');
    const mainEntry = path.join(root, 'main.void');
    const log = path.resolve(__dirname, '../../../../build/phase11-verification/host-details.log');
    await fs.mkdir(path.dirname(log), {recursive:true});
    const record = (message:string) => { console.log(message); appendFileSync(log, message + '\n'); };
    const mainUri = vscode.Uri.file(path.join(root, 'main.void'));
    const workerUri = vscode.Uri.file(path.join(root, 'worker space.void'));
    const children: cp.ChildProcess[] = [];
    const original = cp.spawn;
    let disappearBeforeSpawn: string | undefined;
    // Observe real child creation; no fake transport/server or process implementation.
    (cp as {spawn: typeof cp.spawn}).spawn = ((...args: Parameters<typeof cp.spawn>) => {
        if (String(args[0]) === disappearBeforeSpawn) { unlinkSync(disappearBeforeSpawn!); disappearBeforeSpawn = undefined; }
        const child = original(...args);
        if (String(args[0]) === process.env.VOIDLS_PATH || String(args[0]).includes('voidls')) {
            children.push(child); record('Native server launched: ' + JSON.stringify(args));
            child.once('exit', (code, signal) => record(`Native exit: code=${code}, signal=${signal}`));
            for (const [name, stream] of [['stdin', child.stdin], ['stdout', child.stdout]] as const)
                stream?.on('error', error => record(`Native ${name} error: ${String(error)}`));
            let chunks = 0; child.stdout?.on('data', data => { if(chunks++ < 20) record('Native stdout: ' + String(data)); });
            child.stderr?.on('data', data => console.log('Native stderr:', String(data)));
        }
        return child;
    }) as typeof cp.spawn;
    const unhandled: unknown[] = []; const rejected = (error:unknown) => { unhandled.push(error); record('Unhandled rejection: ' + String(error)); };
    process.on('unhandledRejection', rejected);
    const messages: string[] = [];
    // Installed extensions receive their own VS Code API object. Read the real
    // product output there; development tests also intercept actual UI reports.
    let readingReports = false;
    const reportTimer = process.env.VOID_TEST_VSIX ? setInterval(async () => {
        if (readingReports) return; readingReports = true;
        try {
            const logs = path.join(process.env.VOID_TEST_PROFILE!, 'logs');
            const files = (await fs.readdir(logs, {recursive:true})).filter(f => f.endsWith(path.join('void-language.void-language', 'VOID.log')));
            const content = (await Promise.all(files.map(f => fs.readFile(path.join(logs, f), 'utf8')))).join('\n');
            const reports = content.split('\n').map(line => line.split('[info] ')[1] ?? '').filter(line =>
                line.startsWith('VOID: ') || line.startsWith('VOID native analysis requires') || line.startsWith('VOID requires a filesystem') ||
                line.startsWith('VOID language server failed') || line.startsWith('VOID language server terminated'));
            messages.splice(0, messages.length, ...reports);
        } catch { /* The output directory appears after activation. */ }
        finally { readingReports = false; }
    }, 25) : undefined;
    const originalError = vscode.window.showErrorMessage;
    // Capture UI error reporting without modal notifications obstructing the test.
    (vscode.window as {showErrorMessage: typeof originalError}).showErrorMessage = ((message: string) => {
        messages.push(message); record('Error notification: ' + message); return Promise.resolve(undefined);
    }) as typeof originalError;
    const config = vscode.workspace.getConfiguration('void');
    const extension = vscode.extensions.getExtension('void-language.void-language'); assert(extension);
    if (process.env.VOID_TEST_VSIX) assert(extension.extensionPath.includes(path.sep + 'extensions' + path.sep), 'Test the installed VSIX');
    const gone = (child:cp.ChildProcess) => child.pid === undefined || child.exitCode !== null || child.signalCode !== null;
    let api: {ready():Promise<void>; status():{state:string;entry?:string}; deactivate():Promise<void>} | undefined;
    try {
        console.log('Editor test begins:', process.env.VOID_TEST_MODE);
        const main = await vscode.workspace.openTextDocument(mainUri); await vscode.window.showTextDocument(main);
        api = await extension.activate(); assert(api); await api!.ready(); record('Activation ready: ' + JSON.stringify(api!.status()));
        record('Editor diagnostics: ' + JSON.stringify(vscode.languages.getDiagnostics(mainUri)));
        if (process.env.VOID_TEST_VSIX && process.env.VOID_TEST_MODE !== 'trusted') await until(() => messages.length, n => n > 0, 'installed restriction report');
        if (process.env.VOID_TEST_MODE === 'multi-root') {
            assert.equal(vscode.workspace.workspaceFolders?.length, 2);
            assert.equal(children.length, 0); assert.equal(api!.status().state, 'stopped');
            assert(messages.some(m => m.includes('one project')));
            console.log('PASS real editor multi-root: explanatory error, no native child spawned'); return;
        }
        if (process.env.VOID_TEST_MODE === 'untrusted') {
            assert.equal(vscode.workspace.isTrusted, false);
            await vscode.commands.executeCommand('void.restartLanguageServer');
            assert.equal(children.length, 0); assert.equal(api!.status().state, 'stopped');
            assert(messages.some(m => m.includes('trusted workspace')));
            console.log('PASS real editor restricted trust: no native child spawned'); return;
        }
        assert(vscode.workspace.isTrusted);
        if(process.platform==='win32') {
            // Exercise the exact native discovery inputs supplied by the editor,
            // including URI.fsPath's drive spelling and the Unicode/#/% fixture.
            const workspace=vscode.workspace.workspaceFolders![0].uri.fsPath;
            const active=mainUri.fsPath;
            for(const letter of [workspace[0].toUpperCase(),workspace[0].toLowerCase()]) {
                const selected=JSON.parse(cp.execFileSync(process.env.VOIDLS_PATH!,
                    ['--discover',letter+workspace.slice(1),active,''],{encoding:'utf8',timeout:10000}));
                assert.equal(path.normalize(selected.root),path.normalize(root));
                assert.equal(path.normalize(selected.entry),path.normalize(mainEntry));
                assert.equal(selected.standalone,false);
            }
            record('PASS real Windows editor discovery: upper/lower workspace drives with actual URI.fsPath');
        }
        await until(() => api!.status().state, s => s === 'running', 'initial server');
        assert.equal(children.length, 1);
        // Definition request flushes full-document synchronization through the client.
        const point = new vscode.Position(3, 18);
        const definitions = () => vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', mainUri, point);
        const definitionsValid = await until(definitions, d => !!d?.length, 'imported definition');
        assert.equal(definitionsValid![0].uri.toString(), workerUri.toString());
        assert.equal(definitionsValid![0].range.start.line, 0); assert.equal(definitionsValid![0].range.start.character, 5);
        assert.equal(vscode.languages.getDiagnostics(mainUri).length, 0);
        const references = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider', mainUri, point);
        assert.equal(references?.length, 2);
        const worker = await vscode.workspace.openTextDocument(workerUri); await vscode.window.showTextDocument(worker);
        const originalText = worker.getText();
        async function replace(document:vscode.TextDocument, text:string):Promise<void> {
            const edit = new vscode.WorkspaceEdit(); edit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), text);
            assert(await vscode.workspace.applyEdit(edit));
        }
        const mainText = main.getText();
        const mainEditor=await vscode.window.showTextDocument(main);
        const typingChild=children[children.length-1], typingCount=children.length;
        for(const fragment of ['u','us','unknown','var pending =','return (','func pending(','use "']) {
            await vscode.commands.executeCommand('hideSuggestWidget');
            await replace(main,mainText);mainEditor.selection=new vscode.Selection(0,0,0,0);
            await vscode.commands.executeCommand('type',{text:fragment+'\n'});
            assert(main.getText().startsWith(fragment+'\n'),'real incomplete edit was not applied');
            const menu=await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',mainUri,new vscode.Position(0,fragment.length));
            assert(!menu || menu.items.every(i=>!(typeof i.label==='string'?i.label:i.label.label).includes('$')),'synthetic completion leaked');
            assert.equal(api!.status().state,'running');assert.equal(children.length,typingCount);
            assert(!gone(typingChild),'incomplete typing killed native server');
            await replace(main,mainText);
            await until(definitions,d=>!!d?.length,'valid request after incomplete '+fragment);
            await until(()=>vscode.languages.getDiagnostics(mainUri),ds=>ds.length===0,'typing diagnostic repair');
        }
        record('PASS real typing: incomplete top-level keywords/expressions/imports recover in the same native process');
        for(const [prefix,labels] of [['use lib/',['raylib','raylib/']],['use lib/raylib/',['graphics','raylib']]] as const) {
            await vscode.commands.executeCommand('hideSuggestWidget');
            await replace(main,'');mainEditor.selection=new vscode.Selection(0,0,0,0);
            await vscode.commands.executeCommand('type',{text:prefix});
            const menu=await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',mainUri,new vscode.Position(0,prefix.length),'/');
            const names=menu?.items.map(i=>typeof i.label==='string'?i.label:i.label.label) ?? [];
            assert(labels.every(label=>names.includes(label)),'local library import suggestions '+prefix);
        }
        await replace(main,'use lib/raylib as rl\nfunc entry(x: i32) -> i32:\n    return rl.read(x)\n');
        const localDefinition=await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',mainUri,new vscode.Position(2,16)),d=>!!d?.length,'local library definition');
        assert.equal(localDefinition![0].uri.toString(),vscode.Uri.file(path.join(root,'libs/raylib/raylib.void')).toString());
        const localHover=await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',mainUri,new vscode.Position(2,16));
        assert(localHover?.some(h=>h.contents.some(c=>typeof c==='object' && 'value' in c && c.value.replace(/&nbsp;/g,' ').includes('Local editor library'))),
            JSON.stringify(localHover?.map(h=>h.contents.map(c=>typeof c==='object' && 'value' in c?c.value:c))));
        await replace(main,mainText);assert.equal((await definitions())?.length,1);
        assert.equal(children.length,typingCount);assert(!gone(typingChild));
        record('PASS real editor local libraries: typed imports, nested completion, source definition, docs and same-server repair');
        for (const [prefix,expected] of [['use ','use io/'],['use io/','use io/files'],['use "','use ".vscode/'],['use "app/','use "app/systems/'],['use "app/systems/','use "app/systems/player.void"']]) {
            await vscode.commands.executeCommand('hideSuggestWidget');
            await replace(main,'');mainEditor.selection=new vscode.Selection(0,0,0,0);
            await vscode.commands.executeCommand('type',{text:prefix});
            await new Promise(resolve=>setTimeout(resolve,750));
            await vscode.commands.executeCommand('acceptSelectedSuggestion');
            assert.equal(main.getText(),expected,'automatic import suggestions '+prefix);
        }
        for (const [prefix,label] of [['use ','io/'],['use io/','files'],['use "','app/'],['use "app/','systems/'],['use "app/systems/','player.void']]) {
            await replace(main,prefix);
            const menu=await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',mainUri,new vscode.Position(0,prefix.length),prefix.slice(-1));
            const item=menu?.items.find(i=>(typeof i.label==='string'?i.label:i.label.label)===label);
            assert(item?.textEdit,'nested import '+prefix);
            const edit=new vscode.WorkspaceEdit();edit.set(mainUri,[item.textEdit]);assert(await vscode.workspace.applyEdit(edit));
            if(label==='player.void')assert.equal(main.getText(),'use "app/systems/player.void"');
        }
        await replace(main,'use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.read(x)\n');
        const documented=await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',mainUri,new vscode.Position(2,19));
        assert(documented?.some(h=>h.contents.some(c=>typeof c==='object' && 'value' in c && c.value.includes('fixture'))), JSON.stringify(documented));
        const standard=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',mainUri,new vscode.Position(2,19));
        assert(standard?.[0].uri.fsPath.endsWith(path.join('standard','io','files.void')));
        await replace(main,mainText);
        const hover = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', mainUri, point);
        assert(hover?.some(h => h.contents.some(c => typeof c === 'object' && 'value' in c && c.value.includes('calculate'))));
        const symbols = await vscode.commands.executeCommand<vscode.DocumentSymbol[]>('vscode.executeDocumentSymbolProvider', mainUri);
        assert(symbols?.some(s => s.name === 'entry' && s.range.end.line >= 3));
        const workspaceSymbols = await vscode.commands.executeCommand<vscode.SymbolInformation[]>('vscode.executeWorkspaceSymbolProvider', 'calc');
        assert(workspaceSymbols?.some(s => s.name === 'calculate'));
        await replace(main, mainText.replace('worker.calculate(x)', 'worker.'));
        const completion = await vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider', mainUri, new vscode.Position(3, 18), '.');
        const candidate = completion?.items.find(i => (typeof i.label === 'string' ? i.label : i.label.label) === 'calculate');
        assert(candidate?.textEdit);
        const selected = new vscode.WorkspaceEdit(); selected.set(mainUri, [candidate.textEdit]);
        assert(await vscode.workspace.applyEdit(selected)); assert(main.getText().includes('worker.calculate'));
        await replace(main, mainText.replace('worker.calculate(x)', 'worker.calculate('));
        const signature = await vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider', mainUri, new vscode.Position(3, 28), '(');
        assert(signature?.signatures[0].label.includes('x: i32'));
        await replace(main, mainText);
        assert.equal((await definitions())?.length, 1);
        const rename = await vscode.commands.executeCommand<vscode.WorkspaceEdit>('vscode.executeDocumentRenameProvider', workerUri, new vscode.Position(0, 7), 'compute');
        assert(rename && await vscode.workspace.applyEdit(rename));
        assert(main.getText().includes('worker.compute(x)') && worker.getText().includes('func compute'));
        await replace(worker, originalText); await replace(main, mainText);
        assert.equal((await definitions())?.length, 1);
        await replace(worker, originalText.replace('    return x + 42', '  return x + 42'));
        const formatting = await vscode.commands.executeCommand<vscode.TextEdit[]>('vscode.executeFormatDocumentProvider', workerUri, {tabSize: 4, insertSpaces: true});
        assert(formatting?.length === 1);
        const formatEdit = new vscode.WorkspaceEdit(); formatEdit.set(workerUri, formatting); assert(await vscode.workspace.applyEdit(formatEdit));
        assert.equal(worker.getText(), originalText);
        assert.equal((await vscode.commands.executeCommand<vscode.TextEdit[]>('vscode.executeFormatDocumentProvider', workerUri, {tabSize: 4, insertSpaces: true}))?.length ?? 0, 0);
        await vscode.commands.executeCommand('void.refreshProject');
        assert.equal((await definitions())?.length, 1);
        await replace(worker, originalText.replace('x + 42', 'missing'));
        const diagnostic = await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.some(d => code(d) === 'VOID2002'), 'unsaved unknown identifier');
        const unknown = diagnostic.find(d => code(d) === 'VOID2002')!;
        assert.equal(unknown.severity, vscode.DiagnosticSeverity.Error);
        assert.equal(unknown.range.start.line, 1); assert.equal(unknown.range.start.character, 11); assert.equal(unknown.range.end.character, 18);
        assert(worker.isDirty); assert((await fs.readFile(workerUri.fsPath, 'utf8')).includes('x + 42'));
        assert.equal((await definitions())?.length ?? 0, 0);
        assert.equal((await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider', mainUri, point))?.length ?? 0, 0);
        await replace(worker, originalText);
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'diagnostic clearing');
        assert.equal((await definitions())?.[0].uri.toString(), workerUri.toString());
        const withStrings = originalText.replace('    return x + 42', '    const text: string = "é🎮 # return {{braces}}"\n    return x + 42');
        assert.notEqual(withStrings, originalText);
        await replace(worker, withStrings);
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'valid UTF-8 string source');
        assert.equal((await definitions())?.[0].uri.toString(), workerUri.toString());
        await replace(worker, withStrings.replace('{{braces}}', '{"braces"}'));
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'valid string interpolation');
        assert.equal((await definitions())?.[0].uri.toString(), workerUri.toString());
        await replace(worker, withStrings.replace('{{braces}}', '{braces}'));
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.some(d => code(d)==='VOID2002' && d.message.includes('braces')), 'unknown interpolation identifier diagnostic');
        assert.equal((await definitions())?.length ?? 0, 0);
        await replace(worker, originalText);
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'string edit repair');
        assert.equal((await definitions())?.[0].uri.toString(), workerUri.toString());
        const stage10 = originalText.replace('    return x + 42',
            '    var b = stage10_box()\n    var shown = string(x)\n    var text = """Score:\n        {x:000}\n    """\n    return x + 42') +
            '\nstruct stage10_box:\n    private secret: i32 = 7\n    const id: i32\n';
        await replace(worker, stage10);
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'Stage 10 primitive display and multiline interpolation');
        const conversionHover = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', workerUri, worker.positionAt(stage10.indexOf('string(x)') + 2));
        assert(conversionHover?.some(h => h.contents.some(c => typeof c === 'object' && 'value' in c && c.value.replace(/&nbsp;/g, ' ').includes('value: i32'))), 'Stage 10 conversion hover');
        const holeDefinition = await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider', workerUri, worker.positionAt(stage10.indexOf('{x:000}') + 1));
        assert(holeDefinition?.length, 'Stage 10 original interpolation expression span');
        await replace(worker, stage10.replace('{x:000}', '{x:0.#0}'));
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.some(d => d.message.includes('Required fraction')), 'Stage 10 literal format diagnostic');
        await replace(worker, stage10.replace('    return x + 42', '    return b.secret'));
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.some(d => d.message.includes('not accessible')), 'Stage 10 private field diagnostic');
        await replace(worker, stage10.replace('    return x + 42', '    return b.id'));
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'Stage 10 field repair');
        const fieldText = worker.getText();
        const fieldHover = await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider', workerUri, worker.positionAt(fieldText.indexOf('b.id') + 2));
        assert(fieldHover?.some(h => h.contents.some(c => typeof c === 'object' && 'value' in c && c.value.replace(/&nbsp;/g, ' ').includes('const id'))), 'Stage 10 const field hover');
        await replace(worker, originalText);
        await until(() => vscode.languages.getDiagnostics(workerUri), ds => ds.length === 0, 'Stage 10 editor repair');
        record('Before save'); await worker.save(); record('After save');
        record('Before close'); await vscode.commands.executeCommand('workbench.action.closeAllEditors'); record('After close');
        assert(vscode.window.tabGroups.all.every(group => group.tabs.length === 0));
        // VS Code may retain a closed tab's text model. Language reassignment is
        // documented to close/reopen TextDocuments and exercises real LSP close/open.
        let closed = false; const closeEvent = vscode.workspace.onDidCloseTextDocument(d => { if(d.uri.toString() === workerUri.toString()) closed = true; });
        await vscode.languages.setTextDocumentLanguage(worker, 'plaintext');
        await until(() => closed, Boolean, 'real TextDocument close event'); closeEvent.dispose();
        const reopened = await vscode.workspace.openTextDocument(workerUri);
        await vscode.languages.setTextDocumentLanguage(reopened, 'void');
        record('Document close/open events verified');
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(mainUri));
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(workerUri));
        assert.equal((await definitions())?.[0].uri.toString(), workerUri.toString());
        console.log('Diagnostics/navigation/unsaved repair/close-reopen passed');
        const first = children[0]; await vscode.commands.executeCommand('void.restartLanguageServer');
        await until(() => gone(first), Boolean, 'restart child cleanup');
        assert.equal(children.length, 2); assert.equal(first.exitCode, 0);
        await config.update('project.entry', 'second.void', vscode.ConfigurationTarget.Workspace);
        await until(() => api!.status().entry, e => !!e?.endsWith('second.void'), 'configured entry change');
        await until(() => gone(children[1]), Boolean, 'entry change cleanup');
        assert.equal(messages.length, 0, 'Normal initialization/restart/entry switch must not warn');
        const beforeMissing = children.length;
        await config.update('server.path', path.join(root, 'missing-voidls'), vscode.ConfigurationTarget.Workspace);
        await until(() => api!.status().state, s => s === 'stopped', 'missing executable');
        await until(() => messages.some(m => m.includes('Cannot find voidls')), Boolean, 'missing executable report');
        assert.equal(children.length, beforeMissing);
        await config.update('server.path', process.env.VOIDLS_PATH, vscode.ConfigurationTarget.Workspace);
        await config.update('project.entry', 'main.void', vscode.ConfigurationTarget.Workspace);
        await api!.ready(); assert.equal(api!.status().state, 'running');
        // The real binary disappears after preflight: exercise an actual OS spawn
        // failure without a fake server/transport or substituting compiler behavior.
        const disappearing = path.join(root, process.platform === 'win32' ? 'voidls disappears.exe' : 'voidls disappears');
        await fs.copyFile(process.env.VOIDLS_PATH!, disappearing); await fs.chmod(disappearing, 0o755);
        disappearBeforeSpawn = disappearing;
        await config.update('server.path', disappearing, vscode.ConfigurationTarget.Workspace);
        await api!.ready();
        await until(() => messages.some(m => m.includes('failed to start')), Boolean, 'startup OS failure report');
        assert.equal(api!.status().state, 'stopped');
        await config.update('server.path', process.env.VOIDLS_PATH, vscode.ConfigurationTarget.Workspace);
        await api!.ready(); assert.equal(api!.status().state, 'running');
        // Exercise error-before-close deterministically on every OS using the
        // actual child's transport and the real language client/native server.
        // This is a controlled stream error, not a Windows OS reproduction.
        const transportChild = children[children.length - 1];
        const terminationCount = () => messages.filter(m => m.includes('terminated unexpectedly')).length;
        assert.equal(terminationCount(), 0, 'Intentional stops/startup failures must not report termination');
        const beforeTransportMessages = messages.length;
        const streamError = Object.assign(new Error('Focused transport failure before close'), {code:'EPIPE'});
        assert(transportChild.stdin); transportChild.stdin.emit('error', streamError);
        transportChild.stdin.emit('error', streamError); // Repeated errors still notify once.
        await new Promise(r => setTimeout(r, 50));
        assert.equal(api!.status().state, 'running', 'A stream error alone is not termination');
        assert.equal(messages.length, beforeTransportMessages, 'Transport errors should log without false termination popups');
        transportChild.kill(); // Actual process termination closes the real transport.
        await until(() => api!.status().state, s => s === 'stopped', 'transport error shutdown');
        await until(() => terminationCount(), n => n === 1, 'transport error termination report');
        await until(() => gone(transportChild), Boolean, 'transport error child cleanup');
        const transportCount = children.length;
        await new Promise(r => setTimeout(r, 200)); assert.equal(children.length, transportCount);
        assert.equal(messages.length, beforeTransportMessages + 1, 'Transport failure must notify exactly once');
        await vscode.commands.executeCommand('void.restartLanguageServer');
        assert.equal(api!.status().state, 'running');
        const beforeKillMessages = messages.length;
        const active = children[children.length - 1]; active.kill();
        await until(() => api!.status().state, s => s === 'stopped', 'unexpected termination');
        await until(() => terminationCount(), n => n === 2, 'termination report');
        const count = children.length; await new Promise(r => setTimeout(r, 200)); assert.equal(children.length, count);
        assert.equal(messages.length, beforeKillMessages + 1, 'Killed process must notify exactly once');
        const afterKillMessages = messages.length;
        await vscode.commands.executeCommand('void.restartLanguageServer'); assert.equal(api!.status().state, 'running');
        // No entry discovery: fallback is fixed until explicitly restarted.
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(workerUri));
        const mainDocument = await vscode.workspace.openTextDocument(mainUri);
        await vscode.languages.setTextDocumentLanguage(mainDocument, 'plaintext');
        assert.equal(vscode.workspace.textDocuments.filter(d => d.languageId === 'void').length, 1);
        const outsideWarnings = () => messages.filter(m => m.includes('outside the selected project')).length;
        const beforeFallbackWarnings = outsideWarnings();
        assert.equal(beforeFallbackWarnings, 0, 'Warning deduplication must not mask the selected-file check');
        await fs.rename(mainEntry,mainEntry+'.saved');
        await config.update('project.entry', '', vscode.ConfigurationTarget.Workspace);
        await api!.ready(); assert.equal(path.normalize(api!.status().entry!), path.normalize(workerEntry));
        assert.equal(outsideWarnings(), beforeFallbackWarnings, 'The selected fallback file must not warn about itself');
        await fs.rename(mainEntry+'.saved',mainEntry);
        await vscode.languages.setTextDocumentLanguage(mainDocument, 'void');
        await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(mainUri));
        await api!.ready(); assert.equal(path.normalize(api!.status().entry!), path.normalize(workerEntry));
        await vscode.commands.executeCommand('void.restartLanguageServer');
        assert.equal(path.normalize(api!.status().entry!), path.normalize(mainEntry));
        await config.update('project.entry', 'main.void', vscode.ConfigurationTarget.Workspace); await api!.ready();
        // Calling the actual extension deactivation hook tests clean child shutdown.
        await api?.deactivate();
        await until(() => children.every(gone), Boolean, 'deactivation cleanup');
        assert.equal(children[children.length - 1].exitCode, 0);
        assert.equal(terminationCount(), 2, 'Exactly one report per unexpected failure; none for intentional shutdown');
        // Single-file fallback can legitimately warn about other open documents.
        assert(messages.slice(afterKillMessages).every(m => m.includes('outside the selected project')),
            'Intentional restart/configuration/deactivation must not emit failure notifications');
        assert.equal(unhandled.length, 0, 'Unhandled client rejection');
        console.log('PASS real editor: VOID2002 unsaved repair/ranges, definition/references, no stale queries, close/reopen, restart/configuration/failure/deactivation cleanup');
    } catch (error) { record('Editor test failure: ' + String(error) + (error instanceof Error ? '\n' + error.stack : '')); throw error; } finally {
        if (reportTimer) clearInterval(reportTimer);
        process.off('unhandledRejection', rejected);
        (cp as {spawn: typeof cp.spawn}).spawn = original;
        (vscode.window as {showErrorMessage: typeof originalError}).showErrorMessage = originalError;
        await api?.deactivate();
        try { await until(() => children.every(gone), Boolean, 'final child cleanup'); }
        finally { for (const child of children) if (!gone(child)) child.kill(); }
    }
}
