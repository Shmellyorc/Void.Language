"""Full semantic tokens from the real server and its ordered project snapshots."""
from pathlib import Path
from tempfile import TemporaryDirectory
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame

server = Path(sys.argv[1]).resolve()
method = 'textDocument/semanticTokens/full'
types = ['namespace', 'type', 'struct', 'interface', 'enum', 'typeParameter', 'parameter',
         'variable', 'property', 'enumMember', 'event', 'function', 'method', 'operator']
modifiers = ['declaration', 'definition', 'readonly', 'static', 'defaultLibrary']
defines = ('declaration', 'definition')
manifest = """schema=1
[project]
id='org.test.semantic'
name='Tokens'
version='1.0.0'
kind='application'
[application]
id='org.test.semantic'
[sources]
roots=['src']
"""
helper_text = '''public func print(n: i32) -> i32:
    return n
func twice(n: i32) -> i32:
    return print(n) + print(n)
public value struct size:
    value: i32
'''
source = '''use console
use "./helper.void" as lib
use "./plain.void"

struct point:
    value: i32
func (p point) move(delta: i32) -> i32:
    return p.value + delta
property (p point) size: i32:
    get:
        return p.value

value struct step:
    const move: i32
func (s step) distance() -> i32:
    return s.move
func (s step) string() -> string:
    return "step"

func bump(const amount: i32) -> i32:
    var _local = amount
    return _local + 1

func scoped(flag: bool) -> i32:
    if flag:
        const item = 1
        return item
    else:
        var item = 2
        return item + 1

func main() -> i32:
    var p: point = point(value = 2)
    var s = step(move = 3)
    var size: lib.size = lib.size(value = 4)
    const limit = i32.max_value
    print("雪😀", p.move(bump(s.distance())), p.size, s.move, string(s))
    return lib.print(size.value) + plain.zero()
'''


class SemanticClient(Client):
    refreshes = 0
    held_refresh = None
    hold_refresh = False

    def receive(self):
        while True:
            message = super().receive()
            if message.get('method') != 'workspace/semanticTokens/refresh':
                return message
            self.refreshes += 1
            if self.hold_refresh:
                assert self.held_refresh is None, 'Refreshes must coalesce while a client response is pending'
                self.held_refresh = message['id']
            else:
                self.acknowledge_refresh(message['id'])

    def acknowledge_refresh(self, request_id):
        self.write(frame({'jsonrpc': '2.0', 'id': request_id, 'result': None}))


def request(client, name, params):
    response, notes = client.request(name, params)
    assert 'result' in response, response
    return response['result'], notes


def document(path):
    return {'uri': path.as_uri()}


def opened(client, path, text, language='void'):
    client.notify('textDocument/didOpen', {'textDocument': dict(document(path), languageId=language, version=1, text=text)})


def edit(path, text, version):
    return {'textDocument': dict(document(path), version=version), 'contentChanges': [{'text': text}]}


def tokens(client, path):
    result, notes = request(client, method, {'textDocument': document(path)})
    assert set(result) == {'data'}, result
    return result['data'], notes


def utf16(text):
    return len(text.encode('utf-16-le')) // 2


def decoded(data, text):
    lines = text.splitlines()
    line = start = 0
    previous_end = 0
    result = {}
    assert len(data) % 5 == 0
    for i in range(0, len(data), 5):
        delta_line, delta_start, length, kind, bits = data[i:i + 5]
        assert min(delta_line, delta_start, kind, bits) >= 0 and length > 0
        assert kind < len(types) and bits < 2 ** len(modifiers)
        line += delta_line
        start = delta_start if delta_line else start + delta_start
        assert delta_line or start >= previous_end
        previous_end = start + length
        raw = lines[line].encode('utf-16-le')
        assert previous_end * 2 <= len(raw)
        word = raw[start * 2:previous_end * 2].decode('utf-16-le')
        assert word and word == word.strip()
        result[line, start] = word, types[kind], tuple(m for i, m in enumerate(modifiers) if bits & (1 << i))
    return result


def expect(found, text, prefix, fragment, word, role, *flags):
    line, content = next((i, s) for i, s in enumerate(text.splitlines()) if s.startswith(prefix))
    start = utf16(content[:content.index(fragment) + fragment.index(word)])
    actual = found.get((line, start))
    assert actual == (word, role, flags), (content, word, actual, found)


with TemporaryDirectory(prefix='VOID semantic 雪😀 ') as directory:
    root = Path(directory).resolve()
    src = root / 'src'
    src.mkdir()
    policy, main, helper, other = root / 'void.project.toml', src / 'main.void', src / 'helper.void', src / 'other.void'
    policy.write_bytes(manifest.encode())
    main.write_bytes(source.encode())
    helper.write_bytes(helper_text.encode())
    (src / 'plain.void').write_bytes(b'public func zero() -> i32:\n    return 0\n')
    with SemanticClient(server, root, project=True, entry_override='src/main.void') as client:
        capabilities = {'textDocument': {'semanticTokens': {'requests': {'full': {'delta': True}, 'range': True},
                                                            'formats': ['relative'], 'tokenTypes': types,
                                                            'tokenModifiers': modifiers}},
                        'workspace': {'semanticTokens': {'refreshSupport': True}}}
        result, notes = request(client, 'initialize', {'capabilities': capabilities})
        assert not notes
        caps = result['capabilities']
        assert caps['positionEncoding'] == 'utf-16'
        assert caps['semanticTokensProvider'] == {'legend': {'tokenTypes': types, 'tokenModifiers': modifiers}, 'full': True}
        client.notify('initialized', {})
        assert tokens(client, main)[0] == [], 'A closed disk document must not yield cached tokens'
        text = source.replace('\n', '\r\n')
        opened(client, main, text)
        data, notes = tokens(client, main)
        assert data and not any(n['diagnostics'] for n in notes), (data, notes)
        assert tokens(client, main)[0] == data, 'A stable snapshot gives stable token bytes'
        found = decoded(data, text)
        def role(prefix, fragment, word, kind, *flags):
            expect(found, text, prefix, fragment, word, kind, *flags)
        role('use "./helper', 'as lib', 'lib', 'namespace', 'declaration')
        role('struct point', 'point', 'point', 'struct', *defines)
        role('    value:', 'value', 'value', 'property', *defines)
        role('func (p point)', '(p ', 'p', 'parameter', 'declaration')
        role('func (p point)', 'point', 'point', 'struct')
        role('func (p point)', 'move', 'move', 'method', *defines)
        role('func (p point)', 'delta', 'delta', 'parameter', 'declaration')
        role('    return p.value +', 'p.value', 'p', 'parameter')
        role('    return p.value +', '.value', 'value', 'property')
        role('    return p.value +', '+ delta', 'delta', 'parameter')
        role('property (p point)', 'size', 'size', 'property', *defines, 'readonly')
        role('value struct step', 'step', 'step', 'struct', *defines)
        role('    const move:', 'move', 'move', 'property', *defines, 'readonly')
        role('func (s step)', 'distance', 'distance', 'method', *defines)
        role('func bump', 'bump', 'bump', 'function', *defines)
        role('func bump', 'amount', 'amount', 'parameter', 'declaration', 'readonly')
        role('func bump', ': i32', 'i32', 'type', 'defaultLibrary')
        role('    var _local', '_local', '_local', 'variable', *defines)
        role('    var _local', 'amount', 'amount', 'parameter', 'readonly')
        role('    return _local', '_local', '_local', 'variable')
        role('        return item', 'item', 'item', 'variable', 'readonly')
        role('        return item +', 'item', 'item', 'variable')
        role('    var p:', 'p:', 'p', 'variable', *defines)
        role('    var p:', ': point', 'point', 'struct')
        role('    var p:', '= point', 'point', 'struct')
        role('    var size:', ': lib.', 'lib', 'namespace')
        role('    var size:', ': lib.size', 'size', 'struct')
        role('    const limit', 'limit', 'limit', 'variable', *defines, 'readonly')
        role('    const limit', 'max_value', 'max_value', 'property', 'readonly', 'static', 'defaultLibrary')
        role('    print(', 'print', 'print', 'function', 'defaultLibrary')
        role('    print(', 'p.move', 'p', 'variable')
        role('    print(', '.move(bump', 'move', 'method')
        role('    print(', 'bump', 'bump', 'function')
        role('    print(', 's.distance', 'distance', 'method')
        role('    print(', 'p.size', 'size', 'property', 'readonly')
        role('    print(', 's.move', 'move', 'property', 'readonly')
        role('    print(', 'string(s)', 'string', 'function', 'defaultLibrary')
        role('    return lib.print', 'lib.print', 'lib', 'namespace')
        role('    return lib.print', 'lib.print', 'print', 'function')
        role('    return lib.print', 'plain.zero', 'plain', 'namespace')
        opened(client, helper, helper_text)
        hdata = decoded(tokens(client, helper)[0], helper_text)
        expect(hdata, helper_text, 'public func print', 'print', 'print', 'function', *defines)
        expect(hdata, helper_text, '    return print', 'print(n)', 'print', 'function')

        updated = text.replace('bump', 'advance').replace(') move(', ') translate(').replace('p.move(', 'p.translate(')
        client.notify('textDocument/didChange', edit(main, updated, 2))
        dirty, _ = tokens(client, main)
        assert decoded(dirty, updated) != found and main.read_bytes() == source.encode()
        expect(decoded(dirty, updated), updated, '    print(', 'advance', 'advance', 'function')
        expect(decoded(dirty, updated), updated, '    print(', 'p.translate', 'translate', 'method')
        client.notify('textDocument/didChange', edit(main, 'this stale version must not win', 1))
        assert tokens(client, main)[0] == dirty
        client.sync()
        refreshes = client.refreshes
        client.hold_refresh = True
        client.notify('textDocument/didChange', edit(helper, 'public func print(', 2))
        empty, notes = tokens(client, main)
        assert empty == [] and any(n['diagnostics'] for n in notes) and client.refreshes == refreshes + 1
        client.notify('textDocument/didChange', edit(helper, helper_text, 3))
        assert tokens(client, main)[0] == dirty
        assert client.refreshes == refreshes + 1 and client.held_refresh is not None
        client.hold_refresh = False
        client.acknowledge_refresh(client.held_refresh)
        client.sync()
        assert client.refreshes == refreshes + 2, 'A held refresh must not hide a newer revision'
        shadow = updated.replace('    var _local = amount', '    var amount = 0\r\n    var _local = amount')
        client.notify('textDocument/didChange', edit(main, shadow, 3))
        empty, notes = tokens(client, main)
        assert empty == [] and any('shadowing' in d['message'] for n in notes for d in n['diagnostics']), notes
        client.notify('textDocument/didChange', edit(main, updated, 4))
        assert tokens(client, main)[0] == dirty
        opened(client, policy, '[=roject]', 'toml')
        assert tokens(client, main)[0] == []
        client.notify('textDocument/didClose', {'textDocument': document(policy)})
        assert tokens(client, main)[0] == dirty

        other_text = 'func main() -> i32:\n    var alternate = 9\n    return alternate\n'
        opened(client, other, other_text)
        assert tokens(client, other)[0] == [], 'Unselected entry does not get tokens from an old project'
        result, _ = request(client, 'void/reselectProject', {'entry': 'src/other.void'})
        assert result['entry'] == other.as_posix(), result
        assert tokens(client, main)[0] == []
        expect(decoded(tokens(client, other)[0], other_text), other_text, '    return', 'alternate', 'alternate', 'variable')
        request(client, 'void/reselectProject', {'entry': 'src/main.void'})
        assert tokens(client, main)[0] == dirty
        client.notify('textDocument/didClose', {'textDocument': document(main)})
        assert tokens(client, main)[0] == []
        client.sync()
        client.shutdown()

    with Client(server, main) as legacy:
        result, _ = request(legacy, 'initialize', {'capabilities': {}})
        assert 'semanticTokensProvider' not in result['capabilities']
        legacy.notify('initialized', {})
        opened(legacy, main, source)
        assert tokens(legacy, main)[0] == []
        result, _ = request(legacy, 'textDocument/definition', {'textDocument': document(main),
                                                               'position': {'line': 6, 'character': 9}})
        assert result['uri'] == main.as_uri(), result
        legacy.shutdown()

    with Client(server, main) as client:
        caps = {'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                                                   'tokenTypes': types, 'tokenModifiers': modifiers}}}
        result, _ = request(client, 'initialize', {'capabilities': caps})
        assert result['capabilities']['semanticTokensProvider']['full'] is True
        client.notify('initialized', {})
        opened(client, main, source)
        assert tokens(client, main)[0]
        client.notify('textDocument/didChange', edit(main, 'func main(', 2))
        assert tokens(client, main)[0] == []  # No unsolicited refresh for this client.
        client.shutdown()

print('Semantic tokens passed stable legends and ranges, resolved roles, and project revision checks')
