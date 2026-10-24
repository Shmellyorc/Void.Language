"""Actual borrowed native transfers. No source console or decoder is implied."""
from pathlib import Path
import errno, json, os, pty, select, signal, subprocess, sys, tempfile, termios, tty
binary=Path(sys.argv[1]).resolve()
checks=0
cases=[]
NONE,INVALID,ENCODING,UNSUPPORTED,WOULD_BLOCK,TIMED_OUT,CLOSED,BROKEN_PIPE,INTERRUPTED,IO_FAILURE=range(10)
def check(value, message):
    global checks
    assert value,message
    checks+=1
def progress_record(path):
    snapshot=path.with_name(path.name+'.progress.json')
    return json.loads(snapshot.read_text()) if snapshot.exists() else None
def descriptor_record(path):
    snapshot=path.with_name(path.name+'.flags.json')
    return json.loads(snapshot.read_text()) if snapshot.exists() else None
def report(path):
    values=list(map(int,path.read_text().split()))
    check(len(values)==6,('bad report',values))
    cases.append(dict(case=path.stem,completed=values[0],eof=bool(values[1]),kind=values[2],os_code=values[3],cleanup_code=values[4],child_checks_before_report=values[5],descriptor_flags=descriptor_record(path),native_progress=progress_record(path)))
    return values
with tempfile.TemporaryDirectory(prefix='VOID transfer 🔧 ') as directory:
    root=Path(directory)
    path=root/'host-write.txt'
    host=subprocess.run([binary,'--host-flags',path],capture_output=True,timeout=10)
    check(host.returncode==0 and host.stdout==b'HOST' and not host.stderr,('plain host write',host.returncode,host.stderr,descriptor_record(path)))
    check(report(path)[:4]==[4,0,NONE,0],'plain host write result')
    payload=bytes(range(256))*4096+b'\0UTF8: \xf0\x9f\x94\xa7\r\n\xff'
    for label,source in [('pipe',None),('file',root/'input.bin')]:
        path=root/(label+'.txt')
        if source:
            source.write_bytes(payload)
            with source.open('rb') as input_file:
                run=subprocess.run([binary,'--copy',path],stdin=input_file,capture_output=True,timeout=20)
        else:run=subprocess.run([binary,'--copy',path],input=payload,capture_output=True,timeout=20)
        check(run.returncode==0,(label,run.stderr,descriptor_record(path)))
        check(run.stdout==payload and run.stderr==b'E\0\xff',(label,'binary transfer changed'))
        result=report(path);check(result[:4]==[len(payload),1,NONE,0],(label,result))
    # Real redirected file output, including host-established shared file position.
    path=root/'file-output.txt'
    with (root/'output.bin').open('wb') as output:
        run=subprocess.run([binary,'--copy',path],input=payload,stdout=output,stderr=subprocess.PIPE,timeout=20)
    check(run.returncode==0 and (root/'output.bin').read_bytes()==payload,('file-output',run.stderr))
    report(path)
    path=root/'empty.txt'
    run=subprocess.run([binary,'--copy',path],input=b'',capture_output=True,timeout=10)
    check(run.returncode==0 and not run.stdout and run.stderr==b'E\0\xff','empty input')
    check(report(path)[:3]==[0,1,NONE],'empty input EOF')
    path=root/'records.txt'
    with (root/'records.bin').open('wb') as output:
        run=subprocess.run([binary,'--records',path],stdin=subprocess.DEVNULL,stdout=output,stderr=subprocess.STDOUT,timeout=20)
    data=(root/'records.bin').read_bytes();check(run.returncode==0 and len(data)==128*8193,'native concurrent record count')
    seen=set()
    for offset in range(0,len(data),8193):
        chunk=data[offset:offset+8193];key=(chunk[-1],chunk[0])
        check(chunk[1:]==bytes([chunk[-1]])*8192 and chunk[-1] in b'AB' and key not in seen,'native stdout/stderr record interleaving')
        seen.add(key)
    report(path)
    path=root/'readers.txt';data=bytes(range(200))
    run=subprocess.run([binary,'--readers',path],input=data,capture_output=True,timeout=10)
    check(run.returncode==0 and not run.stderr and sorted(run.stdout)==list(data),'native concurrent input ownership')
    check(report(path)[0]==200,'native reader progress')
    # Short native input while the pipe remains open. Each echo is the barrier
    # before the next write, so reads cannot combine separate pieces.
    path=root/'short.txt';pieces=[b'a',b'\0b',b'\xff\xf0\x9f',b'\x94\xa7\r\n']
    p=subprocess.Popen([binary,'--copy',path,str(sum(map(len,pieces)))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    for piece in pieces:
        p.stdin.write(piece);p.stdin.flush()
        check(p.stdout.read(len(piece))==piece,'short read/write changed bytes')
    p.stdin.close();p.stdin=None;out,err=p.communicate(timeout=10)
    check(p.returncode==0 and not out and err==b'E\0\xff',('short',err))
    check(report(path)[0]==sum(map(len,pieces)),'short read count')
    # PTY modes are configured by the host test. Runtime must leave them intact.
    for label,terminal_input in [('pty-input',True),('pty-output',False)]:
        master,slave=pty.openpty();tty.setraw(slave);before=termios.tcgetattr(slave)
        small=b'PTY\0\xff\xf0\x9f\x94\xa7\r\n';path=root/(label+'.txt')
        try:
            if terminal_input:
                p=subprocess.Popen([binary,'--copy',path,str(len(small))],stdin=slave,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                os.write(master,small);out,err=p.communicate(timeout=10)
            else:
                p=subprocess.Popen([binary,'--copy',path],stdin=subprocess.PIPE,stdout=slave,stderr=subprocess.PIPE)
                _,err=p.communicate(input=small,timeout=10)
                check(bool(select.select([master],[],[],10)[0]),'PTY output absent')
                out=os.read(master,len(small))
            check(p.returncode==0 and out==small and err==b'E\0\xff',(label,out,err))
            check(termios.tcgetattr(slave)==before,'Runtime changed host PTY modes')
            check(report(path)[0]==len(small),(label,'count'))
        finally:os.close(master);os.close(slave)
    # Retain the input writer/output reader until the child has tested its wait.
    for mode,kind in [('--poll',WOULD_BLOCK),('--timeout',TIMED_OUT),('--cancel-read',CLOSED),('--cancel-write',CLOSED),('--closed-input-while-output',CLOSED)]:
        path=root/(mode[2:]+'.txt')
        p=subprocess.Popen([binary,mode,path],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=(lambda:os.close(0)) if mode=='--closed-input-while-output' else None)
        p.wait(timeout=15)
        p.stdin.close();p.stdin=None;out,err=p.communicate(timeout=5)
        check(p.returncode==0,(mode,err,progress_record(path)))
        values=report(path);check(values[2]==kind and values[1]==0,(mode,values))
        if mode in ('--cancel-write','--closed-input-while-output'):check(out==b'W'*values[0] and 0<len(out)<1024*1024,('cancel prefix',values,len(out)))
        else:check(not out and values[0]==0,(mode,'unexpected progress'))
    # Existing host O_NONBLOCK state stays untouched.
    read_fd,write_fd=os.pipe2(os.O_NONBLOCK) if hasattr(os,'pipe2') else os.pipe()
    if not hasattr(os,'pipe2'):os.set_blocking(read_fd,False)
    try:
        path=root/'nonblocking.txt'
        run=subprocess.run([binary,'--poll',path],stdin=read_fd,capture_output=True,timeout=10)
        check(run.returncode==0,('nonblocking',run.stderr));check(report(path)[2]==WOULD_BLOCK,'nonblocking error')
    finally:os.close(read_fd);os.close(write_fd)
    path=root/'interrupt.txt'
    p=subprocess.Popen([binary,'--interrupt',path],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    check(p.stdout.read(1)==b'!','interrupt barrier')
    out,err=p.communicate(input=b'X',timeout=15)
    check(p.returncode==0 and not out,('interrupt',err));check(report(path)[2]==NONE,'interrupted read failed')
    for mode in ['default','handler','ignore','pending']:
        read_fd,write_fd=os.pipe();os.close(read_fd);path=root/('broken-'+mode+'.txt')
        try:
            run=subprocess.run([binary,'--broken',path,mode],stdin=subprocess.DEVNULL,stdout=write_fd,stderr=subprocess.PIPE,timeout=10,restore_signals=True)
            check(run.returncode==0,(mode,run.returncode,run.stderr))
            result=report(path);check(result[:4]==[0,0,BROKEN_PIPE,errno.EPIPE],(mode,result))
        finally:os.close(write_fd)
    path=root/'closed-input.txt'
    run=subprocess.run([binary,'--read-error',path],stdin=subprocess.DEVNULL,capture_output=True,preexec_fn=lambda:os.close(0),timeout=10)
    check(run.returncode==0,run.stderr);check(report(path)[2:4]==[CLOSED,errno.EBADF],'closed input error')
    path=root/'closed-output.txt'
    run=subprocess.run([binary,'--write-error',path],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,preexec_fn=lambda:os.close(1),timeout=10)
    check(run.returncode==0,run.stderr);check(report(path)[2:4]==[CLOSED,errno.EBADF],'closed output error')
    if Path('/dev/full').exists():
        path=root/'full.txt'
        with open('/dev/full','wb',buffering=0) as full:
            run=subprocess.run([binary,'--write-error',path],stdout=full,stderr=subprocess.PIPE,timeout=10)
        check(run.returncode==0,run.stderr);check(report(path)[2:4]==[IO_FAILURE,errno.ENOSPC],'full destination error')
print(f'{checks} real POSIX binary/pipe/file/PTY/poll/deadline/cancel/GC/EINTR/SIGPIPE checks passed')
print(json.dumps(dict(native_cases=cases),sort_keys=True))
