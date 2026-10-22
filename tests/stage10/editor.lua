local function check(value, message) if not value then error(message) end end
local repo=assert(vim.env.VOID_REPO)
local root=vim.fn.tempname()
vim.fn.mkdir(root,'p')
local source={
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
}
local function main()
 vim.opt.runtimepath:prepend(repo..'/editors/neovim')
 vim.opt.runtimepath:prepend(repo..'/editors/vim')
 vim.cmd('filetype plugin indent on')
 vim.cmd('syntax on')
 vim.fn.writefile(source,root..'/main.void')
 require('void').setup({server=assert(vim.env.VOIDLS_PATH),workspace=root})
 vim.cmd.edit(root..'/main.void')
 check(vim.wait(10000,function() local clients=vim.lsp.get_clients({bufnr=0,name='voidls'});return #clients==1 and clients[1].initialized end,10),'server initialization')
 local client=vim.lsp.get_clients({bufnr=0,name='voidls'})[1]
 local uri=vim.uri_from_fname(root..'/main.void')
 local function query(method,line,column)
  local out=client:request_sync(method,{textDocument={uri=uri},position={line=line,character=column}},10000,0)
  check(out and not out.err,'request '..method..': '..vim.inspect(out))
  return out.result
 end
 check(query('textDocument/hover',9,14).contents.value:find('const id',1,true),'const field hover')
 check(query('textDocument/hover',5,18).contents.value:find('string(value: i32)',1,true),'primitive conversion hover')
 local definition=query('textDocument/definition',7,11)
 check(definition.range.start.line==3,'original interpolation expression span')
 local rename=client:request_sync('textDocument/rename',{textDocument={uri=uri},position={line=3,character=13},newName='score'},10000,0)
 check(rename and not rename.err and rename.result,'rename through primitive interpolation')
 local stack=vim.fn.synstack(8,1)
 check(#stack>0 and vim.fn.synIDattr(stack[1],'name')=='voidTripleString','multiline syntax region')
 vim.api.nvim_buf_set_lines(0,9,10,false,{'    return b.secret'})
 check(vim.wait(10000,function()for _,d in ipairs(vim.diagnostic.get(0))do if d.message:find('not accessible',1,true)then return true end end end,10),'dirty field diagnostic')
 vim.api.nvim_buf_set_lines(0,0,-1,false,source)
 check(vim.wait(10000,function()return #vim.diagnostic.get(0)==0 end,10),'dirty repair')
 local temporal={
  'use time',
  'func entry(input: i32) -> i32:',
  '    if make_date(2024, 2, 29) is Ok(var day):',
  '        return day.year as i32',
  '    return input',
 }
 vim.api.nvim_buf_set_lines(0,0,-1,false,temporal)
 check(vim.wait(10000,function()local ok,result=pcall(query,'textDocument/hover',2,11);return ok and result and result.contents.value:find('make_date',1,true)end,10),'temporal hover after native dirty document update')
 local location=query('textDocument/definition',2,11)
 check(location and location.uri:find('/time.void',1,true),'temporal standard-library navigation')
 vim.api.nvim_buf_set_lines(0,3,4,false,{'        return day.'})
 local members=query('textDocument/completion',3,#'        return day.')
 if members.items then members=members.items end
 local names={};for _,item in ipairs(members)do names[item.label]=true end
 check(names.year and names.month and names.day and not names._year,'temporal property completion and private backing field protection')
 client:stop(true)
end
local ok,error_message=pcall(main)
vim.fn.delete(root,'rf')
if not ok then io.stderr:write(tostring(error_message)..'\n');vim.cmd('cquit 1')else print('Neovim field/display/interpolation, temporal APIs and multiline syntax checks passed');vim.cmd('qa!')end
