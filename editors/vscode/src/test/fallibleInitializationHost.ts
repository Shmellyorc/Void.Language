import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import {promises as fs} from 'node:fs';
import * as path from 'node:path';
import {until, workbench} from './creationWorkbench';

export async function run(): Promise<void> {
    const root=process.env.VOID_TEST_ROOT!;
    const report=process.env.VOID_FALLIBLE_REPORT!;
    const file=path.join(root,'main.void'), apiFile=path.join(root,'api.void');
    const original=await fs.readFile(file,'utf8'), originalAPI=await fs.readFile(apiFile,'utf8');
    const document=await vscode.workspace.openTextDocument(file);
    const editor=await vscode.window.showTextDocument(document);
    const extension=vscode.extensions.getExtension('void-language.void-language');assert(extension);
    const api=await extension.activate() as {ready():Promise<void>;status():{state:string};deactivate():Promise<void>};
    const observations:string[]=[];
    const browser=await workbench(process.env.VOID_TEST_CDP_PORT!);
    const point=(needle:string,delta=0)=>{const at=document.getText().indexOf(needle);assert(at>=0,needle);return document.positionAt(at+delta);};
    const hoverText=(items:vscode.Hover[])=>(items??[]).flatMap(h=>h.contents).map(c=>typeof c==='string'?c:'value' in c?c.value:'').join('\n').replaceAll('\\','');
    const replace=async(doc:vscode.TextDocument,text:string)=>{
        const edit=new vscode.WorkspaceEdit();edit.replace(doc.uri,new vscode.Range(doc.positionAt(0),doc.positionAt(doc.getText().length)),text);
        assert(await vscode.workspace.applyEdit(edit));
    };
    try {
        await api.ready();assert.equal(api.status().state,'running');
        const local=await until(async()=>vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',document.uri,point('var made',5)),v=>hoverText(v).includes('result'),'constructor result hover');
        assert(hoverText(local).includes('gate') && hoverText(local).includes('string') && !hoverText(local).includes('.void::'));
        observations.push('Inferred result<gate, string> hover from ordinary constructor call');
        const constructor=await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',document.uri,point('api.gate(value',5));
        assert(hoverText(constructor).includes('gate') && !hoverText(constructor).includes('.void::'));
        const signature=await until(async()=>vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider',document.uri,point('api.gate(value = wanted)',19)),v=>!!v?.signatures.length,'fallible constructor signature');
        assert(signature.signatures[0].label.includes('result<gate, string>') && signature.signatures[0].label.includes('value: i32'));
        assert(JSON.stringify(signature).includes('nonnegative gate value'));
        editor.selection=new vscode.Selection(point('api.gate(value = wanted)',19),point('api.gate(value = wanted)',19));
        await vscode.commands.executeCommand('editor.action.triggerParameterHints');
        await until(()=>browser.evaluate<string[]>("[...document.querySelectorAll('.parameter-hints-widget.visible')].map(e=>e.textContent.replaceAll('\\u00a0',' '))"),v=>v.some(s=>s.includes('result<gate, string>')),'visible fallible signature help');
        observations.push('Actual constructor parameter hints show object result and initializer documentation');
        const definitions=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',document.uri,point('api.gate(value',5));
        assert(definitions.some(d=>d.uri.fsPath===apiFile));
        const references=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeReferenceProvider',document.uri,point('api.gate(value',5));
        assert(references.length>=2);observations.push('Imported constructor definition and references');
        const tokens=await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens',document.uri);
        assert(tokens?.data.length);observations.push('Semantic highlighting for result bindings and success payload');
        await replace(document,original.replace('complete.value','complete.'));
        const cursor=point('complete.',9);editor.selection=new vscode.Selection(cursor,cursor);
        editor.revealRange(new vscode.Range(cursor,cursor),vscode.TextEditorRevealType.InCenter);
        const completion=await until(async()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',document.uri,cursor),v=>!!v?.items.some(i=>(typeof i.label==='string'?i.label:i.label.label)==='value'),'success payload completion');
        assert(completion.items.some(i=>i.kind===vscode.CompletionItemKind.Field || i.kind===vscode.CompletionItemKind.Property));
        await vscode.commands.executeCommand('editor.action.triggerSuggest');
        await until(()=>browser.evaluate<string[]>("[...document.querySelectorAll('.suggest-widget.visible .monaco-list-row .label-name')].map(e=>e.textContent.trim())"),v=>v.includes('value'),'visible success payload suggestion');
        await vscode.commands.executeCommand('hideSuggestWidget');await replace(document,original);
        await until(()=>vscode.languages.getDiagnostics(document.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'repaired call-site diagnostics');
        observations.push('Unsaved payload member recovery and actual completion popup');
        const genericHover=await vscode.commands.executeCommand<vscode.Hover[]>('vscode.executeHoverProvider',document.uri,point('var boxed',5));
        assert(hoverText(genericHover).includes('result') && hoverText(genericHover).includes('box') && !hoverText(genericHover).includes('.void::'));
        const genericSignature=await until(async()=>vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider',document.uri,point('api.box<i32>(value = wanted)','api.box<i32>(value = wa'.length)),v=>!!v?.signatures.length,'generic constructor signature');
        assert(genericSignature.signatures[0].label.includes('result<box<i32>, string>'));
        await replace(document,original.replace('complete_box.value','complete_box.'));
        const genericCompletion=await until(async()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',document.uri,point('complete_box.','complete_box.'.length)),v=>!!v?.items.length,'generic payload completion');
        assert(genericCompletion.items.some(i=>{
            const documentation=typeof i.documentation==='string'?i.documentation:i.documentation?.value??'';
            return documentation.includes('box<i32>') && !documentation.includes('.void::');
        }));
        await replace(document,original);
        observations.push('Concrete generic constructor result/signature and readable declaring type in field completion');
        const module=await vscode.workspace.openTextDocument(apiFile);
        await replace(module,originalAPI.replace('func (g gate) init(value: i32 = 9) -> result<bool, string>','func (g gate) init(value: i32 = 9) -> result<i32, string>'));
        await until(()=>vscode.languages.getDiagnostics(module.uri),v=>v.some(d=>d.message.includes('initializer result must')),'invalid initializer return diagnostic');
        assert.equal(api.status().state,'running');await replace(module,originalAPI);
        await until(()=>vscode.languages.getDiagnostics(module.uri),v=>!v.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'repaired initializer diagnostics');
        const repaired=await until(async()=>vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider',document.uri,point('api.gate(value = wanted)',19)),v=>!!v?.signatures.some(s=>s.label.includes('result<gate, string>')),'repaired constructor signature');
        assert(repaired.signatures.length);observations.push('Invalid initializer annotation diagnoses without terminating the server; repair restores providers');
        assert.equal(await fs.readFile(file,'utf8'),original);assert.equal(await fs.readFile(apiFile,'utf8'),originalAPI);
    } finally {
        browser.close();await fs.writeFile(report,JSON.stringify({editor:vscode.env.appName,version:vscode.version,observations},null,2)+'\n');await api.deactivate();
    }
}
