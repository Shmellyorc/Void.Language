"""G92: actual compiler/server agreement for member types and incomplete edits."""
from pathlib import Path
import subprocess
import sys
import tomllib
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame


class InteractiveClient(Client):
    def receive(self):
        while True:
            message = super().receive()
            if message.get('method') != 'workspace/semanticTokens/refresh':
                return message
            self.write(frame({'jsonrpc': '2.0', 'id': message['id'], 'result': None}))


def run(server, compiler, small_stack=False):
    preexec = None
    if small_stack:
        import resource
        def preexec():
            _, hard = resource.getrlimit(resource.RLIMIT_STACK)
            resource.setrlimit(resource.RLIMIT_STACK, (1048576, hard))
    with TemporaryDirectory(prefix='VOID G92 members 雪 ') as temp:
        root = Path(temp)
        main = root / 'main.void'
        original = (Path(__file__).parent / 'fixtures/member_completion.void').read_text()
        main.write_text(original)
        (root / 'owner.void').write_text('''public value struct box<t>:
    ## Stored payload.
    public value: t
    private secret: i32
public func (b box<t>) read() -> t:
    return b.value
public func (b box<t>) echo<u>(value: u) -> u:
    return value
''')
        with InteractiveClient(server, main, preexec_fn=preexec) as client:
            init, _ = client.request('initialize', {
                'capabilities': {'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']},
                    'publishDiagnostics': {'versionSupport': True}}, 'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True, 'voidIncrementalSync': True}})
            legend = init['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
            client.notify('initialized', {}); client.sync()
            version = 0
            text = ''
            def replace(source, sync=True):
                nonlocal version, text
                version += 1; text = source
                doc = {'uri': main.as_uri(), 'version': version}
                if version == 1:
                    client.notify('textDocument/didOpen', {'textDocument': dict(doc, languageId='void', text=text)})
                else:
                    client.notify('textDocument/didChange', {'textDocument': doc, 'contentChanges': [{'text': text}]})
                return client.sync() if sync else []
            def point(needle):
                assert needle in text, (needle, text)
                prefix = text[:text.rindex(needle) + len(needle)]
                return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}
            def query(method, needle=None):
                params = {'textDocument': {'uri': main.as_uri()}}
                if method == 'references': params['context'] = {'includeDeclaration': True}
                if needle is not None: params['position'] = point(needle)
                result, _ = client.request('textDocument/' + method, params)
                assert 'result' in result, result
                return result['result']
            def items(needle):
                result = query('completion', needle)
                return {i['label']: i for i in (result['items'] if isinstance(result, dict) else result)}
            def complete(source, needle, wanted, excluded=()):
                replace(source)
                result = items(needle)
                assert set(wanted) <= result.keys() and not set(excluded) & result.keys(), (needle, wanted, excluded, result)
                assert all('.void::' not in i['detail'] and '/@pkg/' not in i['detail'] for i in result.values()), result
                return result
            def check(source, accepted=True):
                # Ordinary compiler validity is an independent oracle for the
                # proposed access; fixtures never execute or authorize native code.
                probe = root / 'oracle.void'; probe.write_text(source)
                result = subprocess.run([str(compiler), 'check', str(probe)], capture_output=True, timeout=30, preexec_fn=preexec)
                assert (result.returncode == 0) == accepted, (accepted, result.returncode, result.stdout, result.stderr)

            assert all(not n['diagnostics'] for n in replace(original))
            check(original)
            prefix = original[:original.index('    if ')]
            for expression, needle, wanted in [
                ('s.', '    s.', {'scenes', 'on_update'}),
                ('s.scenes.', '    s.scenes.', {'count', 'get', 'pop', 'push', 'insert', 'remove_at', 'clear'}),
                ('if s.', 'if s.', {'scenes', 'on_update'}),
                ('if s.scenes.', 'if s.scenes.', {'count', 'get'}),
                ('if s.scenes.c', 'if s.scenes.c', {'count', 'clear'})]:
                result = complete(prefix + '    ' + expression + ('\n        return\n' if expression.startswith('if ') else '\n'), needle, wanted)
            assert result['count']['kind'] == 10 and result['count']['detail'] == 'i64'
            assert result['count']['documentation']
            array = complete(prefix + '    s.scenes.\n', '    s.scenes.', {'get', 'push'})
            assert array['get']['detail'] == 'get(index: i32) -> result<scene, string>', array
            assert array['push']['detail'] == 'push(value: scene) -> no result', array
            assert array['get']['documentation'] and array['push']['documentation']
            replace(original)
            assert 'i64' in query('hover', 'if s.scenes.cou')['contents']['value']
            assert query('definition', 'if s.sce')
            assert query('references', 'if s.sce')
            # Parameter help for APIs which previously had no typed declaration.
            replace(prefix + '    s.scenes.insert(\n')
            signature = query('signatureHelp', 's.scenes.insert(')
            assert signature['signatures'][0]['label'] == 'insert(index: i32, value: scene) -> no result', signature
            assert signature['signatures'][0]['parameters'][0]['documentation'], signature

            model = '''value struct leaf:
    ## Horizontal coordinate.
    x: f32
value struct branch:
    tip: leaf
    leaves: [leaf, 2]
struct tree:
    branch: branch
    names: {string: leaf}
func examine(t: tree):
    QUERY
'''
            result = complete(model.replace('QUERY', 't.branch.tip.'), '    t.branch.tip.', {'x'})
            assert result['x']['detail'] == 'f32' and 'Horizontal coordinate' in result['x']['documentation']
            complete(model.replace('QUERY', 't.branch.leaves.'), '    t.branch.leaves.', {'count', 'get'}, {'push', 'pop', 'clear', 'insert', 'remove_at'})
            dictionary = complete(model.replace('QUERY', 't.names.'), '    t.names.', {'count', 'get', 'contains', 'remove', 'clear'}, {'push'})
            assert dictionary['get']['detail'] == 'get(key: string) -> result<leaf, string>', dictionary
            assert dictionary['contains']['detail'] == 'contains(key: string) -> bool', dictionary
            check(model.replace('QUERY', 'print(t.names.contains("a"))'))
            check(model.replace('QUERY', 't.branch.leaves.push(leaf())'), False)
            imported = 'use "./owner.void" as o\nfunc inspect(b: o.box<f32>):\n    b.\n'
            result = complete(imported, '    b.', {'value', 'read', 'echo'}, {'secret'})
            assert result['value']['detail'] == 'f32' and 'Stored payload' in result['value']['documentation']
            assert result['read']['detail'] == 'read() -> f32', result
            assert 'value: u' in result['echo']['detail'], result
            check(imported.replace('    b.', '    print(b.value, b.read(), b.echo(2))'))
            check(imported.replace('    b.', '    print(b.secret)'), False)

            readonly = '''struct node:
    value: i32
    children: [node]
func (n node) read() -> i32:
    return n.value
func (n node) change():
    n.value += 1
property (n node) size: i64:
    get:
        return n.children.count
func (n node) view() -> [node]:
    return n.children
func examine(const n: node):
    QUERY
'''
            result = complete(readonly.replace('QUERY', 'n.'), '    n.', {'value', 'children', 'read', 'size', 'view'}, {'change'})
            check(readonly.replace('QUERY', 'print(n.read(), n.size)'))
            check(readonly.replace('QUERY', 'n.change()'), False)
            complete(readonly.replace('QUERY', 'n.children.'), '    n.children.', {'count', 'get'}, {'push', 'pop', 'insert', 'remove_at', 'clear'})
            complete(readonly.replace('QUERY', 'n.view().'), '    n.view().', {'count', 'get'}, {'push', 'pop', 'clear'})
            complete(readonly.replace('QUERY', 'n.view()[0].'), '    n.view()[0].', {'value', 'read', 'size'}, {'change'})
            complete(readonly.replace('const n: node', 'n: node').replace('QUERY', 'n.view().'), '    n.view().', {'push', 'pop', 'count'})
            complete(readonly.replace('QUERY', 'n.size.'), '    n.size.', {'abs', 'clamp'}, {'push'})
            assert not complete(readonly.replace('const n: node', 'n: node?').replace('QUERY', 'n.'), '    n.', set(), {'value', 'read', 'change'})
            assert not complete(readonly.replace('QUERY', 'n.children.no_such_member.'), '    n.children.no_such_member.', set(), {'count', 'get', 'value'})
            temporary = '''value struct point:
    x: i32
func (p point) read() -> i32:
    return p.x
func (p point) change():
    p.x += 1
func make() -> point:
    return point()
func inspect():
    make().
'''
            complete(temporary, '    make().', {'x', 'read'}, {'change'})
            check(temporary.replace('    make().', '    print(make().read())'))
            check(temporary.replace('    make().', '    make().change()'), False)
            nested_const = temporary[:temporary.index('func make()')] + '''value struct wrapper:
    child: point
func inspect(const holder: wrapper):
    holder.child.
'''
            complete(nested_const, '    holder.child.', {'x', 'read'}, {'change'})
            check(nested_const.replace('    holder.child.', '    holder.child.change()'), False)
            complete('func text(s: string):\n    s.\n', '    s.', {'length', 'byte_length', 'to_utf8'}, {'from_utf8'})
            complete('func text():\n    string.\n', '    string.', {'from_utf8'}, {'length', 'to_utf8'})
            static = '''struct settings:
    static shared = 1
    instance: i32
static func settings.create() -> settings:
    return settings()
func inspect(s: settings):
    QUERY
'''
            complete(static.replace('QUERY', 'settings.'), '    settings.', {'shared', 'create'}, {'instance'})
            complete(static.replace('QUERY', 's.'), '    s.', {'instance'}, {'shared', 'create'})
            complete(static.replace('QUERY', 'settings.create().'), '    settings.create().', {'instance'}, {'shared', 'create'})
            inheritance = '''struct base:
    inherited: i32
struct part:
    promoted: f32
struct child extends base:
    using component: part
    own: i32
func inspect(c: child):
    c.
'''
            complete(inheritance, '    c.', {'inherited', 'component', 'promoted', 'own'})
            enum = 'enum state:\n    idle\n    active\nfunc inspect(s: state):\n    s.\n'
            complete(enum, '    s.', {'name', 'is_defined'})
            complete(enum.replace('    s.', '    state.'), '    state.', {'idle', 'active', 'values'}, {'name'})
            # Never cache another receiver across rapid edits; retain diagnostics
            # and semantic classification around incomplete source.
            replace(original)
            assert query('semanticTokens/full')['data']
            for suffix in ['s.', 's.scenes.', 's.scenes.c', 's.', 's.scenes.g']:
                replace(prefix + '    ' + suffix, sync=False)
                expected = 'scenes' if suffix == 's.' else 'get' if suffix.endswith('.g') else 'count'
                assert expected in items('    ' + suffix)
            assert query('semanticTokens/full')['data']
            notes = replace(original)
            assert any(n.get('version') == version and not n['diagnostics'] for n in notes), notes
            data = query('semanticTokens/full')['data']
            assert any(legend[data[n + 3]] == 'property' for n in range(0, len(data), 5))
            client.shutdown()
        # Cold incomplete input has no earlier valid snapshot to borrow.
        cold = prefix + '    if s.scenes.\n        return\n'
        main.write_text(cold)
        with InteractiveClient(server, main, preexec_fn=preexec) as client:
            client.request('initialize', {'capabilities': {'textDocument': {
                'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {}); client.sync()
            client.notify('textDocument/didOpen', {'textDocument': {'uri': main.as_uri(), 'version': 1,
                'languageId': 'void', 'text': cold}}); client.sync()
            result, _ = client.request('textDocument/completion', {'textDocument': {'uri': main.as_uri()},
                'position': {'line': 10, 'character': len('    if s.scenes.')}})
            assert 'count' in {i['label'] for i in result['result']}, result
            result, _ = client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': main.as_uri()}})
            assert result['result']['data'], result
            client.shutdown()
        # The shared declarations must still lower to the established kernels.
        main.write_text('''func main():
    var values: [i32] = [1, 2]
    values.push(3)
    values.insert(1 as i64, 9)
    var removed = values.remove_at(-1 as i64)
    var found = values.get(0 as i64)
    var popped = values.pop()
    var table: {string: i32} = {"one": 1}
    var contains = table.contains("one")
    var entry = table.get("one")
    var erased = table.remove("one")
    var fixed: [i32, 2] = [4, 5]
    var bytes = "é".to_utf8()
    var decoded = string.from_utf8(bytes)
    print(values.count, removed, found is Ok(1), popped is Ok(2), contains,
          entry is Ok(1), erased, table.count, fixed.count, fixed.get(-1) is Ok(5),
          "é".length, "é".byte_length, decoded is Ok("é"))
    values.clear()
    table.clear()
    print(values.count, table.count)
''')
        expected = '2 3 true true true true true 0 2 true 1 2 true\n0 0\n'
        for flags in (['--no-opt'], []):
            result = subprocess.run([str(compiler), 'run', str(main), '--no-cache', *flags],
                capture_output=True, text=True, timeout=60, preexec_fn=preexec)
            assert result.returncode == 0 and result.stdout == expected, (result.returncode, result.stdout, result.stderr)
        image = root / 'core-members'
        result = subprocess.run([str(compiler), 'build', str(main), '-o', str(image)],
            capture_output=True, text=True, timeout=60, preexec_fn=preexec)
        assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
        result = subprocess.run([str(image)], capture_output=True, text=True, timeout=30, preexec_fn=preexec)
        assert result.returncode == 0 and result.stdout == expected, (result.returncode, result.stdout, result.stderr)
        # A locked package owner keeps its public type names and specialization.
        app = root / 'package-app'; library = app / 'libs/model'
        def cli(*args):
            result = subprocess.run([str(compiler), *map(str, args)], capture_output=True, timeout=30, preexec_fn=preexec)
            assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
        cli('new', app)
        library.parent.mkdir()
        cli('new', library, '--kind', 'library', '--package-name', 'model', '--nested')
        metadata = tomllib.loads((library / 'void.project.toml').read_text())
        (library / metadata['package']['entry']).write_text((root / 'owner.void').read_text())
        manifest = app / 'void.project.toml'
        manifest.write_text(manifest.read_text() + '\n[dependencies.model]\n'
            f'id = "{metadata["package"]["id"]}"\npath = "libs/model"\nversion = "=0.1.0"\n')
        cli('resolve', '--project', app)
        source = 'use pkg/model as model\nfunc main():\n    var b = model.box<f32>(value = 2.0f)\n    print(b.read())\n'
        target = app / 'src/app.void'; target.write_text(source)
        cli('check', '--project', app)
        with InteractiveClient(server, app, project=True, preexec_fn=preexec) as client:
            client.request('initialize', {'capabilities': {'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {}); client.sync()
            client.notify('textDocument/didOpen', {'textDocument': {'uri': target.as_uri(), 'version': 1,
                'languageId': 'void', 'text': source.replace('print(b.read())', 'b.')}}); client.sync()
            response, _ = client.request('textDocument/completion', {'textDocument': {'uri': target.as_uri()},
                'position': {'line': 3, 'character': len('    b.')}})
            result = {i['label']: i for i in response['result']}
            assert {'value', 'read', 'echo'} <= result.keys() and 'secret' not in result, result
            assert result['value']['detail'] == 'f32' and result['read']['detail'] == 'read() -> f32', result
            assert all('/@pkg/' not in i['detail'] and '.void::' not in i['detail'] for i in result.values()), result
            client.shutdown()
    print('G92: receiver/chain types, shared builtin signatures, readonly effects, generics/imports, visibility, static/instance, inheritance/composition, enum/nullable, rapid edits and recovery passed' + (' (1 MiB stack)' if small_stack else ''))


if __name__ == '__main__':
    run(*(Path(p).resolve() for p in sys.argv[1:3]), small_stack='--small-stack' in sys.argv[3:])
