"""Stage 7 real-pipe feature, rewrite and ordering regressions."""
import pathlib, sys, tempfile, time, json, subprocess, os
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client, frame
server=pathlib.Path(sys.argv[1]).resolve()
def at(text, needle, delta=0):
    offset=text.index(needle)+delta;prefix=text[:offset]
    return {'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
def init(c, safe=False):
    caps={'textDocument':{'publishDiagnostics':{'versionSupport':True}},'workspace':{'workspaceEdit':{'documentChanges':safe,'failureHandling':'textOnlyTransactional'}}}
    result=c.request('initialize',{'capabilities':caps})[0]['result']['capabilities']
    assert result['hoverProvider'] and result['documentSymbolProvider'] and result['workspaceSymbolProvider']
    assert bool(result['renameProvider'])==safe
    c.notify('initialized',{});c.sync();return result
with tempfile.TemporaryDirectory(prefix='VOID Stage7 é😀 ') as directory:
    root=pathlib.Path(directory);entry=root/'main.void';worker=root/'worker.void'
    # Byte writes preserve UTF-8 and LF offsets on every platform.
    worker.write_bytes('## @param x First operand.\n## @param y Second operand.\nfunc calculate(x: i32, y: i32) -> i32:\n    return x + y\n'.encode("utf-8"))
    text='# é😀\nuse "/worker.void" as worker\nfunc entry(x: i32) -> i32:\n    var value = x\n    return worker.calculate(value, x)\n'
    entry.write_bytes(text.encode("utf-8"))
    with Client(server,entry) as c:
        init(c,True);uri=entry.as_uri();version=1
        def settext(new):
            global version
            version+=1;c.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':new}]});return c.sync()
        c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':text}});c.sync()
        def q(method,needle=None,delta=0,**more):
            params={'textDocument':{'uri':uri},**more}
            if needle is not None:params['position']=at(active[0],needle,delta)
            return c.request(method,params)[0]
        active=[text]
        hover=q('textDocument/hover','calculate')['result'];assert 'calculate(x: i32, y: i32)' in hover['contents']['value']
        assert hover['range']['start']==at(text,'calculate')
        assert q('textDocument/definition','value,')['result']['range']['start']==at(text,'value =')
        assert len(q('textDocument/references','value,',context={'includeDeclaration':True})['result'])==2
        assert {x['name'] for x in q('textDocument/documentSymbol')['result']}=={'entry','worker'}
        assert {x['name'] for x in c.request('workspace/symbol',{'query':'calc'})[0]['result']}=={'calculate'}
        for line,needle,expected in [('    return val','return val',{'value'}),('    return worker.','return worker.',{'calculate'}),('    return worker.calculate(','return worker.calculate(',{'value','x'}),('    return worker.calculate(x, ','x, ',{'value','x'})]:
            partial=text[:text.index('    return worker.calculate')]+line+'\n';active[0]=partial;settext(partial)
            response=q('textDocument/completion',needle,len(needle))['result'];labels={v['label'] for v in response};assert expected<=labels,(line,labels)
            if '(' in line:
                signature=q('textDocument/signatureHelp',needle,len(needle))['result'];assert signature and signature['activeParameter']==(1 if ', ' in line else 0),signature
            assert q('textDocument/definition','return',7)['result'] is None
        compound=text.replace('value, x','x, value + x');active[0]=compound;settext(compound)
        assert q('textDocument/signatureHelp','value + x',2)['result']['activeParameter']==1
        nested=text.replace('value, x','x, worker.calculate(value, x)');active[0]=nested;settext(nested)
        assert q('textDocument/signatureHelp','value, x',2)['result']['activeParameter']==0
        invalid_context=text.replace('var value = x','var value = missing').replace('value, x','x, val');active[0]=invalid_context;settext(invalid_context)
        assert q('textDocument/signatureHelp','x, val',len('x, val'))['result'] is None
        # Unrelated function errors do not make the current lexical scope historical.
        partial=text.replace('return worker.calculate(value, x)','return val')+'func broken() -> i32:\n    return missing\n'
        active[0]=partial;settext(partial);assert {'value'}<={v['label'] for v in q('textDocument/completion','return val',10)['result']}
        # Broken imports invalidate semantic completion context.
        broken=partial.replace('/worker.void','/missing.void');active[0]=broken;settext(broken);assert q('textDocument/completion','return val',10)['result']==[]
        active[0]=text;settext(text)
        local=q('textDocument/rename','value,',newName='amount')['result'];assert len(local['documentChanges'])==1 and len(local['documentChanges'][0]['edits'])==2
        parameter=q('textDocument/rename','x: i32',newName='input')['result'];assert len(parameter['documentChanges'][0]['edits'])==3
        assert q('textDocument/rename','x: i32',newName='value')['error']['code']==-32602
        rename=c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':at(worker.read_text(encoding='utf-8'),'calculate',2),'newName':'compute'})[0]
        assert 'result' in rename,rename
        changes=rename['result']['documentChanges'];assert len(changes)==2
        actual_versions={d['textDocument']['uri']:d['textDocument']['version'] for d in changes}
        expected_versions={uri:version,worker.as_uri():None}
        assert actual_versions==expected_versions,json.dumps({'actual':actual_versions,'expected':expected_versions},ensure_ascii=True)
        expected_edits={uri:[{'range':{'start':at(text,'calculate'),'end':at(text,'calculate',len('calculate'))},'newText':'compute'}],
                        worker.as_uri():[{'range':{'start':at(worker.read_text(encoding='utf-8'),'calculate'),'end':at(worker.read_text(encoding='utf-8'),'calculate',len('calculate'))},'newText':'compute'}]}
        assert {d['textDocument']['uri']:d['edits'] for d in changes}==expected_edits,changes
        assert q('textDocument/rename','entry',newName='changed')['error']['code']==-32602
        assert c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':at(worker.read_text(encoding='utf-8'),'calculate',2),'newName':'return'})[0]['error']['code']==-32602
        # Legal source rebinding/conflicts are rejected; server writes no files.
        assert c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':at(worker.read_text(encoding='utf-8'),'calculate',2),'newName':'entry'})[0]['error']['code']==-32602
        assert 'func calculate' in worker.read_text(encoding="utf-8")
        worker.write_bytes('func calculate(x: i32, y: i32) -> i32:\n    return x - y\n'.encode("utf-8"))
        assert c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':{'line':2,'character':7},'newName':'compute'})[0]['error']['code']==-32602
        c.request('void/refreshProject',{})
        # Disk refresh preserves a dirty overlay, including errors.
        bad=text.replace('return worker.calculate(value, x)','return missing');active[0]=bad;settext(bad)
        notes=c.request('void/refreshProject',{})[1];assert any(d['code']=='VOID2002' for n in notes for d in n['diagnostics'])
        c.notify('textDocument/didSave',{'textDocument':{'uri':uri},'text':text});notes=c.sync();assert any(d['code']=='VOID2002' for n in notes for d in n['diagnostics'])
        active[0]=text;settext(text)
        ugly=text.replace('    var value = x','  var value=x').replace('    return worker.calculate(value, x)','  return worker.calculate( value,x ) # keep é😀')
        active[0]=ugly;settext(ugly)
        formatted=q('textDocument/formatting',options={'tabSize':4,'insertSpaces':True})['result'];assert len(formatted)==1,formatted
        pretty=formatted[0]['newText'];assert '    var value = x' in pretty and '# keep é😀' in pretty
        active[0]=pretty;settext(pretty);assert q('textDocument/formatting',options={'tabSize':4,'insertSpaces':True})['result']==[]
        assert q('textDocument/formatting',options={'tabSize':4,'insertSpaces':False})['error']['code']==-32602
        c.notify('textDocument/didOpen',{'textDocument':{'uri':(root/'unreachable.void').as_uri(),'languageId':'void','version':1,'text':'func broken(:\n'}})
        assert c.request('textDocument/formatting',{'textDocument':{'uri':(root/'unreachable.void').as_uri()},'options':{'tabSize':4,'insertSpaces':True}})[0]['result']==[]
        c.notify('textDocument/didClose',{'textDocument':{'uri':(root/'unreachable.void').as_uri()}});c.sync()
        # A burst is one ordered selection; queries observe the newest version.
        packets=[]
        for n in range(20):
            version+=1;packets.append(frame({'jsonrpc':'2.0','method':'textDocument/didChange','params':{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':pretty}]}}))
        c.write(b''.join(packets));notes=c.sync();assert len(notes)<=4,len(notes)
        assert [n['version'] for n in notes if n['uri']==uri][-1]==version
        c.write(frame({'jsonrpc':'2.0','id':'cancelled','method':'workspace/symbol','params':{'query':''}})+frame({'jsonrpc':'2.0','method':'$/cancelRequest','params':{'id':'cancelled'}}))
        assert c.response('cancelled')[0]['error']['code']==-32800
        c.notify('$/cancelRequest',{'id':'reused'});assert 'result' in c.request('workspace/symbol',{'query':''},'reused')[0]
        original_worker=worker.read_bytes();worker.unlink()
        c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':worker.as_uri(),'type':3}]});assert any(n['diagnostics'] for n in c.sync())
        worker.write_bytes(original_worker)
        c.notify('workspace/didChangeWatchedFiles',{'changes':[{'uri':worker.as_uri(),'type':1}]});assert all(not n['diagnostics'] for n in c.sync())
        c.shutdown()
    # A dead editor must not leave a server holding inherited stdin open.
    parent=subprocess.Popen([sys.executable,'-c','import sys;sys.stdin.read()'],stdin=subprocess.PIPE)
    with Client(server,entry) as c:
        c.request('initialize',{'processId':parent.pid,'capabilities':{}});c.notify('initialized',{});c.sync();parent.stdin.close();parent.wait(timeout=5)
        c.finish(expected=1)
    with Client(server,entry) as c:
        init(c,False);assert c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':{'line':0,'character':7},'newName':'other'})[0]['error']['code']==-32602;c.shutdown()
    types='value struct point:\n    count: i32\n## @param first First coordinate.\n## @param second Second coordinate.\nfunc (p point) init(first: i32 = 1, second: i32 = 2):\n    p.count = first + second\nfunc (p point) change(value: i32):\n    p.count = value\n'
    valid=types+'func entry(x: i32) -> i32:\n    const p = point(second=x, first=x)\n    return p.count\n'
    entry.write_bytes(valid.encode("utf-8"))
    with Client(server,entry) as c:
        init(c,True)
        result=c.request('textDocument/signatureHelp',{'textDocument':{'uri':entry.as_uri()},'position':at(valid,'second=x',len('second='))})[0]['result']
        assert result and result['activeParameter']==1 and ' = ' in result['signatures'][0]['label'],result
        version=1
        for edited,needle,expected,excluded in [
            (valid.replace('const p = point(second=x, first=x)','var p: poi'),'var p: poi',{'point'},set()),
            (valid.replace('return p.count','return p.'),'return p.',{'count'},{'change'})]:
            c.notify('textDocument/didOpen' if version==1 else 'textDocument/didChange',
                {'textDocument':{'uri':entry.as_uri(),'version':version,**({'text':edited,'languageId':'void'} if version==1 else {})},**({} if version==1 else {'contentChanges':[{'text':edited}]})})
            version+=1
            result=c.request('textDocument/completion',{'textDocument':{'uri':entry.as_uri()},'position':at(edited,needle,len(needle))})[0]['result'];labels={x['label'] for x in result}
            assert expected<=labels and not excluded&labels,(labels,expected)
        c.shutdown()
    entry.write_bytes('const amount = 4\nfunc entry(x: i32) -> i32:\n    return x + amount\n'.encode("utf-8"))
    with Client(server,entry) as c:
        init(c,True)
        result=c.request('textDocument/rename',{'textDocument':{'uri':entry.as_uri()},'position':{'line':0,'character':7},'newName':'bonus'})[0]['result']
        assert result['documentChanges'][0]['textDocument']['version'] is None and len(result['documentChanges'][0]['edits'])==2
        assert c.request('textDocument/hover',{'textDocument':{'uri':entry.as_uri()},'position':{'line':0,'character':7}})[0]['result']['contents']['value'].endswith(' = 4')
        c.shutdown()
# Client URI spelling and selected-root aliases must survive canonical compiler paths.
def rename_uri_regression(folder):
    entry=folder/'main.void';worker=folder/'worker.void'
    text='use "/worker.void" as worker\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x)\n'
    entry.write_bytes(text.encode('utf-8'))
    worker.write_bytes(b'func calculate(x: i32) -> i32:\n    return x\n')
    # Deliberately valid spellings different from the server's normal encoder.
    client_uri=entry.as_uri().replace('file://','FILE://localhost').replace('/main.void','/%6dain.void')
    import re
    client_uri=re.sub(r'%[0-9A-Fa-f]{2}',lambda m:m[0].lower(),client_uri)
    if os.name=='nt':
        drive=client_uri.index('localhost/')+len('localhost/')
        client_uri=client_uri[:drive]+client_uri[drive].swapcase()+client_uri[drive+1:]
    def verify(c,expected):
        response=c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':{'line':0,'character':7},'newName':'compute'})[0]
        assert 'result' in response,response
        changes=response['result']['documentChanges']
        actual={d['textDocument']['uri']:d['textDocument']['version'] for d in changes}
        assert len(changes)==2 and actual==expected,json.dumps({'actual':actual,'expected':expected},ensure_ascii=True)
        edits={d['textDocument']['uri']:d['edits'] for d in changes}
        main_uri=next(u for u in expected if u!=worker.as_uri())
        assert edits=={main_uri:[{'range':{'start':at(text,'calculate'),'end':at(text,'calculate',9)},'newText':'compute'}],
                       worker.as_uri():[{'range':{'start':{'line':0,'character':5},'end':{'line':0,'character':14}},'newText':'compute'}]},edits
    with Client(server,entry) as c:
        init(c,True)
        for version in [0,7,-3]:
            if version==-3:
                c.notify('textDocument/didClose',{'textDocument':{'uri':client_uri}});c.sync()
            if version!=7:
                c.notify('textDocument/didOpen',{'textDocument':{'uri':client_uri,'version':version,'languageId':'void','text':text}})
            else:
                c.notify('textDocument/didChange',{'textDocument':{'uri':entry.resolve().as_uri(),'version':version},'contentChanges':[{'text':text}]})
            c.sync();verify(c,{client_uri:version,worker.as_uri():None})
        c.notify('textDocument/didChange',{'textDocument':{'uri':client_uri,'version':-2},'contentChanges':[{'text':text.replace('worker.calculate(x)','missing')}]})
        notes=c.sync();assert any(n['uri']==client_uri and n['diagnostics'] and n['version']==-2 for n in notes),notes
        c.notify('textDocument/didClose',{'textDocument':{'uri':client_uri}})
        notes=c.sync();assert any(n['uri']==client_uri and not n['diagnostics'] for n in notes),notes
        verify(c,{entry.as_uri():None,worker.as_uri():None})
        if folder.is_symlink():
            redirected=folder.parent/'redirected';redirected.mkdir()
            (redirected/'main.void').write_bytes(entry.read_bytes());(redirected/'worker.void').write_bytes(worker.read_bytes())
            folder.unlink();folder.symlink_to(redirected,target_is_directory=True)
            rejected=c.request('textDocument/rename',{'textDocument':{'uri':worker.as_uri()},'position':{'line':0,'character':7},'newName':'compute'})[0]
            assert rejected['error']['code']==-32602,rejected
        c.shutdown()
with tempfile.TemporaryDirectory(prefix='VOID URI é😀 # % ') as directory:
    folder=pathlib.Path(directory);rename_uri_regression(folder)
    if os.name!='nt':
        # /var -> /private/var on macOS and similar root aliases reproduce on Linux.
        real=folder/'real';real.mkdir();alias=folder/'alias';alias.symlink_to(real,target_is_directory=True)
        rename_uri_regression(alias)
print('Stage 7: completion/recovery, signatures, hover, bindings, symbols, guarded rename, formatting, refresh, batching, cancellation passed')
