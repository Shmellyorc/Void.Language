"""G92 focused corrections: inferred authority and readonly caller contracts."""
import argparse
import json
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

from member_completion import InteractiveClient


NODE = '''struct node:
    value: i32
    children: [node]
func (n node) read() -> i32:
    return n.value
func (n node) change():
    n.value += 1
func (n node) view() -> [node]:
    return n.children
property (n node) size: i64:
    get:
        return n.children.count
func freeze(const n: node) -> node:
    return n
func keep(n: node) -> node:
    return n
func freeze_generic<t>(const value: t) -> t:
    return value
'''


def run(server, compiler, small_stack=False, report=None):
    records = []
    def record(kind, **data):
        records.append(dict(kind=kind, **data))
        if report:
            report.write_text(json.dumps(records, indent=2) + '\n')
    preexec = None
    if small_stack:
        import resource
        def preexec():
            _, hard = resource.getrlimit(resource.RLIMIT_STACK)
            resource.setrlimit(resource.RLIMIT_STACK, (1048576, hard))

    with TemporaryDirectory(prefix='VOID G92 readonly 雪 ') as temp:
        root = Path(temp); path = root / 'main.void'
        # The same owner is also admitted through an explicit local import.
        owner = NODE.replace('struct node:', 'public struct node:')
        owner = owner.replace('    value:', '    public value:').replace('    children:', '    public children:')
        owner = owner.replace('\nfunc ', '\npublic func ').replace('\nproperty ', '\npublic property ')
        owner += 'public value struct box<t>:\n    public item: t\n'
        (root / 'owner.void').write_text(owner)
        initial = NODE + 'func inspect(n: node):\n    print(n.read())\n'
        path.write_text(initial)
        def check(source, accepted=True):
            oracle = root / 'oracle.void'; oracle.write_text(source)
            result = subprocess.run([str(compiler), 'check', str(oracle)], capture_output=True,
                text=True, timeout=30, preexec_fn=preexec)
            record('compiler', source=source, exit=result.returncode, stderr=result.stderr)
            assert (result.returncode == 0) == accepted, records[-1]
            if not accepted:
                assert 'code: VOID2007' in result.stderr, records[-1]

        with InteractiveClient(server, path, preexec_fn=preexec) as client:
            client.request('initialize', {'capabilities': {'textDocument': {
                'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']},
                'publishDiagnostics': {'versionSupport': True}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {}); client.sync()
            version = 0
            def update(source, sync=True):
                nonlocal version
                version += 1
                document = dict(uri=path.as_uri(), version=version)
                if version == 1:
                    client.notify('textDocument/didOpen', {'textDocument': dict(document, languageId='void', text=source)})
                else:
                    client.notify('textDocument/didChange', {'textDocument': document, 'contentChanges': [{'text': source}]})
                return client.sync() if sync else []
            def complete(source, needle, wanted, excluded=(), sync=True):
                update(source, sync)
                before = source[:source.rindex(needle) + len(needle)]
                response, _ = client.request('textDocument/completion', {'textDocument': {'uri': path.as_uri()},
                    'position': {'line': before.count('\n'), 'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}})
                record('completion', source=source, needle=needle, response=response)
                assert 'result' in response, response
                items = response['result']; items = items.get('items', []) if isinstance(items, dict) else items
                items = {i['label']: i for i in items}
                assert set(wanted) <= items.keys() and not set(excluded) & items.keys(), records[-1]
                assert all('.void::' not in i['detail'] and '/@pkg/' not in i['detail'] for i in items.values()), items
                return items
            update(initial)

            alias = NODE + 'func inspect(n: node):\n    var alias = freeze(n)\n    QUERY\n'
            check(alias.replace('QUERY', 'print(alias.read())'))
            check(alias.replace('QUERY', 'alias.change()'), False)
            for expression, wanted, excluded in [
                ('alias.', {'value', 'children', 'read', 'view', 'size'}, {'change'}),
                ('alias.children.', {'count', 'get'}, {'push', 'pop', 'clear', 'insert', 'remove_at'}),
                ('alias.children[0].', {'value', 'read', 'size'}, {'change'}),
                ('alias.view().', {'count', 'get'}, {'push', 'pop', 'clear'})]:
                complete(alias.replace('QUERY', expression), '    ' + expression, wanted, excluded)
            for declaration in ['var other = alias', 'var other = freeze_generic(alias)']:
                current = alias.replace('QUERY', declaration + '\n    other.')
                complete(current, '    other.', {'read', 'value'}, {'change'})
            # Display checking owns temporary argument rewrites. Receiver
            # capture must survive their unwinding, including nested blocks.
            for statement in ['print(alias.)', 'string(alias.)', 'if true:\n        print(alias.)']:
                complete(alias.replace('QUERY', statement), 'alias.', {'read', 'value'}, {'change'})
            generic = alias.replace('freeze(n)', 'freeze_generic(n)')
            check(generic.replace('QUERY', 'print(alias.read())'))
            check(generic.replace('QUERY', 'alias.change()'), False)
            complete(generic.replace('QUERY', 'alias.'), '    alias.', {'read', 'value'}, {'change'})
            mutable = alias.replace('freeze(n)', 'keep(n)')
            check(mutable.replace('QUERY', 'alias.change()'))
            complete(mutable.replace('QUERY', 'alias.'), '    alias.', {'read', 'change', 'value'})
            # Direct calls, alias chains and collection bindings must agree.
            complete(alias.replace('QUERY', 'freeze(n).'), '    freeze(n).', {'read', 'value'}, {'change'})
            lists = 'func freeze(const items: [i32]) -> [i32]:\n    return items\nfunc inspect(items: [i32]):\n    var alias = freeze(items)\n    QUERY\n'
            check(lists.replace('QUERY', 'print(alias.count)'))
            check(lists.replace('QUERY', 'alias.push(1)'), False)
            complete(lists.replace('QUERY', 'alias.'), '    alias.', {'count', 'get'}, {'push', 'pop', 'clear'})

            # A readonly formal's usable members cannot depend on its callers.
            for selected, result_type, safe, wanted in [
                ('n.', 'i32', 'n.read()', {'read', 'value', 'children', 'view', 'size'}),
                ('n.children.', 'i64', 'n.children.count', {'count', 'get'}),
                ('n.view().', 'i64', 'n.view().count', {'count', 'get'})]:
                observed = []
                for binding in ['const', 'var']:
                    current = NODE + f'func inspect(const n: node) -> {result_type}:\n    return QUERY\nfunc main():\n    {binding} item = node()\n    print(inspect(item))\n'
                    check(current.replace('QUERY', safe))
                    observed.append(complete(current.replace('QUERY', selected), 'return ' + selected,
                        wanted, {'change', 'push', 'pop', 'clear'}))
                assert observed[0] == observed[1], observed

            imported = 'use "./owner.void" as model\nfunc inspect(n: model.node):\n    var alias = model.freeze_generic(n)\n    QUERY\n'
            check(imported.replace('QUERY', 'print(alias.read())'))
            check(imported.replace('QUERY', 'alias.change()'), False)
            complete(imported.replace('QUERY', 'alias.'), '    alias.', {'read', 'value'}, {'change'})
            boxed = 'use "./owner.void" as model\nfunc inspect(n: model.box<model.node>):\n    var alias = model.freeze_generic(n)\n    QUERY\n'
            check(boxed.replace('QUERY', 'print(alias.item.read())'))
            check(boxed.replace('QUERY', 'alias.item.change()'), False)
            complete(boxed.replace('QUERY', 'alias.item.'), '    alias.item.', {'read', 'value'}, {'change'})
            caller = 'use "./owner.void" as model\nfunc inspect(const n: model.node) -> i32:\n    return QUERY\nfunc main():\n    const item = model.node()\n    print(inspect(item))\n'
            check(caller.replace('QUERY', 'n.read()'))
            complete(caller.replace('QUERY', 'n.'), 'return n.', {'read', 'value'}, {'change'})

            good = alias.replace('QUERY', 'print(alias.read())')
            assert all(not n['diagnostics'] for n in update(good))
            response, _ = client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': path.as_uri()}})
            assert response['result']['data'], response
            for expression, wanted in [('alias.', {'read'}), ('alias.r', {'read'}), ('alias.children.', {'count', 'get'}), ('alias.children.c', {'count'})]:
                complete(alias.replace('QUERY', expression), '    ' + expression, wanted,
                    {'change', 'push', 'pop', 'clear'} - wanted, sync=False)
            response, _ = client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': path.as_uri()}})
            assert response['result']['data'], response
            notes = update(good)
            assert any(n.get('version') == version and not n['diagnostics'] for n in notes), notes
            client.shutdown()
    print('G92 readonly: inferred aliases, transitive/generic/imported chains, const/mutable callers, compiler agreement and incomplete recovery passed' + (' (1 MiB stack)' if small_stack else ''))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('server', type=Path)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--small-stack', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    run(args.server.resolve(), args.compiler.resolve(), args.small_stack, args.report)
