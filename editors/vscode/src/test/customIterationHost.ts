import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root=process.env.VOID_TEST_ROOT!, report=process.env.VOID_ITERATION_REPORT!;
    const file=path.join(root,'main.void'), apiFile=path.join(root,'api.void');
    const original=await fs.readFile(file,'utf8'), originalAPI=await fs.readFile(apiFile,'utf8');
    const document=await vscode.workspace.openTextDocument(file);
    const editor=await vscode.window.showTextDocument(document);
    const extension=vscode.extensions.getExtension('void-language.void-language');assert(extension);
    const api=await extension.activate() as {ready():Promise<void>;status():{state:string};deactivate():Promise<void>};
    const observations:string[]=[];const browser=await workbench(process.env.VOID_TEST_CDP_PORT!);
    const point=(needle:string,delta=0)=>{const at=document.getText().indexOf(needle);assert(at>=0,needle);return document.positionAt(at+delta);};
    const text=(items:vscode.Hover[])=>(items??[]).flatMap(h=>h.contents).map(c=>typeof c==='string'?c:'value' in c?c.value:'').join('\n').replaceAll('\\','');
    const replace=async(doc:vscode.TextDocument,value:string)=>{
        const edit=new vscode.WorkspaceEdit();edit.replace(doc.uri,new vscode.Range(doc.positionAt(0),doc.positionAt(doc.getText().length)),value);
        assert(await vscode.workspace.applyEdit(edit));
    };
    try {
        await api.ready();assert.equal(api.status().state,'running');
        for(const [needle,delta,type] of [['for element',5,'i32'],['element, key in',10,'i64'],['var state',5,'cursor<i32>'],['var yielded',5,'option<i32>']] as const) {
            const hover=await until(async()=>vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',document.uri,point(needle,delta)),v=>text(v).includes(type),'iteration hover '+needle);
            assert(!text(hover).includes('.void::')&&!text(hover).includes('$iterator$'));
        }
        observations.push('Inferred item/index and concrete generic iterator/option hover');
        const signature=await until(async()=>vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider',document.uri,point('values.iterator()','values.iterator('.length)),v=>!!v?.signatures.length,'parameterless iterator signature');
        assert(signature.signatures[0].label.includes('cursor<i32>')&&signature.signatures[0].parameters?.length===0);
        const signaturePoint=point('values.iterator()','values.iterator('.length);
        editor.selection=new vscode.Selection(signaturePoint,signaturePoint);
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        await until(()=>browser.evaluate<string[]>("[...document.querySelectorAll('.parameter-hints-widget.visible')].map(e=>e.textContent.replaceAll('\\u00a0',' '))"),v=>v.some(s=>s.includes('cursor<i32>')),'visible iterator signature');
        observations.push('Actual parameter hints show parameterless iterator() and cursor<i32> result');
        const definitions=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',document.uri,point('values.iterator()',9));
        assert(definitions.some(d=>d.uri.fsPath===apiFile));
        const references=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider',document.uri,point('values.iterator()',9));
        assert(references.length>=2);observations.push('Imported generic protocol definition and references');
        const tokens=await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens',document.uri);
        assert(tokens?.data.length);observations.push('Semantic highlighting for iterator calls and loop binders');
        await replace(document,original.replace('for repeated in values:', 'for repeated in values.:'));
        const cursor=point('values.:','values.'.length);editor.selection=new vscode.Selection(cursor,cursor);
        editor.revealRange(new vscode.Range(cursor,cursor),vscode.TextEditorRevealType.InCenter);
        const completion=await until(async()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',document.uri,cursor),v=>!!v?.items.some(i=>(typeof i.label==='string'?i.label:i.label.label)==='iterator'),'incomplete loop completion');
        assert(completion.items.some(i=>i.detail?.includes('cursor<i32>')));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(()=>browser.evaluate<string[]>("[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row .label-name')].map(e=>e.textContent.trim())"),v=>v.includes('iterator'),'actual iterator completion popup');
        await vscode.commands.executeCommand('hideSuggestWidget');await replace(document,original);
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'repaired loop diagnostics');
        observations.push('Unsaved loop header recovery and actual iterator completion popup');
        await replace(document,original.replace('for repeated in values:', 'for repeated in reverse values:'));
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>v.some(d=>d.message.includes('reverse_iterator')),'missing reverse diagnostic');
        assert.equal(api.status().state,'running');await replace(document,original);
        const module=await vscode.workspace.openTextDocument(apiFile);
        await replace(module,originalAPI.replace('next() -> option<t>','next() -> t'));
        await until(()=>vscode.languages.getDiagnostics(module.uri),v=>v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'invalid next diagnostic');
        assert.equal(api.status().state,'running');await replace(module,originalAPI);
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'repaired protocol diagnostics');
        observations.push('Missing reverse and invalid next diagnose; repair preserves a live server');
        assert.equal(await fs.readFile(file,'utf8'),original);assert.equal(await fs.readFile(apiFile,'utf8'),originalAPI);
    } finally {
        browser.close();await fs.writeFile(report,JSON.stringify({editor:vscode.env.appName,version:vscode.version,observations},null,2)+'\n');await api.deactivate();
    }
}
