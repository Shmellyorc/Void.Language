import * as vscode from 'vscode';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import {promises as fs} from 'node:fs';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
const events: string[]=[];
async function until<T>(read:()=>T|PromiseLike<T>,valid:(value:T)=>boolean,label:string):Promise<T>{const end=Date.now()+25000;while(Date.now()<end){const value=await read();if(valid(value)){events.push(label);return value;}await new Promise(resolve=>setTimeout(resolve,75));}throw new Error('Timed out: '+label);}
function code(d:vscode.Diagnostic):unknown{return typeof d.code==='object'?d.code.value:d.code;}
function entryIs(entry:string|undefined, target:vscode.Uri):boolean{return entry!==undefined&&vscode.Uri.file(entry).fsPath===target.fsPath;}
async function replace(document:vscode.TextDocument,text:string){const edit=new vscode.WorkspaceEdit();edit.replace(document.uri,new vscode.Range(document.positionAt(0),document.positionAt(document.getText().length)),text);assert(await vscode.workspace.applyEdit(edit));}
async function unreferenced(root: string, entry: vscode.TextDocument, main: string): Promise<vscode.Uri> {
 const app=vscode.Uri.file(path.join(root,'src/app.void'));
 const body=`struct game:
    name: string
    running: bool

func (g game) init(name: string):
    g.name = name

public func play():
    var test = game(name = "player")
`;
 await fs.writeFile(app.fsPath,body);
 const document=await vscode.workspace.openTextDocument(app);const editor=await vscode.window.showTextDocument(document);
 const tokens=()=>vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens',app);
 const initial=await until(tokens,t=>!!t?.data.length,'semantic highlighting before main imports app.void');
 const point=document.positionAt(body.indexOf('game(name'));
 const definition=()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',app,point);
 await until(definition,locations=>!!locations?.some(l=>l.uri.fsPath===app.fsPath && l.range.start.line===4),'unreferenced constructor definition');
 const formatting=(insertSpaces:boolean)=>{
  editor.options={insertSpaces,tabSize:4};
  return vscode.commands.executeCommand<vscode.TextEdit[]>('vscode.executeFormatDocumentProvider',app,{insertSpaces,tabSize:4});
 };
 const applyFormatting=async(insertSpaces:boolean,label:string)=>{
  const edits=await until(()=>formatting(insertSpaces),value=>!!value?.length,label);
  const edit=new vscode.WorkspaceEdit();edit.set(app,edits);assert(await vscode.workspace.applyEdit(edit));
 };
 await applyFormatting(false,'unreferenced compiler-validated tabs');assert(document.getText().includes('\tname: string'));
 await applyFormatting(true,'unreferenced compiler-validated spaces');assert.equal(document.getText(),body);
 await replace(document,body.replace('name = "player")','na'));
 const argument=point.translate(0,7);
 await until(()=>vscode.commands.executeCommand<vscode.SignatureHelp>('vscode.executeSignatureHelpProvider',app,argument),
  h=>h?.signatures[0]?.label==='game(name: string)' && h.activeParameter===0,'unreferenced incomplete constructor signature');
 await until(()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',app,argument),
  l=>!!l?.items.some(i=>(typeof i.label==='string'?i.label:i.label.label)==='name ='),'unreferenced named argument completion');
 await until(()=>vscode.languages.getDiagnostics(app),d=>d.some(e=>e.severity===vscode.DiagnosticSeverity.Error),'unreferenced dirty-source diagnostics');
 assert(document.isDirty);assert.equal(await fs.readFile(app.fsPath,'utf8'),body);
 await until(definition,locations=>!locations?.length,'failed unreferenced analysis drops previous definitions');
 await replace(document,body);
 await replace(entry,'use "./app.void" as app\n'+main.replace('    return','    app.play()\n    return'));
 await until(tokens,t=>!!t?.data.length && Buffer.from(t.data.buffer).equals(Buffer.from(initial.data.buffer)),'importing app.void preserves its semantic roles');
 await until(definition,locations=>!!locations?.length,'imported constructor still resolves');
 await replace(entry,main);await vscode.window.showTextDocument(entry);
 return app;
}
async function projectTasks(root:string, source:vscode.TextDocument, profile:string):Promise<void> {
 const config=vscode.workspace.getConfiguration('void');
 const args=['two words; &', '--literal', '${workspaceFolder}', ''];
 const program=`use os
use pkg/example
func main() -> i32:
    if os.args() is Ok(var values):
        if values.count == 4:
            if values[0] == "two words; &" and values[1] == "--literal" and values[2] == "$" + "{{workspaceFolder}}" and values[3] == "":
                return example.status()
    return 23
`;
 await config.update('tasks.profile',profile,vscode.ConfigurationTarget.Workspace);
 await config.update('tasks.runArguments',args,vscode.ConfigurationTarget.Workspace);
 await replace(source,program);assert(source.isDirty);
 for(const action of ['check','build','run'])
  assert.equal(await vscode.commands.executeCommand(`void.${action}Project`),undefined,'dirty source must block '+action);
 events.push('dirty source blocks all project commands');
 assert(await source.save());
 const run=async(action:string,expected:number)=>{
  const exits=new Map<vscode.TaskExecution,number|undefined>();
  const listener=vscode.tasks.onDidEndTaskProcess(event=>exits.set(event.execution,event.exitCode));
  try {
   const task=await vscode.commands.executeCommand<vscode.TaskExecution>(`void.${action}Project`);
   assert(task);assert.equal(task.task.definition.profile,profile);
   const code=await until(()=>exits.get(task),()=>exits.has(task),action+' task exits');
   assert.equal(code,expected,action+' task process exit status');
  } finally {listener.dispose();}
 };
 await run('check',0);
 await run('build',0);
 const executable=path.join(root,'build',path.parse(source.uri.fsPath).name+(process.platform==='win32'?'.exe':''));
 await assert.rejects(promisify(execFile)(executable,args,{cwd:root,timeout:10000}),(error:NodeJS.ErrnoException)=>Number(error.code)===17);
 await run('run',17);
 await replace(source,'func main() -> i32:\n    return missing_value\n');assert(await source.save());
 await run('check',1);
 await until(()=>vscode.languages.getDiagnostics(source.uri),diagnostics=>diagnostics.some(d=>d.source==='VOID compiler' &&
  typeof code(d)==='string' && d.message.includes('missing_value') && d.range.start.line===1),'compiler task diagnostic in Problems at its source');
 await replace(source,program);assert(await source.save());
 await run('check',0);
 await until(()=>vscode.languages.getDiagnostics(source.uri),diagnostics=>!diagnostics.some(d=>d.source==='VOID compiler'),'successful Check clears old task diagnostics');
}
async function createTaskProject(root:string):Promise<void> {
 const compiler=path.join(path.dirname(process.env.VOIDLS_PATH!),process.platform==='win32'?'voidc.exe':'voidc');
 const execute=promisify(execFile);
 await execute(compiler,['new',root,'--nested','--source','src/launcher.void'],{timeout:10000});
 const dependency=path.join(root,'deps/example');
 await fs.mkdir(path.join(dependency,'src'),{recursive:true});
 await fs.writeFile(path.join(dependency,'void.project.toml'),"schema=1\n[project]\nid='org.test.example.project'\nname='Example'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.test.example'\nname='example'\nentry='src/api.void'\n");
 await fs.writeFile(path.join(dependency,'src/api.void'),'public func status() -> i32:\n    return 17\n');
 await fs.appendFile(path.join(root,'void.project.toml'),"\n[dependencies.example]\nid='org.test.example'\npath='deps/example'\nversion='=1.0.0'\n");
 await execute(compiler,['resolve','--project',root],{timeout:10000});
}
async function legacyRename(root:string,ready:()=>Promise<void>,running:()=>boolean):Promise<void> {
 const entry=vscode.Uri.file(path.join(root,'legacy entry.void'));
 const moved=vscode.Uri.file(path.join(root,'legacy renamed.void'));
 await fs.writeFile(entry.fsPath,'func entry(value: i32) -> i32:\n    return value\n');
 const config=vscode.workspace.getConfiguration('void');
 await config.update('project.entry',entry.fsPath,vscode.ConfigurationTarget.Workspace);
 await config.update('project.root','',vscode.ConfigurationTarget.Workspace);
 await ready();
 assert(running());
 const rename=async(from:vscode.Uri,to:vscode.Uri)=>{
  const edit=new vscode.WorkspaceEdit();edit.renameFile(from,to);assert(await vscode.workspace.applyEdit(edit));
 };
 await rename(entry,moved);
 await until(()=>vscode.languages.getDiagnostics(entry),diagnostics=>diagnostics.some(d=>code(d)==='VOID3002'),'legacy entry rename publishes missing source at old URI');
 assert(running());
 await rename(moved,entry);
 await until(()=>vscode.languages.getDiagnostics(entry),diagnostics=>!diagnostics.length,'legacy entry restoration clears diagnostics in the same editor session');
 const document=await vscode.workspace.openTextDocument(entry);await vscode.window.showTextDocument(document);
 await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',entry,new vscode.Position(1,12)),
  locations=>!!locations?.some(l=>l.uri.fsPath===entry.fsPath),'legacy navigation recovers without restarting');
 assert(running());
}
export async function run():Promise<void>{
 let passed=false;const root=process.env.VOID_TEST_ROOT!;const manifest=process.env.VOID_S3_MANIFEST!;const output=process.env.VOID_S3_HOST_EVIDENCE;let api:{ready():Promise<void>;status():{state:string;entry?:string};deactivate():Promise<void>}|undefined;
 try {
  assert(vscode.workspace.isTrusted);const toml=vscode.Uri.file(path.join(root,'void.project.toml'));let document=await vscode.workspace.openTextDocument(toml);await vscode.window.showTextDocument(document);
  const extension=vscode.extensions.getExtension('void-language.void-language');assert(extension);
  if(process.env.VOID_TEST_EXTENSIONS){
   assert.equal(path.dirname(await fs.realpath(extension.extensionPath)),await fs.realpath(process.env.VOID_TEST_EXTENSIONS));
   const readme=await fs.readFile(path.join(extension.extensionPath,'readme.md'),'utf8');
   assert(readme.includes('https://github.com/Shmellyorc/Void.Language/blob/main/docs/EDITOR_INTEGRATION.md#native-aot-source-debugging'));
   events.push('installed VSIX and its packaged README selected');
  }
  api=await extension.activate();await api!.ready();
  await until(()=>vscode.languages.getDiagnostics(toml),ds=>ds.some(d=>code(d)==='VOID5010'),'no-main project session opens with manifest diagnostics');assert.equal(api!.status().state,'running');assert.equal(api!.status().entry,undefined);
  const game=vscode.Uri.file(path.join(root,'src/game.void'));const main='use "./helper.void" as h\nfunc main() -> i32:\n    return h.value()\n';await vscode.workspace.fs.writeFile(game,Buffer.from(main));let source=await vscode.workspace.openTextDocument(game);await vscode.window.showTextDocument(source);
  await until(()=>api!.status().entry,p=>entryIs(p,game),'file creation selects any-name main');await until(()=>vscode.languages.getDiagnostics(toml),ds=>!ds.some(d=>code(d)==='VOID5010'),'missing-main diagnostic clears');
  const helper=vscode.Uri.file(path.join(root,'src/helper.void'));const point=new vscode.Position(2,14);await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',game,point),locations=>!!locations?.some(l=>l.uri.fsPath===helper.fsPath),'real editor project navigation');
  await until(()=>vscode.commands.executeCommand<vscode.DocumentSymbol[]>('vscode.executeDocumentSymbolProvider',game),symbols=>!!symbols?.some(symbol=>symbol.name==='main'),'real editor project symbols');
  const app=await unreferenced(root,source,main);
  await replace(document,'schema=1\n# é😀\n[=roject]\n');const errors=await until(()=>vscode.languages.getDiagnostics(toml),ds=>ds.some(d=>code(d)==='VOID5001'),'dirty TOML diagnostic publication');const error=errors.find(d=>code(d)==='VOID5001')!;assert.deepEqual(error.range,new vscode.Range(2,1,2,1));
  await until(()=>vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',game,point),locations=>!locations?.length,'failed manifest hides historical semantic results');
  await replace(document,manifest);await until(()=>vscode.languages.getDiagnostics(toml),ds=>!ds.some(d=>d.severity===vscode.DiagnosticSeverity.Error),'malformed manifest repair');
  const partial=main.replace('h.value()','h.');await replace(source,partial);await until(()=>vscode.commands.executeCommand<vscode.CompletionList>('vscode.executeCompletionItemProvider',game,new vscode.Position(2,13)),list=>!!list?.items.some(item=>(typeof item.label==='string'?item.label:item.label.label)==='value'),'shared compiler project recovery completion');await replace(source,main);await source.save();await document.save();
  const renamed=vscode.Uri.file(path.join(root,'src/program.void'));const rename=new vscode.WorkspaceEdit();rename.renameFile(game,renamed);assert(await vscode.workspace.applyEdit(rename));await until(()=>api!.status().entry,p=>entryIs(p,renamed),'editor rename updates entry without rebasing root');source=await vscode.workspace.openTextDocument(renamed);await vscode.window.showTextDocument(source);await vscode.commands.executeCommand('void.restartLanguageServer');await api!.ready();
  const second=vscode.Uri.file(path.join(root,'src/second.void'));await vscode.workspace.fs.writeFile(second,Buffer.from('func main() -> i32:\n    return 4\n'));const extra=await vscode.workspace.openTextDocument(second);await vscode.window.showTextDocument(extra);await until(()=>vscode.languages.getDiagnostics(toml),ds=>ds.some(d=>code(d)==='VOID5010'),'second main invalidates project selection');
  const remove=new vscode.WorkspaceEdit();remove.deleteFile(second);assert(await vscode.workspace.applyEdit(remove));await until(()=>api!.status().entry,p=>entryIs(p,renamed),'editor deletion restores unique entry');
  // Switch to a deliberately selected nested project in the same trusted window.
  const child=path.join(root,'child');
  if(process.env.VOID_TEST_PROJECT_TASKS)await createTaskProject(child);
  else {
   await fs.mkdir(path.join(child,'src'),{recursive:true});
   await fs.writeFile(path.join(child,'void.project.toml'),manifest.replaceAll('org.test','org.child'));
  }
  const childEntry=vscode.Uri.file(path.join(child,'src/launcher.void'));await fs.writeFile(childEntry.fsPath,'func main() -> i32:\n    return 9\n');const childDocument=await vscode.workspace.openTextDocument(childEntry);await vscode.window.showTextDocument(childDocument);await vscode.workspace.getConfiguration('void').update('project.root',child,vscode.ConfigurationTarget.Workspace);await api!.ready();await until(()=>api!.status().entry,p=>entryIs(p,childEntry),'deliberate project switch uses a fresh pinned session');
  const old=await vscode.commands.executeCommand<vscode.Location[]>('vscode.executeDefinitionProvider',renamed,point);assert(!old?.length,'old project queries must not reuse a previous session index');const oldSymbols=await vscode.commands.executeCommand<vscode.SymbolInformation[]>('vscode.executeWorkspaceSymbolProvider','value');assert(!oldSymbols?.some(symbol=>symbol.location.uri.fsPath===helper.fsPath),'old project symbols must not survive a switch');events.push('old project semantic results unavailable after switch');
  const oldApp=await vscode.commands.executeCommand<vscode.SemanticTokens>('vscode.provideDocumentSemanticTokens',app);assert(!oldApp?.data.length,'unreferenced semantic results must not survive a switch');
  if(process.env.VOID_TEST_PROJECT_TASKS){
   await projectTasks(child,childDocument,process.env.VOID_TEST_PROJECT_TASKS);
   await legacyRename(root,()=>api!.ready(),()=>api!.status().state==='running');
  }
  passed=true;console.log('project_editor_host: '+events.length+' checks passed');
 } finally {await api?.deactivate();if(output)await fs.writeFile(output,JSON.stringify({events,status:passed?'PASS':'INCOMPLETE',platform:process.platform,editor:vscode.version},null,2)+'\n');}
}
