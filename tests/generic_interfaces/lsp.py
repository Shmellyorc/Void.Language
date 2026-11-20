"""Compiler-owned generic interface metadata and invalid-to-valid recovery."""
from pathlib import Path
import json
import os
import shutil
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client
source = Path(__file__).with_name('editor.void').read_text()
report = Path(sys.argv[2]).resolve()
records = []
def record(kind, value):
    records.append(dict(kind=kind, value=value))
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(records, indent=2) + '\n')
def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1]))
with tempfile.TemporaryDirectory(prefix='VOID G82 LSP ') as directory:
    file = Path(directory) / 'main.void'; file.write_text(source); uri = file.as_uri()
    shutil.copyfile(Path(__file__).with_name('api.void'), file.with_name('api.void'))
    def small_stack():
        import resource
        resource.setrlimit(resource.RLIMIT_STACK, (1048576,1048576))
    client = Client(Path(sys.argv[1]).resolve(), file, preexec_fn=small_stack if os.name=='posix' else None)
    with client:
        kinds = ['type','interface','struct','variable','function','method','property','typeParameter','parameter']
        init, _ = client.request('initialize', {'capabilities':{'textDocument':{'publishDiagnostics':{'versionSupport':True},'semanticTokens':{'requests':{'full':True},'formats':['relative'],'tokenTypes':kinds,'tokenModifiers':['declaration','readonly']}}}})
        record('initialize', init)
        client.notify('initialized', {})
        client.notify('textDocument/didOpen', {'textDocument':{'uri':uri,'languageId':'void','version':1,'text':source}})
        notes = client.sync(); record('initial_diagnostics', notes)
        assert not [d for n in notes for d in n['diagnostics'] if d.get('severity')==1], notes
        def request(method, needle=None, delta=0, **extra):
            params = {'textDocument':{'uri':uri}, **extra}
            if needle: params['position'] = point(source, needle, delta)
            response, _ = client.request('textDocument/' + method, params)
            record(method, response); assert 'error' not in response, response
            return response.get('result')
        hover = request('hover', 'interface channel', len('interface ch'))
        assert 'interface channel<t>' in str(hover).replace('\\',''), hover
        hover = request('hover', 'func read() -> t', len('func re'))
        assert '-> t' in str(hover).replace('\\','') and 'latest value' in str(hover), hover
        pair = request('hover', 'func read_pair()', len('func read_pa'))
        assert '(first: t, second: t)' in str(pair).replace('\\',''), pair
        hover = request('hover', 'return current.read()', len('return current.re'))
        assert 'i32' in str(hover) and '-> t' not in str(hover), hover
        hover = request('hover', 'const words:', len('const wo'))
        assert 'reader<string>' in str(hover).replace('\\','') and '/@pkg/' not in str(hover), hover
        signature = request('signatureHelp', 'current.write(replacement)', len('current.write(repl'))
        assert signature and signature['activeParameter']==0 and 'value: i32' in str(signature), signature
        completion = request('completion', 'return current.read()', len('return current.'))
        items = completion.get('items', []) if isinstance(completion,dict) else completion
        assert {'read','write'} <= {i['label'] for i in items}, completion
        definition = request('definition', 'return current.read()', len('return current.re'))
        location = definition[0] if isinstance(definition,list) else definition
        assert location['range']['start']['line']==6, definition
        references = request('references', 'func read() -> t', len('func re'), context={'includeDeclaration':True})
        assert len(references)>=2, references
        symbols = request('documentSymbol')
        def flatten(xs):
            return [item for x in xs for item in [x, *flatten(x.get('children', []))]]
        assert any(s['name'].startswith('channel') and s['kind']==11 for s in flatten(symbols)), symbols
        tokens = request('semanticTokens/full'); assert tokens and tokens['data'], tokens
        legend = init['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        line=column=0; decoded=[]
        for a,b,length,kind,modifiers in zip(*[iter(tokens['data'])]*5):
            line+=a; column=b if a else column+b
            decoded.append((line,column,legend[kind]))
        assert (3,10,'interface') in decoded, decoded
        for version, text in enumerate([source.replace('current.write(replacement)', 'current.write("wrong")'), source],2):
            client.notify('textDocument/didChange', {'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]})
            notes = client.sync(); record('edited_diagnostics',notes)
            assert any(n.get('version')==version for n in notes), notes
            errors = [d for n in notes if n.get('version')==version for d in n['diagnostics'] if d.get('severity')==1]
            if version==2: assert any('Type mismatch' in d['message'] for d in errors), notes
            else: assert not errors, notes
        client.shutdown()
print('G82 LSP: symbolic/concrete hover, completion, signature, navigation, references, interface symbols/tokens and recovery passed')
