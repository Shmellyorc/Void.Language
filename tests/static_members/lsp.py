"""G80: actual voidls static members, navigation, tokens and edited recovery."""
from pathlib import Path
import json
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client
SOURCE = '''struct settings:
    ## Current player limit.
    static count = 4
    private static hidden = false
    static const label = "Boss"
    instance: i32
## Adjust the shared limit.
## @param amount Players to add.
## @param label Human-readable reason.
## @return The updated limit.
static func settings.add(amount: i32, label: string = "Boss") -> i32:
    settings.hidden = false
    settings.count += amount
    return settings.count
value struct bucket<t>:
    ## Current optional payload for this concrete type.
    static item: t?
    value: t?
## Read the concrete payload.
## @return Current optional payload.
static func bucket<t>.read() -> t?:
    return bucket<t>.item
## Preserve a method-specific generic argument.
## @param value Argument to preserve.
## @return The supplied value.
static func bucket<t>.echo<u>(value: u) -> u:
    return value
func keep<u>(value: u) -> u:
    return value
func main() -> i32:
    var reason = "test"
    var result = settings.add(2, reason)
    var composed = keep(bucket<i32>.echo(7))
    var selected = keep(bucket<i32>.echo<string>(reason))
    var payload = bucket<i32>.read()
    var stored = bucket<i32>.item
    print(settings.count)
    return result - 6
'''
def point(text, needle, delta=0):
    prefix=text[:text.index(needle)+delta]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n',1)[-1]))
records=[];report=Path(sys.argv[2]).resolve()
def record(kind,value):
    records.append(dict(kind=kind,value=value));report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps(records,indent=2)+'\n')
with tempfile.TemporaryDirectory(prefix='VOID G80 LSP ') as directory:
    file=Path(directory)/'main.void';file.write_text(SOURCE);uri=file.as_uri()
    with Client(Path(sys.argv[1]).resolve(),file) as client:
        init,_=client.request('initialize',{'capabilities':{'textDocument':{'publishDiagnostics':{'versionSupport':True},'semanticTokens':{'requests':{'full':True},'formats':['relative'],'tokenTypes':['type','struct','variable','function','method','property','typeParameter','parameter'],'tokenModifiers':['declaration','definition','readonly','static']}}}})
        client.notify('initialized',{});client.sync()
        client.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'version':1,'languageId':'void','text':SOURCE}})
        notes=client.sync();record('initial',notes);assert all(not x['diagnostics'] for x in notes),notes
        def request(method,text=SOURCE,needle=None,delta=0,extra=None):
            params={'textDocument':{'uri':uri}}
            if needle is not None:params['position']=point(text,needle,delta)
            if extra:params.update(extra)
            response,_=client.request('textDocument/'+method,params);record(method,response);assert 'error' not in response,response
            return response['result']
        hover=request('hover',needle='print(settings.count)',delta=len('print(settings.')+1)
        assert 'static' in str(hover) and 'count' in str(hover) and 'i32' in str(hover),hover
        hover=request('hover',needle='settings.add(2',delta=len('settings.')+1)
        assert 'amount: i32' in str(hover) and '-> i32' in str(hover),hover
        hover=request('hover',needle='bucket<i32>.read()',delta=len('bucket<i32>.')+1)
        assert 'i32?' in str(hover) and '.void::' not in str(hover),hover
        hover=request('hover',needle='var stored = bucket<i32>.item',delta=len('var stored = bucket<i32>.')+1)
        assert 'i32?' in str(hover) and '.void::' not in str(hover),hover
        hover=request('hover',needle='static item: t?',delta=len('static ')+1)
        assert 't?' in str(hover),hover
        for needle, expected in [('var composed =', 'i32'), ('var selected =', 'string')]:
            hover=request('hover',needle=needle,delta=len('var ')+1)
            assert expected in str(hover) and '.void::' not in str(hover),hover
        nested=request('signatureHelp',needle='bucket<i32>.echo<string>(reason)',delta=len('bucket<i32>.echo<string>(')+2)
        assert nested and nested['activeParameter']==0 and 'value: string' in str(nested),nested
        signature=request('signatureHelp',needle='settings.add(2, reason)',delta=len('settings.add(2, ')+2)
        assert signature['activeParameter']==1 and 'amount: i32' in str(signature) and 'Human-readable reason' in str(signature),signature
        definition=request('definition',needle='print(settings.count)',delta=len('print(settings.')+1)
        target=definition[0] if isinstance(definition,list) else definition
        assert target and target['range']['start']['line']==2,definition
        references=request('references',needle='print(settings.count)',delta=len('print(settings.')+1,extra={'context':{'includeDeclaration':True}})
        assert len(references)>=4,references
        symbols=request('documentSymbol');assert 'count' in str(symbols) and 'add' in str(symbols),symbols
        tokens=request('semanticTokens/full');assert tokens and tokens['data'],tokens
        legend=init['result']['capabilities']['semanticTokensProvider']['legend'];line=column=0;decoded=[]
        values=tokens['data']
        for i in range(0,len(values),5):
            dl,dc,length,kind,mods=values[i:i+5];line+=dl;column=column+dc if dl==0 else dc
            decoded.append((line,column,legend['tokenTypes'][kind],[x for n,x in enumerate(legend['tokenModifiers']) if mods&(1<<n)]))
        assert any(line==2 and kind=='property' and 'static' in mods for line,col,kind,mods in decoded),decoded
        assert any(line==10 and kind=='method' and 'static' in mods for line,col,kind,mods in decoded),decoded
        for version,(probe,needle,expected,excluded) in enumerate([
            (SOURCE.replace('print(settings.count)','print(settings.)'),'print(settings.',{'count','label','add'},{'hidden','instance'}),
            (SOURCE.replace('print(settings.count)','print(bucket<i32>.)'),'print(bucket<i32>.',{'item','read','echo'},{'value'})],2):
            client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':probe}]});client.sync()
            completion=request('completion',probe,needle,len(needle));items=completion if isinstance(completion,list) else completion['items'];labels={x['label'] for x in items}
            assert expected<=labels and not excluded&labels,(expected,excluded,items)
            assert '$static' not in str(items) and '.void::' not in str(items),items
        invalid=SOURCE.replace('settings.count += amount','settings.count = "wrong"')
        client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':4},'contentChanges':[{'text':invalid}]});notes=client.sync();record('invalid',notes);assert any(x['diagnostics'] for x in notes),notes
        client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':5},'contentChanges':[{'text':SOURCE}]});notes=client.sync();record('recovered',notes);assert any(x.get('version')==5 and not x['diagnostics'] for x in notes),notes
        (file.parent/'library.void').write_text('struct remote:\n    static number = 1\nstatic func remote.bump() -> i32:\n    return remote.number\n')
        imported='use "./library.void" as lib\n'+SOURCE.replace('print(settings.count)','print(lib.remote.number)')
        client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':6},'contentChanges':[{'text':imported}]});notes=client.sync();record('imports',notes);assert all(not x['diagnostics'] for x in notes),notes
        probe=imported.replace('print(lib.remote.number)','print(lib.)')
        client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':7},'contentChanges':[{'text':probe}]});client.sync()
        completion=request('completion',probe,'print(lib.',len('print(lib.'));items=completion if isinstance(completion,list) else completion['items'];labels={x['label'] for x in items}
        assert 'remote' in labels and not {'number','bump'}&labels,items
        client.shutdown()
print('G80 LSP: static hover, signatures, definitions, references, semantic modifiers, generic completion and recovery passed')
