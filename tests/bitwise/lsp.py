"""G81 actual LSP type information, signatures, coloring, edits and formatting."""
from pathlib import Path
import json
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client
source = Path(__file__).with_name('editor.void').read_text()
records = []
report = Path(sys.argv[2]).resolve()
def record(kind, value):
    records.append(dict(kind=kind, value=value))
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(records, indent=2) + '\n')
def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1]))
with tempfile.TemporaryDirectory(prefix='VOID G81 LSP ') as directory:
    file = Path(directory) / 'main.void'; file.write_text(source); uri = file.as_uri()
    with Client(Path(sys.argv[1]).resolve(), file) as client:
        init, _ = client.request('initialize', {'capabilities':{'textDocument':{'publishDiagnostics':{'versionSupport':True},'semanticTokens':{'requests':{'full':True},'formats':['relative'],'tokenTypes':['variable','function','parameter','type'],'tokenModifiers':['declaration','readonly']}}}})
        client.notify('initialized', {})
        client.notify('textDocument/didOpen', {'textDocument':{'uri':uri,'languageId':'void','version':1,'text':source}})
        notes = client.sync(); record('initial_diagnostics', notes)
        assert not [d for n in notes for d in n['diagnostics'] if d.get('severity')==1], notes
        def request(method, text=source, needle=None, delta=0, **extra):
            params = {'textDocument':{'uri':uri}, **extra}
            if needle: params['position'] = point(text, needle, delta)
            response, _ = client.request('textDocument/' + method, params)
            record(method, response)
            assert 'error' not in response, response
            return response.get('result')
        hover = request('hover', needle='print(result)', delta=7)
        assert 'u8' in str(hover) and '.void::' not in str(hover), hover
        hover = request('hover', needle='var complement', delta=5)
        assert 'u8' in str(hover), hover
        signature = request('signatureHelp', needle='shift_byte(1, count)', delta=len('shift_byte(1, co'))
        assert signature['activeParameter']==1 and 'count: i64' in str(signature) and 'modulo eight' in str(signature), signature
        tokens = request('semanticTokens/full'); assert tokens and tokens['data'], tokens
        definition = request('definition', needle='shift_byte(1, count)', delta=2)
        location = definition[0] if isinstance(definition,list) else definition
        assert location['range']['start']['line']==4, definition
        edits = request('formatting', options={'tabSize':4,'insertSpaces':True})
        formatted = source
        for edit in sorted(edits or [], key=lambda e:(e['range']['start']['line'],e['range']['start']['character']), reverse=True):
            def offset(p): return sum(len(l) for l in formatted.splitlines(keepends=True)[:p['line']]) + p['character']
            a, b = offset(edit['range']['start']), offset(edit['range']['end'])
            formatted = formatted[:a]+edit['newText']+formatted[b:]
        assert '<<' in formatted and '>>=' in formatted, formatted
        for version, text in enumerate([source.replace('result >>= 8', 'result >>= true'), formatted], 2):
            client.notify('textDocument/didChange', {'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]})
            notes = client.sync(); record('edited_diagnostics', notes)
            assert any(n.get('version')==version for n in notes), notes
            ds = [d for n in notes if n.get('version')==version for d in n['diagnostics'] if d.get('severity')==1]
            if version==2: assert any('Shifts require integer' in d['message'] for d in ds), notes
            else: assert not ds, notes
        hover = request('hover', text=formatted, needle='print(result)', delta=7)
        assert 'u8' in str(hover), hover
        client.shutdown()
print('G81 LSP: types, parameter help, navigation, tokens, formatting and edit recovery passed')
