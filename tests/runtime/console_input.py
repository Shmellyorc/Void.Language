"""Real Step 25 native queue/UTF-8/line probes; source console APIs are not implied."""
from pathlib import Path
import json, os, pty, select, subprocess, sys, tempfile, termios, tty
binary=Path(sys.argv[1]).resolve()
checks=0
cases=[]
def check(value, message):
    global checks
    assert value,message
    checks+=1
def frames(data):
    result=[]
    while data:
        check(len(data)>=9,('truncated frame',data))
        size=int.from_bytes(data[1:9],'little');check(len(data)>=9+size,'truncated frame payload')
        result.append((chr(data[0]),data[9:9+size]));data=data[9+size:]
    return result
def exact(pipe,count):
    result=b''
    while len(result)<count:
        check(bool(select.select([pipe],[],[],10)[0]),'native frame wait timed out')
        value=os.read(pipe.fileno(),count-len(result));check(bool(value),'native process ended before frame')
        result+=value
    return result
def one(pipe):
    header=exact(pipe,9);return chr(header[0]),exact(pipe,int.from_bytes(header[1:],'little'))
def record(path):
    count=int(path.read_text());cases.append(dict(case=path.stem,child_checks_before_report=count))
with tempfile.TemporaryDirectory(prefix='VOID queued input 🔧 ') as directory:
    root=Path(directory)
    source='\ufeffA\0é🎮\r\nB\n\rCR\r\r\n\nfinal'.encode()
    expected=[('T','\ufeffA\0é🎮'.encode()),('T',b'B'),('T',b''),('T',b'CR'),('T',b''),('T',b''),('T',b'final'),('F',b'')]
    for kind in ['pipe','file']:
        path=root/(kind+'.txt')
        if kind=='file':
            (root/'input.bin').write_bytes(source)
            with (root/'input.bin').open('rb') as stream:run=subprocess.run([binary,'--lines',path],stdin=stream,capture_output=True,timeout=15)
        else:run=subprocess.run([binary,'--lines',path],input=source,capture_output=True,timeout=15)
        check(run.returncode==0,(kind,run.stderr));check(frames(run.stdout)==expected,(kind,frames(run.stdout)));record(path)
    # 4096-byte native buffers must split this 7-byte Unicode/NUL cycle at
    # every phase. This also proves lines have no arbitrary one-buffer ceiling.
    path=root/'large-line.txt';large=('é🎮\0'*5000).encode()
    run=subprocess.run([binary,'--lines',path],input=large+b'\r\nTAIL\n',capture_output=True,timeout=15)
    check(run.returncode==0,run.stderr);check(frames(run.stdout)==[('T',large),('T',b'TAIL'),('F',b'')],'large line/native Unicode boundaries');record(path)
    path=root/'invalid-boundary.txt';prefix=b'r'*4095+b'\xe2\x82'
    run=subprocess.run([binary,'--recover',path],input=prefix+b'Z\n',capture_output=True,timeout=15)
    check(run.returncode==0,run.stderr);check(frames(run.stdout)==[('E',prefix),('T',b'Z'),('F',b'')],'native invalid-prefix boundary');record(path)
    # Every raw/text/character/line transition after CR, with both LF and non-LF.
    # The writer stays open while waiting for the first record: no CR lookahead wait.
    for method in ['line','char','text','bytes']:
        for continuation in [b'\nB\n',b'B\n']:
            path=root/(method+('-lf' if continuation.startswith(b'\n') else '-other')+'.txt')
            p=subprocess.Popen([binary,'--live',path,method],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            p.stdin.write(b'head\r');p.stdin.flush()
            check(one(p.stdout)==('T',b'head'),('standalone CR',method))
            p.stdin.write(continuation);p.stdin.flush()
            if method=='line':check(one(p.stdout)==('T',b'B'),('logical CRLF',method))
            else:
                first=continuation[:1];check(one(p.stdout)==('B' if method=='bytes' else 'T',first),('raw LF ownership',method))
                check(one(p.stdout)==('T',b'B' if first==b'\n' else b''),('after mixed read',method))
            p.stdin.close();p.stdin=None;out,err=p.communicate(timeout=10)
            check(p.returncode==0 and not out,('live',method,err));record(path)
    # A timeout starts only once the prefix has already entered the shared queue.
    for kind,prefix,method,retry,answer in [('line',b'part','line',b'next\n',('T',b'next')),('char',b'\xe2','bytes',b'\x82\xac',('B',b'\x82\xac'))]:
        path=root/('timeout-'+kind+'.txt')
        p=subprocess.Popen([binary,'--timeout-'+kind,path,method],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        p.stdin.write(b'@'+prefix);p.stdin.flush()
        check(one(p.stdout)==('R',b''),'timeout prefix barrier')
        check(one(p.stdout)==('E',prefix),('timeout partial',kind))
        p.stdin.write(retry);p.stdin.flush();check(one(p.stdout)==answer,('no undisclosed half line/scalar',kind))
        p.stdin.close();p.stdin=None;out,err=p.communicate(timeout=10)
        check(p.returncode==0 and not out,('timeout',kind,err));record(path)
    path=root/'malformed.txt';bad=b'ok\xed\xa0\x80Z\nbad\xe2\x82'
    run=subprocess.run([binary,'--recover',path],input=bad,capture_output=True,timeout=10)
    check(run.returncode==0,run.stderr);check(frames(run.stdout)==[('E',b'ok\xed\xa0\x80'),('T',b'Z'),('E',b'bad\xe2\x82'),('F',b'')],'malformed input/recovery/EOF');record(path)
    # Kernel reads may coalesce these one-byte writes; deterministic unit cases
    # separately prove every native transfer split for scalar and line decoding.
    path=root/'bytewise.txt';p=subprocess.Popen([binary,'--lines',path,'1'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    for byte in 'é🎮\0\r'.encode():os.write(p.stdin.fileno(),bytes([byte]))
    check(one(p.stdout)==('T','é🎮\0'.encode()),'bytewise pipe input')
    p.stdin.close();p.stdin=None;out,err=p.communicate(timeout=10)
    check(p.returncode==0 and not out,err);record(path)
    for kind in ['input','output']:
        master,slave=pty.openpty();tty.setraw(slave);before=termios.tcgetattr(slave);path=root/('pty-'+kind+'.txt')
        small='Å\0🔧\r\nB\n'.encode();want=[('T','Å\0🔧'.encode()),('T',b'B')]
        try:
            if kind=='input':
                p=subprocess.Popen([binary,'--lines',path,'2'],stdin=slave,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                os.write(master,small);out,err=p.communicate(timeout=10)
            else:
                p=subprocess.Popen([binary,'--lines',path,'2'],stdin=subprocess.PIPE,stdout=slave,stderr=subprocess.PIPE)
                _,err=p.communicate(input=small,timeout=10);check(bool(select.select([master],[],[],10)[0]),'PTY frames absent');out=os.read(master,4096)
            check(p.returncode==0 and frames(out)==want,(kind,out,err));check(termios.tcgetattr(slave)==before,'Runtime changed PTY input modes');record(path)
        finally:os.close(master);os.close(slave)
    path=root/'blocked.txt';p=subprocess.Popen([binary,'--blocked',path],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    p.stdin.write(b'head\r\nTAIL');p.stdin.flush();check(one(p.stdout)==('T',b'head'),'buffered GC first line')
    check(one(p.stdout)==('E',b'\nTAIL'),'cancelled line lost buffered prefix/continuation')
    p.wait(timeout=10);p.stdin.close();p.stdin=None;out,err=p.communicate(timeout=5)
    check(p.returncode==0 and not out,('GC/shutdown/shared queue',err));record(path)
print(f'{checks} real POSIX shared input/UTF8/line/CR/mixed/timeout/GC checks passed')
print(json.dumps(dict(native_cases=cases),sort_keys=True))
