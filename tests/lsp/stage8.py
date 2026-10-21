"""Sprint 1 process-level imports, metadata recovery, docs and readonly sources."""
import pathlib, sys, tempfile, os, subprocess, json
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID discovery é😀 ') as temp:
    root=pathlib.Path(temp);project=root/'project';library=root/'stdlib'
    (project/'app/systems').mkdir(parents=True);(library/'io').mkdir(parents=True)
    os.environ['VOID_STDLIB_ROOT']=str(library)
    api='## Fixture documentation.\n## @param x incoming number\nfunc read(x: i32) -> i32:\n    return x\n'
    (library/'io/files.void').write_text(api)
    (project/'app/systems/player.void').write_text(api)
    text='use io/files as files\nfunc entry(x: i32) -> i32:\n    return files.read(x)\n'
    entry=project/'main.void';entry.write_text(text)
    selected=json.loads(subprocess.check_output([server,'--discover',str(project),str(project/'app/systems/player.void'),'']))
    assert selected=={'entry':str(entry),'root':str(project),'standalone':False},selected
    if len(sys.argv)>2:
        compiler=pathlib.Path(sys.argv[2]).resolve()
        assert 'checked successfully' in subprocess.check_output([compiler,'check',project],text=True)
        assert subprocess.check_output([compiler,'run',project,'7','--no-opt'],text=True).strip()=='7'
    with Client(server,entry) as c:
        caps=c.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}})[0]['result']['capabilities']
        assert set([' ', '/', '"']).issubset(caps['completionProvider']['triggerCharacters'])
        c.notify('initialized',{});c.sync();uri=entry.as_uri();version=1
        c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':text}});c.sync()
        def q(method,line,char,**extra):return c.request(method,{'textDocument':{'uri':uri},'position':{'line':line,'character':char},**extra})[0]
        assert q('textDocument/definition',0,7)['result']['uri']==(library/'io/files.void').as_uri()
        definition=q('textDocument/definition',2,19)['result'];assert definition['uri']==(library/'io/files.void').as_uri(),definition
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
