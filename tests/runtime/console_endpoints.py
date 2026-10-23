"""Real inherited endpoints: no keyboard, sleeps, hidden terminals or source I/O."""
from pathlib import Path
import os, pty, subprocess, sys, tempfile
checks=0
binary=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID console endpoints ') as directory:
    root=Path(directory)
    master,slave=pty.openpty()
    try:
        for label,kinds,stdin,stdout,stderr,close_input in [
            ('independent',[3,2,1],subprocess.PIPE,'file',slave,False),
            ('same_output',[1,2,2],slave,'file',subprocess.STDOUT,False),
            ('pipes',[3,3,3],subprocess.PIPE,subprocess.PIPE,subprocess.PIPE,False),
            ('closed_input',[0,2,2],None,'file',subprocess.STDOUT,True),
        ]:
            with (root/(label+'.output')).open('wb') as file:
                report=root/(label+'.txt')
                args=dict(stdin=stdin,stdout=file if stdout=='file' else stdout,stderr=stderr,timeout=10)
                if close_input:args['preexec_fn']=lambda:os.close(0)
                command=[binary,'--probe',report]
                payload='keep 🎮\0'.encode('utf-8') if stdin==subprocess.PIPE else None
                if payload is not None:command.append('--verify-input');args.pop('stdin');args['input']=payload
                result=subprocess.run(command,**args)
                assert result.returncode==0,(label,result.returncode,result.stderr)
                if payload is not None:
                    assert Path(str(report)+'.payload').read_bytes()==payload,label
                    checks+=1
                rows=[list(map(int,line.split())) for line in report.read_text().splitlines()]
                assert len(rows)==3,rows
                for n,(identity,kind,redirected,code) in enumerate(rows):
                    assert identity==n and kind==kinds[n] and bool(redirected)==(kind!=1),(label,rows)
                    assert (code!=0)==(kind==0),(label,rows)
                    checks+=1
    finally:
        os.close(master);os.close(slave)
print(f'{checks} real POSIX endpoint identity/redirection/borrowing checks passed')
