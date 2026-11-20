"""G86 live async protocol: inferred binders, generic declarations and incomplete loops."""
from pathlib import Path
import atexit,hashlib,json,os,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools'))
from lsp_client import Client
from test_output import output_directory
server,report=(Path(p).resolve() for p in sys.argv[1:]);records=[]
source=Path(__file__).with_name('editor.void').read_text()
api=(Path(__file__).resolve().parents[3]/'examples/async/advanced/sequence.void').read_text()
def save(kind,value):
    records.append(dict(kind=kind,value=value));report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(server_sha256=hashlib.sha256(server.read_bytes()).hexdigest(),
        stack_bytes=1048576,source=source,api=api,records=records),indent=2)+'\n')
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
with tempfile.TemporaryDirectory(prefix='lsp-',dir=output_directory()) as directory:
    root=Path(directory);main=root/'main.void';main.write_text(source);module=root/'api.void';module.write_text(api)
    with Client(server,main,preexec_fn=limits if os.name=='posix' else None) as client:
        atexit.register(lambda: save('process',dict(code=client.process.poll(),stderr=client.logs.decode())))
        initialize,_=client.request('initialize',{'initializationOptions':{'voidInteractivePresentation':True},'capabilities':{'textDocument':{'publishDiagnostics':{'versionSupport':True},'semanticTokens':{'requests':{'full':True},'formats':['relative'],'tokenTypes':['struct','variable','parameter','property','method'],'tokenModifiers':['declaration']}}}})
        save('initialize',initialize);client.notify('initialized',{})
        versions={}
        def update(file,text):
            version=versions.get(file,0)+1;versions[file]=version
            if version==1:client.notify('textDocument/didOpen',{'textDocument':{'uri':file.as_uri(),'languageId':'void','version':1,'text':text}})
            else:client.notify('textDocument/didChange',{'textDocument':{'uri':file.as_uri(),'version':version},'contentChanges':[{'text':text}]})
            notes=client.sync();save('edit',notes)
            return [d for n in notes if n.get('version')==version for d in n['diagnostics'] if d.get('severity')==1]
        def query(method,needle=None,delta=0,text=source,file=main,**extra):
            params={'textDocument':{'uri':file.as_uri()},**extra}
            if needle:
                before=text[:text.index(needle)+delta];params['position']={'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1])}
            response,_=client.request('textDocument/'+method,params);save(method,response);assert 'error' not in response,response
            if method in ('hover','signatureHelp','completion'):assert not any(t in str(response) for t in ('.void::','/@pkg/','$iterator$','$task:')),response
            return response.get('result')
        assert not update(main,source)
        for needle,delta,expected in [('for await element',11,'i32'),('element, key in',10,'i64'),('var state',5,'cursor'),('var yielded',5,'option'),('var pending',5,'task<option<i32>>')]:
            hover=query('hover',needle,delta);assert expected in str(hover),hover
        help=query('signatureHelp','values.async_iterator()',len('values.async_iterator('))
        assert help and 'cursor<i32>' in str(help),help
        definitions=query('definition','values.async_iterator()',len('values.async_ite'))
        locations=definitions if isinstance(definitions,list) else [definitions]
        assert any(x['uri']==module.as_uri() and x['range']['start']['line']==api[:api.index('func (self sequence<t>) async_iterator')].count('\n') for x in locations),definitions
        references=query('references','values.async_iterator()',len('values.async_ite'),context={'includeDeclaration':True})
        assert len(references)>=2,references
        tokens=query('semanticTokens/full');assert tokens and tokens['data'],tokens
        legend=initialize['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        line=column=0;decoded=[]
        data=tokens['data'];lines=source.splitlines()
        for i in range(0,len(data),5):
            dl,dc,length,kind,modifiers=data[i:i+5];line+=dl;column=dc if dl else column+dc
            decoded.append((lines[line][column:column+length],legend[kind],modifiers))
        for word,kind in [('async_iterator','method'),('next','method'),('pending','variable'),('element','variable'),('key','variable')]:
            assert any(w==word and k==kind for w,k,m in decoded),(word,decoded)
        # Collection completion in a partially written loop header.
        broken=source.replace('for await repeated in values:', 'for await repeated in values.:')
        assert update(main,broken)
        completion=query('completion','values.:',len('values.'),text=broken)
        items=completion.get('items',[]) if isinstance(completion,dict) else completion
        assert 'async_iterator' in {i['label'] for i in items},completion
        assert not update(main,source)
        # Loop body binders retain type/member assistance in an unsaved edit.
        broken=source.replace('print(element, key)','print(element.)')
        assert update(main,broken)
        query('completion','element.)',len('element.'),text=broken)
        assert not update(main,source)
        # Invalid protocols and incomplete headers must preserve server liveness.
        for broken in [source.replace('for await repeated in values:', 'for await repeated in [1, 2]:'),
                       source.replace('for await repeated in values:', 'for await repeated in :'),
                       source.replace('for await repeated in values:', 'for await :'),
                       source.replace('for await repeated in values:', 'for await repeated, in values:')]:
            assert update(main,broken)
            query('semanticTokens/full')
            assert not update(main,source)
        assert not update(module,api)
        broken_api=api.replace('next() -> option<t>','next() -> t')
        assert update(module,broken_api)
        assert client.process.poll() is None
        assert not update(module,api)
        assert 'i32' in str(query('hover','for await element',11))
        assert main.read_text()==source and module.read_text()==api
        client.shutdown()
    save('exit',dict(code=client.process.returncode,stderr=client.logs.decode()))
    assert client.process.returncode==0 and not client.logs,client.logs
print('G86 LSP: '+str(len(records))+' records; generic protocol/binders, navigation, signatures and recovery passed at 1 MiB')
