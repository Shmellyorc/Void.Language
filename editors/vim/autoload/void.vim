" Vim 9.1+ and pinned yegappan/lsp. Configuration is explicit trusted user code.
function! void#setup(server, entry, ...) abort
  if !has('patch-9.1.0') | throw 'VOID requires Vim 9.1+' | endif
  let executable = empty(a:server) ? exepath('voidls') : fnamemodify(a:server, ':p')
  if !executable(executable) | throw 'Install voidls on PATH or configure an executable' | endif
  let boundary = a:0 ? a:1 : getcwd()
  let override = empty(a:entry) ? '' : fnamemodify(a:entry, ':p')
  let command = join(map([executable, '--discover', boundary, expand('%:p'), override], 'shellescape(v:val)'), ' ')
  let result = system(command)
  if v:shell_error | throw 'VOID project discovery failed: ' . result | endif
  let project = json_decode(result)
  let entry = project.entry
  call LspAddServer([{'name':'voidls', 'filetype':['void'], 'path':executable, 'args':['--entry',entry]}])
  augroup void_lsp_keys
    autocmd!
    autocmd FileType void nnoremap <buffer> gd <Cmd>LspGotoDefinition<CR>
    autocmd FileType void nnoremap <buffer> gr <Cmd>LspShowReferences<CR>
    autocmd FileType void nnoremap <buffer> K <Cmd>LspHover<CR>
    autocmd FileType void nnoremap <buffer> <C-k> <Cmd>LspShowSignature<CR>
    autocmd FileType void nnoremap <buffer> <leader>vf <Cmd>VoidFormat<CR>
    autocmd FileType void nnoremap <buffer> <leader>vr <Cmd>VoidRefresh<CR>
  augroup END
  command! VoidRefresh call LspRequestCustom('voidls','void/refreshProject',{})
  command! VoidFormat call void#format()
  command! VoidRestart LspServer restart
endfunction
function! void#format() abort
  " Whole-document formatting only. Check changedtick after the synchronous RPC
  " before applying. The Vim client remains unable to enforce rename versions.
  call void#format_rpc()
endfunction
function! void#format_rpc() abort
  call void_format#Apply()
endfunction
