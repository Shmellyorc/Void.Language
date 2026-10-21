"""Real POSIX Vim typing/automatic-menu acceptance with the pinned LSP client."""
import os,pty,pathlib,tempfile,subprocess,time,select,json,fcntl,termios,struct,argparse
p=argparse.ArgumentParser();p.add_argument("--server",required=True);p.add_argument("--vim",default="vim");p.add_argument("--vim-client",required=True);p.add_argument("--output",required=True);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];results={};server=pathlib.Path(a.server).resolve()
def quote(s):return "'"+str(s).replace("'","''")+"'"
for prefix in ['use ','use io/','use "','use "app/','use "app/systems/']:
 with tempfile.TemporaryDirectory(prefix='VOID Vim automatic ') as d:
  root=pathlib.Path(d);(root/'standard/io').mkdir(parents=True);(root/'app/systems').mkdir(parents=True)
  (root/'main.void').write_text('func entry(x: i32) -> i32:\n    return x\n');(root/'standard/io/files.void').write_text('');(root/'app/systems/player.void').write_text('')
  plugin=pathlib.Path(a.vim_client).resolve()
  config=root/'init.vim';config.write_text('set nocompatible hidden noswapfile\nset completeopt=menuone,noinsert\n'+
   'execute "set runtimepath^=" . fnameescape('+quote(plugin)+')\nexecute "set runtimepath^=" . fnameescape('+quote(repo/'editors/vim')+')\nexecute "source " . fnameescape('+quote(plugin/'plugin/lsp.vim')+')\nfiletype plugin indent on\ncall void#setup('+quote(server)+", '', "+quote(root)+')\ncall LspEnable()\n'+
   'function! Capture(timer) abort\n if exists("*LspServerReady") && LspServerReady() && !filereadable('+quote(root/'ready')+')\n call writefile(["ready"],'+quote(root/'ready')+')\n endif\n if pumvisible() && !filereadable('+quote(root/'menu')+')\n call writefile([json_encode(complete_info(["items"]))],'+quote(root/'menu')+')\n endif\nendfunction\ncall timer_start(20,function("Capture"),{"repeat":-1})\n')
  master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,100,0,0))
  env=dict(os.environ,TERM='xterm-256color',VOID_STDLIB_ROOT=str(root/'standard'),XDG_STATE_HOME=str(root/'state'),XDG_CACHE_HOME=str(root/'cache'),XDG_DATA_HOME=str(root/'data'))
  proc=subprocess.Popen([a.vim,'-N','-u',str(config),'-i','NONE','-n',str(root/'main.void')],stdin=slave,stdout=slave,stderr=slave,env=env,start_new_session=True);os.close(slave);terminal=bytearray()
  def drain():
   while select.select([master],[],[],0)[0]:
    try:terminal.extend(os.read(master,65536))
    except OSError:return
  def wait(file,seconds=10):
   end=time.monotonic()+seconds
   while time.monotonic()<end:
    drain()
    if file.exists():return True
    time.sleep(.02)
   return False
  try:
   assert wait(root/'ready'),('ready',terminal.decode(errors='replace')[-2000:])
   os.write(master,b'ggdGi'+prefix.encode());assert wait(root/'menu',5),(prefix,terminal.decode(errors='replace')[-1200:])
   results[prefix]=json.loads((root/'menu').read_text())
   command=':call writefile([getline(1)],'+quote(root/'accepted')+')\r'
   os.write(master,b'\x0e\x19\x1b'+command.encode());assert wait(root/'accepted',3)
   results[prefix]['accepted']=(root/'accepted').read_text().strip()
   expected={'use ':'use io/','use io/':'use io/files','use "':'use "app/','use "app/':'use "app/systems/','use "app/systems/':'use "app/systems/player.void"'}
   assert results[prefix]['accepted']==expected[prefix],results[prefix]
  finally:
   os.write(master,b'\x1b:qa!\r')
   try:proc.wait(5)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
   os.close(master)
pathlib.Path(a.output).write_text(json.dumps(results,indent=2)+'\n');print('Real Vim terminal: automatic menus and acceptance passed for all five prefixes')
