local function check(value, message) if not value then error(message) end end
local repo = assert(vim.env.VOID_REPO)
local root = vim.fn.tempname()
vim.fn.mkdir(root, 'p')
local source = {
 'use "./game.void" as game',
 '## Shared score.',
 'var score = 0',
 'func main():',
 '    game.reward()',
 '    print(score, game.defeated)',
}
local function main()
 vim.opt.runtimepath:prepend(repo .. '/editors/neovim')
 vim.cmd('filetype plugin indent on')
 vim.fn.writefile(source, root .. '/main.void')
 vim.fn.writefile({'var defeated = 0', 'func reward():', '    score += 10', '    defeated += 1'}, root .. '/game.void')
 require('void').setup({server = assert(vim.env.VOIDLS_PATH), workspace = root})
 vim.cmd.edit(root .. '/main.void')
 check(vim.wait(10000, function()
  local clients = vim.lsp.get_clients({bufnr = 0, name = 'voidls'})
  return #clients == 1 and clients[1].initialized
 end, 10), 'server initialization')
 local client = vim.lsp.get_clients({bufnr = 0, name = 'voidls'})[1]
 local uri = vim.uri_from_fname(root .. '/main.void')
 local function query(method, line, column, extra)
  local params = {textDocument = {uri = uri}, position = {line = line, character = column}}
  for key, value in pairs(extra or {}) do params[key] = value end
  local result = client:request_sync(method, params, 10000, 0)
  check(result and not result.err, method .. ': ' .. vim.inspect(result))
  return result.result
 end
 check(query('textDocument/hover', 5, 10).contents.value:find('var score: i32', 1, true), 'global hover')
 local definition = query('textDocument/definition', 5, 23)
 check(definition and definition.uri:find('/game.void', 1, true), 'cross-file global definition')
 local refs = query('textDocument/references', 2, 5, {context = {includeDeclaration = true}})
 check(#refs == 3, 'root global reference identity')
 local rename = query('textDocument/rename', 2, 5, {newName = 'points'})
 check(rename and #rename.documentChanges == 2, 'global safe rename')
 vim.api.nvim_buf_set_lines(0, 5, 6, false, {'    print(game.)'})
 check(vim.wait(10000, function()
  local items = query('textDocument/completion', 5, #'    print(game.')
  if items.items then items = items.items end
  for _, item in ipairs(items) do if item.label == 'defeated' then return true end end
  return false
 end, 20), 'global alias completion after native document synchronization')
 vim.api.nvim_buf_set_lines(0, 0, -1, false, source)
 check(vim.wait(10000, function() return #vim.diagnostic.get(0) == 0 end, 10), 'overlay repair')
 vim.lsp.stop_client(client.id, true)
 print('Stage 11 Neovim global checks passed')
end
local okay, failure = pcall(main)
vim.fn.delete(root, 'rf')
if not okay then io.stderr:write(tostring(failure) .. '\n'); vim.cmd('cquit 1') else vim.cmd('qa!') end
