"""Compare the same one-worker async IO loop with HTTP absent, idle and paused."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import tempfile
import threading
import fixture

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host',type=Path);parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--baseline',action='store_true',help='Run the same IO loop without HTTP imports on the baseline compiler')
    args=parser.parse_args();host=args.host.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='VOID IO HTTP comparison ') as directory:
        work=Path(directory);(work/'input.bin').write_bytes(bytes(65536))
        peer=fixture.Server(('127.0.0.1',0),fixture.Handler);peer.daemon_threads=True
        threading.Thread(target=peer.serve_forever,daemon=True).start()
        source=Path(__file__).with_suffix('.void').read_text().replace('@SOURCE@',str(work/'input.bin').replace('\\','\\\\'))
        source=source.replace('@DESTINATION@',str(work/'output.bin').replace('\\','\\\\')).replace('@BASE@',f'http://127.0.0.1:{peer.server_port}')
        if args.baseline:
            source=source.replace('use net/http\n','')
            source=source[:source.index('async func entry(')]+'async func entry(mode: i32) -> i32:\n    return await exercise_io()\n'
        path=work/'comparison.void';path.write_text(source);(args.evidence/'comparison.void').write_text(source)
        try:
            result=subprocess.run([str(host),str(path)],cwd=work,capture_output=True,timeout=180)
            (args.evidence/'native.stdout').write_bytes(result.stdout);(args.evidence/'native.stderr').write_bytes(result.stderr)
            assert result.returncode==0,result.stderr.decode(errors='replace')
            rows=[json.loads(line) for line in result.stdout.splitlines()]
            assert len(rows)==9 and (work/'output.bin').stat().st_size==16777216
            report=dict(binary_sha256=hashlib.sha256(host.read_bytes()).hexdigest(),rows=rows,
                medians={name:statistics.median(row['seconds'] for row in rows if row['mode']==mode)
                         for mode,name in enumerate(['no_http','idle_http','paused_http'])},
                baseline=args.baseline,
                scope='Same IO loop, one worker, 256 async 64-KiB writes per invocation; JIT compilation excluded; all baseline modes omit HTTP' if args.baseline else
                      'Same optimized source/Runtime, one worker, 256 async 64-KiB writes per invocation; JIT compilation excluded; compare the separate baseline receipt')
            (args.evidence/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
        finally:peer.shutdown();peer.server_close()
    print('One-worker IO absent/idle/paused HTTP comparison passed')

if __name__=='__main__':main()
