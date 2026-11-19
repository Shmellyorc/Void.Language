// Real editor provider/UI qualification; isolated profiles, no extension install.
const fs = require('node:fs/promises');
const path = require('node:path');
const net = require('node:net');
const assert = require('node:assert/strict');
const extension = path.resolve(__dirname, '../../editors/vscode');
const good = 'use pkg/raylib as rl\nfunc acquire(file_name: string) -> result<rl.image, rl.load_error>:\n    return rl.try_load_image(file_name = file_name)\nfunc main():\n    var outcome = acquire("missing.png")\n';

exports.launch = async function () {
    for (const name of ['VOID_G77_EDITOR_OUTPUT','VOID_G77_PROJECT','VOIDLS_PATH','VOIDC_PATH','VSCODE_EXECUTABLE_PATH']) assert(process.env[name], name);
    const output = path.resolve(process.env.VOID_G77_EDITOR_OUTPUT);
    await fs.mkdir(output);
    const project = path.join(output, 'project');
    await fs.cp(process.env.VOID_G77_PROJECT, project, {recursive: true});
    for (const file of await fs.readdir(path.join(project,'src'))) await fs.unlink(path.join(project,'src',file));
    await fs.writeFile(path.join(project,'src/app.void'),good);
    await fs.mkdir(path.join(project,'.vscode'),{recursive:true});
    await fs.writeFile(path.join(project,'.vscode/settings.json'),JSON.stringify({
        'void.server.path':path.resolve(process.env.VOIDLS_PATH),'void.compiler.path':path.resolve(process.env.VOIDC_PATH),'void.project.root':'.','void.trace.server':'off'}));
    const profile=path.join(output,'profile');await fs.mkdir(path.join(profile,'User'),{recursive:true});
    await fs.writeFile(path.join(profile,'User/settings.json'),JSON.stringify({'security.workspace.trust.enabled':false,'workbench.startupEditor':'none','telemetry.telemetryLevel':'off','update.mode':'none','extensions.autoCheckUpdates':false,'files.hotExit':'off'}));
    const port=await new Promise(resolve=>{const s=net.createServer();s.listen(0,'127.0.0.1',()=>{const p=s.address().port;s.close(()=>resolve(p));});});
    const {runTests}=require(path.join(extension,'node_modules/@vscode/test-electron'));
    await runTests({vscodeExecutablePath:process.env.VSCODE_EXECUTABLE_PATH,extensionDevelopmentPath:extension,
        extensionTestsPath:__filename,extensionTestsEnv:{VOID_G77_PROJECT:project,VOID_G77_PORT:String(port)},
        launchArgs:[project,'--user-data-dir='+profile,'--extensions-dir='+path.join(output,'extensions'),
            '--disable-extensions','--disable-updates','--disable-gpu','--skip-welcome','--skip-release-notes','--remote-debugging-port='+port]});
};

exports.run = async function () {
    const vscode=require('vscode');
    const {until,workbench}=require(path.join(extension,'out/test/creationWorkbench.js'));
    const report={version:vscode.version,server:process.env.VOIDLS_PATH,checks:[]};
    const output=process.env.VOID_G77_EDITOR_OUTPUT;
    const save=()=>fs.writeFile(path.join(output,'results.json'),JSON.stringify(report,null,2));
    const document=await vscode.workspace.openTextDocument(vscode.Uri.file(path.join(process.env.VOID_G77_PROJECT,'src/app.void')));
    const editor=await vscode.window.showTextDocument(document);
    const api=await vscode.extensions.getExtension('void-language.void-language').activate();
    const browser=await workbench(process.env.VOID_G77_PORT);
    const textOf=value=>typeof value==='string'?value:value?.value??'';
    const normalized=value=>textOf(value).replaceAll('&nbsp;', ' ').replaceAll('&lt;', '<').replaceAll('&gt;', '>').replace(/\\(.)/g,'$1');
    const at=needle=>document.positionAt(document.getText().lastIndexOf(needle)+needle.length);
    const replace=async text=>{const e=new vscode.WorkspaceEdit();e.replace(document.uri,new vscode.Range(document.positionAt(0),document.positionAt(document.getText().length)),text);assert(await vscode.workspace.applyEdit(e));};
    try {
        await api.ready();
        const hover=await until(()=>vscode.commands.executeCommand('vscode.executeHoverProvider',document.uri,at('rl.try_load_i')),
            hs=>hs?.some(h=>h.contents.some(c=>normalized(c).includes('load_error'))),'fallible resource hover');
        const hoverText=hover.flatMap(h=>h.contents.map(normalized)).join('\n');
        assert(hoverText.includes('result<image, load_error>')&&hoverText.includes('Ok')&&hoverText.includes('Err')&&hoverText.includes('unload_image'),hoverText);
        report.hover=hoverText;report.checks.push('Readable result type, success/failure and ownership hover');await save();
        const errorHover=await vscode.commands.executeCommand('vscode.executeHoverProvider',document.uri,at('rl.load_erro'));
        assert(errorHover?.some(h=>h.contents.some(c=>normalized(c).includes('Recoverable raylib'))),JSON.stringify(errorHover));
        report.checks.push('Public managed error type source documentation');
        await replace(good+'    rl.try_load_');
        const completions=await until(()=>vscode.commands.executeCommand('vscode.executeCompletionItemProvider',document.uri,document.positionAt(document.getText().length)),
            r=>r?.items.some(i=>(typeof i.label==='string'?i.label:i.label.label)==='try_load_texture'),'fallible completion');
        const item=completions.items.find(i=>(typeof i.label==='string'?i.label:i.label.label)==='try_load_texture');
        assert(item.detail.includes('result<texture, load_error>'),item.detail);
        assert(normalized(item.documentation).includes('unload_texture')&&normalized(item.documentation).includes('Err'),normalized(item.documentation));
        report.completion={detail:item.detail,documentation:normalized(item.documentation)};
        report.checks.push('Discoverable try_load_ completion with owning-result docs');
        for(const [call,active,label,description] of [
            ['rl.try_load_image(',0,'file_name: string','Relative paths use the process working directory.'],
            ['rl.try_load_image_raw(width = ',1,'width: i32','Width in pixels.'],
            ['rl.try_load_shader(fs_file_name = ',1,'fs_file_name: c_string?','Optional fragment-shader file path.'],
            ['rl.try_load_font_ex(font_size = ',1,'font_size: i32','Font size in pixels.']]) {
            await replace(good+'    '+call);
            const help=await until(()=>vscode.commands.executeCommand('vscode.executeSignatureHelpProvider',document.uri,document.positionAt(document.getText().length),'('),
                h=>h?.signatures.length>0&&h.activeParameter===active,'signature '+call);
            const signature=help.signatures[help.activeSignature??0];const parameter=signature.parameters[active];
            assert(signature.label.includes('load_error>')&&!signature.label.includes('/@pkg/'),signature.label);
            assert(normalized(parameter.documentation).includes(description),normalized(parameter.documentation));
            const printed=Array.isArray(parameter.label)?signature.label.slice(...parameter.label):parameter.label;
            assert.equal(printed,label);report.checks.push('Named active parameter '+call+' = '+label);
        }
        editor.selection=new vscode.Selection(document.positionAt(document.getText().length),document.positionAt(document.getText().length));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        await until(()=>browser.visible('font_size','.parameter-hints-widget.visible'),n=>n>0,'visible named parameter widget');
        const targets=await(await fetch('http://127.0.0.1:'+process.env.VOID_G77_PORT+'/json/list')).json();
        const socket=new WebSocket(targets.find(t=>t.url.includes('workbench.html')).webSocketDebuggerUrl);
        await new Promise(resolve=>socket.addEventListener('open',resolve,{once:true}));
        const shot=new Promise(resolve=>socket.addEventListener('message',e=>resolve(JSON.parse(String(e.data)).result),{once:true}));
        socket.send(JSON.stringify({id:1,method:'Page.captureScreenshot',params:{format:'png'}}));
        const png=await shot;socket.close();await fs.writeFile(path.join(output,'workbench.png'),Buffer.from(png.data,'base64'));
        report.checks.push('Actual named parameter help rendered');
        await replace(good);await until(()=>vscode.languages.getDiagnostics(document.uri),d=>d.length===0,'valid document recovery');
        report.checks.push('Valid unsaved document recovers without diagnostics');
        report.status='PASS';await save();console.log('G77 real editor PASS:',report.checks.length,'checks');
    } catch(error){report.status='FAIL';report.error=String(error.stack??error);await save();throw error;}
    finally {browser.close();await api.deactivate();}
};
