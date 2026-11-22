"""Compiler-owned editor consent: exact identity, recapture, bounded redirected refusal."""
from pathlib import Path
import json
import os
import queue
import subprocess
import sys
import tempfile
import threading

compiler = str(Path(sys.argv[1]).resolve())
session = 'a' * 64
prefix = '\x1eVOID_NATIVE_APPROVAL '

def run(args, env, status=0):
    p = subprocess.run([compiler, *map(str, args)], env=env, capture_output=True, timeout=90, text=True)
    assert p.returncode == status, (args, p.returncode, p.stdout, p.stderr)
    return p

with tempfile.TemporaryDirectory(prefix='VOID editor approval ') as directory:
    base = Path(directory); root = base/'project'
    env = dict(os.environ, XDG_DATA_HOME=str(base/'data')); env.pop('CI', None)
    run(['new', 'app', root], env)
    run(['add', 'raylib', '--project', root], env)
    (root/'src/app.void').write_text('use pkg/raylib as rl\nfunc main() -> i32:\n    var dt = rl.get_frame_time()\n    return dt as i32\n')
    catalog = root/'libs/raylib/.void/native/void.native.toml'
    original = catalog.read_bytes()
    args = ['build', '--project', root, '--profile', 'debug', '--native-approval-stdio', session]
    def review(reply, mutate=None):
        child = subprocess.Popen([compiler, *map(str, args)], env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        pending = queue.Queue(); lines=[]
        def read():
            for line in child.stderr:
                lines.append(line);pending.put(line)
            pending.put(None)
        reader = threading.Thread(target=read, daemon=True);reader.start()
        try:
            while True:
                line = pending.get(timeout=20)
                assert line is not None, ''.join(lines)
                if line.startswith(prefix):break
            request = json.loads(line[len(prefix):]);assert request['session']==session
            assert request['schema']==1 and len(request['id'])==64 and 'org.voidlang.raylib' in request['provider'], request
            if mutate:mutate()
            if reply is not None:child.stdin.write(reply(request)+'\n');child.stdin.flush()
            else:child.stdin.close()
            code = child.wait(timeout=30);reader.join(timeout=5)
            return request,code,''.join(lines)
        finally:
            if child.poll() is None:child.kill();child.wait()
            reader.join(timeout=5)
    # Ordinary redirected execution remains immediate and cannot save consent.
    run(['build', '--project', root], env, 2)
    for response in [lambda r:'deny', lambda r:'approve '+'0'*64, None]:
        _,code,_=review(response);assert code==2, code
        assert 'No native approvals' in run(['native','approvals'],env).stdout
    request,code,output=review(lambda r:'approve '+r['id']);assert code==0, output
    assert '\x1eVOID_NATIVE_APPROVAL_END '+session in output
    before = run(['native','approvals'],env).stdout
    records = list((base/'data/void/native-approvals').glob('*/approval'))
    assert len(records)==1,records
    saved = [(p.read_bytes(),p.stat().st_mtime_ns) for p in records]
    reused = run(args,env)
    assert prefix not in reused.stderr and '\x1eVOID_NATIVE_APPROVAL_END '+session in reused.stderr
    assert saved==[(p.read_bytes(),p.stat().st_mtime_ns) for p in records]
    catalog.write_bytes(original+b'\n# New provider identity\n')
    changed,code,_=review(lambda r:'deny');assert code==2 and changed['id']!=request['id']
    # An authorized identity changed while the review was open cannot be saved.
    changed,code,output=review(lambda r:'approve '+r['id'],lambda:catalog.write_bytes(original+b'\n# Changed during review\n'))
    assert code!=0 and run(['native','approvals'],env).stdout==before,(code,output)
    catalog.write_bytes(original)
    run(args,dict(env,CI='1'),2)
    # The seal must exist for pure applications too, before any application stderr.
    pure = base/'pure';run(['new','app',pure],env)
    (pure/'src/app.void').write_text('use console\nfunc main():\n    var written = console.stderr.write("application stderr")\n')
    result = run(['run','--project',pure,'--native-approval-stdio',session],env)
    assert result.stderr.index('\x1eVOID_NATIVE_APPROVAL_END '+session)<result.stderr.index('application stderr'),result.stderr
    print('PASS: deny/EOF/wrong identity, explicit approval, byte-stable reuse, fresh identity, review-time change, CI refusal, pure-app phase seal')
