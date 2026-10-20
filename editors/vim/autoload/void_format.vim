vim9script
import autoload 'lsp/lsp.vim'
export def Apply()
    var server = lsp.Server()
    if empty(server) | return | endif
    var buf = bufnr()
    var tick = b:changedtick
    var params = server.getTextDocPosition(false)
    var reply = server.rpc('textDocument/formatting', {textDocument: params.textDocument, options: {tabSize: 4, insertSpaces: true}})
    if bufnr() != buf || b:changedtick != tick || empty(reply) || empty(reply.result) | return | endif
    if len(reply.result) != 1 || reply.result[0].range.start != {line: 0, character: 0}
        throw 'VOID formatter returned an unsupported edit'
    endif
    var lines = split(reply.result[0].newText, '\r\n\|\n\|\r', true)
    var final_eol = !empty(lines) && lines[-1] == ''
    if final_eol | remove(lines, -1) | endif
    if empty(lines) | lines = [''] | endif
    var view = winsaveview()
    setline(1, lines)
    if line('$') > len(lines) | deletebufline(buf, len(lines) + 1, '$') | endif
    &l:endofline = final_eol
    winrestview(view)
enddef
