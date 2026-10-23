if exists('b:current_syntax') | finish | endif
syn keyword voidKeyword operator func var const return if else for in reverse break continue pass use as private public internal struct value interface virtual override extends super property get set enum match has is and or not with defer async await spawn event
syn keyword voidType i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bool string
syn keyword voidConstant true false null Ok Err
syn match voidNumber '\<\d\+\(\.\d\+\)\?[fd]\?\>'
syn region voidString start='"' skip='\\.' end='"' contains=voidEscape,voidBraceEscape,voidInterpolation
syn region voidTripleString start='"""' skip='\\.' end='"""' contains=voidEscape,voidBraceEscape,voidInterpolation
syn match voidEscape '\\\([nrt0"\\]\|u{[0-9a-fA-F]\+}\)' contained
syn region voidInterpolation matchgroup=Delimiter start='{\%({\)\@!' end='}' contained contains=voidKeyword,voidType,voidConstant,voidNumber,voidString,voidTripleString
syn match voidBraceEscape '{{\|}}' contained
syn match voidComment '#.*$'
hi def link voidKeyword Keyword
hi def link voidType Type
hi def link voidConstant Constant
hi def link voidNumber Number
hi def link voidString String
hi def link voidTripleString String
hi def link voidEscape SpecialChar
hi def link voidBraceEscape SpecialChar
hi def link voidComment Comment
" Triple strings can cross any number of lines; don't guess their state from
" a closing quote encountered during Vim's backwards synchronization.
syn sync fromstart
let b:current_syntax = 'void'
