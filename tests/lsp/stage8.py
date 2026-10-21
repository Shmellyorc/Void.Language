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
        else: assert result[key]==str(expected.resolve()),result
def reject(workspace, active='', override='', message=None):
    result=subprocess.run([server,'--discover',str(workspace),str(active),str(override)],capture_output=True,text=True,encoding="utf-8")
    assert result.returncode==2,result
    if message: assert message in result.stderr,result.stderr
def source_uri(actual, expected):
    if os.name=='nt':
        from urllib.parse import urlsplit
        from urllib.request import url2pathname
        assert os.path.samefile(url2pathname(urlsplit(actual).path),expected),actual
    else: assert actual==expected.resolve().as_uri(),actual

def drive_spellings(path):
    drive=path.drive
    if len(drive)!=2 or drive[1]!=':': return [path]
    return [pathlib.Path(letter+str(path)[1:]) for letter in [drive[0].upper(),drive[0].lower()]]

def windows_path(function, path):
    import ctypes
    from ctypes import wintypes
    get=getattr(ctypes.WinDLL('kernel32',use_last_error=True),function)
    get.argtypes=[wintypes.LPCWSTR,wintypes.LPWSTR,wintypes.DWORD];get.restype=wintypes.DWORD
    size=get(str(path),None,0)
    if not size: return None
    buffer=ctypes.create_unicode_buffer(size)
    length=get(str(path),buffer,size)
    if not length or length>=size: return None
    return pathlib.Path(buffer.value)

def short_path(path): return windows_path('GetShortPathNameW',path)

with tempfile.TemporaryDirectory(prefix='VOID discovery é😀 # % ') as temp:
    root=pathlib.Path(temp);project=root/'project';library=root/'stdlib'
    (project/'app/systems').mkdir(parents=True);(library/'io').mkdir(parents=True)
    os.environ['VOID_STDLIB_ROOT']=str(library)
    api='## Fixture documentation.\n## @param x incoming number\nfunc read(x: i32) -> i32:\n    return x\n'
    (library/'io/files.void').write_text(api,encoding='utf-8')
    (project/'app/systems/player.void').write_text(api,encoding='utf-8')
    text='use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.read(x)\n'
    entry=project/'main.void';entry.write_text(text,encoding='utf-8')
    active=project/'app/systems/player.void'
    selected=discover(project,active)
    selection(selected,entry,project)
    selection(discover(project),entry,project)
    nested=project/'app/main.void';nested.write_text('invalid selected source must not trigger fallback',encoding='utf-8')
    selection(discover(project,active),nested,project/'app')
    selection(discover(project,active,entry),entry,project)
    nested.unlink()
    reject(project,library/'io/files.void',message='outside selected workspace')
    sibling=root/'project-other';sibling.mkdir()
    reject(project,sibling/'new.void',message='outside selected workspace')
    # Exact main spelling and descendant directory spelling remain authoritative.
    case=project/'case';case.mkdir();(case/'Main.void').write_text(api,encoding='utf-8')
    reject(project,case/'new.void',message='exact case')
    if os.name=='nt': reject(project,project/'APP/systems/player.void',message='alias')
    standalone=root/'standalone';standalone.mkdir();saved=standalone/'saved.void';saved.write_text(api,encoding='utf-8')
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
        try:
            reject(workspace,link/'new.void')
            if os.name=='nt':
                for w in drive_spellings(workspace):
                    for a in drive_spellings(link/'new.void'): reject(w,a)
        finally:
            if link.is_symlink(): link.unlink()
            else: link.rmdir()
    if os.name=='nt':
        # Expand only the temp ancestor: descendants retain their exact VFS names.
        long_root=root.resolve();short_root=short_path(long_root)
        roots=[long_root]
        has_short=short_root is not None and os.path.normcase(str(short_root))!=os.path.normcase(str(long_root))
        if has_short:
            assert os.path.samefile(short_root,long_root);roots.append(short_root)
        else: print('SKIP: distinct Windows 8.3 spelling unavailable')
        spellings=[p for r in roots for p in drive_spellings(r)]
        # Keep redirected Windows cp1252 test logs safe without losing scalars.
        print('Windows drive discovery fixture:',json.dumps({'original':str(root),'canonical':str(long_root),
            'expanded_lower':str(windows_path('GetLongPathNameW',drive_spellings(long_root)[-1]))},ensure_ascii=True))
        for w in spellings:
            for a in spellings:
                selection(discover(w/'standalone',a/'standalone/saved.void'),saved,standalone,True)
                pending=discover(w/'standalone',a/'standalone/new.void')
                assert pending['standalone'] and pathlib.Path(pending['entry']).name=='new.void',pending
                assert os.path.samefile(pending['root'],standalone) and os.path.samefile(pathlib.Path(pending['entry']).parent,standalone),pending
        for workspace in [p/'project' for p in spellings]:
            selection(discover(workspace),entry,project)
            for spelling in spellings:
                selection(discover(workspace,spelling/'project/app/systems/player.void'),entry,project)
                selection(discover(workspace,spelling/'project/main.void'),entry,project)
                reject(workspace,spelling/'project-other/new.void',message='outside selected workspace')
                reject(workspace,spelling/'project/APP/systems/player.void',message='alias')
                selection(discover(workspace,spelling/'project/app/systems/player.void',spelling/'project/main.void'),entry,project)
                nested.write_text('invalid selected source must not trigger fallback',encoding='utf-8')
                try: selection(discover(workspace,spelling/'project/app/systems/player.void'),nested,project/'app')
                finally: nested.unlink()
        # An unrelated drive must stay outside even if SUBST maps the same files.
        import ctypes
        drives=ctypes.WinDLL('kernel32').GetLogicalDrives()
        assert drives,'GetLogicalDrives failed'
        unused=next((chr(65+n)+':' for n in range(25,-1,-1) if not (drives & (1<<n))),None)
        if unused:
            mapped=subprocess.run(['subst',unused,str(long_root)],capture_output=True)
            if mapped.returncode==0:
                try:
                    for workspace in [p/'project' for p in spellings]:
                        reject(workspace,pathlib.Path(unused+'/project/main.void'),message='outside selected workspace')
                finally: subprocess.run(['subst',unused,'/D'],check=True,capture_output=True)
                print('Windows unrelated mapped-drive rejection passed')
            else: print('SKIP: disposable SUBST drive unavailable')
        else: print('SKIP: no unused drive for SUBST coverage')
        # Always reject another drive, including when mapping is unavailable.
        other='D' if long_root.drive[:1].upper()!='D' else 'C'
        reject(long_root/'project',pathlib.Path(other+str(long_root/'project/main.void')[1:]))
        if has_short:
            # Short-name descendants remain aliases, regardless of drive case.
            long_child=project/'long-project-directory';long_child.mkdir()
            short_child=short_path(long_child.resolve())
            if short_child and short_child.name!=long_child.name:
                for spelling in spellings:
                    reject(spelling/'project',spelling/'project'/short_child.name/'new.void',message='alias')
            else: print('SKIP: descendant short-name alias unavailable')
            print('Windows short/long workspace ancestor checks passed')
        print('Windows upper/lower drive workspace and active checks passed')
    else: print('SKIP: Windows 8.3 coverage requires Windows')
    if os.name!='nt':
        # Model macOS /var -> /private/var on every POSIX host. The alias is
        # above the workspace; aliases within the VFS remain forbidden.
        alias=root/'temp-alias';alias.symlink_to(root.resolve(),target_is_directory=True)
        inside=project/'inside-alias';inside.symlink_to(project.resolve(),target_is_directory=True)
        outside=project/'outside-alias';outside.symlink_to(library.resolve(),target_is_directory=True)
        back=project/'parent-alias';back.symlink_to(root.resolve(),target_is_directory=True)
        try:
            for workspace in [project.resolve(),alias/'project']:
                for spelling in [root.resolve(),root,alias]:
                    selection(discover(workspace,spelling/'project/app/systems/player.void'),entry,project)
                    selection(discover(workspace,spelling/'project/main.void'),entry,project)
                    reject(workspace,spelling/'project-other/new.void',message='outside selected workspace')
                    reject(workspace,spelling/'project/inside-alias/app/systems/player.void',message='alias')
                    reject(workspace,spelling/'project/outside-alias/io/files.void',message='alias')
                    reject(workspace,spelling/'project/parent-alias/project/new.void',message='alias')
                    reject(workspace,spelling/'project/APP/systems/player.void')
                    reject(workspace,spelling/'project/case/new.void',message='exact case')
                    nested.write_text('invalid selected source must not trigger fallback',encoding='utf-8')
                    try: selection(discover(workspace,spelling/'project/app/systems/player.void'),nested,project/'app')
                    finally: nested.unlink()
        finally:
            back.unlink();outside.unlink();inside.unlink();alias.unlink()
        print('POSIX workspace ancestor alias and descendant rejection checks passed')
    if len(sys.argv)>2:
        compiler=pathlib.Path(sys.argv[2]).resolve()
        unicode_entry=standalone/'entrée😀.void';unicode_entry.write_text(api.replace('func read(', 'func entry('),encoding='utf-8')
        # Exercise native argv for directories, files, supplementary characters,
        # both JIT modes, the Debug cache path and native object output paths.
        def cli(*args):
            return subprocess.check_output([compiler,*args],text=True,encoding='utf-8',timeout=60)
        for target in [project,entry]:
            assert 'checked successfully' in cli('check',target)
            for flags in [[],['--no-opt']]:
                assert cli('run',target,'7',*flags).strip()=='7'
        for _ in range(2):assert cli('run',entry,'7','--cache').strip()=='7'
        for flags in [[],['--no-opt']]:
            obj=root/('résultat😀'+str(len(flags))+'.o')
            cli('emit',project,'-o',obj,*flags);assert obj.is_file() and obj.stat().st_size>0
        proof=root/'preuve😀.o';cli('--emit-proof',proof);assert proof.is_file() and proof.stat().st_size>0
        # Unicode ancestors are supported; VFS module basenames remain ASCII.
        invalid=subprocess.run([compiler,'check',unicode_entry],capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert invalid.returncode==1 and 'Unicode module filenames are deferred' in invalid.stderr,invalid
        missing=project/'absent😀.void'
        failure=subprocess.run([compiler,'check',missing],capture_output=True,text=True,encoding='utf-8',timeout=60)
        assert failure.returncode==1 and missing.name in failure.stderr,failure
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
