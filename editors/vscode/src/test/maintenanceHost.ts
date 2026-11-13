import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';

export async function run():Promise<void> {
    assert.equal(vscode.version,'1.141.0');
    const root=process.env.VOID_TEST_ROOT!;
    const uri=vscode.Uri.file(path.join(root,'src/app.void'));
    const document=await vscode.workspace.openTextDocument(uri);
    const editor=await vscode.window.showTextDocument(document);
    const extension=vscode.extensions.getExtension('void-language.void-language');assert(extension);
    const api=await extension.activate() as {ready():Promise<void>;deactivate():Promise<void>};
    const events:string[]=[];
    async function until<T>(read:()=>T|Thenable<T>,valid:(value:T)=>boolean,label:string):Promise<T> {
        const deadline=Date.now()+30000;
        while(Date.now()<deadline) {const value=await read();if(valid(value)){events.push(label);return value;}await new Promise(r=>setTimeout(r,100));}
        throw new Error('Timed out: '+label);
    }
    const manifest=path.join(root,'void.project.toml'),lock=path.join(root,'void.lock');
    const manifestBytes=await fs.readFile(manifest),lockBytes=await fs.readFile(lock);
    const local=path.join(root,'libs/math/math.void');const localBytes=await fs.readFile(local);
    const original=document.getText();
    async function probe(text:string,name:string) {
        assert(await editor.edit(edit=>edit.insert(document.positionAt(document.getText().length),'\nfunc probe():\n    '+text+'\n')));
        const position=document.positionAt(document.getText().length-1);
        await until(()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',uri,position),
            result=>!!result?.items.some(item=>(typeof item.label==='string'?item.label:item.label.label)===name),name+' completion');
        assert(await editor.edit(edit=>edit.delete(new vscode.Range(document.positionAt(original.length),document.positionAt(document.getText().length)))));
        assert(await document.save());
    }
    try {
        await api.ready();
        await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',uri,
            document.positionAt(original.indexOf('p.answer')+4)),
            result=>!!result?.some(item=>item.uri.fsPath===vscode.Uri.file(path.join(root,'libs/source/src/repo.void')).fsPath),'pkg definition uses verified physical libs view');
        await probe('p.ans','answer');await probe('m.ans','answer');
        await fs.writeFile(local,localBytes.toString()+'\npublic func edited_value() -> i32:\n    return 3\n');
        await probe('m.edited_','edited_value');
        await fs.writeFile(manifest,Buffer.concat([manifestBytes,Buffer.from('\n# editor manifest update\n')]));
        await until(()=>vscode.languages.getDiagnostics(vscode.Uri.file(manifest)),d=>!d.some(x=>x.severity===vscode.DiagnosticSeverity.Error),'manifest trivia update retains package identity');
        const changed=lockBytes.toString().replace(/tree = "[a-f0-9]{64}"/,'tree = "'+'0'.repeat(64)+'"');assert.notEqual(changed,lockBytes.toString());
        await fs.writeFile(lock,changed);
        await until(()=>vscode.languages.getDiagnostics(vscode.Uri.file(manifest)),d=>d.some(x=>x.message.includes('Stale or tampered')),'lock change invalidates package analysis');
        await fs.writeFile(lock,lockBytes);
        await until(()=>vscode.languages.getDiagnostics(vscode.Uri.file(manifest)),d=>!d.some(x=>x.severity===vscode.DiagnosticSeverity.Error),'restored verified lock recovers analysis');
        for(const command of ['check','package']) {
            const exits=new Map<vscode.TaskExecution,number|undefined>();
            const listener=vscode.tasks.onDidEndTaskProcess(event=>exits.set(event.execution,event.exitCode));
            try {const task=await vscode.commands.executeCommand<vscode.TaskExecution>(`void.${command}Project`);assert(task);
                const exit=await until(()=>exits.get(task),()=>exits.has(task),command+' Project task');assert.equal(exit,0);
            } finally {listener.dispose();}
        }
        await fs.writeFile(process.env.VOID_G26_EDITOR_EVIDENCE!,JSON.stringify({status:'PASS',editor:vscode.version,platform:process.platform,events},null,2));
    } finally {
        await fs.writeFile(manifest,manifestBytes);await fs.writeFile(lock,lockBytes);await fs.writeFile(local,localBytes);
        await api.deactivate();
    }
}
