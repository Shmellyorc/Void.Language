"""S3 real-pipe project preflight, overlays, diagnostics, navigation and file events."""
from pathlib import Path
import sys,json,tempfile,subprocess
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client,frame
server=Path(sys.argv[1]).resolve();evidence=Path(sys.argv[2]) if len(sys.argv)>2 else None
rows=[];checks=0
manifest="schema=1\n[project]\nid='org.test.sources'\nname='Sources'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n[sources]\nroots=['src']\n"
main='use "./helper.void" as h\nfunc main() -> i32:\n    return h.value()\n'
def check(v,message):
 global checks
 checks+=1
 assert v,message
def preflight(workspace,active='',root='',entry=''):
 args=[server,'--discover-project',workspace,active,root,entry];p=subprocess.run(list(map(str,args)),capture_output=True,timeout=30);rows.append({'argv':list(map(str,args)),'status':p.returncode,'stdout':p.stdout.decode(),'stderr':p.stderr.decode()});check(p.returncode==0,p.stderr);check(len(p.stdout)<=65536,'summary bounded');return json.loads(p.stdout)
def query(c,method,params=None):
 response,notes=c.request(method,params or {});rows.append({'method':method,'params':params,'response':response,'diagnostics':notes});check('error' not in response,response);return response['result'],notes
def state(c):return query(c,'void/projectState')[0]
def opened(c,path,text,version=1,language='void'):
 c.notify('textDocument/didOpen',{'textDocument':{'uri':path.as_uri(),'languageId':language,'version':version,'text':text}});return c.sync()
def change(c,path,text,version):
 c.notify('textDocument/didChange',{'textDocument':{'uri':path.as_uri(),'version':version},'contentChanges':[{'text':text}]});return c.sync()
def close(c,path):c.notify('textDocument/didClose',{'textDocument':{'uri':path.as_uri()}});return c.sync()
def codes(notes):return [d['code'] for n in notes for d in n['diagnostics']]
def definition(c,path,text,word):
 at=text.index(word);prefix=text[:at];return query(c,'textDocument/definition',{'textDocument':{'uri':path.as_uri()},'position':{'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}})[0]
try:
 with tempfile.TemporaryDirectory(prefix='VOID S3 protocol é😀 ') as temporary:
  root=Path(temporary);(root/'src').mkdir();toml=root/'void.project.toml';toml.write_bytes(manifest.encode());(root/'src/helper.void').write_bytes(b'public func value() -> i32:\n    return 23\n');game=root/'src/game.void';other=root/'src/other.void'
  initial=preflight(root);check(initial['mode']=='project' and initial['kind']=='application' and initial['entry'] is None and initial['status']=='error','no-main preflight is a successful error-bearing summary')
  with Client(server,root,project=True) as c:
   caps=query(c,'initialize',{'capabilities':{'textDocument':{'publishDiagnostics':{'relatedInformation':True,'versionSupport':True}},'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}})[0]['capabilities'];check(caps['experimental']['voidProjectProtocol']==1 and caps['experimental']['voidToolingProtocol']==1,'separate protocol capabilities');c.notify('initialized',{});check('VOID5010' in codes(c.sync()),'initial no-main diagnostic')
   opened(c,game,main);ready=state(c);check(ready['entry']==game.as_posix() and ready['status']=='ready','virtual source entry');check(definition(c,game,main,'value()')['uri']==(root/'src/helper.void').as_uri(),'project-root import navigation')
   hover=query(c,'textDocument/hover',{'textDocument':{'uri':game.as_uri()},'position':{'line':2,'character':14}})[0];check('value()' in hover['contents']['value'],'project semantic hover')
   change(c,game,main.replace('return h.value()','return   h.value()'),2);formatted=query(c,'textDocument/formatting',{'textDocument':{'uri':game.as_uri()},'options':{'tabSize':4,'insertSpaces':True}})[0];check(len(formatted)==1 and formatted[0]['newText']==main,'project formatting uses pinned root validation');change(c,game,main,3)
   renamed_symbol=query(c,'textDocument/rename',{'textDocument':{'uri':game.as_uri()},'position':{'line':2,'character':14},'newName':'renamed_helper'})[0];check(len(renamed_symbol['documentChanges'])==2,'project symbol rename validates the full compiler graph')
   notes=opened(c,toml,'schema=1\n# é😀\n[=roject]\n',1,'toml');bad=state(c);check(bad['status']=='error' and bad['entry'] is None,'dirty malformed TOML cannot retain entry');d=next(d for n in notes for d in n['diagnostics'] if d['code']=='VOID5001');check(d['range']=={'start':{'line':2,'character':1},'end':{'line':2,'character':1}},'manifest UTF-16 range');check(definition(c,game,main,'value()') is None,'no stale successful navigation')
   change(c,toml,manifest,2);check(state(c)['status']=='ready','manifest repair');check(definition(c,game,main,'value()') is not None,'navigation recovers')
   inline="schema=1\nproject={id='org.test.sources',name='é😀',version='bad',kind='application'}\napplication={id='org.test.app'}\n";notes=change(c,toml,inline,3);d=next(d for n in notes for d in n['diagnostics'] if d['code']=='VOID5005');column=len(inline.splitlines()[1].split("'bad'")[0].encode('utf-16-le'))//2;check(d['range']=={'start':{'line':1,'character':column},'end':{'line':1,'character':column+5}},'same-line astral manifest UTF-16 range');change(c,toml,manifest,4)
   ambiguous=opened(c,other,'func main() -> i32:\n    return 4\n');check('VOID5010' in codes(ambiguous) and state(c)['entry'] is None,'second dirty main invalidates entry')
   c.notify('workspace/didDeleteFiles',{'files':[{'uri':other.as_uri()}]});c.sync();check(state(c)['entry']==game.as_posix(),'deleted dirty main tombstoned');close(c,other)
   # A watch deletion can arrive before rename. The dirty bytes are retained by
   # the open document, but never used as a deleted source entry.
   c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':game.as_uri(),'type':3}]});c.sync();check(state(c)['entry'] is None,'watch deletion hides open source')
   renamed=root/'src/renamed.void';c.notify('workspace/didRenameFiles',{'files':[{'oldUri':game.as_uri(),'newUri':renamed.as_uri()}]});c.sync();check(state(c)['entry']==renamed.as_posix(),'rename transfers owned dirty bytes');close(c,game);opened(c,renamed,main)
   check(state(c)['requiresReselection'],'entry rename requires deliberate reselection');selected=query(c,'void/reselectProject',{})[0];check(not selected['requiresReselection'],'explicit reselection')
   versions=[int(v['selectionRevision']) for v in c.project_states];check(versions==sorted(set(versions)),'published revisions strictly increase')
   previous=state(c)['selectionRevision'];change(c,toml,'[=roject]',1);check(state(c)['selectionRevision']==previous,'stale TOML version rejected')
   child=root/'src/child';child.mkdir();opened(c,child/'void.project.toml','invalid child boundary',1,'toml');notes=opened(c,child/'helper.void','func value():\n    pass\n');check('VOID5009' in codes(notes) and state(c)['status']=='error','dirty nested project denies source overlay');close(c,child/'helper.void');close(c,child/'void.project.toml');check(state(c)['status']=='ready','closing boundary overlays repairs session')
   wrong=child/'void.Project.toml';wrong.write_bytes(b'invalid boundary case');c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':wrong.as_uri(),'type':1}]});notes=c.sync();check('VOID5009' in codes(notes) and state(c)['status']=='error','wrong-case manifest watcher invalidates through compiler admission');wrong.unlink();c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':wrong.as_uri(),'type':3}]});c.sync();check(state(c)['status']=='ready','boundary removal restores selection')
   c.notify('workspace/didDeleteFiles',{'files':[{'uri':toml.as_uri()}]});c.sync();check(state(c)['status']=='error','selected manifest deletion never downgrades');change(c,toml,manifest,5);check(state(c)['status']=='ready','whole text edit repairs a tombstoned open manifest without dereference')
   # Queries queued after edits observe the latest revision; no older success
   # may overtake a malformed manifest and publish navigation.
   c.write(frame({'jsonrpc':'2.0','method':'textDocument/didChange','params':{'textDocument':{'uri':toml.as_uri(),'version':6},'contentChanges':[{'text':'[=roject]'}]}})+frame({'jsonrpc':'2.0','method':'void/projectState','id':'latest','params':{}}));response,notes=c.response('latest');check(response['result']['status']=='error' and 'VOID5001' in codes(notes),'queued edits precede current result')
   c.shutdown()
  # Disk creation/rename/deletion and nearest project selection use the compiler.
  game.write_bytes(main.encode());check(preflight(root)['entry']==game.as_posix(),'disk parity');child=root/'src/child';(child/'void.project.toml').write_bytes(manifest.replace("roots=['src']","roots=['.']").encode());(child/'launcher.void').write_bytes(b'func main() -> i32:\n    return 2\n');check(preflight(root,child/'launcher.void')['root']==child.as_posix(),'nearest manifest before legacy main');pinned=preflight(root,child/'launcher.void',root);check(pinned['status']=='error' and pinned['entry'] is None,'explicit root never follows child active document')
  legacy=root/'bare.void';legacy.write_bytes(b'func entry(x: i32) -> i32:\n    return x\n');check(preflight(root,game,'',legacy)['mode']=='legacy','entry-only setting preserves legacy mode')
  library=manifest.replace("kind='application'","kind='library'").replace("[application]\nid='org.test.app'","[package]\nid='org.test.lib'\nname='library'");toml.write_bytes(library.encode())
  with Client(server,root,project=True) as c:
   c.initialize();info=state(c);check(info['kind']=='library' and info['entry'] is None and info['status']=='ready','library session no fabricated entry');check(definition(c,game,main,'value()') is not None,'library semantic navigation');c.shutdown()
 print(f'project_protocol: {checks} checks passed')
finally:
 if evidence:evidence.write_text(json.dumps({'checks':checks,'commands_and_requests':rows},indent=2)+'\n')
