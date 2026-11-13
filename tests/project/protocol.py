"""S3 real-pipe project preflight, overlays, diagnostics, navigation and file events."""
from pathlib import Path
import os,sys,json,tempfile,subprocess
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
def invoke_preflight(*operands):
 args=list(map(str,[server,'--discover-project',*operands]));p=subprocess.run(args,capture_output=True,timeout=30);rows.append({'argv':args,'status':p.returncode,'stdout':p.stdout.decode(),'stderr':p.stderr.decode(errors='replace')});return p
def preflight(workspace,active='',root='',entry=''):
 p=invoke_preflight(workspace,active,root,entry);check(p.returncode==0 and not p.stderr,p.stderr);check(len(p.stdout)<=65536,'summary bounded');return json.loads(p.stdout)
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
def unreferenced(root):
 root.mkdir();(root/'void.project.toml').write_text(manifest.replace("roots=['src']","roots=['.']"),encoding='utf-8')
 entry=root/'main.void';entry_text='private const hidden = 23\nfunc main() -> i32:\n    return hidden\n';entry.write_text(entry_text,encoding='utf-8')
 app=root/'app.void'
 body=('struct game:\n    name: string\n    running: bool\n\n'
       'func (g game) init(name: string):\n    g.name = name\n\n'
       'public func play():\n    var test = game(name = "player")\n')
 app.write_text(body,encoding='utf-8')
 doc={'textDocument':{'uri':app.as_uri()}}
 with Client(server,root,project=True) as c:
  c.initialize();before=state(c);check(before['status']=='ready','application selected before independent file opens')
  opened(c,app,body)
  check(definition(c,app,body,'game(name')['uri']==app.as_uri(),'independent constructor definition')
  hover=query(c,'textDocument/hover',dict(doc,position={'line':5,'character':6}))[0]
  check(hover and 'name' in hover['contents']['value'],'independent property hover')
  formatting=lambda spaces:query(c,'textDocument/formatting',dict(doc,options={'tabSize':4,'insertSpaces':spaces}))[0]
  tabs=formatting(False);check(len(tabs)==1 and '\tname: string' in tabs[0]['newText'],'independent compiler-validated tabs')
  change(c,app,tabs[0]['newText'],2)
  check(formatting(True)[0]['newText']==body,'independent compiler-validated spaces')
  change(c,app,body.replace('name = "player")','na'),3)
  position={'line':8,'character':22}
  signature=query(c,'textDocument/signatureHelp',dict(doc,position=position))[0]
  check(signature and signature['signatures'][0]['label']=='game(name: string)','independent incomplete constructor signature')
  items=query(c,'textDocument/completion',dict(doc,position=position))[0]
  check(any(item['label']=='name =' for item in items),'independent named argument completion')
  notes=change(c,app,'use pkg/undeclared\n'+body,4)
  check(any('Undeclared direct package import' in d['message'] for n in notes if n['uri']==app.as_uri() for d in n['diagnostics']),'independent imports retain lock policy')
  check(definition(c,app,'use pkg/undeclared\n'+body,'game(name') is None,'failed document drops previous definitions')
  after=state(c)
  check(after['entry']==before['entry'] and after['snapshotKey']==before['snapshotKey'] and after['status']=='ready','independent overlays never change application selection')
  notes=change(c,app,body+'\nfunc main() -> i32:\n    return hidden\n',5)
  check(any('hidden' in d['message'] for n in notes if n['uri']==app.as_uri() for d in n['diagnostics']),'independent module cannot see private entry symbols')
  check(state(c)['snapshotKey']==before['snapshotKey'],'a dirty main in an existing unreferenced file does not enter discovery')
  change(c,app,body,6)
  imported_entry='use "./app.void" as app\n'+entry_text.replace('    return','    app.play()\n    return')
  opened(c,entry,imported_entry)
  check(definition(c,app,body,'game(name') is not None,'same definition after explicit import')

  def app_publication(notes):
   publications=[n for n in notes if n['uri']==app.as_uri()]
   check(len(publications)==1,publications)
   return publications[0]

  unfinished=app_publication(change(c,app,'func unfinished(',7))
  check(unfinished['diagnostics'] and state(c)['status']=='error','a referenced dirty file still affects the selected graph')
  retained=app_publication(change(c,entry,entry_text,2))
  check(retained['version']==7 and retained['diagnostics']==unfinished['diagnostics'],retained)
  check(state(c)['status']=='ready','removing the import separates dirty errors from the application again')

  check(app_publication(change(c,app,body,8))['diagnostics']==[],'correction clears the independent error')
  bad_body=body.replace('game(name = "player")','missing')
  independent=app_publication(change(c,app,bad_body,9))
  check(independent['version']==9 and codes([independent])==['VOID2002']
        and "Unknown identifier 'missing'." in independent['diagnostics'][0]['message'],independent)
  check(state(c)['status']=='ready','independent error leaves the selected application ready')
  reimported=app_publication(change(c,entry,imported_entry,3))
  check(reimported['version']==9 and reimported['diagnostics']==independent['diagnostics'],reimported)
  check(int(reimported['voidProjectRevision'])>int(independent['voidProjectRevision']),'graph change advances the publication revision')
  check(state(c)['status']=='error','reimport brings the dirty error into the selected graph')

  disk_body=bad_body.replace('missing','disk_missing')
  app.write_text(disk_body,encoding='utf-8')
  check(app_publication(change(c,app,body,10))['diagnostics']==[],'valid overlay supersedes the bad disk source')
  check(state(c)['status']=='ready','valid overlay repairs the graph')
  fallback=app_publication(close(c,app))
  check('version' not in fallback and codes([fallback])==['VOID2002']
        and "Unknown identifier 'disk_missing'." in fallback['diagnostics'][0]['message'],fallback)
  check(state(c)['status']=='error','closing a referenced overlay restores disk diagnostic authority')

  reopened=app_publication(opened(c,app,bad_body,0))
  check(reopened['version']==0 and reopened['diagnostics']==independent['diagnostics'],reopened)
  check(int(reopened['voidProjectRevision'])>int(fallback['voidProjectRevision']),'reopen advances ownership at a reset client version')
  alt=root/'alternate.void'
  alt.write_text(entry_text,encoding='utf-8')
  alternate,notes=query(c,'void/reselectProject',{'entry':alt.name})
  check(alternate['entry']==alt.as_posix() and alternate['status']=='ready','explicit entry switch leaves the error outside the selected graph')
  switched=app_publication(notes)
  check(switched['version']==0 and switched['diagnostics']==reopened['diagnostics'],switched)
  original,notes=query(c,'void/reselectProject',{'entry':entry.name})
  check(original['status']=='error',original)
  switched_back=app_publication(notes)
  check(switched_back['version']==0 and switched_back['diagnostics']==reopened['diagnostics'],switched_back)

  close(c,entry)
  cleared=app_publication(close(c,app))
  check('version' not in cleared and not cleared['diagnostics'],cleared)
  check(definition(c,app,body,'game(name') is None,'closing unreferenced file drops its analysis')
  c.shutdown()
def watcher_overlays(root):
 root.mkdir()
 (root/'void.project.toml').write_text(manifest.replace("roots=['src']","roots=['.']"),encoding='utf-8')
 entry=root/'main.void'
 part=root/'part.void'
 original='value struct point:\n    count: i32\npublic func value() -> i32:\n    const p = point(count = 1)\n    return p.count\n'
 dirty=original.replace('count','dirty_count').replace('return p.dirty_count','return p.dirty_count + missing')
 replacement=original.replace('count','disk_count')
 graph='use "./part.void" as part\nfunc main() -> i32:\n    return part.value()\n'
 independent='func main() -> i32:\n    return 0\n'
 entry.write_text(graph,encoding='utf-8')
 part.write_text(original,encoding='utf-8')
 with Client(server,root,project=True,entry_override='main.void') as c:
  c.initialize()
  def watch(*events):
   c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':path.as_uri(),'type':kind} for path,kind in events]})
   return c.sync()
  def publication(notes,path):
   found=[n for n in notes if n['uri']==path.as_uri()]
   check(len(found)==1,notes)
   return found[0]
  def completion(text,field):
   line=text.splitlines()[-1]
   return query(c,'textDocument/completion',{'textDocument':{'uri':part.as_uri()},'position':{'line':len(text.splitlines())-1,'character':line.index('p.'+field)+2}})[0]
  opened(c,entry,graph)
  opened(c,part,original)
  current=publication(change(c,part,dirty,2),part)
  diagnostic=current['diagnostics']
  check(len(diagnostic)==1 and diagnostic[0]['code']=='VOID2002' and "Unknown identifier 'missing'." in diagnostic[0]['message'],current)
  part.unlink()
  retained=publication(watch((part,3)),part)
  check(retained['version']==2 and retained['diagnostics']==diagnostic,retained)
  check([item['label'] for item in completion(dirty,'dirty_count')]==['dirty_count'],'dirty completion survives watched physical deletion')
  check(not part.exists(),'analysis leaves the deleted disk source absent')
  notes=change(c,entry,independent,2)
  retained=publication(notes,part)
  check(state(c)['status']=='ready' and retained['version']==2 and retained['diagnostics']==diagnostic,'independent diagnostics survive graph removal after a watch')
  retained=publication(watch((part,3),(part,3)),part)
  check(retained['version']==2 and retained['diagnostics']==diagnostic,'duplicate delayed deletions retain the current independent diagnostic')
  notes=change(c,entry,graph,3)
  check(publication(notes,part)['diagnostics']==diagnostic,'reimport uses the current overlay once')
  notes=close(c,part)
  missing=publication(notes,entry)
  closed=publication(notes,part)
  check('version' not in closed and not closed['diagnostics'],'actual close clears the independent diagnostic')
  check(codes([missing])==['VOID3002'] and 'Cannot find imported file' in missing['diagnostics'][0]['message'],'closing a referenced virtual source exposes the missing disk import')
  check(completion(original,'count')==[],'closed missing source supplies no completion')
  part.write_text(replacement,encoding='utf-8')
  notes=watch((part,3))
  check(all(not n['diagnostics'] for n in notes) and state(c)['status']=='ready','late delete sees the restored closed source')
  check([item['label'] for item in completion(replacement,'disk_count')]==['disk_count'],'restored disk source supplies current completion')
  reopened=publication(opened(c,part,dirty,0),part)
  check(reopened['version']==0 and reopened['diagnostics']==diagnostic,'reset-version reopen restores the dirty source')
  retained=publication(watch((part,3),(part,1),(part,2)),part)
  check(retained['version']==0 and retained['diagnostics']==diagnostic,'watch batches retain the open source over replacement disk bytes')
  notes=close(c,part)
  check(all(not n['diagnostics'] for n in notes),'close falls back to the restored disk source')
  check([item['label'] for item in completion(replacement,'disk_count')]==['disk_count'],'close restores disk completion authority')
  renamed=root/'renamed.void'
  opened(c,part,dirty,1)
  part.rename(renamed)
  watch((part,3),(renamed,1))
  c.notify('workspace/didRenameFiles',{'files':[{'oldUri':part.as_uri(),'newUri':renamed.as_uri()}]})
  c.sync()
  notes=change(c,entry,graph.replace('part.void','renamed.void'),4)
  check(publication(notes,renamed)['diagnostics']==diagnostic,'explicit rename transfers the dirty source')
  close(c,part)
  c.notify('workspace/didDeleteFiles',{'files':[{'uri':renamed.as_uri()}]})
  notes=c.sync()
  check(codes([publication(notes,entry)])==['VOID3002'],'explicit deletion retains its unavailable-source contract')
  c.shutdown()
 entry.write_text(independent,encoding='utf-8')
 config=root/'void.project.toml'
 policy=config.read_text(encoding='utf-8')
 with Client(server,root,project=True,entry_override='main.void') as c:
  c.initialize()
  opened(c,config,policy,1,'toml')
  config.unlink()
  watch((config,3))
  check(state(c)['status']=='ready','watched deletion preserves the open manifest buffer')
  notes=close(c,config)
  check(codes([publication(notes,config)])==['VOID5009'],'closing the missing manifest exposes physical project invalidation')
  config.write_text(policy,encoding='utf-8')
  watch((config,3))
  check(state(c)['status']=='ready','late deletion sees the restored closed manifest')
  c.shutdown()

def preflight_boundaries(workspace):
 root=workspace/'parent';child=root/'child';child.mkdir(parents=True)
 policy=manifest.replace("roots=['src']","roots=['.']")
 root_manifest=root/'void.project.toml';root_manifest.write_bytes(policy.encode())
 child_manifest=child/'void.project.toml';child_manifest.write_bytes(policy.encode())
 entry=root/'main.void';child_source=child/'launcher.void'
 body=b'func main() -> i32:\n    return 2\n'
 entry.write_bytes(body);child_source.write_bytes(body)
 outside=workspace/'outside.void';outside.write_bytes(body)
 def pinned(active):return preflight(workspace,active,root,'main.void')
 def error(info,selected,code,message):
  check(info['schema']==2 and info['mode']=='project' and info['root']==selected.as_posix() and info['manifest']==(selected/'void.project.toml').as_posix(),'selected project stays pinned')
  check(info['status']=='error' and info['entry'] is None,'domain error cannot publish an entry')
  check(info['diagnosticCount']==1 and len(info['diagnostics'])==1,info)
  diagnostic=info['diagnostics'][0]
  check(diagnostic['code']==code and message in diagnostic['message'] and '\n' not in diagnostic['message'],diagnostic)
 error(pinned(child_source),root,'VOID5009','crosses nested project boundary')
 error(pinned(child_manifest),root,'VOID5009','Active manifest is outside')
 error(pinned(outside),root,'VOID5009','Active document is outside')
 error(pinned(root/'notes.txt'),root,'VOID3001','must name a .void source file')
 error(pinned(root/'@stdlib'/'main.void'),root,'VOID5006','exact canonical root-relative')
 error(pinned(root/'Main.void'),root,'VOID3001','case collision')
 for active in (root_manifest,root/'virtual'/'draft.void'):
  info=pinned(active);check(info['status']=='ready' and info['entry']==entry.as_posix(),info)
 nearest=preflight(workspace,child_source)
 check(nearest['status']=='ready' and nearest['root']==child.as_posix() and nearest['entry']==child_source.as_posix(),nearest)
 child_manifest.write_bytes(b'[=roject]\n')
 error(pinned(child_source),root,'VOID5009','crosses nested project boundary')
 error(pinned(child_manifest),root,'VOID5009','Active manifest is outside')
 error(preflight(workspace,child_source),child,'VOID5001','Invalid project TOML')
 child_manifest.unlink();wrong=child/'void.Project.toml';wrong.write_bytes(policy.encode())
 error(pinned(child_source),root,'VOID5009','Manifest must use exact case')
 error(pinned(wrong),root,'VOID3001','must name a .void source file')
 error(preflight(workspace,child_source),child,'VOID5009','Manifest must use exact case')
 for args,message in (((workspace,),b'Usage:'),((root,outside,'',''),b'outside selected workspace')):
  process=invoke_preflight(*args)
  check(process.returncode==2 and not process.stdout and message in process.stderr,'invocation and unpinned path discovery are process errors')
 if os.name!='nt':
  alias=root/'alias.void';alias.symlink_to(outside)
  error(pinned(alias),root,'VOID3001','Symbolic links are not supported')
  unreadable=root/'unreadable.void';unreadable.write_bytes(body);unreadable.chmod(0)
  try:
   try:unreadable.read_bytes()
   except PermissionError:error(pinned(unreadable),root,'VOID3002','Cannot read admitted project source')
   else:rows.append({'skip':'chmod did not deny this process read access'})
  finally:unreadable.chmod(0o600)
 else:
  import _winapi,stat
  alias=root/'junction';_winapi.CreateJunction(str(workspace),str(alias))
  try:error(pinned(alias/'outside.void'),root,'VOID3001','filesystem alias')
  finally:alias.rmdir()
  unreadable=root/'unreadable.void';unreadable.write_bytes(body)
  # Deny reads to another process without changing the machine or fixture ACLs.
  handle=_winapi.CreateFile(str(unreadable),_winapi.GENERIC_READ,0,0,_winapi.OPEN_EXISTING,stat.FILE_ATTRIBUTE_NORMAL,0)
  try:error(pinned(unreadable),root,'VOID3002','Cannot read admitted project source')
  finally:_winapi.CloseHandle(handle)
try:
 with tempfile.TemporaryDirectory(prefix='VOID S3 protocol é😀 ') as temporary:
  root=Path(temporary);(root/'src').mkdir();toml=root/'void.project.toml';toml.write_bytes(manifest.encode());(root/'src/helper.void').write_bytes(b'public func value() -> i32:\n    return 23\n');game=root/'src/game.void';other=root/'src/other.void'
  relative=root/'bare.void';relative.write_bytes(b'func entry(x: i32) -> i32:\n    return x\n')
  with Client(server,Path('bare.void'),cwd=root) as legacy:
   check(not any(n['diagnostics'] for n in legacy.initialize()),'legacy relative --entry initializes beside a manifest');location=query(legacy,'textDocument/definition',{'textDocument':{'uri':relative.as_uri()},'position':{'line':0,'character':6}})[0];check(location['uri']==relative.as_uri(),'legacy relative entry navigation');legacy.shutdown()
  with Client(server,relative) as legacy:
   legacy.initialize()
   opened(legacy,relative,relative.read_text(encoding='utf-8'))
   moved=relative.with_name('bare renamed.void')
   relative.rename(moved)
   legacy.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':relative.as_uri(),'type':3},{'uri':moved.as_uri(),'type':1}]})
   notes=legacy.sync()
   check(any(n['uri']==relative.as_uri() and not n['diagnostics'] for n in notes),'legacy open entry retains its buffer after a disk rename')
   position={'textDocument':{'uri':relative.as_uri()},'position':{'line':0,'character':6}}
   check(query(legacy,'textDocument/definition',position)[0]['uri']==relative.as_uri(),'legacy open entry remains navigable after a disk rename')
   notes=close(legacy,relative)
   check(any(n['uri']==relative.as_uri() and any(d['code']=='VOID3002' for d in n['diagnostics']) for n in notes),'closing the renamed legacy entry exposes its missing disk source')
   check(query(legacy,'textDocument/definition',position)[0] is None,'closed missing legacy entry invalidates its index')
   moved.rename(relative)
   legacy.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':moved.as_uri(),'type':3},{'uri':relative.as_uri(),'type':1}]})
   notes=legacy.sync()
   check(any(n['uri']==relative.as_uri() and not n['diagnostics'] for n in notes),'legacy restored entry clears diagnostic in the same server')
   check(query(legacy,'textDocument/definition',position)[0]['uri']==relative.as_uri(),'legacy restored entry can be navigated in the same server')
   legacy.shutdown()
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
   # This virtual entry has no physical file. A disk watch leaves its open buffer authoritative.
   c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':game.as_uri(),'type':3}]});c.sync();check(state(c)['entry']==game.as_posix() and state(c)['status']=='ready','watch deletion preserves the open entry')
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
  preflight_boundaries(root/'preflight')
  unreferenced(root/'unreferenced')
  watcher_overlays(root/'watcher')
 print(f'project_protocol: {checks} checks passed')
finally:
 if evidence:evidence.write_text(json.dumps({'checks':checks,'commands_and_requests':rows},indent=2)+'\n')
