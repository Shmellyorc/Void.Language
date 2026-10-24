"""Real POSIX key bytes and configured modes, retaining raw termios snapshots."""
import json,os,pathlib,pty,select,subprocess,sys,termios,time
from console_posix import mode_record, same_termios
binary=str(pathlib.Path(sys.argv[1]).resolve());records=[];checks=0
# enum order is the public/native shared contract; byte assertions independently
# verify characters/unknown sequences, never merely a successful return code.
cases=[('é'.encode(),0),('🎮'.encode(),0),(b'\r',1),(b'\t',2),(b'\x7f',3),
       (b'\x1b[A',5),(b'\x1b[B',6),(b'\x1b[D',7),(b'\x1b[C',8),(b'\x1b[H',9),(b'\x1b[F',10),
       (b'\x1b[2~',11),(b'\x1b[3~',12),(b'\x1b[5~',13),(b'\x1b[6~',14)]
for n,seq in enumerate([b'\x1bOP',b'\x1bOQ',b'\x1bOR',b'\x1bOS',*[('\x1b[%d~'%n).encode() for n in [15,17,18,19,20,21,23,24]]]):cases.append((seq,15+n))
cases += [(b'\x1b[999~',27),(b'\x03',0),(b'\x1b',4)]
for unclosed in [False,True]:
 m,s=pty.openpty();original=termios.tcgetattr(s)
 p=subprocess.Popen([binary,str(len(cases)),'unclosed' if unclosed else 'closed'],stdin=s,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=dict(os.environ,TERM='xterm-256color'))
 data=b'';sent=False;start=time.monotonic()
 try:
  while p.poll() is None:
   assert time.monotonic()-start<30,(data,p.poll())
   if select.select([p.stdout],[],[],.02)[0]:data+=os.read(p.stdout.fileno(),65536)
   if b'READY\n' in data and not sent:
    active=termios.tcgetattr(s);assert not active[3]&(termios.ECHO|termios.ICANON|termios.ISIG),active
    os.write(m,b''.join(payload for payload,_ in cases));sent=True;checks+=1
  tail,errors=p.communicate(timeout=3);data+=tail;assert p.returncode==0 and not errors,(p.returncode,data,errors)
  assert same_termios(original,termios.tcgetattr(s)),('mode differs',original,termios.tcgetattr(s));checks+=2
  rows=data.splitlines();assert rows[0]==b'READY' and len(rows)==len(cases)+1,rows
  for row,(payload,key) in zip(rows[1:],cases):
   received = b'\n' if payload == b'\r' else payload
   expected=f'{key} 0 0 1 {received.hex()}'.encode();assert row==expected,(row,expected);checks+=1
  records.append(dict(unclosed=unclosed,stdout_hex=data.hex(),stderr_hex=errors.hex(),restored_exactly=termios.tcgetattr(s)==original,restored_configuration=True,original_mode=mode_record(original),restored_mode=mode_record(termios.tcgetattr(s)),seconds=time.monotonic()-start))
 finally:
  if p.poll() is None:p.kill()
  p.communicate();os.close(m);os.close(s)
for echo_newline in [False,True]:
 for signals in [False,True]:
  m,s=pty.openpty();mode=termios.tcgetattr(s)
  mode[3] = (mode[3]|termios.ECHONL) if echo_newline else (mode[3]&~termios.ECHONL)
  mode[3] = (mode[3]|termios.ISIG) if signals else (mode[3]&~termios.ISIG)
  termios.tcsetattr(s,termios.TCSANOW,mode);original=termios.tcgetattr(s)
  p=subprocess.Popen([binary,'default'],stdin=s,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=dict(os.environ,TERM='dumb'))
  try:
   assert p.stdout.readline()==b'READY\n'
   start=time.monotonic()
   while termios.tcgetattr(s)[3]&termios.ICANON:
    assert time.monotonic()-start<2
    select.select([],[],[],.001)
   active=termios.tcgetattr(s)
   assert not active[3]&(termios.ECHO|termios.ECHONL) and bool(active[3]&termios.ISIG)==signals
   os.write(m,b'\n')
   out,errors=p.communicate(timeout=5)
   assert p.returncode==0 and out==b'1\n' and not errors,(out,errors)
   assert not select.select([m],[],[],0)[0],"unexpected native echo"
   assert same_termios(original,termios.tcgetattr(s))
   checks+=5;records.append(dict(default=True,echo_newline=echo_newline,signals=signals,restored_exactly=termios.tcgetattr(s)==original,restored_configuration=True,original_mode=mode_record(original),active_mode=mode_record(active),restored_mode=mode_record(termios.tcgetattr(s)),stdout_hex=out.hex()))
  finally:
   if p.poll() is None:p.kill()
   p.communicate();os.close(m);os.close(s)
m,s=pty.openpty();original=termios.tcgetattr(s)
try:
 p=subprocess.run([binary,'closed-input'],stdin=s,capture_output=True,timeout=5)
 import errno
 assert p.returncode==0 and p.stdout==f'6 {errno.EBADF}\n'.encode() and not p.stderr,(p.returncode,p.stdout,p.stderr)
 assert termios.tcgetattr(s)==original
 checks+=3;records.append(dict(closed_input=True,stdout_hex=p.stdout.hex(),restored_exactly=termios.tcgetattr(s)==original,restored_configuration=True,original_mode=mode_record(original),restored_mode=mode_record(termios.tcgetattr(s))))
finally:
 os.close(m);os.close(s)
m,s=pty.openpty();mode=termios.tcgetattr(s);mode[3]&=~termios.ECHO;termios.tcsetattr(s,termios.TCSANOW,mode);original=termios.tcgetattr(s)
try:
 os.write(m,b'abc')
 p=subprocess.Popen([binary,'canonical-availability'],stdin=s,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 first=p.stdout.readline();os.write(m,mode[6][termios.VERASE]+b'\n');out,errors=p.communicate(timeout=5)
 assert p.returncode==0 and first==b'AVAILABLE 0\n' and out==b'ab\n' and not errors,(p.returncode,first,out,errors)
 assert termios.tcgetattr(s)==original
 checks+=3;records.append(dict(canonical_editor=True,stdout_hex=(first+out).hex(),original_mode=mode_record(original),restored_mode=mode_record(termios.tcgetattr(s)),restored_exactly=termios.tcgetattr(s)==original,restored_configuration=True))
finally:
 if p.poll() is None:p.kill();p.wait()
 os.close(m);os.close(s)
pathlib.Path(binary).parent.joinpath('step30-native-keyboard-results.json').write_text(json.dumps(dict(checks=checks,records=records),indent=2)+'\n')
print(checks,'native PTY key/mode checks passed')

# Step31: real process Streams shared by three Runtime lifetimes; no injected
# terminal adapter, sleeps as a race oracle, or hidden terminal input channel.
m,s=pty.openpty();mode=termios.tcgetattr(s);mode[0]&=~(termios.ICRNL|termios.INLCR|termios.IGNCR);termios.tcsetattr(s,termios.TCSANOW,mode);original=termios.tcgetattr(s)
try:
 p=subprocess.Popen([binary,'shared'],stdin=s,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=dict(os.environ,TERM='xterm-256color'))
 first=p.stdout.readline();assert first==b'READY\n',first
 active=termios.tcgetattr(s);assert not active[3]&(termios.ECHO|termios.ICANON|termios.ISIG),active
 os.write(m,'é\r\nZ\n'.encode())
 out,errors=p.communicate(timeout=15)
 assert p.returncode==0,(p.returncode,out,errors)
 rows=out.splitlines();assert rows[-1]=='DONE:é'.encode(),rows
 assert sorted(rows[:-1])==sorted(('O%d:é🎮'%n).encode() for n in range(32)),rows
 assert sorted(errors.splitlines())==sorted(('E%d:é🎮'%n).encode() for n in range(32)),errors
 assert same_termios(original,termios.tcgetattr(s)) and not select.select([m],[],[],0)[0],'native modes/echo changed'
 checks+=7;records.append(dict(shared_runtimes=3,stdout_hex=(first+out).hex(),stderr_hex=errors.hex(),original_mode=mode_record(original),active_mode=mode_record(active),restored_mode=mode_record(termios.tcgetattr(s)),restored_exactly=termios.tcgetattr(s)==original,restored_configuration=True))
finally:
 if p.poll() is None:p.kill();p.wait()
 os.close(m);os.close(s)
pathlib.Path(binary).parent.joinpath('step31-native-keyboard-results.json').write_text(json.dumps(dict(checks=checks,records=records),indent=2)+'\n')
print(checks,'native PTY key/mode integration checks passed')
