vim9script
set nocompatible hidden
execute 'set runtimepath^=' .. fnameescape($VOID_VIM_CLIENT)
execute 'set runtimepath^=' .. fnameescape($VOID_REPO .. '/editors/vim')
execute 'source ' .. fnameescape($VOID_VIM_CLIENT .. '/plugin/lsp.vim')
import autoload 'lsp/lsp.vim'
filetype plugin indent on
call LspOptionsSet({autoComplete: false, autoHighlightDiags: true})
call void#setup($VOIDLS_PATH, $VOID_STAGE8_EXPLICIT_ENTRY ==# '1' ? $VOID_STAGE7_FIXTURE .. '/main.void' : '', $VOID_STAGE7_FIXTURE)
call LspEnable()
def Wait(Predicate: func(): bool)
  var start = reltime()
  while !Predicate()
    if reltimefloat(reltime(start)) > 10 | throw 'timeout' | endif
    sleep 10m
  endwhile
enddef
try
  execute 'edit ' .. fnameescape($VOID_STAGE7_FIXTURE .. '/main.void')
  Wait(() => g:LspServerReady())
  assert_equal('void', &filetype)
  assert_equal(4, &shiftwidth)
  var server = lsp.Server()
  var point = {textDocument: server.getTextDocPosition(false).textDocument, position: {line: 2, character: 18}}
  var hover = server.rpc('textDocument/hover', point)
  assert_match('calculate', hover.result.contents.value)
  var symbols = server.rpc('workspace/symbol', {query: 'calc'})
  assert_equal(1, len(symbols.result))
  var original = getline(1, '$')
  for prefix in ['use ', 'use io/', 'use "', 'use "app/', 'use "app/systems/']
    setline(1, prefix)
    listener_flush()
    var menu = server.rpc('textDocument/completion', {textDocument: point.textDocument, position: {line: 0, character: strlen(prefix)}}).result
    assert_false(empty(menu.items))
    if prefix ==# 'use "app/systems/'
      assert_equal('player.void"', menu.items[0].textEdit.newText)
    endif
  endfor
  setline(1, 'use io/files as files')
  setline(3, '    return files.read(x)')
  listener_flush()
  var standardHover = server.rpc('textDocument/hover', {textDocument: point.textDocument, position: {line: 2, character: 19}})
  assert_match('Editor fixture API.', standardHover.result.contents.value)
  var standardDef = server.rpc('textDocument/definition', {textDocument: point.textDocument, position: {line: 2, character: 19}})
  assert_match('/standard/io/files.void$', standardDef.result.uri)
  var composite = ['use console', 'enum parcel:', '    empty', '    data(value: [i32])', 'func entry(x: i32) -> i32:', '    var p = parcel.data([x])', '    print(p)', '    var r = console.stdout.write("é😀")', '    if p is parcel.data(var numbers):', '        return numbers.count as i32', '    return x']
  setline(1, composite)
  listener_flush()
  var consoleHover = server.rpc('textDocument/hover', {textDocument: point.textDocument, position: {line: 7, character: 29}})
  assert_match('Write UTF-8', consoleHover.result.contents.value)
  var consoleDef = server.rpc('textDocument/definition', {textDocument: point.textDocument, position: {line: 7, character: 29}})
  assert_match('/standard/console.void$', consoleDef.result.uri)
  var patternDef = server.rpc('textDocument/definition', {textDocument: point.textDocument, position: {line: 8, character: 20}})
  assert_equal(3, patternDef.result.range.start.line)
  var printHover = server.rpc('textDocument/hover', {textDocument: point.textDocument, position: {line: 6, character: 6}})
  assert_match('\*values', printHover.result.contents.value)
  setline(6, '    var p = parcel.')
  listener_flush()
  var cases = server.rpc('textDocument/completion', {textDocument: point.textDocument, position: {line: 5, character: 19}})
  assert_equal(['data', 'empty'], mapnew(cases.result, (_, item) => item.label)->sort())
  setline(1, composite)
  setline(7, '    print(p, flush = ')
  listener_flush()
  var printHelp = server.rpc('textDocument/signatureHelp', {textDocument: point.textDocument, position: {line: 6, character: 21}})
  assert_equal(4, printHelp.result.activeParameter)
  assert_match('file: console.output', printHelp.result.signatures[0].label)
  deletebufline('%', 4, '$')
  setline(1, original)
  listener_flush()
  setline(3, '    return worker.')
  listener_flush()
  point.position.character = 18
  var items = server.rpc('textDocument/completion', point)
  assert_equal('calculate', items.result[0].label)
  setline(3, '    return worker.calculate(')
  listener_flush()
  point.position.character = 28
  var sig = server.rpc('textDocument/signatureHelp', point)
  assert_equal(0, sig.result.activeParameter)
  setline(3, '    return worker.calculate(x)')
  listener_flush()
  cursor(3, 19)
  LspGotoDefinition
  Wait(() => expand('%:t') ==# 'worker.void')
  Wait(() => g:LspServerReady())
  var worker = lsp.Server()
  var rename = worker.rpc('textDocument/rename', {textDocument: worker.getTextDocPosition(false).textDocument, position: {line: 0, character: 7}, newName: 'compute'}, {handleError: false})
  assert_true(empty(rename) || has_key(rename, 'error'))
  setline(2, '    return missing')
  listener_flush()
  Wait(() => !empty(lsp#diag#GetDiagsForBuf()))
  assert_equal('VOID2002', lsp#diag#GetDiagsForBuf()[0].code)
  setline(2, '  return x + 1')
  listener_flush()
  Wait(() => empty(lsp#diag#GetDiagsForBuf()))
  VoidFormat
  assert_equal('    return x + 1', getline(2))
  VoidRefresh
  VoidRestart
  Wait(() => g:LspServerReady())
  var restarted = lsp.Server()
  var restartedHover = restarted.rpc('textDocument/hover', {textDocument: restarted.getTextDocPosition(false).textDocument, position: {line: 0, character: 7}})
  assert_match('calculate', restartedHover.result.contents.value)
  LspServer stop
  Wait(() => !g:LspServerRunning('void'))
  if !empty(v:errors) | throw string(v:errors) | endif
  writefile([json_encode({editor: execute('version')->split('\n')[0], passed: ['shipped adapter', 'console source docs/navigation', 'enum pattern navigation/completion', 'print hover/signature', 'hover', 'symbols', 'incomplete completion', 'signature', 'definition', 'rename disabled', 'dirty diagnostics', 'guarded formatting', 'refresh', 'restart', 'stop']})], $VOID_STAGE7_OUTPUT)
catch
  writefile([v:exception, v:throwpoint, execute('messages')], $VOID_STAGE7_OUTPUT .. '.error')
  cquit 1
endtry
qa!
