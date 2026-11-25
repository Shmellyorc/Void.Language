" Research-only real Vim/yegappan client probe; uses an isolated runtimepath.
set nocompatible
set hidden
execute 'set runtimepath^=' . fnameescape($VOID_VIM_CLIENT)
execute 'source ' . fnameescape($VOID_VIM_CLIENT . '/plugin/lsp.vim')
filetype on
autocmd BufRead,BufNewFile *.void setfiletype void
call LspOptionsSet({'autoComplete': v:false, 'autoHighlightDiags': v:true})
call LspAddServer([{'name':'void-research','filetype':['void'],'path':$VOIDLS_PATH,'args':['--entry',$VOID_STAGE7_FIXTURE.'/main.void']}])
call LspEnable()
function! WaitUntil(Predicate)
  let start=reltime()
  while !a:Predicate()
    if reltimefloat(reltime(start))>10 | throw 'probe timeout' | endif
    sleep 10m
  endwhile
endfunction
try
  execute 'edit ' . fnameescape($VOID_STAGE7_FIXTURE . '/main.void')
  setfiletype void
  call WaitUntil({-> LspServerReady()})
  call cursor(3,19)
  LspGotoDefinition
  call WaitUntil({-> expand('%:t') ==# 'worker.void'})
  call assert_equal(1,line('.'))
  call WaitUntil({-> LspServerReady()})
  call setline(2,'    return missing')
  " Flush Vim buffer listeners explicitly in the noninteractive Ex probe.
  call listener_flush()
  call WaitUntil({-> !empty(lsp#diag#GetDiagsForBuf())})
  call assert_equal('VOID2002',lsp#diag#GetDiagsForBuf()[0].code)
  call setline(2,'    return x + 1')
  call listener_flush()
  call WaitUntil({-> empty(lsp#diag#GetDiagsForBuf())})
  execute 'buffer ' . bufnr($VOID_STAGE7_FIXTURE . '/main.void')
  call cursor(3,19)
  LspGotoDefinition
  call WaitUntil({-> expand('%:t') ==# 'worker.void'})
  LspServer stop
  call WaitUntil({-> !LspServerRunning('void')})
  if !empty(v:errors) | throw string(v:errors) | endif
  call writefile([json_encode({'editor':execute('version')->split('\n')[0], 'client_sha':$VOID_VIM_CLIENT_SHA, 'passed':['attachment','definition','unsaved VOID2002','repair','definition after repair','client stop']})],$VOID_STAGE7_OUTPUT)
catch
  call writefile([v:exception,v:throwpoint,execute('messages')],$VOID_STAGE7_OUTPUT . '.error')
  cquit 1
endtry
qa!
