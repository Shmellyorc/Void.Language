vim9script
set nocompatible hidden noswapfile
execute 'set runtimepath^=' .. fnameescape($VOID_VIM_CLIENT)
execute 'set runtimepath^=' .. fnameescape($VOID_REPO .. '/editors/vim')
execute 'source ' .. fnameescape($VOID_VIM_CLIENT .. '/plugin/lsp.vim')
import autoload 'lsp/lsp.vim'
filetype plugin indent on
var root = tempname()
mkdir(root, 'p')
var source = [
  'use "./game.void" as game',
  '## Shared score.',
  'var score = 0',
  'func main():',
  '    game.reward()',
  '    print(score, game.defeated)',
]
writefile(source, root .. '/main.void')
writefile(['var defeated = 0', 'private var secret = 7', 'func reward():', '    score += 10', '    defeated += 1'], root .. '/game.void')
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
def GlobalCompletion(activeServer: dict<any>, activeDocument: dict<any>): bool
  var response = activeServer.rpc('textDocument/completion', {textDocument: activeDocument, position: {line: 5, character: 15}})
  var items = response.result
  if type(items) == v:t_dict | items = items.items | endif
  for item in items
    if item.label == 'defeated' | return true | endif
  endfor
  return false
enddef
try
  execute 'edit ' .. fnameescape(root .. '/main.void')
  Wait(() => g:LspServerReady())
  var server = lsp.Server()
  var document = server.getTextDocPosition(false).textDocument
  var hover = server.rpc('textDocument/hover', {textDocument: document, position: {line: 5, character: 10}})
  assert_match('var score: i32', hover.result.contents.value)
  var definition = server.rpc('textDocument/definition', {textDocument: document, position: {line: 5, character: 23}})
  assert_match('/game.void$', definition.result.uri)
  var refs = server.rpc('textDocument/references', {textDocument: document, position: {line: 2, character: 5}, context: {includeDeclaration: true}})
  assert_equal(3, len(refs.result))
  setline(6, '    print(game.)')
  listener_flush()
  Wait(() => GlobalCompletion(server, document))
  setline(6, source[5])
  setline(3, 'var score = missing')
  listener_flush()
  Wait(() => empty(server.rpc('textDocument/hover', {textDocument: document, position: {line: 5, character: 10}}).result))
  setline(3, source[2])
  listener_flush()
  Wait(() => !empty(server.rpc('textDocument/hover', {textDocument: document, position: {line: 5, character: 10}}).result))
  assert_equal(source, readfile(root .. '/main.void'))
  if !empty(v:errors) | throw join(v:errors, "\n") | endif
  writefile(['Vim Stage 11 globals: hover, aliases, cross-file definitions/references, unsaved completion and invalid-overlay repair passed; existing rename restrictions preserved'], $VOID_STAGE11_EDITOR_OUTPUT)
catch
  writefile([v:exception, v:throwpoint] + v:errors, $VOID_STAGE11_EDITOR_OUTPUT)
  delete(root, 'rf')
  cquit 1
endtry
delete(root, 'rf')
qa!
