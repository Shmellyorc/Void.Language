import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root=process.env.VOID_TEST_ROOT!, report=process.env.VOID_CONDITIONAL_REPORT!;
    const file=path.join(root,'main.void'), apiFile=path.join(root,'api.void'), manifestFile=path.join(root,'void.project.toml');
    const original=await fs.readFile(file,'utf8'), originalManifest=await fs.readFile(manifestFile,'utf8');
    const document=await vscode.workspace.openTextDocument(file), editor=await vscode.window.showTextDocument(document);
    const extension=vscode.extensions.getExtension('void-language.void-language');assert(extension);
    const api=await extension.activate() as {ready():Promise<void>;status():{state:string};deactivate():Promise<void>};
    const observations:string[]=[];const browser=await workbench(process.env.VOID_TEST_CDP_PORT!);
    const point=(needle:string,delta=0)=>{const at=document.getText().indexOf(needle);assert(at>=0,needle);return document.positionAt(at+delta);};
    const text=(items:vscode.Hover[])=>(items??[]).flatMap(h=>h.contents).map(c=>typeof c==='string'?c:'value' in c?c.value:'').join('\n');
    const replace=async(doc:vscode.TextDocument,value:string)=>{
        const edit=new vscode.WorkspaceEdit();edit.replace(doc.uri,new vscode.Range(doc.positionAt(0),doc.positionAt(doc.getText().length)),value);assert(await vscode.workspace.applyEdit(edit));
    };
    try {
        await api.ready();assert.equal(api.status().state,'running');
        await until(async()=>vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',document.uri,point('api.value()',5)),v=>text(v).includes('i32'),'active conditional hover');
        const definitions=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',document.uri,point('api.value()',5));
        assert(definitions.some(d=>d.uri.fsPath===apiFile && d.range.start.line===1));
        const references=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider',document.uri,point('api.value()',5));assert(references.length>=2);
        observations.push('Active declaration hover, definition and references use selected API');
        const tokens=await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens',document.uri);assert(tokens?.data.length);
        const inactive=await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',document.uri,point('excluded',3));assert(!inactive?.length);
        // A custom theme rule proves the editor actually applies compiler inactive metadata.
        await vscode.workspace.getConfiguration('editor').update('semanticTokenColorCustomizations',{enabled:true,rules:{'comment.inactive':'#778899'}},vscode.ConfigurationTarget.Workspace);
        editor.revealRange(new vscode.Range(point('excluded'),point('excluded')),vscode.TextEditorRevealType.InCenter);
        await until(()=>browser.evaluate<boolean>("[...document.querySelectorAll('.view-line span')].some(e=>e.textContent.includes('excluded') && getComputedStyle(e).color==='rgb(119, 136, 153)')"),Boolean,'visible inactive conditional source color');
        observations.push('Inactive source has no hover and renders using comment.inactive semantic metadata');
        await replace(document,original.replace('api.extra()','api.'));
        const cursor=point('api.\n',4);editor.selection=new vscode.Selection(cursor,cursor);editor.revealRange(new vscode.Range(cursor,cursor));
        await until(async()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',document.uri,cursor),v=>!!v?.items.some(i=>(typeof i.label==='string'?i.label:i.label.label)==='extra'),'active feature completion');
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(()=>browser.evaluate<string[]>("[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row .label-name')].map(e=>e.textContent.trim())"),v=>v.includes('extra'),'visible active conditional completion');
        observations.push('Active feature completion appears in the actual suggestion popup');
        await vscode.commands.executeCommand('hideSuggestWidget');await replace(document,original);
        const manifest=await vscode.workspace.openTextDocument(manifestFile);
        await replace(manifest,originalManifest.replace('extra=true','extra=false'));
        await until(async()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',document.uri,point('api.value()',5)),v=>!!v?.some(d=>d.uri.fsPath===apiFile && d.range.start.line===6),'dirty feature configuration navigation');
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'inactive declarations remain error free');
        observations.push('Unsaved manifest condition changes update navigation and exclude active-only calls');
        await replace(manifest,originalManifest);
        await replace(document,original.replace('compile if feature.extra:','compile if feature.:'));
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>v.some(d=>String(typeof d.code==='object'?d.code.value:d.code)==='VOID5101'),'incomplete condition diagnostic');
        assert.equal(api.status().state,'running');await replace(document,original);
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'repaired condition');
        observations.push('Incomplete condition diagnoses and repairs with the server still running');
        const continued=original.replace('        api.extra()', '        print(\n      "continued"\n)\n        api.extra()');
        await replace(document,continued);
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'active multiline continuation');
        await replace(manifest,originalManifest.replace('extra=true','extra=false'));
        await until(async()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',document.uri,point('api.value()',5)),v=>!!v?.some(d=>d.range.start.line===6),'excluded multiline selection');
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'excluded multiline continuation');
        observations.push('Cosmetic multiline call indentation remains error free in both feature configurations');
        await replace(document,original);await replace(manifest,originalManifest);

        assert.equal(await fs.readFile(file,'utf8'),original);assert.equal(await fs.readFile(manifestFile,'utf8'),originalManifest);
    } finally {
        browser.close();await fs.writeFile(report,JSON.stringify({editor:vscode.env.appName,version:vscode.version,observations},null,2)+'\n');await api.deactivate();
    }
}
