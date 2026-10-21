"""Sprint 1 process-level imports, metadata recovery, docs and readonly sources."""
import pathlib, sys, tempfile, os, subprocess, json
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
def discover(workspace, active='', override=''):
    return json.loads(subprocess.check_output([server,'--discover',str(workspace),str(active),str(override)]))
def selection(result, entry, root, standalone=False):
    assert result['standalone'] is standalone,result
    for key,expected in [('entry',entry),('root',root)]:
        # Windows canonical paths may expand ancestor 8.3 names; do not fold
        # arbitrary VFS components or mask spelling changes on other platforms.
        if os.name=='nt': assert os.path.samefile(result[key],expected),result
        else: assert result[key]==str(expected),result
def reject(workspace, active='', override='', message=None):
    result=subprocess.run([server,'--discover',str(workspace),str(active),str(override)],capture_output=True,text=True)
    assert result.returncode==2,result
    if message: assert message in result.stderr,result.stderr
def source_uri(actual, expected):
    if os.name=='nt':
        from urllib.parse import urlsplit
        from urllib.request import url2pathname
        assert os.path.samefile(url2pathname(urlsplit(actual).path),expected),actual
    else: assert actual==expected.as_uri(),actual

def short_path(path):
    import ctypes
    from ctypes import wintypes
    get=ctypes.WinDLL('kernel32',use_last_error=True).GetShortPathNameW
    get.argtypes=[wintypes.LPCWSTR,wintypes.LPWSTR,wintypes.DWORD];get.restype=wintypes.DWORD
    size=get(str(path),None,0)
    if not size: return None
    buffer=ctypes.create_unicode_buffer(size)
    length=get(str(path),buffer,size)
    if not length or length>=size: return None
    return pathlib.Path(buffer.value)

with tempfile.TemporaryDirectory(prefix='VOID discovery é😀 ') as temp:
    root=pathlib.Path(temp);project=root/'project';library=root/'stdlib'
    (project/'app/systems').mkdir(parents=True);(library/'io').mkdir(parents=True)
    os.environ['VOID_STDLIB_ROOT']=str(library)
    api='## Fixture documentation.\n## @param x incoming number\nfunc read(x: i32) -> i32:\n    return x\n'
    (library/'io/files.void').write_text(api)
    (project/'app/systems/player.void').write_text(api)
    text='use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.read(x)\n'
    entry=project/'main.void';entry.write_text(text)
    active=project/'app/systems/player.void'
    selected=discover(project,active)
    selection(selected,entry,project)
    selection(discover(project),entry,project)
    nested=project/'app/main.void';nested.write_text('invalid selected source must not trigger fallback')
    selection(discover(project,active),nested,project/'app')
    selection(discover(project,active,entry),entry,project)
    nested.unlink()
    reject(project,library/'io/files.void',message='outside selected workspace')
    sibling=root/'project-other';sibling.mkdir()
    reject(project,sibling/'new.void',message='outside selected workspace')
    # Exact main spelling and descendant directory spelling remain authoritative.
    case=project/'case';case.mkdir();(case/'Main.void').write_text(api)
    reject(project,case/'new.void',message='exact case')
    if os.name=='nt': reject(project,project/'APP/systems/player.void',message='alias')
    standalone=root/'standalone';standalone.mkdir();saved=standalone/'saved.void';saved.write_text(api)
    selection(discover(standalone,saved),saved,standalone,True)
    unsaved=discover(standalone,standalone/'new.void')
    assert unsaved['standalone'] and pathlib.Path(unsaved['entry']).name=='new.void',unsaved
    selection(discover('',saved),saved,standalone,True)
    for name,target,workspace in [('inside-link',project/'app',project),('outside-link',library,project),('return-link',project,project)]:
        link=(root if name=='return-link' else project)/name
        try: link.symlink_to(target,target_is_directory=True)
        except OSError:
            if os.name!='nt': raise
            # Directory junctions do not require Windows symlink privileges.
            made=subprocess.run(['cmd','/c','mklink','/J',str(link),str(target)],capture_output=True)
            if made.returncode:
                print('SKIP: directory alias creation unavailable');continue
        try: reject(workspace,link/'new.void')
        finally:
            if link.is_symlink(): link.unlink()
            else: link.rmdir()
    if os.name=='nt':
        # Expand only the temp ancestor: descendants retain their exact VFS names.
        long_root=root.resolve();short_root=short_path(long_root)
        if short_root is None or os.path.normcase(str(short_root))==os.path.normcase(str(long_root)):
            print('SKIP: distinct Windows 8.3 spelling unavailable')
        else:
            assert os.path.samefile(short_root,long_root)
            for workspace in [long_root/'project',short_root/'project']:
                for spelling in [long_root,short_root]:
                    selection(discover(workspace,spelling/'project/app/systems/player.void'),entry,project)
                    selection(discover(workspace,spelling/'project/main.void'),entry,project)
                    reject(workspace,spelling/'project-other/new.void',message='outside selected workspace')
                    reject(workspace,spelling/'project/APP/systems/player.void',message='alias')
                    nested.write_text('invalid selected source must not trigger fallback')
                    try: selection(discover(workspace,spelling/'project/app/systems/player.void'),nested,project/'app')
                    finally: nested.unlink()
            # A short-name component *inside* the VFS remains an alias, even
            # though alternate spellings of the workspace prefix are allowed.
            long_child=project/'long-project-directory';long_child.mkdir()
            short_child=short_path(long_child.resolve())
            if short_child and short_child.name!=long_child.name:
                reject(long_root/'project',long_root/'project'/short_child.name/'new.void',message='alias')
                reject(short_root/'project',short_root/'project'/short_child.name/'new.void',message='alias')
            else: print('SKIP: descendant short-name alias unavailable')
            print('Windows short/long workspace ancestor checks passed')
    else: print('SKIP: Windows 8.3 coverage requires Windows')
    if len(sys.argv)>2:
        compiler=pathlib.Path(sys.argv[2]).resolve()
        assert 'checked successfully' in subprocess.check_output([compiler,'check',project],text=True)
        assert subprocess.check_output([compiler,'run',project,'7','--no-opt'],text=True).strip()=='7'
    with Client(server,entry.resolve()) as c:
        caps=c.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}})[0]['result']['capabilities']
        assert set([' ', '/', '"']).issubset(caps['completionProvider']['triggerCharacters'])
        c.notify('initialized',{});c.sync();uri=entry.resolve().as_uri();version=1
        c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':text}});c.sync()
        def q(method,line,char,**extra):return c.request(method,{'textDocument':{'uri':uri},'position':{'line':line,'character':char},**extra})[0]
        source_uri(q('textDocument/definition',0,7)['result']['uri'],library/'io/files.void')
        definition=q('textDocument/definition',2,19)['result'];source_uri(definition['uri'],library/'io/files.void')
        assert 'Fixture documentation.' in q('textDocument/hover',2,19)['result']['contents']['value']
        assert q('textDocument/rename',2,19,newName='renamed')['error']['code']==-32602
        stduri=definition['uri']
        if os.name=='nt':
            from urllib.parse import urlsplit,urlunsplit
            parts=urlsplit(stduri);path=parts.path
            alternate=urlunsplit(parts._replace(path=path[:1]+path[1].swapcase()+path[2:]))
            assert c.request('textDocument/hover',{'textDocument':{'uri':alternate},'position':{'line':2,'character':7}})[0]['result']
        c.notify('textDocument/didOpen',{'textDocument':{'uri':stduri,'languageId':'void','version':1,'text':'malicious overlay'}});c.sync()
        assert q('textDocument/definition',2,19)['result']==definition
        assert c.request('textDocument/formatting',{'textDocument':{'uri':stduri},'options':{'tabSize':4,'insertSpaces':True}})[0]['result']==[]
        def replace(value):
            global version
            version+=1;c.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':value}]});c.sync()
        for prefix,label,new in [('use ','io/','io/'),('use io/','files','files'),('use "','app/','app/'),('use "app/','systems/','systems/'),('use "app/systems/','player.void','player.void"')]:
            replace(prefix);response=q('textDocument/completion',0,len(prefix))['result'];assert not response['isIncomplete']
            item=next(i for i in response['items'] if i['label']==label);assert item['textEdit']['newText']==new,item
        replace('\ufeffuse "')
        bom=q('textDocument/completion',0,6)['result']['items']
        assert any(i['label']=='app/' and i['textEdit']['range']['start']['character']==6 for i in bom),bom
        replace('use "app/systems/plZZ.void" as player # é😀\r\n')
        items=q('textDocument/completion',0,19)['result']['items'];edit=next(i['textEdit'] for i in items if i['label']=='player.void')
        assert edit['range']=={'start':{'line':0,'character':17},'end':{'line':0,'character':26}},edit
        assert edit['newText']=='player.void'
        replace('use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.read(')
        sig=q('textDocument/signatureHelp',2,22)['result']['signatures'][0]
        assert 'Fixture documentation.' in sig['documentation'] and sig['parameters'][0]['documentation']=='incoming number',sig
        replace('use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.re')
        items=q('textDocument/completion',2,19)['result'];assert items[0]['label']=='read' and 'Fixture documentation.' in items[0]['documentation'],items
print('Stage 8 compiler-backed LSP discovery/docs/readonly checks passed')
