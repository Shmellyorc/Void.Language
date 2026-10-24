"""Withhold pipe input until the actual zero-timeout boundary; no timing sleeps."""
from pathlib import Path
import fcntl,json,os,pty,struct,subprocess,sys,termios,tempfile
base=Path(__file__).resolve().parent;root=base.parents[3];name=sys.argv[1];build=root/'build'/('step42-ci98-'+name)
master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack("HHHH",24,80,0,0))
try:
    # Force the original native fixture to finish its poll before any payload write.
    with tempfile.TemporaryDirectory() as tmp:
        report=Path(tmp)/'original.txt';p=subprocess.Popen([root/'build'/('step42-ci97-'+name)/'void_runtime_console_terminal',report,'redirected-query'],stdin=subprocess.PIPE,stdout=slave,stderr=subprocess.PIPE,env=dict(os.environ,TERM='xterm-256color'))
        p.wait(timeout=10);_,errors=p.communicate(timeout=2);rows=report.read_text().splitlines()
        assert p.returncode==0 and rows[2].startswith('3 ') and rows[3]=='' and not errors,(rows,errors)
        original=dict(status=p.returncode,rows=rows,delivery='withheld until after child exit',stderr_hex=errors.hex())
    p=subprocess.Popen([build/'reproduce'],stdin=subprocess.PIPE,stdout=slave,stderr=subprocess.PIPE,env=dict(os.environ,TERM='xterm-256color'))
    assert p.stderr.readline()==b'READY\n'
    p.stdin.write(b'redirected\x00\xff');p.stdin.flush();out,err=p.communicate(timeout=10)
    assert p.returncode==0 and b'first_error=4 first_bytes=0 later=7265646972656374656400ff' in err,(p.returncode,out,err)
    cancellation=subprocess.run([build/'reproduce','admission'],capture_output=True,timeout=10)
    assert cancellation.returncode==0 and cancellation.stdout==b'admitted=1 completed=0 error=6\n' and not cancellation.stderr,cancellation
    record=dict(original_native_fixture=original,same_process_later_input=dict(status=p.returncode,stderr_hex=err.hex()),admission_before_write=dict(status=cancellation.returncode,stdout_hex=cancellation.stdout.hex()))
    (base/(name+'-reproduction-records.json')).write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
finally:os.close(master);os.close(slave)
