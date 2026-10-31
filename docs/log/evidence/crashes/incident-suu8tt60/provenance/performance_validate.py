"""Fresh, expanded repair acceptance; never rewrites pre-repair evidence."""
from pathlib import Path
import json, os, re, shutil, sys
import support as h

profile=Path(sys.argv[1]).resolve()
out=Path(sys.argv[2]).resolve()
assert not out.exists(), 'Choose a fresh evidence directory'
out.mkdir(parents=True)
env=dict(os.environ)
if '--strict' in sys.argv:
    env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86',
               LSAN_OPTIONS='exitcode=87')
sources=[p for name in ('src','include','stdlib','tests/log','tests/async','tests/managed',
                        'tests/runtime','tests/growable_arrays','tests/fixed_arrays',
                        'tests/arrays','tests/text_encoding','tests/stage8/sprint5','tests/io')
         for p in (h.ROOT/name).rglob('*') if p.is_file() and '__pycache__' not in str(p)]
sources += [h.ROOT/'CMakeLists.txt']
freeze={str(p.relative_to(h.ROOT)):h.crash.digest(p) for p in sources}
(out/'source-freeze.json').write_text(json.dumps(freeze,indent=2)+'\n')
targets=['voidc','voidls','void_io_runner','void_log_host','void_log_probe_normal',
         'void_log_probe_optimized','void_advanced_tests','void_globals','void_console_source',
         'void_io_ownership','void_io_native','void_io_native_jobs','void_io_kernel',
         'void_async_tests','void_gc_buffers','void_growable_arrays_tests',
         'void_fixed_arrays_tests','void_arrays_source','void_retention_current','void_runtime_tests',
         'void_text_audit','void_text_ownership']
fixtures=('spawn spawn_cancel spawn_failure activation managed large closures loops values methods '
          'lifecycle lifecycle_exits lifecycle_cancelled lifecycle_failures lifecycle_primary '
          'lifecycle_resources lifecycle_values').split()
targets += ['void_async_aot_'+fixture+'_'+mode for fixture in fixtures for mode in ('normal','optimized')]
targets += ['void_retention_aot_'+fixture+'_'+mode for fixture in ('growth','workers') for mode in ('normal','optimized')]
pattern=('^(log_|lsp_log$|io_kernel$|io_ownership$|io_native|io_async_source$|io_errors_async$|'
         'io_async_acquisition$|async_source$|async_ir$|async_parallel$|async_lifecycle$|'
         'stage11_globals$|stage10_console_source$|advanced_source$|gc_buffers$|runtime_coordination$|'
         'growable_arrays_source$|fixed_arrays_source$|arrays_source$|'
         'async_syntax_source$|async_syntax_stack$|async_aot_|async_retention_|'
         'text_encoding_(text|hex|base64|ascii|oracles|interop|large|errors|stack|ir|ownership)$)')
for label,command,timeout in [
    ('configure',['cmake','-S',h.ROOT,'-B',profile],1800),
    ('build',['cmake','--build',profile,'--target',*targets,'-j','2'],3600),
    ('ctest',['ctest','--test-dir',profile,'-R',pattern,'--output-on-failure','-j','1'],14400),
]:
    print(label,profile,flush=True)
    result=h.run(command,env=env,small=True,timeout=timeout,sources=sources,
                 context={'repair_validation':str(out),'phase':label})
    (out/(label+'.stdout')).write_bytes(result.stdout)
    (out/(label+'.stderr')).write_bytes(result.stderr)
    if label=='ctest':
        shutil.copy2(profile/'Testing/Temporary/LastTest.log',out/'LastTest.log')
        print(result.stdout.decode(),end='',flush=True)
assert all(h.crash.digest(h.ROOT/name)==digest for name,digest in freeze.items()), 'Source changed during validation'
tools={name:h.crash.digest(profile/name) for name in [*targets,'libvoid_runtime.a','void-native.txt','CMakeCache.txt']}
(out/'tool-hashes.json').write_text(json.dumps(tools,indent=2)+'\n')
(out/'environment.json').write_text(json.dumps({name:env.get(name) for name in ('ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS')},indent=2)+'\n')
print('Repair profile acceptance complete',profile,flush=True)
