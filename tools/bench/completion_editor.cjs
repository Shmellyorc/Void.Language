// G19 native VS Code replay using the existing extension and workbench helpers.
// Invoke launch() through a native Node runtime; run() is loaded by VS Code.
const path = require('node:path');
const fs = require('node:fs/promises');
const os = require('node:os');
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const root = path.resolve(__dirname, '../..');
const extension = path.join(root, 'editors/vscode');

exports.launch = async function () {
    const {runTests} = require(path.join(extension, 'node_modules/@vscode/test-electron'));
    for (const name of ['VOIDC_PATH','VOIDLS_PATH','VSCODE_EXECUTABLE_PATH','VOID_G19_EDITOR_OUTPUT','VOID_TEST_CDP_PORT'])
        assert(process.env[name], 'Set '+name);
    const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'VOID G19 editor '));
    try {
        const project = path.join(temp, 'project');
        await fs.cp(path.join(__dirname, 'compiler_project'), project, {recursive:true});
        execFileSync(process.env.VOIDC_PATH, ['resolve','--project',project], {encoding:'utf8'});
        await fs.mkdir(path.join(project,'.vscode'));
        await fs.writeFile(path.join(project,'.vscode/settings.json'), JSON.stringify({
            'void.server.path':process.env.VOIDLS_PATH, 'void.project.root':'.', 'void.trace.server':'off'}));
        const profile=path.join(temp,'profile');await fs.mkdir(path.join(profile,'User'),{recursive:true});
        await fs.writeFile(path.join(profile,'User/settings.json'), JSON.stringify({
            'security.workspace.trust.enabled':false,'workbench.startupEditor':'none',
            'telemetry.telemetryLevel':'off','update.mode':'none','files.hotExit':'off',
            'editor.wordBasedSuggestions':'off'}));
        await runTests({vscodeExecutablePath:process.env.VSCODE_EXECUTABLE_PATH,
            extensionDevelopmentPath:extension,extensionTestsPath:__filename,
            extensionTestsEnv:{VOID_G19_PROJECT:project},
            launchArgs:[project,'--user-data-dir='+profile,'--extensions-dir='+path.join(temp,'extensions'),
                '--disable-extensions','--disable-updates','--disable-gpu','--skip-welcome','--skip-release-notes',
                '--remote-debugging-port='+process.env.VOID_TEST_CDP_PORT]});
    } finally {await fs.rm(temp,{recursive:true,force:true,maxRetries:5,retryDelay:100});}
};

exports.run = async function () {
    const vscode = require('vscode');
    const {until,workbench} = require(path.join(extension,'out/test/creationWorkbench.js'));
    const project=process.env.VOID_G19_PROJECT;
    const stock=vscode.Uri.file(path.join(project,'src/stock.void'));
    const disk=await fs.readFile(stock.fsPath,'utf8');
    const dep=path.join(project,'deps/rates/src/api.void');const original=await fs.readFile(dep,'utf8');
    const api=await vscode.extensions.getExtension('void-language.void-language').activate();
    const browser=await workbench(process.env.VOID_TEST_CDP_PORT);
    try {
        const doc=await vscode.workspace.openTextDocument(stock);
        const editor=await vscode.window.showTextDocument(doc);await api.ready();
        const at=(needle,extra)=>doc.positionAt(doc.getText().indexOf(needle)+extra);
        const complete=async pos=>(await vscode.commands.executeCommand('vscode.executeCompletionItemProvider',stock,pos))?.items??[];
        const labels=items=>items.map(i=>typeof i.label==='string'?i.label:i.label.label);
        const member=()=>complete(at('item.units',5));
        await until(member,x=>labels(x).includes('unit_price'),'G08 member completion');
        const scopeLabels=labels(await complete(at('return item',7)));
        assert(scopeLabels.includes('discount'));
        assert(scopeLabels.includes('rates') && scopeLabels.includes('bulk_threshold'));
        // Record the current namespace-member limitation; do not mistake it
        // for an experimental reuse regression or silently claim it works.
        const importedMemberLabels=labels(await complete(at('rates.bulk_threshold',6)));
        const times=[];
        for(let i=0;i<10;++i){const start=performance.now();const items=await member();times.push(performance.now()-start);assert(labels(items).includes('units'));}
        let pos=at('item.units',5);editor.selection=new vscode.Selection(pos,pos);editor.revealRange(new vscode.Range(pos,pos));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(()=>browser.visible('unit_price','.suggest-widget.visible .monaco-list-row'),n=>n>0,'rendered member popup');
        await vscode.commands.executeCommand('hideSuggestWidget');
        const replace=async text=>{const e=new vscode.WorkspaceEdit();e.replace(stock,new vscode.Range(doc.positionAt(0),doc.positionAt(doc.getText().length)),text);assert(await vscode.workspace.applyEdit(e));};
        await replace(disk.replace('item.units','item.'));
        pos=at('item.',5);await until(()=>complete(pos),x=>labels(x).includes('unit_price'),'dirty incomplete member');
        assert(doc.isDirty);assert.equal(await fs.readFile(stock.fsPath,'utf8'),disk);
        await replace(disk);await until(member,x=>labels(x).includes('units'),'restored member');
        assert(original.includes('return 2'));
        await fs.writeFile(dep,original.replace('return 2','return 3'));
        await vscode.commands.executeCommand('void.refreshProject');
        await until(member,x=>x.length===0,'changed locked dependency refusal');
        await fs.writeFile(dep,original);await until(member,x=>labels(x).includes('units'),'restored dependency');
        const sorted=[...times].sort((a,b)=>a-b);
        await fs.writeFile(process.env.VOID_G19_EDITOR_OUTPUT,JSON.stringify({version:vscode.version,
            query_ms:times,median_ms:(sorted[4]+sorted[5])/2,worst_ms:sorted[9],imported_namespace_member_labels:importedMemberLabels,
            checks:['local','imported','member','rendered popup','dirty incomplete','unchanged disk','dependency refusal','dependency restore'],
            boundary:'VS Code provider command latency; excludes rendered-popup polling'},null,2));
        console.log('g19_editor: frozen-project completion, rendered popup, dirty buffers and dependency authority passed');
    } finally {await fs.writeFile(dep,original);browser.close();await api.deactivate();}
};
