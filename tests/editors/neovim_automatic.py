"""Real POSIX Neovim terminal typing/automatic-menu acceptance probe."""
import os,pty,pathlib,tempfile,subprocess,time,select,json,fcntl,termios,struct,argparse
p=argparse.ArgumentParser();p.add_argument("--server",required=True);p.add_argument("--nvim",default="nvim");p.add_argument("--output",required=True);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(a.output).resolve();server=pathlib.Path(a.server).resolve()
with tempfile.TemporaryDirectory(prefix='VOID real terminal ') as d:
 root=pathlib.Path(d);(root/'standard/io').mkdir(parents=True);(root/'app/systems').mkdir(parents=True)
 (root/'main.void').write_text('func entry(x: i32) -> i32:\n    return x\n');(root/'standard/io/files.void').write_text('');(root/'app/systems/player.void').write_text('')
 config=root/'init.lua';config.write_text('vim.opt.completeopt="menu,menuone,noinsert"\n'+'vim.opt.runtimepath:prepend('+json.dumps(str(repo/'editors/neovim'))+')\nvim.opt.runtimepath:prepend('+json.dumps(str(repo/'editors/vim'))+')\nvim.cmd("filetype plugin indent on")\nrequire("void").setup({server='+json.dumps(str(server))+',workspace='+json.dumps(d)+'})\n')
 sock=root/'nvim.sock';master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,100,0,0))
 env=dict(os.environ,TERM='xterm-256color',VOID_STDLIB_ROOT=str(root/'standard'),XDG_STATE_HOME=str(root/'state'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'))
 p=subprocess.Popen([a.nvim,'--listen',str(sock),'-u',str(config),'-i','NONE',str(root/'main.void')],stdin=slave,stdout=slave,stderr=slave,env=env,start_new_session=True);os.close(slave)
 terminal=bytearray()
 def drain():
  while select.select([master],[],[],0)[0]:
   try:terminal.extend(os.read(master,65536))
   except OSError:return
 def expr(value):
  drain();r=subprocess.run([a.nvim,'--server',str(sock),'--remote-expr',value],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=3);return r.stdout.strip() if r.returncode==0 else ''
 def wait(fn,seconds=10):
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   drain()
   if fn():return True
   time.sleep(.03)
  return False
 try:
  assert wait(lambda:sock.exists() and expr('luaeval("#vim.lsp.get_clients({bufnr=0})")')=='1'),'initialization'
  records={}
  for prefix in ['use ','use io/','use "','use "app/','use "app/systems/']:
   os.write(master,b'\x1bggdGi'+prefix.encode())
   assert wait(lambda:expr('pumvisible()')=='1',5),(prefix,expr('mode()'),expr('getline(1)'))
   menu=json.loads(expr("json_encode(complete_info(['items']))"));records[prefix]=menu
   os.write(master,b'\x19\x1b');assert wait(lambda:expr('mode()')=='n',2)
   records[prefix]['accepted']=expr('getline(1)')
   expected={'use ':'use io/','use io/':'use io/files','use \"':'use \"app/','use \"app/':'use \"app/systems/','use \"app/systems/':'use \"app/systems/player.void\"'}
   assert records[prefix]['accepted']==expected[prefix],records[prefix]
  out.write_text(json.dumps(records,indent=2)+'\n');print('Real Neovim terminal: automatic menus and acceptance passed for all five prefixes')
 finally:
  try:expr('execute("qa!")')
  except Exception:pass
  try:p.wait(5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  pathlib.Path(str(out)+".terminal.log").write_bytes(terminal)
  os.close(master)
