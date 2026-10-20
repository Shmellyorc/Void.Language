vim9script
set nocompatible hidden
execute 'set runtimepath^=' .. fnameescape($VOID_VIM_CLIENT)
execute 'set runtimepath^=' .. fnameescape($VOID_REPO .. '/editors/vim')
execute 'source ' .. fnameescape($VOID_VIM_CLIENT .. '/plugin/lsp.vim')
import autoload 'lsp/lsp.vim'
filetype plugin indent on
call LspOptionsSet({autoComplete: false, autoHighlightDiags: true})
call void#setup($VOIDLS_PATH, $VOID_STAGE7_FIXTURE .. '/main.void')
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
  writefile([json_encode({editor: execute('version')->split('\n')[0], passed: ['shipped adapter', 'hover', 'symbols', 'incomplete completion', 'signature', 'definition', 'rename disabled', 'dirty diagnostics', 'guarded formatting', 'refresh', 'restart', 'stop']})], $VOID_STAGE7_OUTPUT)
catch
  writefile([v:exception, v:throwpoint, execute('messages')], $VOID_STAGE7_OUTPUT .. '.error')
  cquit 1
endtry
qa!
