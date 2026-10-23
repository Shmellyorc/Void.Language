vim9script
set nocompatible hidden noswapfile
execute 'set runtimepath^=' .. fnameescape($VOID_VIM_CLIENT)
execute 'set runtimepath^=' .. fnameescape($VOID_REPO .. '/editors/vim')
execute 'source ' .. fnameescape($VOID_VIM_CLIENT .. '/plugin/lsp.vim')
import autoload 'lsp/lsp.vim'
filetype plugin indent on
syntax on
var root = tempname()
mkdir(root, 'p')
var source = [
  'struct box:',
  '    private secret: i32 = 7',
  '    const id: i32',
  'func entry(input: i32) -> i32:',
  '    var b = box()',
  '    var shown = string(input)',
  '    var text = """Score:',
  '        {input:000}',
  '    """',
  '    return b.id + (shown.length as i32)',
]
writefile(source, root .. '/main.void')
call LspOptionsSet({autoComplete: false, autoHighlightDiags: true})
call void#setup($VOIDLS_PATH, '', root)
call LspEnable()
def Wait(Predicate: func(): bool)
  var start = reltime()
  while !Predicate()
    if reltimefloat(reltime(start)) > 10 | throw 'timeout' | endif
    sleep 10m
  endwhile
enddef
# Delay only the edit application; the reply comes from the real server.
def g:Step39FormatRpc(method: string, params: any): dict<any>
  var reply = call(g:Step39NativeRpc, [method, params])
  if method == 'textDocument/formatting'
    assert_false(empty(reply.result), 'stale native reply must contain a proposed edit')
    append(0, '# newer revision')
    listener_flush()
  endif
  return reply
enddef
try
  execute 'edit ' .. fnameescape(root .. '/main.void')
  Wait(() => g:LspServerReady())
  var server = lsp.Server()
  var document = server.getTextDocPosition(false).textDocument
  var hover = server.rpc('textDocument/hover', {textDocument: document, position: {line: 9, character: 14}})
  assert_match('const id', hover.result.contents.value)
  hover = server.rpc('textDocument/hover', {textDocument: document, position: {line: 5, character: 18}})
  assert_match('string(value: i32)', hover.result.contents.value)
  var definition = server.rpc('textDocument/definition', {textDocument: document, position: {line: 7, character: 11}})
  assert_equal(3, definition.result.range.start.line)
  assert_equal('voidTripleString', synIDattr(synstack(8, 1)[0], 'name'))
  setline(10, '    return b.secret')
  listener_flush()
  Wait(() => !empty(lsp#diag#GetDiagsForBuf()))
  assert_match('not accessible', lsp#diag#GetDiagsForBuf()[0].message)
  setline(1, source)
  listener_flush()
  Wait(() => empty(lsp#diag#GetDiagsForBuf()))
  var operators = [
    'value struct number:',
    '    value: i32',
    '    operator +(const left: number, const right: number) -> number:',
    '        return left',
    '    operator %(const left: number, const right: number) -> number:',
    '        return left',
    'func entry(input: i32) -> i32:',
    '    var a = number(value = input)',
    '    var b = a + a',
    '    b %= a',
    '    var text = "{input:000}"',
    '    return b.value',
  ]
  setline(1, operators)
  listener_flush()
  # Barrier: a successful response from the updated document, never old metadata.
  Wait(() => !empty(server.rpc('textDocument/hover', {textDocument: document, position: {line: 8, character: 14}}).result))
  hover = server.rpc('textDocument/hover', {textDocument: document, position: {line: 8, character: 14}})
  assert_match('operator +(', hover.result.contents.value)
  assert_match('Effects: synchronous', hover.result.contents.value)
  definition = server.rpc('textDocument/definition', {textDocument: document, position: {line: 9, character: 6}})
  assert_equal(4, definition.result.range.start.line)
  var items = server.rpc('textDocument/completion', {textDocument: document, position: {line: 8, character: 14}}).result
  assert_true(index(mapnew(items, (_, item) => item.label), '+') >= 0)
  assert_true(index(mapnew(items, (_, item) => item.label), '%') >= 0)
  assert_equal(-1, index(mapnew(items, (_, item) => item.label), '-'))
  items = server.rpc('textDocument/completion', {textDocument: document, position: {line: 10, character: len('    var text = "{input:')}}).result
  assert_true(index(mapnew(items, (_, item) => item.label), '000') >= 0)
  assert_true(index(mapnew(items, (_, item) => item.label), 'X') >= 0)
  var ugly = mapnew(operators, (_, text) => substitute(text, '^ *', '\=repeat(" ", len(submatch(0)) / 2)', ''))
  ugly[10] = '  var text = """é😀 {{escaped}}:'
  insert(ugly, '    {input:>8|000}', 11)
  insert(ugly, '  """', 12)
  setline(1, ugly)
  listener_flush()
  VoidFormat
  assert_equal('    var a = number(value = input)', getline(8))
  assert_match('"""é😀 {{escaped}}:\n    {input:>8|000}\n  """', join(getline(1, '$'), "\n"))
  listener_flush()
  assert_equal([], server.rpc('textDocument/formatting', {textDocument: document, options: {tabSize: 4, insertSpaces: true}}).result)
  setline(1, ugly)
  listener_flush()
  g:Step39NativeRpc = server.rpc
  server.rpc = function('g:Step39FormatRpc')
  VoidFormat
  server.rpc = g:Step39NativeRpc
  assert_equal('# newer revision', getline(1))
  assert_equal('  var a = number(value = input)', getline(9))
  var temporal = [
    'use time',
    'func entry(input: i32) -> i32:',
    '    if make_date(2024, 2, 29) is Ok(var day):',
    '        return day.year as i32',
    '    return input',
  ]
  setline(1, temporal)
  deletebufline(bufnr(), len(temporal) + 1, '$')
  listener_flush()
  Wait(() => !empty(server.rpc('textDocument/hover', {textDocument: document, position: {line: 2, character: 11}}).result))
  hover = server.rpc('textDocument/hover', {textDocument: document, position: {line: 2, character: 11}})
  assert_match('make_date', hover.result.contents.value)
  definition = server.rpc('textDocument/definition', {textDocument: document, position: {line: 2, character: 11}})
  assert_match('/time.void$', definition.result.uri)
  if !empty(v:errors) | throw join(v:errors, "\n") | endif
  writefile(['Vim field/display/interpolation, operators, temporal APIs, format suggestions, conservative formatting/stale edits and multiline syntax checks passed'], $VOID_STAGE10_EDITOR_OUTPUT)
catch
  writefile([v:exception, v:throwpoint] + v:errors, $VOID_STAGE10_EDITOR_OUTPUT)
  delete(root, 'rf')
  cquit 1
endtry
delete(root, 'rf')
qa!
