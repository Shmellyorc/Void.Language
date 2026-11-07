from pathlib import Path
import sys,tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();count=0
with tempfile.TemporaryDirectory(prefix='VOID host frontend ') as directory:
 work=Path(directory);source=work/'main.void'
 def check(text,valid=True):
  global count
  source.write_text(text);h.run([compiler,'check',source],work,status=0 if valid else 1,sources=(source,),small=True);count+=1
 for call in ['os.args()','crypto.sha256()','crypto.verify_ed25519([],[],[])','io.create_private_directory("/tmp/new")','io.open_private_directory("/tmp/owned")','process.start("/bin/true",[],[],"/tmp")']:
  check('use os\nuse crypto\nuse io\nuse process\nfunc main():\n    var answer = '+call+'\n')
 for call in ['os.args(0)','crypto.sha256(0)','crypto.verify_ed25519("a",[],[])','process.start("relative",[],[],"/tmp")','io.create_private_directory(3)']:
  check('use os\nuse crypto\nuse io\nuse process\nfunc main():\n    var answer = '+call+'\n',valid='relative' in call)
 for text in ['func main():\n    var answer = _host_args_count()\n','use crypto\nfunc main():\n    var answer = crypto._host_hash_open(0)\n','use crypto\nfunc main():\n    var answer = crypto.sha256_state()\n','use io\nfunc main():\n    var answer = io.private_directory()\n','use process\nfunc main():\n    var answer = process.child()\n','use archive\nfunc main():\n    var answer = archive.zip_reader()\n','use crypto\nvar state = crypto.sha256()\nfunc main():\n    var payload = state._local\n']:
  check(text,False)
 (work/'crypto.void').write_text('func forge():\n    var answer = _host_hash_open(0)\n')
 check('use "./crypto.void"\nfunc main():\n    forge()\n',False)
 # Compiler-owned worker checking follows ordinary transitive call dependencies.
 for call in ['os.args()','crypto.sha256()','io.create_private_directory("/tmp/new")','process.start("/bin/true",[],[],"/tmp")']:
  text='use os\nuse crypto\nuse io\nuse process\nfunc work(n: i32) -> i32:\n    var answer = '+call+'\n    return n\nasync func main():\n    var task = spawn work(1)\n    var output = await task\n'
  check(text,False)
print(f'{count} frontend/authority checks; real 1 MiB stack')
