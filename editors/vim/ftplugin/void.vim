if exists('b:did_ftplugin') | finish | endif
let b:did_ftplugin = 1
setlocal expandtab shiftwidth=4 softtabstop=4 tabstop=4 commentstring=#\ %s
let b:undo_ftplugin = 'setlocal expandtab< shiftwidth< softtabstop< tabstop< commentstring<'
