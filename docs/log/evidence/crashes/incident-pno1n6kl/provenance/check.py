import sys
import support as h
try:
    r=h.run([sys.argv[1], 'check', sys.argv[2] if len(sys.argv)>2 else h.ROOT/'tests/log/smoke.void'])
    print(r.stdout.decode(), r.stderr.decode())
except AssertionError as problem:
    print('Source check failed; original receipt retained.')
    latest=max(h.EVIDENCE.glob('run-*'),key=lambda p:p.stat().st_mtime_ns)
    print((latest/'stderr.bin').read_text()[:14000])
    raise SystemExit(1)
