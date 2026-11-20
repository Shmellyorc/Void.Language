"""G83 ordinary provider metadata, builtin operators and current edit diagnostics."""
from pathlib import Path
import json,os,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
source=Path(__file__).with_name('editor.void').read_text();records=[];report=Path(sys.argv[2]).resolve()
def record(kind,value):
    records.append(dict(kind=kind,value=value));report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps(records,indent=2)+'\n')
def point(needle,delta=0):
    text=source[:source.index(needle)+delta];return dict(line=text.count('\n'),character=len(text.rsplit('\n',1)[-1]))
def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
with tempfile.TemporaryDirectory(prefix='VOID G83 LSP ') as directory:
    file=Path(directory)/'main.void';file.write_text(source);uri=file.as_uri()
    with Client(Path(sys.argv[1]).resolve(),file,preexec_fn=stack if os.name=='posix' else None) as client:
        init,_=client.request('initialize',{'capabilities':{'textDocument':{'publishDiagnostics':{'versionSupport':True},'semanticTokens':{'requests':{'full':True},'formats':['relative'],'tokenTypes':['type','struct','variable','function','method','property','parameter','operator'],'tokenModifiers':['declaration','readonly']}}}})
        client.notify('initialized',{});client.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':1,'text':source}})
        notes=client.sync();record('initial',notes);assert not [d for n in notes for d in n['diagnostics'] if d.get('severity')==1],notes
        def request(method,needle=None,delta=0,**extra):
            params={'textDocument':{'uri':uri},**extra}
            if needle:params['position']=point(needle,delta)
            response,_=client.request('textDocument/'+method,params);record(method,response);assert 'error' not in response,response
            assert '$collection_equal' not in str(response) and '/@pkg/' not in str(response),response
            return response.get('result')
        assert 'bool' in str(request('hover','var equal',5))
        assert 'bool' in str(request('hover','var different',5))
        assert '[i32]' in str(request('hover','var left',5)).replace('\\','')
        signature=request('signatureHelp','same(left, right)',len('same(left, ri'))
        assert signature and signature['activeParameter']==1 and 'right: readonly [i32]' in str(signature),signature
        completion=request('completion','return left == right',len('return left '))
        items=completion.get('items',[]) if isinstance(completion,dict) else completion
        assert {'==','!='}<={i['label'] for i in items},completion
        definition=request('definition','same(left, right)',2);location=definition[0] if isinstance(definition,list) else definition
        assert location['range']['start']['line']==4,definition
        references=request('references','var left',5,context={'includeDeclaration':True});assert len(references)>=3,references
        operators=request('references','operator ==',len('operator '),context={'includeDeclaration':True});assert len(operators)>=2,operators
        tokens=request('semanticTokens/full');assert tokens and tokens['data'],tokens
        symbols=request('documentSymbol');assert '$collection' not in str(symbols) and '$enum' not in str(symbols),symbols
        for version,text in [(2,source.replace('const right: [i32]','const right: [bool]')),(3,source)]:
            client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]});notes=client.sync();record('edit',notes)
            assert any(n.get('version')==version for n in notes),notes
            errors=[d for n in notes if n.get('version')==version for d in n['diagnostics'] if d.get('severity')==1]
            if version==2:assert any('matching collection and element types' in d['message'] for d in errors),notes
            else:assert not errors,notes
        client.shutdown()
print('G83 LSP: bool/types, operator completion, custom references, signatures, navigation, tokens and recovery passed')
