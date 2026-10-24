"""Focused original source-key fixture, emitted by accepted compiler, new Runtime.
This checks linked AOT at normal/O2, not a freshly relinked compiler/JIT.
"""
from pathlib import Path
import hashlib,json,os,pty,select,shutil,subprocess,sys,tempfile,termios,time
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tests/runtime'))
from console_posix import mode_record,same_termios
name=sys.argv[1];build=root/'build'/('step42-ci97-'+name);old=root/'build'/('step42-ci96-'+name)
sdk=root/'.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64'
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86',TERM='xterm-256color')
payload="é🎮\r\t\x7f\x1b[A\x1b[24~\x1b[999~\x03\x1b".encode()
expected=b"caps true true\navailable false\nREADY\n"+"écharacter é 2 false false 1\n🎮character 🎮 4 false false 1\n".encode()+b"enter  1 false false 1\ntab  1 false false 1\nbackspace  1 false false 1\nup  3 false false 1\nf12  5 false false 1\nunknown  6 false false 1\n\x03character \x03 1 false false 1\nescape  1 false false 1\nclosed closed\n"
records=[]
with tempfile.TemporaryDirectory(prefix='void-ci97-source-') as temp:
 path=Path(temp);shutil.copytree(root/'stdlib',path/'stdlib');shutil.copy2(root/'tests/stage10/console_keys.void',path/'console_keys.void')
 for optimized in (False,True):
  obj=path/('keys'+str(optimized)+'.o');exe=obj.with_suffix('.bin')
  commands=[[old/'voidc','emit',path/'console_keys.void','-o',obj,*([] if optimized else ['--no-opt'])],
            [sdk/'bin/clang','--driver-mode=g++','-pthread','-ldl','--ld-path='+str(sdk/'bin/ld.lld'),*(['-fsanitize=address,undefined'] if name=='san' else []),obj,build/'libvoid_runtime.a',build/'libvoid_native_fixture.a','-o',exe]]
  for command in commands:
   result=subprocess.run(list(map(str,command)),env=env,capture_output=True,timeout=120)
   assert result.returncode==0,(command,result.returncode,result.stdout,result.stderr)
  master,slave=pty.openpty();before=termios.tcgetattr(slave);p=subprocess.Popen([exe],stdin=slave,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env)
  data=b'';sent=False;start=time.monotonic()
  try:
   while p.poll() is None:
    assert time.monotonic()-start<15
    if select.select([p.stdout],[],[],.02)[0]:data+=os.read(p.stdout.fileno(),65536)
    if b'READY\n' in data and not sent:os.write(master,payload);sent=True
   out,errors=p.communicate(timeout=3);data+=out;after=termios.tcgetattr(slave)
   records.append(dict(optimized=optimized,commands=[list(map(str,c)) for c in commands],returncode=p.returncode,stdout_hex=data.hex(),stderr_hex=errors.hex(),original_mode=mode_record(before),restored_mode=mode_record(after),restored_configuration=same_termios(before,after),seconds=time.monotonic()-start))
   (base/(name+'-source-pty-records.json')).write_text(json.dumps(records,indent=2)+'\n')
   assert p.returncode==0 and data==expected and not errors,(p.returncode,data,errors)
   assert same_termios(before,after)
  finally:
   if p.poll() is None:p.kill()
   p.communicate();os.close(master);os.close(slave)
print(name,'normal/O2 independently linked source PTY: exact original key bytes/output and restoration pass')
