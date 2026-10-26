"""Stage 10 compiler-owned operator and format intelligence over real LSP."""
from pathlib import Path
import sys, tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
checks = 0

def check(value, message):
    global checks
    assert value, message
    checks += 1

with tempfile.TemporaryDirectory(prefix='VOID Step38 é😀 ') as directory:
    file = Path(directory) / 'main.void'
    source = '''value struct box<t>:
    value: t
    ## Exact sum of two boxes.
    operator +(const left: box<t>, const right: box<t>) -> box<t>:
        return box<t>(value = left.value + right.value)
    operator %(const left: box<t>, const right: box<t>) -> box<t>:
        return box<t>(value = left.value % right.value)
    operator ==(const left: box<t>, const right: box<t>) -> bool:
        return left.value == right.value
    private operator -(const left: box<t>, const right: box<t>) -> box<t>:
        return left
func entry(input: i32) -> i32:
    var a = box<i32>(value = input)
    var b = box<i32>(value = 3)
    var c = a + b
    c %= b
    var x = box<i64>(value = 4)
    var y = x + x
    if c != a:
        return c.value
    return input
'''
    file.write_text(source)
    uri = file.as_uri()
    with Client(server, file) as client:
        client.request('initialize', {'capabilities': {'textDocument': {'documentSymbol': {'hierarchicalDocumentSymbolSupport': True}}, 'workspace': {'workspaceEdit': {'documentChanges': True, 'failureHandling': 'textOnlyTransactional'}}}})
        client.notify('initialized', {})
        client.sync()
        version = 0
        current = source
        def replace(text):
            global version, current
            version += 1
            current = text
            if version == 1:
                client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'languageId': 'void', 'version': version, 'text': text}})
            else:
                client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': text}]})
            return [d for n in client.sync() for d in n['diagnostics']]
        def position(needle, delta=0):
            at = current.index(needle) + delta
            line = current.count('\n', 0, at)
            start = current.rfind('\n', 0, at) + 1
            return {'line': line, 'character': len(current[start:at].encode('utf-16-le')) // 2}
        def request(method, needle, delta=0, **extra):
            return client.request(method, {'textDocument': {'uri': uri}, 'position': position(needle, delta), **extra})[0]
        def query(method, needle, delta=0, **extra):
            response = request(method, needle, delta, **extra)
            check('error' not in response, response)
            return response.get('result')
        def hover(needle, delta=0):
            out = query('textDocument/hover', needle, delta)
            check(out, (needle, out))
            return out['contents']['value']
        def completion(needle, delta=0):
            out = query('textDocument/completion', needle, delta)
            return {c['label']: c for c in (out.get('items', []) if isinstance(out, dict) else out)}
        check(not replace(source), 'operator source')
        for needle, delta, glyph, owner in [('a + b', 2, '+', 'box<i32>'), ('x + x', 2, '+', 'box<i64>'), ('c %= b', 2, '%', 'box<i32>'), ('c != a', 2, '==', 'box<i32>')]:
            text = hover(needle, delta)
            check('operator '+glyph+'(' in text and owner in text and 'Effects:' in text and 'readonly' in text, text)
            definition = query('textDocument/definition', needle, delta)
            check(definition and definition['range']['start']['line'] in [3,5,7], definition)
            refs = query('textDocument/references', needle, delta, context={'includeDeclaration': True})
            check(len(refs) >= 2, refs)
            check('error' in request('textDocument/rename', needle, delta, newName='sum'), 'glyph rename allowed')
        items = completion('a + b', 2)
        check('+' in items and '%' in items and '==' in items and '!=' in items and '-' not in items, items)
        check('box<i32>' in items['+']['detail'] and 'pending analysis' not in items['+']['detail'] and 'Exact sum' in items['+']['documentation'], items)
        items = completion('c %= b', 2)
        check('+=' in items and '%=' in items and '==' not in items and '-=' not in items, items)
        symbols = client.request('textDocument/documentSymbol', {'textDocument': {'uri': uri}})[0]['result']
        check(any(s['kind']==25 for s in symbols), symbols)
        # Ordinary binding rename still validates all operator references.
        renamed = query('textDocument/rename', 'input: i32', 1, newName='score')
        check(renamed and renamed['documentChanges'], renamed)
        check(replace(source.replace('a + b','a - b')), 'private operator accepted')
        check(query('textDocument/hover', 'a - b', 2) is None, 'stale hover')
        check(not replace(source), 'repair')
        check('box<i64>' in hover('x + x',2), 'repair exact selection')
        generic_body=source[:source.index('func entry')]+'value struct other:\n    value: i32\n    operator +(const left: other, const right: other) -> other:\n        return left\nfunc sum<t>(const left: t, const right: t) -> t:\n    return left + right\nfunc entry(input: i32) -> i32:\n    var a = sum(box<i32>(), box<i32>())\n    var b = sum(box<i64>(), box<i64>())\n    var c = sum(other(), other())\n    return input\n'
        check(not replace(generic_body),'polymorphic operator body')
        text=hover('return left + right',len('return left '))
        check('box<i32>' in text and 'box<i64>' in text and 'other' in text and 'Selected specializations' in text,text)
        targets=query('textDocument/definition','return left + right',len('return left '))
        check(isinstance(targets,list) and len(targets)==2,targets)
        refs=query('textDocument/references','return left + right',len('return left '),context={'includeDeclaration':True})
        check(len(refs)==3,refs)
        items=completion('return left + right',len('return left '))
        check('+' in items and 'box<i32>' in items['+']['detail'] and 'box<i64>' in items['+']['detail'] and 'other' in items['+']['detail'] and '%' not in items,items)
        check('error' in request('textDocument/rename','return left + right',len('return left '),newName='sum'),'polymorphic glyph rename allowed')
        # All declared glyph families share compiler selection, documentation and spans.
        owner='value struct number:\n    value: i32\n'
        for glyph in ['+', '-', '*', '/', '%', '==', '<', '>', '<=', '>=']:
            result='bool' if glyph in ['==','<','>','<=','>='] else 'number'
            value='true' if result=='bool' else 'left'
            owner+=f'    operator {glyph}(const left: number, const right: number) -> {result}:\n        return {value}\n'
        for glyph in ['+','-']:
            owner+=f'    operator {glyph}(const value: number) -> number:\n        return value\n'
        body='func entry(input: i32) -> i32:\n    var a = number(value = input)\n'
        for n,glyph in enumerate(['+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=']):
            body+=f'    var result_{n} = a {glyph} a\n'
        body+='    var positive = +a\n    var negative = -a\n    return input\n'
        check(not replace(owner+body),'glyph families source')
        for glyph in ['+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=']:
            text=hover(f'a {glyph} a',2)
            check('operator '+('==' if glyph=='!=' else glyph)+'(' in text,text)
        for name in ['positive','negative']:
            text=hover(f'{name} = ',len(name+' = '))
            check('value: readonly' in text and 'left:' not in text,text)
        generic_fields='value struct shell<t>:\n    private secret: t\n    const id: t\nfunc entry(input: i32) -> i32:\n    var item = shell<i32>()\n    return item.id\n'
        check(not replace(generic_fields),'generic field source')
        text=hover('item.id',5)
        check('const id: i32' in text and 'shell<i32>' in text,text)
        replace(generic_fields.replace('return item.id','return item.'))
        items=completion('return item.',len('return item.'))
        check('id' in items and 'secret' not in items and 'const' in items['id']['detail'],items)
        # Direct-owner imports govern both execution and completion.
        foreign=Path(directory)/'foreign.void'
        foreign.write_text('value struct token:\n    value: i32\n    operator +(const left: token, const right: token) -> i32:\n        return left.value + right.value\n')
        imported='use "foreign.void" as f\nfunc entry(input: i32) -> i32:\n    var a = f.token(value = input)\n    return a + a\n'
        check(not replace(imported),'imported operators')
        check('+' in completion('a + a',2),'direct owner completion')
        check(query('textDocument/definition','a + a',2)['uri']==foreign.as_uri(),'foreign declaration identity')
        # Nominal hooks keep ordinary documentation, concrete types and references.
        hooks='struct record:\n    private const value: i32 = 4\n## A readonly custom display hook.\nfunc (self record) string() -> string:\n    return "{self.value}"\n## A checked custom pattern hook.\nfunc (self record) format(spec: string) -> result<string, string>:\n    return format(self.value, spec)\nfunc entry(input: i32) -> i32:\n    var item = record()\n    var default_text = string(item)\n    var custom_text = format(item, "000")\n    var explicit_text = item.string()\n    return input\n'
        check(not replace(hooks),'hook source')
        for needle,glyph,line in [('string(item)','string',3), ('format(item, "000")','format',6), ('item.string()','string',3)]:
            delta=5 if needle.startswith('item.') else 2
            text=hover(needle,delta)
            check(glyph+'(' in text and 'custom' in text and 'Declared by' in text,text)
            check(query('textDocument/definition',needle,delta)['range']['start']['line']==line,'custom hook definition')
        refs=query('textDocument/references','item.string()',5,context={'includeDeclaration':True})
        check(len(refs)==3,refs)
        aligned=hooks.replace('    var explicit_text = item.string()', '    var explicit_text = item.string()\n    var aligned_text = format(item, "<12")')
        check(not replace(aligned),'layout-only hook source')
        check(query('textDocument/definition','format(item, "<12")',2)['range']['start']['line']==3,'layout-only conversion must use string hook')
        dynamic=hooks+'func render(item: record, spec: string) -> result<string, string>:\n    return format(item, spec)\n'
        check(not replace(dynamic),'dynamic hook source')
        targets=query('textDocument/definition','format(item, spec)',2)
        check(isinstance(targets,list) and len(targets)==2,'dynamic format lost conditional hook targets')
        # A malformed hole must clear the previous index, then recover original spans.
        check(replace(hooks.replace('item.string()', 'item.string(')),'malformed call diagnostic')
        check(query('textDocument/hover','string(item)',2) is None,'stale hook result')
        check(not replace(hooks),'hook repair')
        numeric = '''## @param value The input number.
func identity(value: i32) -> i32:
    return value
func entry(input: i32) -> i32:
    var text = """é😀 {{escaped}}:
        {identity(input):}
    """
    var shown = format(input, "")
    return input
'''.replace('\n','\r\n')
        check(not replace(numeric), 'CRLF numeric')
        items = completion('input):}', len('input):'))
        check('000' in items and 'X' in items and '<12' in items, items)
        items = completion('format(input, "")',len('format(input, "'))
        check('N2' in items and 'E3' in items and 'B' in items, items)
        check('identity' in hover('identity(input)',2), 'hole function hover')
        check(query('textDocument/definition','identity(input)',2)['range']['start']['line']==1, 'hole definition')
        refs = query('textDocument/references','value: i32',1,context={'includeDeclaration':True})
        check(len(refs)==2, refs)
        # Signature help in an existing interpolation call, followed by dirty completion.
        signature = query('textDocument/signatureHelp','identity(input)',len('identity('))
        check(signature and 'value' in signature['signatures'][0]['label'], signature)
        replace(numeric.replace('identity(input)', 'identity(inp)'))
        items=completion('identity(inp)',len('identity(inp'))
        check('input' in items, items)
        check(not replace(numeric.replace('identity(input):','identity(input) ')),'hole without format delimiter')
        items=completion('identity(input) }',len('identity(input) '))
        check('000' not in items,'format edit offered without colon')
        floating=numeric.replace('format(input, "")','format(2.0d, "")')
        replace(floating)
        items=completion('format(2.0d, "")',len('format(2.0d, "'))
        check('F2' in items and 'X' not in items and 'B' not in items, items)
        temporal='''use time
func entry(input: i32) -> i32:
    if make_date(2024, 2, 29) is Ok(var day):
        var text = "{day:}"
        var shown = format(day, "")
        return day.year as i32
    return input
'''
        check(not replace(temporal), 'temporal source')
        for needle,delta in [('day:}',4), ('format(day, "")',len('format(day, "'))]:
            items=completion(needle,delta)
            check('yyyy-MM-dd' in items and 'HH:mm:ss' not in items and 'X' not in items, items)
        check(query('textDocument/definition','day.year',5)['uri'].endswith('/time.void'), 'library identity')
        check('error' in request('textDocument/rename','day.year',5,newName='years'), 'library rename allowed')
        check('make_date' in hover('make_date(2024',2), 'temporal docs')
        client.shutdown()
print(f'{checks} Stage 10 LSP intelligence checks passed')
