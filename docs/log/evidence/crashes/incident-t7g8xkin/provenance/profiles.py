"""Focused profile build/test orchestration; shared crash recorder owns failures."""
from pathlib import Path
import os,sys,json
import support as h
profile=Path(sys.argv[1]).resolve();env=dict(os.environ)
if '--strict' in sys.argv:
    env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86',
               LSAN_OPTIONS='exitcode=87')
h.run(['cmake','-S',h.ROOT,'-B',profile],env=env,timeout=1800)
h.run(['cmake','--build',profile,'--target','voidc','voidls','void_io_runner','void_log_host','void_log_probe_normal','void_log_probe_optimized','-j','2'],env=env,timeout=1800)
if '--build-only' not in sys.argv:
    result=h.run(['ctest','--test-dir',profile,'-R','^(log_|lsp_log|io_kernel$|io_ownership$|io_native$|stage11_globals$|stage10_console$|advanced_language$)','--output-on-failure','-j','1'],env=env,timeout=7200)
    print(result.stdout.decode(),end='')
print('Logging profile build complete:',profile)
