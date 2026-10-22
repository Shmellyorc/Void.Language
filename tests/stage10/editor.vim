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
  if !empty(v:errors) | throw join(v:errors, "\n") | endif
  writefile(['Vim field/display/interpolation and multiline syntax checks passed'], $VOID_STAGE10_EDITOR_OUTPUT)
catch
  writefile([v:exception, v:throwpoint] + v:errors, $VOID_STAGE10_EDITOR_OUTPUT)
  delete(root, 'rf')
  cquit 1
endtry
delete(root, 'rf')
qa!
