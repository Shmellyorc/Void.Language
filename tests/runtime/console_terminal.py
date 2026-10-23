"""Actual POSIX PTY facilities; this is a host responder, never a VOID input path."""
import errno, fcntl, json, os, pathlib, pty, select, struct, subprocess, sys, tempfile, termios, time
binary=str(pathlib.Path(sys.argv[1]).resolve());checks=0;records=[]
with tempfile.TemporaryDirectory(prefix='VOID terminal29 ') as temporary:
    database=pathlib.Path(temporary)/'db';database.mkdir();description=pathlib.Path(temporary)/'custom.src'
    description.write_text(r"void-step29|VOID deterministic capability fixture, cols#80, lines#24, colors#16, pairs#256, cup=C%p1%d\,%p2%d;, civis=H;, cnorm=S;, setaf=F%p1%d;, setab=G%p1%d;, op=R;, clear=K;, bel=B;, tsl=\E]2;, fsl=^G,"+'\n')
    subprocess.run(['tic','-o',str(database),str(description)],check=True,capture_output=True)
    for name,terminal,mode,input_tty,error_tty in [('custom','void-step29','presentation',True,False),('presentation','xterm-256color','presentation',True,False),('dumb','dumb','presentation',True,False),('unknown','VOID-absent-terminal','presentation',True,False),('query','xterm-256color','query',True,False),('timeout','xterm-256color','timeout',True,False),('redirected-input','xterm-256color','redirected-query',False,False),('cooked','xterm-256color','redirected-query',True,False),('both-output','xterm-256color','presentation',True,True)]:
        master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0));before=termios.tcgetattr(slave)
        active=termios.tcgetattr(slave)
        if name!='cooked':active[3]&=~(termios.ICANON|termios.ECHO);active[6][termios.VMIN]=1;active[6][termios.VTIME]=0
        termios.tcsetattr(slave,termios.TCSANOW,active);report=pathlib.Path(temporary)/(name+'.txt');start=time.monotonic();child=subprocess.Popen([binary,str(report),mode],stdin=slave if input_tty else subprocess.PIPE,stdout=slave,stderr=slave if error_tty else subprocess.PIPE,env=dict(os.environ,TERM=terminal,**({"TERMINFO":str(database)} if name=="custom" else {})));data=b'';answered=False
        if not input_tty:child.stdin.write(b'redirected\x00\xff');child.stdin.close();child.stdin=None
        try:
            while child.poll() is None:
                assert time.monotonic()-start<10,(name,data)
                if select.select([master],[],[],0.05)[0]:data+=os.read(master,65536)
                if name in ('query','timeout') and b'\x1b[6n' in data and not answered:os.write(master,b'user\xc3\xa9\x1b[12;34Rtail' if name=='query' else b'pre\x1b[3;');answered=True
            while select.select([master],[],[],0)[0]:
                try:data+=os.read(master,65536)
                except OSError as e:
                    if e.errno==errno.EIO:break
                    raise
            _,errors=child.communicate(timeout=5);assert child.returncode==0 and not errors,(name,data,errors)
            assert termios.tcgetattr(slave)==active,(name,'terminal mode changed')
            rows=report.read_text().splitlines();caps=list(map(int,rows[0].split()));assert caps[0]&1 and caps[1]&2 if input_tty else caps[1]&2
            assert bool(caps[2]&16)==error_tty,(name,caps)
            assert rows[1]=='0 80 24',(name,rows)
            if name in ('dumb','unknown'):
                assert not (caps[1]&~(2|16)) and data==b'',(name,caps,data)
                assert rows[2]=='3 3 3 3 3 3 3 '
            elif name=='custom':
                assert rows[2]=='0 0 0 0 0 0 0 ' and rows[3]=='1',(name,rows)
                assert data==b'C0,0;H;F9;G4;R;K;C0,0;\x1b]2;'+ 'VOID é🎮'.encode()+b'\x07B;',(name,data)
                assert not caps[1]&32,(name,caps)
            elif mode=='presentation':
                assert rows[2]=='0 0 0 0 0 0 0 ' and rows[3]=='1',(name,rows)
                assert b'\x1b[1;1H' in data and b'\x1b[?25l' in data and b'\x1b[91m\x1b[44m' in data and b'\x1b[39;49m' in data and b'\x1b[2J' in data and 'VOID é🎮'.encode() in data and data.endswith(b'\a'),(name,data)
            elif name=='query':assert rows[2]=='0 33 11' and bytes.fromhex(rows[3])=='userétail'.encode(),(name,rows,data)
            elif name=='timeout':assert bytes.fromhex(rows[3])==b'pre\x1b[3;' and rows[2].startswith('5 ') and 0.15<time.monotonic()-start<5 and data==b'\x1b[6n',(name,rows,data)
            else:
                assert rows[2].startswith('3 ') and not data,(name,rows,data)
                if not input_tty:assert bytes.fromhex(rows[3])==b'redirected\x00\xff',(name,rows)
            checks+=8;records.append(dict(case=name,report=rows,terminal_hex=data.hex(),elapsed_seconds=time.monotonic()-start))
        finally:
            if child.poll() is None:child.kill()
            child.communicate();os.close(master);os.close(slave)
    # Real redirected streams: every control is unsupported and both byte streams empty.
    report=pathlib.Path(temporary)/'redirected.txt';r=subprocess.run([binary,str(report),'presentation'],stdin=subprocess.DEVNULL,capture_output=True,timeout=10,env=dict(os.environ,TERM='xterm-256color'));assert r.returncode==0 and not r.stdout and not r.stderr
    rows=report.read_text().splitlines();assert rows[0]=='1 2 2 ' and rows[1].startswith('3 ') and rows[2]=='3 3 3 3 3 3 3 ' and rows[3]=='1',rows
    checks+=5;records.append(dict(case='redirected',report=rows,terminal_hex=''))
pathlib.Path(binary).parent.joinpath('step29-native-terminal-results.json').write_text(json.dumps(dict(checks=checks,records=records),indent=2)+'\n')
print(checks,'real POSIX terminal checks;',len(records),'process cases')
