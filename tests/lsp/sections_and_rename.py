"""Exercise live section edits and transactional compiler-owned rename."""
from pathlib import Path
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
worker_source = '''public value struct packet:
    public amount: i32
public func (item packet) measure() -> i32:
    return item.amount
'''
source = '''use "./worker.void" as worker
# section Header 😀
enum token:
    missing
    present(value: i32)
func identity<t>(value: t) -> t:
    return value
func entry(input: i32) -> i32:
    # section Local
    var item = worker.packet(amount = input)
    var captured = item.measure()
    var callback = func() -> i32:
        print("😀", captured)
        return captured
    var wrapped = token.present(callback())
    return match wrapped:
        token.present(var result): identity<i32>(value = result)
        token.missing: 0
    # endsection
# endsection
# endsection
# section
# section Unclosed
'''

with TemporaryDirectory(prefix='void-sections-') as temporary:
    root = Path(temporary).resolve()
    worker = root / 'worker.void'
    entry = root / 'main.void'
    worker.write_text(worker_source, encoding='utf-8')
    entry.write_text(source, encoding='utf-8')
    with Client(server, entry) as client:
        response, _ = client.request('initialize', {
            'capabilities': {
                'textDocument': {'documentSymbol': {'hierarchicalDocumentSymbolSupport': True}},
                'workspace': {'workspaceEdit': {'documentChanges': True, 'failureHandling': 'textOnlyTransactional'}}
            }})
        assert response['result']['capabilities']['foldingRangeProvider']
        client.notify('initialized', {})
        client.sync()
        document = {'uri': entry.as_uri()}
        client.notify('textDocument/didOpen', {'textDocument': {
            **document, 'languageId': 'void', 'version': 1, 'text': source}})
        client.sync()

        def query(method, **params):
            return client.request('textDocument/' + method, {'textDocument': document, **params})[0]

        def position(fragment, delta=0):
            offset = source.index(fragment) + delta
            prefix = source[:offset]
            return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}

        def regions():
            return [fold for fold in query('foldingRange')['result'] if fold.get('kind') == 'region']

        assert any(fold.get('kind') is None for fold in query('foldingRange')['result'])
        assert regions() == [
            {'startLine': 1, 'endLine': 19, 'kind': 'region'},
            {'startLine': 8, 'endLine': 18, 'kind': 'region'},
        ]
        symbols = query('documentSymbol')['result']
        title = next(item for item in symbols if item['name'] == 'Header 😀')
        assert title['selectionRange']['end']['character'] == 19, title
        assert any(item['name'] == 'entry' for item in symbols), symbols

        def expect_rename(fragment, delta, replacement, counts):
            point = position(fragment, delta)
            response = query('prepareRename', position=point)
            assert 'result' in response and response['result'], response
            response = query('rename', position=point, newName=replacement)
            assert 'result' in response, response
            changes = response['result']['documentChanges']
            assert {change['textDocument']['uri']: len(change['edits']) for change in changes} == counts, changes
            for change in changes:
                info = change['textDocument']
                assert info['version'] == (1 if info['uri'] == entry.as_uri() else None), info
                assert all(edit['newText'] == replacement for edit in change['edits'])
            return changes

        local = {entry.as_uri(): 1, worker.as_uri(): 2}
        expect_rename('item.measure()', len('item.'), 'read_value', {entry.as_uri(): 1, worker.as_uri(): 1})
        expect_rename('worker.packet(', len('worker.'), 'record', local)
        expect_rename('amount =', 0, 'number', {entry.as_uri(): 1, worker.as_uri(): 2})
        expect_rename('token.present(', len('token.'), 'delivered', {entry.as_uri(): 3})
        expect_rename('identity<i32>', 0, 'preserve', {entry.as_uri(): 2})
        expect_rename('identity<t>', len('identity<'), 'element', {entry.as_uri(): 3})
        expect_rename('value = result', 0, 'element', {entry.as_uri(): 3})
        expect_rename('use "./worker.void" as worker', len('use "./worker.void" as '), 'helpers', {entry.as_uri(): 2})
        changes = expect_rename('captured =', 0, 'observed', {entry.as_uri(): 3})
        positions = [edit['range']['start'] for edit in changes[0]['edits']]
        assert position('print("😀", captured)', len('print("😀", ')) in positions, positions
        rejected = query('rename', position=position('captured ='), newName='item')
        assert rejected['error']['code'] == -32602, rejected

        # The current buffer owns the section text even when semantic analysis
        # fails, and a closing line typed halfway through is an ordinary comment.
        incomplete = source.replace('# section Local', '# sect').replace('worker.packet(', 'worker.missing(')
        client.notify('textDocument/didChange', {'textDocument': {**document, 'version': 2},
                      'contentChanges': [{'text': incomplete}]})
        assert regions() == [{'startLine': 1, 'endLine': 18, 'kind': 'region'}]
        assert any(item['name'] == 'Header 😀' for item in query('documentSymbol')['result'])
        rejected = query('rename', position=position('captured ='), newName='observed')
        assert rejected['error']['code'] == -32602, rejected
        client.sync()
        client.shutdown()
families = """interface readable:
    func read() -> i32
struct base:
    n: i32
virtual func (item base) read() -> i32:
    return item.n
struct child extends base:
    offset: i32
override func (item child) read() -> i32:
    return item.n + item.offset
property (item child) amount: i32:
    get:
        return item.n
    set(value):
        item.n = value
func entry(input: i32) -> i32:
    var object = child()
    object.amount = input
    var view: readable = object
    return view.read() + object.read() + object.amount
"""
with TemporaryDirectory(prefix='void-rename-family-') as temporary:
    file = Path(temporary).resolve() / 'main.void'
    file.write_text(families, encoding='utf-8')
    with Client(server, file) as client:
        client.request('initialize', {'capabilities': {
            'workspace': {'workspaceEdit': {'documentChanges': True, 'failureHandling': 'textOnlyTransactional'}}}})
        client.notify('initialized', {})
        client.sync()
        for name, line, expected in [('read', 1, {1, 4, 8, 19}), ('amount', 10, {10, 17, 19}),
                                     ('base', 2, {2, 4, 6}), ('readable', 0, {0, 18})]:
            column = families.splitlines()[line].index(name)
            result, _ = client.request('textDocument/rename', {'textDocument': {'uri': file.as_uri()},
                'position': {'line': line, 'character': column}, 'newName': 'replacement'})
            assert 'result' in result, (name, result)
            edits = result['result']['documentChanges'][0]['edits']
            assert {edit['range']['start']['line'] for edit in edits} == expected, (name, edits)
            assert len(edits) == len(expected) + (name == 'read'), (name, edits)
        client.shutdown()
print('Live section folding/navigation and transactional rename passed')
