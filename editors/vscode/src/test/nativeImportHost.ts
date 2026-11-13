import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';

export async function run():Promise<void> {
    const root=process.env.VOID_TEST_ROOT!;
    const entry=(await fs.readdir(path.join(root,'src'))).find(name=>name.endsWith('.void'))!;
    const document=await vscode.workspace.openTextDocument(vscode.Uri.file(path.join(root,'src',entry)));
    await vscode.window.showTextDocument(document);
    const extension=vscode.extensions.getExtension('void-language.void-language');assert(extension);
    const api=await extension.activate() as {ready():Promise<void>;deactivate():Promise<void>};
    const events:string[]=[];
    const generatedUri=vscode.Uri.file(path.join(root,'src/imported/api.void'));
    const generated=await vscode.workspace.openTextDocument(generatedUri);
    const original=generated.getText();
    async function until<T>(read:()=>T|Thenable<T>,valid:(value:T)=>boolean,label:string):Promise<T> {
        const deadline=Date.now()+30000;
        while(Date.now()<deadline) {const value=await read();if(valid(value)){events.push(label);return value;}await new Promise(resolve=>setTimeout(resolve,100));}
        throw new Error('Timed out: '+label);
    }
    try {
        assert.equal(vscode.version,'1.141.0');await api.ready();
        const position=document.positionAt(document.getText().indexOf('ffi.g25_add')+6);
        await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',document.uri,position),
            result=>!!result?.some(item=>item.uri.fsPath===generatedUri.fsPath),'generated wrapper definition resolves to physical api.void');
        const nativeCall=original.indexOf('native.g25_add')+9;
        await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',generatedUri,generated.positionAt(nativeCall)),
            result=>!!result?.some(item=>item.uri.fsPath.endsWith(path.join('imported','native.void'))),'native definition resolves to physical native.void');
        assert(original.includes('return native.g25_add(p0, p1) + 1'));
        const updated=original.replace('return native.g25_add(p0, p1) + 1','return native.g25_add(p0, p1) + 2');
        const edit=new vscode.WorkspaceEdit();edit.replace(generatedUri,new vscode.Range(generated.positionAt(0),generated.positionAt(original.length)),updated);
        assert(await vscode.workspace.applyEdit(edit));assert(await generated.save());
        assert((await fs.readFile(generatedUri.fsPath,'utf8')).includes('return native.g25_add(p0, p1) + 2'));
        events.push('native editor saves an actual editable generated VOID wrapper');
        const exits=new Map<vscode.TaskExecution,number|undefined>();const listener=vscode.tasks.onDidEndTaskProcess(event=>exits.set(event.execution,event.exitCode));
        try {
            const task=await vscode.commands.executeCommand<vscode.TaskExecution>('void.checkProject');assert(task);
            const code=await until(()=>exits.get(task),()=>exits.has(task),'Check Project compiles edited source');assert.equal(code,0);
        } finally {listener.dispose();}
        const compiler=path.join(path.dirname(process.env.VOIDLS_PATH!),process.platform==='win32'?'voidc.exe':'voidc');
        const {stdout,stderr}=await promisify(execFile)(compiler,['run','--project',root,'--allow-native','--profile','debug','--no-cache'],{cwd:root,timeout:120000});
        assert(stdout.includes('G25 VALUES: 44'),stdout+stderr);events.push('normal compiler executes native-editor-edited source: 44');
        await until(()=>vscode.languages.getDiagnostics(document.uri),result=>!result.some(item=>item.severity===vscode.DiagnosticSeverity.Error),'consumer has no editor errors');
        await fs.writeFile(process.env.VOID_NATIVE_IMPORT_EDITOR_EVIDENCE!,JSON.stringify({status:'PASS',editor:vscode.version,platform:process.platform,events},null,2));
    } finally {
        if(generated.getText()!==original) {
            const restore=new vscode.WorkspaceEdit();restore.replace(generatedUri,new vscode.Range(generated.positionAt(0),generated.positionAt(generated.getText().length)),original);
            await vscode.workspace.applyEdit(restore);await generated.save();
        }
        await api.deactivate();
    }
}
