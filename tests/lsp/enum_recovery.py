"""Exercise enum completion and tokens from current compiler analysis."""
from pathlib import Path
from shutil import copy2
import subprocess
import sys
from tempfile import TemporaryDirectory

from editor_recovery import Client, query, request, update

FIXTURE = Path(__file__).parent / 'fixtures' / 'enum_recovery'
COMPLETE = (FIXTURE / 'main.void').read_text(encoding='utf-8')
ARM = 'process_type.none:'
MEMBERS = ['incorrect', 'none', 'unknown']
REF_MEMBER = ('enumMember', ('readonly', 'static'))
DECL_MEMBER = ('enumMember', ('declaration', 'definition', 'readonly', 'static'))
ENUM = ('enum', ())
DECL_ENUM = ('enum', ('declaration', 'definition'))
CAPABILITIES = {'textDocument': {'semanticTokens': {
    'requests': {'full': True}, 'formats': ['relative'], 'tokenTypes': ['enum', 'enumMember'], 'tokenModifiers': [],
}, 'publishDiagnostics': {'versionSupport': True}}}
IMPORTS = '''public enum report:
    idle
    done(amount: i32)

private enum hidden:
    secret

public enum status(u8):
    calm = 0
    noisy = 3
'''
EXTRA = '''
func imported() -> i32:
    var state = events.status.calm
    var report = events.report.done(8)
    var label = match report:
        events.report.done(var amount): amount
        events.report.idle: 0
    match state:
        events.status.calm:
            return label
        events.status.noisy:
            return 2
    return 3
'''


def position(text, offset):
    before = text[:offset]
    return {'line': before.count('\n'), 'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}


def token_records(client, path, text, legend):
    raw = query(client, 'semanticTokens/full', path)['data']
    assert len(raw) % 5 == 0, raw
    line = start = end = 0
    tokens = {}
    lines = [line.encode('utf-16-le') for line in text.splitlines()]
    for i in range(0, len(raw), 5):
        dl, ds, length, token_type, bits = raw[i:i + 5]
        line += dl
        start = ds if dl else start + ds
        assert length and (dl or start >= end), raw
        end = start + length
        assert end * 2 <= len(lines[line]), (line, start, end)
        word = lines[line][start * 2:end * 2].decode('utf-16-le')
        kind = legend['tokenTypes'][token_type]
        modifiers = tuple(m for n, m in enumerate(legend['tokenModifiers']) if bits & 1 << n)
        tokens[line, start] = word, (kind, modifiers)
    return tokens


def expect_token(tokens, text, offset, word, role):
    p = position(text, offset)
    assert tokens.get((p['line'], p['character'])) == (word, role), (p, word, tokens)


def strict_check(compiler, root, source, accepted, code=None):
    (root / 'main.void').write_bytes(source.encode())
    checked = subprocess.run([compiler, 'check', '--project', root], timeout=30, capture_output=True)
    assert (checked.returncode == 0) == accepted, (source, checked)
    if code:
        assert code.encode() in checked.stderr, checked.stderr


class Fixture:
    def __init__(self, client, path):
        self.client, self.path = client, path
        capabilities = request(client, 'initialize', {'capabilities': CAPABILITIES})['capabilities']
        assert capabilities['positionEncoding'] == 'utf-16'
        assert '.' in capabilities['completionProvider']['triggerCharacters']
        self.legend = capabilities['semanticTokensProvider']['legend']
        assert self.legend['tokenTypes'][4] == 'enum' and self.legend['tokenTypes'][9] == 'enumMember'
        assert self.legend['tokenModifiers'] == ['declaration', 'definition', 'readonly', 'static', 'defaultLibrary']
        client.notify('initialized', {})
        client.sync()
        self.version = 0

    def edit(self, text, *, broken=False, code=None, diagnostic_at=None):
        self.version += 1
        self.text = text
        diagnostics = update(self.client, self.path, text, self.version)
        errors = [d for d in diagnostics if d['severity'] == 1]
        assert bool(errors) == broken, (text, errors)
        if code:
            assert any(d['code'] == code and (diagnostic_at is None or d['range']['start'] == position(text, diagnostic_at))
                       for d in errors), diagnostics
        assert request(self.client, 'void/projectState', {})['status'] == ('error' if broken else 'ready'), diagnostics

    def items(self, cursor, expected, prefix='', suffix=''):
        result = request(self.client, 'textDocument/completion', {
            'textDocument': {'uri': self.path.as_uri()}, 'position': position(self.text, cursor),
            'context': {'triggerKind': 2, 'triggerCharacter': '.'},
        })
        result = [i for i in result if i['kind'] == 20]
        assert sorted(item['label'] for item in result) == sorted(expected), (self.text, cursor, result, expected)
        for item in result:
            edit = item['textEdit']
            assert edit == {'newText': item['label'], 'range': {
                'start': position(self.text, cursor - len(prefix)), 'end': position(self.text, cursor + len(suffix)),
            }}, item
        return {item['label']: item['detail'] for item in result}

    def tokens(self, path=None, text=None):
        return token_records(self.client, path or self.path, text or self.text, self.legend)

    def transition(self, good, complete, *, offset=None, final_invalid=True):
        offset = good.index(complete) if offset is None else offset
        assert good[offset:offset + len(complete)] == complete
        for prefix, suggestions in [('', MEMBERS), ('i', ['incorrect']), ('in', ['incorrect']), ('incorrect', ['incorrect'])]:
            spelling = 'process_type.' + prefix
            text = good[:offset] + spelling + good[offset + len(complete):]
            start = offset + len('process_type.')
            code = 'VOID1102' if prefix in ('', 'in') else 'VOID1101' if complete.endswith(':') else None
            # A complete name without its pattern colon or the second return
            # component is still an invalid document.
            self.edit(text, broken=prefix != 'incorrect' or final_invalid, code=code)
            self.items(start + len(prefix), suggestions, prefix)
            tokens = self.tokens()
            expect_token(tokens, text, offset, 'process_type', ENUM)
            if prefix == 'incorrect':
                expect_token(tokens, text, start, prefix, REF_MEMBER)
            else:
                p = position(text, start)
                assert (p['line'], p['character']) not in tokens, tokens


def run(server, compiler):
    with TemporaryDirectory(prefix='VOID enum recovery 雪 ') as directory:
        root = Path(directory).resolve()
        for name in ('void.project.toml', 'main.void', 'app.void'):
            copy2(FIXTURE / name, root / name)
        main = root / 'main.void'
        other = root / 'other.void'
        imports = root / 'events.void'
        imports.write_bytes(IMPORTS.encode())
        strict_check(compiler, root, COMPLETE, True)
        with Client(server, root, project=True) as client:
            fixture = Fixture(client, main)
            fixture.edit(COMPLETE)
            tokens = fixture.tokens()
            expect_token(tokens, COMPLETE, COMPLETE.index('enum process_type') + len('enum '), 'process_type', DECL_ENUM)
            for member in MEMBERS:
                expect_token(tokens, COMPLETE, COMPLETE.index('    ' + member + '\n') + 4, member, DECL_MEMBER)
            pattern = COMPLETE.index(ARM)
            expect_token(tokens, COMPLETE, pattern, 'process_type', ENUM)
            expect_token(tokens, COMPLETE, pattern + len('process_type.'), 'none', REF_MEMBER)
            returned = COMPLETE.index('process_type.unknown')
            expect_token(tokens, COMPLETE, returned, 'process_type', ENUM)
            expect_token(tokens, COMPLETE, returned + len('process_type.'), 'unknown', REF_MEMBER)
            result = query(client, 'hover', main, COMPLETE, 'result:')
            assert 'process_type' in result['contents']['value'], result

            for source in (COMPLETE, COMPLETE.replace('-> (process_type, string):',
                                                     '-> (status: process_type, message: string):')):
                fixture.transition(source, ARM)
                colon = source.replace(ARM, 'process_type.:')
                fixture.edit(colon, broken=True, code='VOID1102', diagnostic_at=colon.index('process_type.:') + len('process_type.'))
                fixture.items(colon.index('process_type.:') + len('process_type.'), MEMBERS)
                for prefix in ('i', 'in'):
                    colon = source.replace(ARM, f'process_type.{prefix}:')
                    fixture.edit(colon, broken=True)
                    start = colon.index(f'process_type.{prefix}:')
                    fixture.items(start + len('process_type.') + len(prefix), ['incorrect'], prefix)
                    expect_token(fixture.tokens(), colon, start, 'process_type', ENUM)
                bad_return = 'process_type.unknown, "Unknown error"'
                fixture.transition(source, bad_return)
            ordinary = COMPLETE.replace('var game = app()', 'var selected = process_type.none\n    var game = app()')
            fixture.transition(ordinary, 'process_type.none', final_invalid=False)
            match_expression = COMPLETE.replace('        match result:', '''        var code = match result:
            process_type.none: 1
            _: 2
        match result:''')
            fixture.transition(match_expression, 'process_type.none: 1')
            separate_return = COMPLETE + '\nfunc pick() -> process_type:\n    return process_type.none\n'
            fixture.transition(separate_return, 'process_type.none', offset=separate_return.rindex('process_type.none'),
                               final_invalid=False)

            unicode_source = COMPLETE.replace('var game = app()', 'print("😀", process_type.i)\n    var game = app()')
            fixture.edit(unicode_source, broken=True)
            cursor = unicode_source.index('process_type.i)') + len('process_type.i')
            assert position(unicode_source, cursor)['character'] != len(unicode_source[:cursor].rsplit('\n', 1)[-1])
            fixture.items(cursor, ['incorrect'], 'i')
            expect_token(fixture.tokens(), unicode_source, cursor - len('process_type.i'), 'process_type', ENUM)

            imported = 'use "/events.void" as events\n' + COMPLETE + EXTRA
            assert not update(client, imports, IMPORTS, 1)
            fixture.edit(imported)
            tokens = fixture.tokens()
            for spelling, name, member in [('events.status.calm', 'status', 'calm'),
                                            ('events.report.done(8)', 'report', 'done'),
                                            ('events.report.done(var', 'report', 'done'),
                                            ('events.report.idle:', 'report', 'idle')]:
                site = imported.index(spelling)
                expect_token(tokens, imported, site, 'events', ('namespace', ()))
                expect_token(tokens, imported, site + len('events.'), name, ENUM)
                expect_token(tokens, imported, site + len('events.' + name + '.'), member, REF_MEMBER)
            imported_tokens = fixture.tokens(imports, IMPORTS)
            expect_token(imported_tokens, IMPORTS, IMPORTS.index('status(u8)'), 'status', DECL_ENUM)
            expect_token(imported_tokens, IMPORTS, IMPORTS.index('calm = 0'), 'calm', DECL_MEMBER)

            for original, editing, expected, prefix in [
                ('events.status.calm\n', 'events.status.\n', ['calm', 'noisy'], ''),
                ('events.status.calm:', 'events.status.\n', ['calm', 'noisy'], ''),
                ('events.report.done(8)', 'events.report.', ['done', 'idle'], ''),
                ('events.report.done(var', 'events.report.d(var', ['done'], 'd'),
            ]:
                text = imported.replace(original, editing, 1)
                fixture.edit(text, broken=True)
                before_args = editing.split('(')[0].rstrip()
                items = fixture.items(text.index(editing) + len(before_args), expected, prefix)
                if 'done' in items:
                    assert items['done'].startswith('done(amount: i32) -> '), items
            missing_payload = imported.replace('events.report.done(8)', 'events.report.done')
            fixture.edit(missing_payload, broken=True)
            wrong_pattern_payload = imported.replace('events.report.done(var amount): amount', 'events.report.done(): 1')
            fixture.edit(wrong_pattern_payload, broken=True)

            for source, needle in [
                (imported.replace('events.status.calm\n', 'events.hidden.\n'), 'events.hidden.'),
                (imported.replace('events.status.calm\n', 'events.unknown.\n'), 'events.unknown.'),
                (imported.replace(ARM, 'events.report.'), 'events.report.'),
                (COMPLETE.replace(ARM, 'missing.'), 'missing.'),
                (COMPLETE.replace('        match result:', '        var process_type = 2\n        match result:')
                         .replace(ARM, 'process_type.'), 'process_type.'),
                (imported.replace('    var state = ', '    var events = 2\n    var state = ')
                         .replace('events.status.calm\n', 'events.status.\n'), 'events.status.'),
                (COMPLETE.replace('use "/app.void"', 'use pkg/undeclared').replace(ARM, 'process_type.'), 'process_type.'),
            ]:
                fixture.edit(source, broken=True)
                fixture.items(source.index(needle) + len(needle), [])
                assert not fixture.tokens(), source

            dirty = COMPLETE.replace('    unknown\n', '    unknown\n    unsaved\n').replace(ARM, 'process_type.')
            fixture.edit(dirty, broken=True)
            cursor = dirty.index('process_type.') + len('process_type.')
            fixture.items(cursor, MEMBERS + ['unsaved'])
            denied = dirty.replace('use "/app.void"', 'use pkg/undeclared')
            fixture.edit(denied, broken=True)
            fixture.items(denied.index('process_type.') + len('process_type.'), [])
            assert not fixture.tokens()
            fixture.edit(dirty, broken=True)
            client.notify('textDocument/didChange', {'textDocument': {'uri': main.as_uri(), 'version': fixture.version - 1},
                                                     'contentChanges': [{'text': COMPLETE}]})
            client.sync()
            fixture.items(cursor, MEMBERS + ['unsaved'])
            assert main.read_bytes() == COMPLETE.encode()
            client.notify('textDocument/didClose', {'textDocument': {'uri': main.as_uri()}})
            client.sync()
            assert not fixture.tokens()
            fixture.text = COMPLETE
            fixture.items(COMPLETE.index(ARM) + len('process_type.'), MEMBERS, suffix='none')

            other.write_bytes(b'func main():\n    pass\n')
            selection = request(client, 'void/reselectProject', {'entry': 'other.void'})
            assert selection['status'] == 'ready', selection
            switched = 'func main():\n    var item = process_type.\n'
            assert update(client, other, switched, 1)
            assert query(client, 'completion', other, switched, 'process_type.', True) == []
            assert not fixture.tokens()
            client.shutdown()

        other.unlink()
        strict_check(compiler, root, COMPLETE.replace(ARM, 'process_type.'), False, 'VOID1102')
        strict_check(compiler, root, COMPLETE.replace(ARM, 'process_type.incorrect'), False, 'VOID1101')
        strict_check(compiler, root, missing_payload, False)
        strict_check(compiler, root, wrong_pattern_payload, False)
        strict_check(compiler, root, COMPLETE.replace('process_type.unknown, "Unknown error"', 'process_type.unknown'), False)
    print('enum recovery: exact ranges, current presentation, G15, imports, payloads, numeric members and strict checking passed')


if __name__ == '__main__':
    run(*(Path(arg).resolve() for arg in sys.argv[1:]))
