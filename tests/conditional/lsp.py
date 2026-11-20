"""G87 real-pipe project conditions, inactive tokens, selection changes and recovery."""
from pathlib import Path
import atexit,hashlib,json,os,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
from test_output import output_directory
server,report=(Path(p).resolve() for p in sys.argv[1:]);records=[]
manifest="schema=1\n[project]\nid='org.test.conditional'\nname='Conditional'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.conditional.app'\n[build.conditions]\nextra=true\n"
source='''use "./api.void" as api
func main():
    print(api.value())
    compile if feature.extra:
        api.extra()
    compile else:
        print("disabled")
    compile if feature.extra:
        print(
      api.value())
    compile else:
        print(
"disabled"
)
    compile if false:
        excluded $ invalid "
'''
api=Path(__file__).with_name('api.void').read_text()
def save(kind,value):
    records.append(dict(kind=kind,value=value));report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(server_sha256=hashlib.sha256(server.read_bytes()).hexdigest(),stack_bytes=1048576,source=source,api=api,manifest=manifest,records=records),indent=2)+'\n')
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
with tempfile.TemporaryDirectory(prefix='lsp-conditional-',dir=output_directory()) as directory:
    root=Path(directory);main=root/'main.void';main.write_text(source);module=root/'api.void';module.write_text(api);toml=root/'void.project.toml';toml.write_text(manifest)
    with Client(server,root,project=True,preexec_fn=limits if os.name=='posix' else None) as client:
        atexit.register(lambda:save('process',dict(code=client.process.poll(),stderr=client.logs.decode())))
        initialized,_=client.request('initialize',{'capabilities':{'textDocument':{'semanticTokens':{'requests':{'full':True},'formats':['relative'],'tokenTypes':['comment','keyword','function'],'tokenModifiers':['inactive']},'publishDiagnostics':{'versionSupport':True}}}})
        save('initialize',initialized);client.notify('initialized',{});versions={}
        def update(file,text):
            version=versions.get(file,0)+1;versions[file]=version
            if version==1:client.notify('textDocument/didOpen',{'textDocument':{'uri':file.as_uri(),'languageId':'toml' if file==toml else 'void','version':version,'text':text}})
            else:client.notify('textDocument/didChange',{'textDocument':{'uri':file.as_uri(),'version':version},'contentChanges':[{'text':text}]})
            notes=client.sync();save('edit',notes)
            return [d for n in notes for d in n['diagnostics'] if d.get('severity')==1]
        def query(method,needle=None,delta=0,text=source,file=main,**extra):
            params={'textDocument':{'uri':file.as_uri()},**extra}
            if needle:
                before=text[:text.index(needle)+delta];params['position']={'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1])}
            response,_=client.request('textDocument/'+method,params);save(method,response);assert 'error' not in response,response
            return response.get('result')
        def tokens():
            result=query('semanticTokens/full');assert result and result['data'],result
            legend=initialized['result']['capabilities']['semanticTokensProvider']['legend'];line=column=0;decoded=[]
            for i in range(0,len(result['data']),5):
                dl,dc,length,kind,modifiers=result['data'][i:i+5];line+=dl;column=dc if dl else column+dc
                decoded.append((line,source.splitlines()[line][column:column+length],legend['tokenTypes'][kind],[name for n,name in enumerate(legend['tokenModifiers']) if modifiers&(1<<n)]))
            save('decoded',decoded);return decoded
        assert not update(main,source)
        assert 'i32' in str(query('hover','api.value()',5))
        selected=query('definition','api.value()',5);assert selected['uri']==module.as_uri() and selected['range']['start']['line']==1,selected
        assert 'i32' in str(query('hover','      api.value())',11))
        assert query('hover','excluded',3) is None
        assert query('definition','excluded',3) is None
        inactive=query('completion','excluded',3);assert not inactive or not inactive.get('items') if isinstance(inactive,dict) else not inactive,inactive
        data=tokens();assert any('excluded' in text and kind=='comment' and 'inactive' in modifiers for line,text,kind,modifiers in data),data
        assert any('compile if' in text and kind=='keyword' for line,text,kind,modifiers in data),data
        assert query('formatting',options={'tabSize':4,'insertSpaces':True})==[]
        # Completion recovery uses exactly the same manifest conditions as parsing.
        broken=source.replace('api.extra()','api.')
        assert update(main,broken)
        completion=query('completion','api.\n',4,text=broken);items=completion.get('items',[]) if isinstance(completion,dict) else completion
        assert 'extra' in {item['label'] for item in items},completion
        assert not update(main,source)
        assert not update(toml,manifest.replace('extra=true','extra=false'))
        selected=query('definition','api.value()',5);assert selected['range']['start']['line']==6,selected
        assert query('hover','      api.value())',11) is None
        data=tokens();assert any('api.extra()' in text and 'inactive' in modifiers for line,text,kind,modifiers in data),data
        # No completion or imports from an inactive branch, even while it is malformed.
        assert not update(main,broken)
        completion=query('completion','api.\n',4,text=broken);assert not completion or not completion.get('items') if isinstance(completion,dict) else not completion,completion
        assert not update(main,source)
        assert not update(toml,manifest)
        for header in ['compile if feature.:','compile if (true and):','compile else if true:','compile if target_os == "linxu":']:
            bad=source.replace('compile if feature.extra:',header)
            assert update(main,bad)
            query('semanticTokens/full');assert client.process.poll() is None
            assert not update(main,source)
        assert not update(module,api)
        assert main.read_text()==source and module.read_text()==api and toml.read_text()==manifest
        client.shutdown()
    save('exit',dict(code=client.process.returncode,stderr=client.logs.decode()));assert client.process.returncode==0 and not client.logs,client.logs
print('G87 LSP: '+str(len(records))+' records at 1 MiB')
