if exists('b:did_indent') | finish | endif
let b:did_indent = 1
setlocal autoindent expandtab shiftwidth=4 softtabstop=4
let b:undo_indent = 'setlocal autoindent< expandtab< shiftwidth< softtabstop<'
