"""Run the independent host observer on a real PTY, retaining raw returns/modes."""
from pathlib import Path
import json,os,platform,pty,subprocess,sys,termios
base=Path(__file__).resolve().parent
binary=Path(sys.argv[1]).resolve();master,slave=pty.openpty()
try:
 before=termios.tcgetattr(slave)
 report=base/('host-'+platform.system().lower()+'.json')
 run=subprocess.run([binary,report],stdin=slave,capture_output=True,timeout=10)
 print(json.dumps(dict(platform=platform.platform(),returncode=run.returncode,stdout_hex=run.stdout.hex(),stderr_hex=run.stderr.hex(),report=str(report))))
 assert run.returncode==0 and run.stdout==b'HOST' and not run.stderr
 assert json.loads(report.read_text())['native_returns']==[4,0,0,0,0,0]
finally:
 termios.tcsetattr(slave,termios.TCSANOW,before)
 os.close(master);os.close(slave)
