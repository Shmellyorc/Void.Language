-- Research-only real Neovim client probe; no editor plugin is installed.
local root = assert(vim.env.VOID_STAGE7_FIXTURE)
local output = assert(vim.env.VOID_STAGE7_OUTPUT)
local function check(ok, msg) if not ok then error(msg) end end
local function wait(fn) local where=debug.traceback(); check(vim.wait(10000, fn, 10), 'timed out: '..where) end
local function main()
  vim.cmd('filetype plugin indent on')
  vim.filetype.add({extension={void='void'}})
  vim.lsp.config('void_research', {cmd={vim.env.VOIDLS_PATH, '--entry', root..'/main.void'}, filetypes={'void'},
    root_dir=function(_, on_dir) on_dir(root) end, flags={debounce_text_changes=0}})
  vim.lsp.enable('void_research')
  vim.cmd.edit(vim.fn.fnameescape(root..'/main.void'))
  wait(function() return #vim.lsp.get_clients({bufnr=0}) == 1 and vim.lsp.get_clients({bufnr=0})[1].initialized end)
  local client = vim.lsp.get_clients({bufnr=0})[1]
  local mainbuf = vim.api.nvim_get_current_buf()
  local params={textDocument={uri=vim.uri_from_fname(root..'/main.void')},position={line=2,character=18}}
  local function definition()
    local response=client:request_sync('textDocument/definition',params,10000,mainbuf)
    check(response and not response.err, 'definition request failed')
    return response.result
  end
  check(definition().uri == vim.uri_from_fname(root..'/worker.void'), 'wrong definition')
  vim.cmd.edit(vim.fn.fnameescape(root..'/worker.void'))
  local worker=vim.api.nvim_get_current_buf()
  wait(function() return #vim.lsp.get_clients({bufnr=worker}) == 1 end)
  vim.api.nvim_buf_set_lines(worker,1,2,false,{'    return missing'})
  wait(function() for _,d in ipairs(vim.diagnostic.get(worker)) do if d.code=='VOID2002' then return true end end end)
  check(definition() == vim.NIL or definition() == nil, 'historical definition returned')
  vim.api.nvim_buf_set_lines(worker,1,2,false,{'    return x + 1'})
  wait(function() return #vim.diagnostic.get(worker)==0 end)
  check(definition().uri == vim.uri_from_fname(root..'/worker.void'), 'repair definition missing')
  local refs=client:request_sync('textDocument/references',vim.tbl_extend('force',params,{context={includeDeclaration=true}}),10000,mainbuf)
  check(refs and #refs.result==2,'wrong reference count')
  client:stop()
  wait(function() return client:is_stopped() end)
  vim.fn.writefile({vim.json.encode({editor=vim.version(),passed={'attachment','definition','unsaved VOID2002','stale suppression','repair','references','graceful client stop'}})},output)
end
local ok,err=pcall(main)
if not ok then vim.fn.writefile({tostring(err)},output..'.error'); vim.cmd('cquit 1') else vim.cmd('qa!') end
