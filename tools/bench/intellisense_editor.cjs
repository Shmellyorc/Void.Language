// Real VS Code provider + rendered-popup measurements in an isolated profile.
// launch(): native Node 24; run(): loaded by the VS Code extension test host.
const fs = require('node:fs/promises');
const path = require('node:path');
const assert = require('node:assert/strict');
const net = require('node:net');
const extension = path.resolve(__dirname, '../../editors/vscode');
const good = 'use pkg/raylib as rl\n\nfunc main():\n    var shade = rl.color(10, 20, 30, 255)\n    rl.clear_background(shade)\n';

function summary(samples) {
    const s = [...samples].sort((a,b) => a-b), n = s.length;
    return {median_ms: n%2?s[n>>1]:(s[n/2-1]+s[n/2])/2,
        p95_ms:s[Math.ceil(.95*n)-1],worst_ms:s[n-1],samples_ms:samples};
}

exports.launch = async function () {
    for (const key of ['VOIDLS_PATH','VOIDC_PATH','VOID_G76_TEMPLATE','VOID_G76_EDITOR_OUTPUT','VSCODE_EXECUTABLE_PATH'])
        assert(process.env[key], 'Set '+key);
    const base = path.resolve(process.env.VOID_G76_EDITOR_OUTPUT);
    await fs.mkdir(base); // Fresh evidence; never overwrite an earlier run.
    const project = path.join(base,'project');
    await fs.cp(process.env.VOID_G76_TEMPLATE,project,{recursive:true});
    await fs.writeFile(path.join(project,'src/app.void'),good);
    await fs.mkdir(path.join(project,'.vscode'),{recursive:true});
    await fs.writeFile(path.join(project,'.vscode/settings.json'),JSON.stringify({
        'void.server.path':path.resolve(process.env.VOIDLS_PATH),'void.compiler.path':path.resolve(process.env.VOIDC_PATH),
        'void.project.root':'.','void.trace.server':'off'}));
    const profile=path.join(base,'profile');await fs.mkdir(path.join(profile,'User'),{recursive:true});
    await fs.writeFile(path.join(profile,'User/settings.json'),JSON.stringify({
        'security.workspace.trust.enabled':false,'workbench.startupEditor':'none',
        'telemetry.telemetryLevel':'off','update.mode':'none','files.hotExit':'off',
        'extensions.autoCheckUpdates':false,'editor.wordBasedSuggestions':'off'}));
    const port=await new Promise((resolve,reject)=>{
        const socket=net.createServer();socket.once('error',reject);
        socket.listen(0,'127.0.0.1',()=>{const port=socket.address().port;socket.close(()=>resolve(port));});
    });
    const {runTests}=require(path.join(extension,'node_modules/@vscode/test-electron'));
    await runTests({vscodeExecutablePath:process.env.VSCODE_EXECUTABLE_PATH,extensionDevelopmentPath:extension,
        extensionTestsPath:__filename,extensionTestsEnv:{VOID_G76_PROJECT:project,VOID_G76_CDP_PORT:String(port)},
        launchArgs:[project,'--user-data-dir='+profile,'--extensions-dir='+path.join(base,'extensions'),
            '--disable-extensions','--disable-updates','--disable-gpu','--skip-welcome','--skip-release-notes',
            '--remote-debugging-port='+port]});
};

exports.run = async function () {
    const vscode=require('vscode');
    const {until,workbench}=require(path.join(extension,'out/test/creationWorkbench.js'));
    const output=process.env.VOID_G76_EDITOR_OUTPUT;
    const uri=vscode.Uri.file(path.join(process.env.VOID_G76_PROJECT,'src/app.void'));
    const document=await vscode.workspace.openTextDocument(uri);
    const editor=await vscode.window.showTextDocument(document);
    const api=await vscode.extensions.getExtension('void-language.void-language').activate();
    const browser=await workbench(process.env.VOID_G76_CDP_PORT);
    const report={version:vscode.version,server:process.env.VOIDLS_PATH,checks:[],measurements:{}};
    const write=()=>fs.writeFile(path.join(output,'results.json'),JSON.stringify(report,null,2));
    const labels=items=>items.map(item=>typeof item.label==='string'?item.label:item.label.label);
    const at=needle=>document.positionAt(document.getText().lastIndexOf(needle)+needle.length);
    const complete=pos=>vscode.commands.executeCommand('vscode.executeCompletionItemProvider',uri,pos);
    const replace=async text=>{
        const edit=new vscode.WorkspaceEdit();edit.replace(uri,new vscode.Range(document.positionAt(0),document.positionAt(document.getText().length)),text);
        assert(await vscode.workspace.applyEdit(edit));
    };
    const choose=pos=>{editor.selection=new vscode.Selection(pos,pos);editor.revealRange(new vscode.Range(pos,pos));};
    try {
        const cold=performance.now();await api.ready();
        await until(()=>complete(at('    rl.')),r=>labels(r?.items??[]).includes('clear_background'),'raylib completion');
        report.cold_ready_ms=performance.now()-cold;
        const samples=[];
        for(let n=0;n<55;++n){const start=performance.now();const result=await complete(at('    rl.'));
            assert(labels(result.items).includes('clear_background'));if(n>=5)samples.push(performance.now()-start);}
        report.measurements.warm_provider=summary(samples);
        const menu=await complete(at('    rl.'));
        report.sample_detail=menu.items.find(i=>labels([i])[0]==='fade')?.detail;
        report.checks.push('warm actual provider responses contain raylib members');await write();

        // Renderer clocks measure suggestion DOM/layout and the following paint.
        // Polling only retrieves timestamps; it is not the measured duration.
        const rendered=[];
        for(let n=0;n<8;++n){
            await vscode.commands.executeCommand('hideSuggestWidget');await replace(good+'    ');choose(document.positionAt(document.getText().length));
            await until(()=>browser.evaluate(`[...document.querySelectorAll('.suggest-widget.visible')].filter(e=>e.getBoundingClientRect().width).length`),n=>n===0,'previous popup hidden');
            await browser.evaluate(`(() => {
                const colors=()=>[...document.querySelectorAll('.view-lines .view-line span')]
                    .filter(e=>!e.childElementCount && /\\bcolor\\b/.test(e.textContent))
                    .map(e=>({text:e.textContent,color:getComputedStyle(e).color}));
                window.__voidG76={start:performance.now(),color_before:colors(),color_samples:[]};
                const colorFrame=()=>{
                    window.__voidG76.color_samples.push(colors());
                    if(!window.__voidG76.paint_ms)requestAnimationFrame(colorFrame);
                };requestAnimationFrame(colorFrame);
                const watch=new MutationObserver(() => {
                    const rows=[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row')];
                    if(!rows.some(e=>e.getBoundingClientRect().width && e.innerText.trim()))return;
                    watch.disconnect();window.__voidG76.dom_ms=performance.now()-window.__voidG76.start;
                    window.__voidG76.labels=rows.map(e=>e.innerText);
                    requestAnimationFrame(()=>requestAnimationFrame(()=>window.__voidG76.paint_ms=performance.now()-window.__voidG76.start));
                });watch.observe(document.body,{childList:true,subtree:true,attributes:true});
            })()`);
            for(const text of ['r','l','.'])await vscode.commands.executeCommand('type',{text});
            const visible=await until(()=>browser.evaluate('window.__voidG76'),value=>value?.paint_ms>0,'painted rapid rl. suggestion');
            rendered.push(visible);assert(labels((await complete(at('rl.'))).items).includes('clear_background'));
            assert(visible.color_before.length,'Rendered color call must be visible');
            const palette=new Set(visible.color_before.map(t=>t.color));
            assert(visible.color_samples.every(sample=>sample.length && sample.every(t=>palette.has(t.color))),
                'Unchanged rendered call lost its color during next-line typing');
        }
        report.measurements.rapid_rl_dom=summary(rendered.map(r=>r.dom_ms));
        report.measurements.rapid_rl_paint=summary(rendered.map(r=>r.paint_ms));
        report.rendered_observations=rendered;report.checks.push('rapid r/l/dot creates visible popup; unchanged rendered call retains coloring');await write();

        await vscode.commands.executeCommand('hideSuggestWidget');
        const broken=good.replace('    rl.clear_background(shade)','    var pending = rl.');
        const edits=[];
        for(let n=0;n<15;++n){const start=performance.now();await replace(broken);
            await until(()=>complete(at('rl.')),r=>labels(r?.items??[]).includes('clear_background'),'assignment completion');
            edits.push(performance.now()-start);await replace(good);}
        report.measurements.assignment_edit_to_provider=summary(edits);
        await replace(good.replace('rl.clear_background(shade)','rl.fade(shade, '));
        const signature=await until(()=>vscode.commands.executeCommand('vscode.executeSignatureHelpProvider',uri,at('rl.fade(shade, ')),Boolean,'raylib parameter help');
        assert.equal(signature.activeParameter,1);report.signature=signature;
        const active=signature.signatures[signature.activeSignature??0];
        const label=active.parameters[1].label;
        assert(Array.isArray(label) && active.label.slice(...label).includes('alpha'),'Precise active-parameter offsets');
        report.invalid_diagnostics=await until(()=>vscode.languages.getDiagnostics(uri),
            ds=>ds.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'incomplete call produces actual diagnostics');
        await replace(good);
        await until(()=>vscode.languages.getDiagnostics(uri),ds=>ds.length===0,'valid source diagnostics restored');
        const hover=await vscode.commands.executeCommand('vscode.executeHoverProvider',uri,
            document.positionAt(document.getText().indexOf('rl.color')+3));
        assert(hover?.length,'Actual raylib hover');report.hover=hover;
        report.checks.push('assignment member edit, parameter help, invalid-to-valid diagnostics');

        const legend=await vscode.commands.executeCommand('vscode.provideDocumentSemanticTokensLegend',uri);
        const tokens=await vscode.commands.executeCommand('vscode.provideDocumentSemanticTokens',uri);
        assert(tokens?.data.length>0);report.semantic_legend=legend;report.semantic_token_count=tokens.data.length/5;
        report.checks.push('semantic tokens available after repairs');
        choose(at('    rl.clear_b'));await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(()=>browser.visible('clear_background','.suggest-widget.visible'),n=>n>0,'final popup');
        // Preserve the actual workbench, not a generated mockup.
        const targets=await(await fetch('http://127.0.0.1:'+process.env.VOID_G76_CDP_PORT+'/json/list')).json();
        const target=targets.find(t=>t.url.includes('workbench.html'));
        const socket=new WebSocket(target.webSocketDebuggerUrl);
        await new Promise(resolve=>socket.addEventListener('open',resolve,{once:true}));
        const shot=new Promise(resolve=>socket.addEventListener('message',e=>resolve(JSON.parse(String(e.data)).result),{once:true}));
        socket.send(JSON.stringify({id:1,method:'Page.captureScreenshot',params:{format:'png'}}));
        const screenshot=await shot;socket.close();await fs.writeFile(path.join(output,'workbench.png'),Buffer.from(screenshot.data,'base64'));
        report.status='PASS';await write();console.log('G76 real editor:',JSON.stringify(report.measurements));
    } catch(error){report.status='FAIL';report.error=String(error?.stack??error);await write();throw error;}
    finally{browser.close();await api.deactivate();}
};
