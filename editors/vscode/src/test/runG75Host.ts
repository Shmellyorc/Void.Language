import {execFileSync, spawn} from 'node:child_process';
import {runTests} from '@vscode/test-electron';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import * as os from 'node:os';
import {createServer} from 'node:net';

async function main(): Promise<void> {
    if(process.platform!=='linux')throw new Error('This override UI replay requires the native Linux editor. Other hosts remain unqualified.');
    const compiler=process.env.VOIDC_PATH!,server=process.env.VOIDLS_PATH!;
    const temporary=await fs.mkdtemp(path.join(os.tmpdir(),'VOID G75 editor '));
    let succeeded=false;
    try {
        for(const mode of (process.env.VOID_G75_MODES ?? 'cancel-retry,concurrent,cancel-child,selection,nested,slow,exception,zero,older').split(',')) {
            const control=path.join(temporary,mode);await fs.mkdir(control);
            const workspace=path.join(control,'project');
            if(mode==='nested')await fs.mkdir(workspace);
            const root=mode==='nested'?path.join(workspace,'child'):workspace;
            execFileSync(compiler,['new','app',root]);
            execFileSync(compiler,['build','--project',root,'--profile','debug']);
            if(mode==='nested')await fs.cp(path.join(root,'.vscode'),path.join(workspace,'.vscode'),{recursive:true});
            const config=path.join(workspace,'.vscode/settings.json');
            const wrapper=path.join(control,'voidc');
            const delegate=mode==='older'?process.env.VOID_G74_COMPILER!:compiler;
            await fs.writeFile(wrapper,`#!/usr/bin/python3
import os,sys,time,json
from pathlib import Path
control=Path(${JSON.stringify(control)})
if sys.argv[1:]==['--capabilities']:
    os.execv(${JSON.stringify(delegate)},[${JSON.stringify(delegate)},*sys.argv[1:]])
if sys.argv[1:2]==['build']:
    (control/'argv.json').write_text(json.dumps(sys.argv[1:]))
    (control/'entered').write_text('started')
    if ${mode==='zero'?'True':'False'}:
        sys.exit(0) # A successful process without publication is not a build.
    if ${['slow','concurrent','cancel-child','selection'].includes(mode)?'True':'False'}:
        while not (control/'release').exists():time.sleep(0.05)
        if ${mode==='slow'?'True':'False'}:sys.exit(1)
os.execv(${JSON.stringify(delegate)},[${JSON.stringify(delegate)},*sys.argv[1:]])
`,{mode:0o700});
            await fs.writeFile(config,JSON.stringify({
                'void.server.path':server,'void.compiler.path':wrapper,'debug.onTaskErrors':'abort',
                'void.project.root':['selection','nested'].includes(mode)?root:undefined,
                'void.project.entry':mode==='selection'?path.join(root,'src/app.void'):undefined,
                'lldb-dap.executable-path':process.env.VOID_LLDB_DAP,
            }));
            if(mode==='exception' || mode==='cancel-retry') {
                await fs.copyFile(path.join(root,'.vscode/tasks.json'),path.join(control,'original-tasks.json'));
                await fs.rm(path.join(root,'.vscode/tasks.json'));
            }
            const profile=path.join(control,'profile');await fs.mkdir(path.join(profile,'User'),{recursive:true});
            await fs.writeFile(path.join(profile,'User/settings.json'),JSON.stringify({
                'telemetry.telemetryLevel':'off','update.mode':'none','extensions.autoCheckUpdates':false,
                'extensions.autoUpdate':false,'workbench.startupEditor':'none','chat.disableAIFeatures':true,
                'files.hotExit':'off','window.dialogStyle':'custom',
            }));
            const listener=createServer();await new Promise<void>(resolve=>listener.listen(0,'127.0.0.1',resolve));
            const port=(listener.address() as {port:number}).port;await new Promise<void>(resolve=>listener.close(()=>resolve()));
            if(mode==='exception' || mode==='cancel-retry') {
                // VS Code refuses modal dialogs under --extensionTestsPath.
                // A disposable development harness in a normal real editor
                // exercises the task-exception Debug Anyway path instead.
                const harness=path.join(control,'harness');await fs.mkdir(harness);
                const result=path.join(control,'result.json');
                await fs.writeFile(path.join(harness,'package.json'),JSON.stringify({
                    name:'void-g75-qualification',publisher:'void-test',version:'0.0.0',
                    engines:{vscode:'^1.144.0'},activationEvents:['onStartupFinished'],main:'index.cjs',
                }));
                await fs.writeFile(path.join(harness,'index.cjs'),`
const vscode=require('vscode'),fs=require('node:fs');
exports.activate=()=>{setTimeout(async()=>{
    let result;
    try {await require(${JSON.stringify(path.resolve(__dirname,'g75LaunchHost.js'))}).run();result={ok:true};}
    catch(error){result={ok:false,error:String(error),stack:error.stack};}
    result.output=vscode.window.activeTextEditor?.document.uri.fsPath;
    fs.writeFileSync(${JSON.stringify(result)},JSON.stringify(result));
    await vscode.commands.executeCommand('workbench.action.quit');
},0);};`);
                const env: NodeJS.ProcessEnv={...process.env,VOID_G75_MODE:mode,VOID_G75_CONTROL:control,VOID_TEST_CDP_PORT:String(port)};
                delete env.ELECTRON_RUN_AS_NODE;
                await new Promise<void>((resolve,reject)=>{
                    const child=spawn(process.env.VSCODE_EXECUTABLE_PATH!,[root,
                        '--extensionDevelopmentPath='+path.resolve(__dirname,'../..'),'--extensionDevelopmentPath='+harness,
                        '--user-data-dir='+profile,'--shared-data-dir='+path.join(control,'shared'),'--extensions-dir='+process.env.VOID_LLDB_EXTENSIONS,
                        '--remote-debugging-port='+port,'--disable-workspace-trust','--disable-updates','--disable-gpu',
                        '--skip-welcome','--skip-release-notes'],{env,stdio:'inherit'});
                    const timeout=setTimeout(()=>{child.kill();},45000);
                    child.once('error',error=>{clearTimeout(timeout);reject(error);});
                    child.once('close',()=>{clearTimeout(timeout);resolve();});
                });
                console.log('g75_normal_progress: '+await fs.readFile(path.join(control,'progress'),'utf8').catch(()=>''));
                const receipt=JSON.parse(await fs.readFile(result,'utf8'));
                console.log('g75_real_editor_exception: '+JSON.stringify(receipt));
                if(!receipt.ok)throw new Error(receipt.error);
                continue;
            }
            await runTests({vscodeExecutablePath:process.env.VSCODE_EXECUTABLE_PATH,
                extensionDevelopmentPath:path.resolve(__dirname,'../..'),
                extensionTestsPath:path.resolve(__dirname,'g75LaunchHost.js'),
                extensionTestsEnv:{VOID_G75_MODE:mode,VOID_G75_CONTROL:control,VOID_TEST_CDP_PORT:String(port)},
                launchArgs:[workspace,'--user-data-dir='+profile,'--shared-data-dir='+path.join(control,'shared'),'--extensions-dir='+process.env.VOID_LLDB_EXTENSIONS,
                    '--remote-debugging-port='+port,'--disable-updates','--disable-gpu','--skip-welcome','--skip-release-notes'],
            });
        }
        succeeded=true;
    } finally {
        if(succeeded && !process.env.VOID_G75_KEEP_EVIDENCE)await fs.rm(temporary,{recursive:true,force:true,maxRetries:5,retryDelay:100});
        else console.log('G75 editor evidence retained at '+temporary);
    }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
