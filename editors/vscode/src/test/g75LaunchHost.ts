import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import {join} from 'node:path';
import {until, workbench, pause} from './creationWorkbench';

export async function run(): Promise<void> {
    const folder=vscode.workspace.workspaceFolders![0];
    const mode=process.env.VOID_G75_MODE!;
    const root=mode==='nested'?join(folder.uri.fsPath,'child'):folder.uri.fsPath;
    const control=process.env.VOID_G75_CONTROL!;
    const events: unknown[]=[];
    const launches: string[]=[];
    const tasks: number[]=[];
    const receipts: unknown[]=[];
    const owned=[vscode.debug.onDidStartDebugSession(s=>launches.push(s.type)),
        vscode.tasks.onDidEndTaskProcess(e=>{if(e.execution.task.definition.command==='debugBuild') {
            tasks.push(e.exitCode ?? -1);receipts.push(e.execution.task.definition.receipt);
        }})];
    await fs.appendFile(join(control,'progress'),'before UI '+process.env.VOID_TEST_CDP_PORT+'\n');
    const ui=await workbench(process.env.VOID_TEST_CDP_PORT!);
    await fs.appendFile(join(control,'progress'),'UI connected\n');
    try {
        await vscode.extensions.getExtension('void-language.void-language')!.activate();
        await fs.appendFile(join(control,'progress'),'extension active\n');
        await until(async()=>{try{return await vscode.commands.executeCommand<string>('void.debugProjectRoot');}catch{return undefined;}},v=>v===root,'ready project');
        await fs.appendFile(join(control,'progress'),'project ready\n');
        const program=join(root,'build','app');
        const original=await fs.readFile(program);
        const attempt=vscode.debug.startDebugging(folder,'VOID Debug project').then(Boolean,()=>false);
        await fs.appendFile(join(control,'progress'),'debug requested\n');
        if(mode==='cancel-retry') {
            await until(()=>ui.visible('Could not find the task','.monaco-dialog-box'),Boolean,'missing-task dialog');
            await ui.click('Cancel','.monaco-dialog-box');
            assert.equal(await attempt,false);
            assert.deepEqual(tasks,[]);assert.deepEqual(launches,[]);
            await fs.copyFile(join(control,'original-tasks.json'),join(root,'.vscode/tasks.json'));
            await until(async()=>vscode.tasks.fetchTasks({type:'void'}),v=>v.some(t=>t.definition.command==='debugBuild'),'restored task');
            assert(await Promise.race([vscode.debug.startDebugging(folder,'VOID Debug project'),
                pause(10000).then(()=>{throw new Error('Retried Debug launch did not resolve.');})]));
            await until(()=>tasks.length,v=>v===1,'retried compiler task');
            assert.deepEqual(tasks,[0]);
            await until(()=>launches.length,v=>v===1,'retried adapter');
        } else if(['concurrent','cancel-child','selection'].includes(mode)) {
            await until(()=>fs.readFile(join(control,'entered'),'utf8').catch(()=>''),Boolean,'actual compiler child');
            if(mode==='concurrent') {
                assert.equal(await vscode.debug.startDebugging(folder,'VOID Debug project').then(Boolean,()=>false),false);
                await fs.writeFile(join(control,'release'),'continue');
                assert(await attempt);
                await until(()=>tasks.length,v=>v===1,'single invocation task');
                assert.deepEqual(tasks,[0]);
            } else {
                if(mode==='cancel-child') {
                    await vscode.debug.stopDebugging();
                    for(const execution of vscode.tasks.taskExecutions)
                        if(execution.task.definition.command==='debugBuild')execution.terminate();
                } else {
                    await fs.copyFile(join(root,'src/app.void'),join(root,'src/other.void'));
                    await vscode.workspace.getConfiguration('void').update('project.entry',join(root,'src/other.void'),vscode.ConfigurationTarget.Workspace);
                    await until(async()=>{
                        try{return await vscode.commands.executeCommand('void.debugProjectRoot');}catch{return undefined;}
                    },v=>v===root,'reselected entry');
                }
                await fs.writeFile(join(control,'release'),'continue');
                assert.equal(await attempt,false);
                assert.deepEqual(launches,[]);
                await until(()=>tasks.length,v=>v===1,'invalidated invocation task');
                if(mode==='cancel-child') {
                    assert.notEqual(tasks[0],0);
                    assert.deepEqual(await fs.readFile(program),original);
                } else assert.equal(tasks[0],0); // successful old build is not this selected launch
                assert(await vscode.debug.startDebugging(folder,'VOID Debug project'));
                await until(()=>tasks.length,v=>v===2,'fresh selected compiler task');
                assert.equal(tasks[1],0);
                assert.notEqual(receipts[0],receipts[1]);
            }
            await until(()=>launches.length,v=>v===1,'one authorized adapter');
        } else if(mode==='slow') {
            await until(()=>fs.readFile(join(control,'entered'),'utf8').catch(()=>''),Boolean,'actual delayed compiler start');
            // This is the host's real 10-second slow-task notification, not a
            // mocked TaskRunResult or a fabricated child-close event.
            await until(()=>ui.visible('Waiting for preLaunchTask', '.notifications-list-container'),Boolean,'slow prelaunch notification');
            await ui.click('Debug Anyway','.notifications-list-container');
            assert.equal(await attempt,false);
            assert.deepEqual(launches,[]);assert.deepEqual(tasks,[]);
            await fs.writeFile(join(control,'release'),'fail');
            await until(()=>tasks.length,v=>v===1,'delayed compiler failure');
            assert.equal(tasks[0],1);
        } else if(mode==='exception') {
            await until(()=>ui.visible('Could not find the task', '.monaco-dialog-box'),Boolean,'missing-task notification');
            await ui.click('Debug Anyway','.monaco-dialog-box');
            // A normal editor displays the guard's failure before resolving the
            // startDebugging request. Dismiss that error, never authorize a launch.
            await until(()=>ui.visible('VOID requires this launch', '.monaco-dialog-box'),Boolean,'receipt refusal');
            await ui.click('Cancel','.monaco-dialog-box');
            assert.equal(await attempt,false);
            assert.deepEqual(launches,[]);assert.deepEqual(tasks,[]);
        } else if(mode==='zero') {
            assert.equal(await attempt,false);
            await until(()=>tasks.length,v=>v===1,'zero-exit compiler task');
            assert.deepEqual(tasks,[0]);assert.deepEqual(launches,[]);
        } else {
            assert(await attempt);
            await until(()=>tasks.length,v=>v===1,'older compiler task');
            assert.equal(tasks[0],0);
            await until(()=>launches.length,v=>v===1,'older compiler adapter');
            const args=JSON.parse(await fs.readFile(join(control,'argv.json'),'utf8')) as string[];
            if(mode==='older')assert(!args.includes('--status'),JSON.stringify(args));
            else {
                assert.equal(mode,'nested');
                assert.equal(args[args.indexOf('--project')+1],root);
                assert.equal(typeof receipts[0],'number');
            }
            await vscode.debug.stopDebugging();
        }
        const preserved=!['older','cancel-retry','concurrent','cancel-child','selection','nested'].includes(mode);
        if(preserved)assert.deepEqual(await fs.readFile(program),original);
        events.push({mode,tasks,receipts,launches,oldArtifactPreserved:preserved});
        await fs.writeFile(join(control,'events.json'),JSON.stringify(events));
        console.log('g75_launch_host: '+JSON.stringify(events));
    } catch (error) {
        console.log('g75_workbench_failure: '+await ui.evaluate<string>('document.body.innerText'));
        throw error;
    } finally {
        await vscode.debug.stopDebugging();
        await fs.writeFile(join(control,'release'),'fail');
        ui.close();owned.forEach(x=>x.dispose());
    }
}
