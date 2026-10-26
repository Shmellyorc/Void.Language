-- Compiler-owned language service; this adapter only controls editor lifecycle/edits.
local M = {}
local function client()
  return vim.lsp.get_clients({bufnr=0,name='voidls'})[1]
end
-- The builtin signature command leaves an old float open on a null reply.
-- Own only this adapter's window; documentation and suppression belong to voidls.
local signature_window,signature_generation=nil,0
local function close_signature()
  if signature_window and vim.api.nvim_win_is_valid(signature_window) then
    vim.api.nvim_win_close(signature_window,true)
  end
  signature_window=nil
end
function M.signature_help()
  signature_generation=signature_generation+1
  local generation=signature_generation
  close_signature()
  local c=client();if not c then return end
  local buf=vim.api.nvim_get_current_buf()
  local win=vim.api.nvim_get_current_win()
  local cursor=vim.api.nvim_win_get_cursor(win)
  local tick=vim.api.nvim_buf_get_changedtick(buf)
  c:request('textDocument/signatureHelp',vim.lsp.util.make_position_params(win,c.offset_encoding),function(err,result)
    if generation~=signature_generation or not vim.api.nvim_buf_is_valid(buf) or
      vim.api.nvim_get_current_buf()~=buf or vim.api.nvim_get_current_win()~=win or
      vim.api.nvim_buf_get_changedtick(buf)~=tick or
      not vim.deep_equal(vim.api.nvim_win_get_cursor(win),cursor) then return end
    if err or not result or not result.signatures or #result.signatures==0 then return end
    local lines,highlight=vim.lsp.util.convert_signature_help_to_markdown_lines(result,vim.bo[buf].filetype,c.server_capabilities.signatureHelpProvider.triggerCharacters)
    if not lines or #lines==0 then return end
    local preview
    preview,signature_window=vim.lsp.util.open_floating_preview(lines,'markdown',{
      focus_id='void.signature_help',
      close_events={'CursorMoved','CursorMovedI','InsertCharPre','TextChanged','TextChangedI','BufLeave'},
    })
    if highlight then
      vim.hl.range(preview,vim.api.nvim_create_namespace('void.signature_help'),'LspSignatureActiveParameter',
        {highlight[1],highlight[2]},{highlight[3],highlight[4]})
    end
  end,buf)
end
function M.versioned_edit(edit)
  if not edit or edit.changes or not edit.documentChanges then return false end
  for _, change in ipairs(edit.documentChanges) do
    if not change.textDocument or not change.edits then return false end
    local d = change.textDocument
    if d.version == vim.NIL or d.version == nil then
      local buf=vim.uri_to_bufnr(d.uri)
      if vim.api.nvim_buf_is_loaded(buf) and vim.bo[buf].modified then
        vim.notify('VOID: newly opened dirty rename target rejected',vim.log.levels.WARN)
        return false
      end
    else
      local buf = vim.uri_to_bufnr(d.uri)
      if not vim.api.nvim_buf_is_loaded(buf) or vim.lsp.util.buf_versions[buf] ~= d.version then
        vim.notify('VOID: stale rename rejected', vim.log.levels.WARN)
        return false
      end
    end
  end
  -- No yield between the version preflight and the built-in edit application.
  vim.lsp.util.apply_workspace_edit(edit, 'utf-16')
  return true
end
function M.format()
  local c=client(); if not c then return end
  local buf=vim.api.nvim_get_current_buf()
  local tick=vim.api.nvim_buf_get_changedtick(buf)
  c:request('textDocument/formatting', {textDocument={uri=vim.uri_from_bufnr(buf)},options={tabSize=4,insertSpaces=true}}, function(err, edits)
    if err then vim.notify(err.message,vim.log.levels.WARN);return end
    if vim.api.nvim_buf_is_valid(buf) and vim.api.nvim_buf_is_loaded(buf) and vim.api.nvim_buf_get_changedtick(buf)==tick then
      vim.lsp.util.apply_text_edits(edits or {},buf,'utf-16')
    else vim.notify('VOID: stale formatting rejected',vim.log.levels.WARN) end
  end,buf)
end
function M.refresh()
  local c=client();if c then c:request('void/refreshProject',{},function(err) if err then vim.notify(err.message) end end,0) end
end
function M.setup(opts)
  opts=opts or {}
  assert(vim.fn.has('nvim-0.11.5')==1,'VOID requires Neovim 0.11.5+')
  local executable=opts.server or vim.fn.exepath('voidls')
  assert(executable~='' and vim.fn.executable(executable)==1,'Install voidls on PATH or set server to an absolute executable')
  executable=vim.fn.fnamemodify(executable,':p')
  local entry=opts.entry and vim.fn.fnamemodify(opts.entry,':p') or nil
  local selected=false
  vim.filetype.add({extension={void='void'}})
  local group=vim.api.nvim_create_augroup('VoidLanguage',{clear=true})
  vim.api.nvim_create_autocmd('FileType',{group=group,pattern='void',callback=function(args)
    vim.bo[args.buf].expandtab=true;vim.bo[args.buf].shiftwidth=4;vim.bo[args.buf].tabstop=4
    if not selected then
      local result=vim.system({executable,'--discover',opts.workspace or vim.fn.getcwd(),vim.api.nvim_buf_get_name(args.buf),entry or ''},{text=true}):wait(10000)
      assert(result.code==0,result.stderr or 'VOID project discovery failed')
      local project=vim.json.decode(result.stdout)
      local standalone=project.standalone
      entry=project.entry
      local root=project.root
      local caps=vim.lsp.protocol.make_client_capabilities()
      caps.workspace.workspaceEdit.documentChanges=true
      vim.lsp.config('voidls',{
        cmd={executable,'--entry',entry},filetypes={'void'},capabilities=caps,
        init_options={voidVersionedEdits=true},
        root_dir=function(buf,on_dir)
          local file=vim.api.nvim_buf_get_name(buf)
          if (standalone and file==entry) or (not standalone and file:sub(1,#root+1)==root..'/') then on_dir(root) end
        end,
        handlers={['textDocument/rename']=function(err,result)
          if err then vim.notify(err.message,vim.log.levels.WARN) elseif result then M.versioned_edit(result) end
        end},
        on_attach=function(c,buf)
          if vim.lsp.completion then vim.lsp.completion.enable(true,c.id,buf,{autotrigger=true}) end
          local keys={gd=vim.lsp.buf.definition,gr=vim.lsp.buf.references,K=vim.lsp.buf.hover,
            ['<leader>rn']=vim.lsp.buf.rename,['<leader>vf']=M.format,['<leader>vr']=M.refresh,
            ['<C-k>']=M.signature_help,['<leader>vd']=vim.diagnostic.open_float}
          for key,fn in pairs(keys)do vim.keymap.set('n',key,fn,{buffer=buf,silent=true})end
        end,
      })
      selected=true;vim.lsp.enable('voidls')
    end
  end})
  vim.api.nvim_create_user_command('VoidFormat',M.format,{force=true})
  vim.api.nvim_create_user_command('VoidRefresh',M.refresh,{force=true})
  vim.api.nvim_create_user_command('VoidRestart',function()
    for _,c in ipairs(vim.lsp.get_clients({name='voidls'}))do c:stop()end
    vim.lsp.enable('voidls',false);vim.lsp.enable('voidls',true)
  end,{force=true})
end
return M
