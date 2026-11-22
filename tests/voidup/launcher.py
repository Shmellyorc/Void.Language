"""A redirected launcher cancellation must stop its manager and compiler child."""
import argparse
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk', type=Path, required=True)
p.add_argument('--source', type=Path, default=ROOT/'apps/voidup/launcher.cpp')
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
assert sys.platform == 'linux', 'Native Linux regression; other platforms need native qualification'
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
def alive(pid):
    try:
        return Path(f'/proc/{pid}/stat').read_text().split(') ', 1)[1][0] != 'Z'
    except FileNotFoundError:
        return False
with tempfile.TemporaryDirectory(prefix='voidup-cancel-') as directory:
    work = Path(directory)
    harness = work/'probe.cpp'
    harness.write_text('#define VOIDUP_BOOTSTRAP_ID "test"\n#define main bootstrap_main\n#include '+
                       json.dumps(str(a.source.resolve()))+'\n#undef main\n'+
                       'int main(int argc,char** argv){if(fs::path(argv[0]).filename()=="voidup")return run_main(argc,argv);return run(argv[1],{argv[2],argv[3]});}\n')
    binary = work/'probe'
    subprocess.run([str(a.sdk/'bin/clang++'), '--no-default-config', '-std=c++17',
                    '--ld-path='+str(a.sdk/'bin/ld.lld'),
                    *(['-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if a.sanitize else []),
                    str(harness), '-o', str(binary)], check=True)
    root = work/'installed'
    (root/'bin').mkdir(parents=True)
    proxy = root/'bin/voidup'
    shutil.copyfile(binary, proxy); proxy.chmod(0o755)
    retained = root/'bootstraps/test/bin/voidup'
    retained.parent.mkdir(parents=True)
    retained.write_text('#!'+sys.executable+'\nimport json,sys\nprint(json.dumps(sys.argv[1:]))\n')
    retained.chmod(0o755)
    for operands, expected in [(['list'], ['--root', str(root), 'list']),
                               (['--root', str(root), 'list'], ['--root', str(root), 'list']),
                               (['run', 'default', 'voidc', '--', '--root', 'literal'], ['--root', str(root), 'run', 'default', 'voidc', '--', '--root', 'literal'])]:
        result = subprocess.run([str(proxy), *operands], capture_output=True, text=True, check=True, preexec_fn=limits)
        assert json.loads(result.stdout) == expected, result.stdout
    script = work/'manager.py'
    script.write_text('''import os,subprocess,sys
if os.isatty(0): assert os.tcgetpgrp(0)==os.getpgrp(), 'Lost interactive foreground group'
child=subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)'])
with open(sys.argv[1], 'w') as output: output.write(str(os.getpid())+' '+str(child.pid))
child.wait()
''')
    for sig in (signal.SIGTERM, signal.SIGINT):
        ready = work/('ready-'+str(sig))
        parent = subprocess.Popen([str(binary), sys.executable, str(script), str(ready)],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  start_new_session=True, preexec_fn=limits)
        children = []
        try:
            deadline = time.monotonic()+10
            while not ready.exists() or not ready.stat().st_size:
                assert parent.poll() is None and time.monotonic() < deadline
                time.sleep(0.01)
            children = list(map(int, ready.read_text().split()))
            parent.send_signal(sig)
            parent.wait(timeout=10)
            deadline = time.monotonic()+2
            while any(alive(pid) for pid in children) and time.monotonic() < deadline:
                time.sleep(0.01)
            assert not any(alive(pid) for pid in children), {'signal': sig.name, 'surviving_children': [pid for pid in children if alive(pid)]}
            parent.communicate(timeout=5)
        finally:
            for pid in children:
                if alive(pid):
                    os.kill(pid, signal.SIGKILL)
            if parent.poll() is None:
                parent.kill()
            parent.communicate(timeout=5)
    # A real controlling terminal must keep the original foreground group so
    # compiler-owned interactive consent and terminal Ctrl-C still work.
    import pty
    ready = work/'ready-terminal'
    pid, terminal = pty.fork()
    if pid == 0:
        limits()
        os.execv(binary, [str(binary), sys.executable, str(script), str(ready)])
    children = []
    reaped = False
    try:
        deadline = time.monotonic()+10
        while not ready.exists() or not ready.stat().st_size:
            found, _ = os.waitpid(pid, os.WNOHANG)
            reaped = bool(found)
            assert not found and time.monotonic() < deadline
            time.sleep(0.01)
        children = list(map(int, ready.read_text().split()))
        os.write(terminal, b'\x03')
        while not reaped and time.monotonic() < deadline:
            reaped = bool(os.waitpid(pid, os.WNOHANG)[0])
            if not reaped: time.sleep(0.01)
        assert reaped and not any(alive(child) for child in children), children
    finally:
        for child in children:
            if alive(child): os.kill(child, signal.SIGKILL)
        if not reaped:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        os.close(terminal)
print('PASS: redirected SIGTERM/SIGINT stop manager and compiler descendants')
print('PASS: controlling terminal foreground group and Ctrl-C preserved')
print('PASS: manager proxy defaults, explicit --root and literal forwarded --root')
