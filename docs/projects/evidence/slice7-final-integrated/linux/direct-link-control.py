"""Supplemental nine-case replay with only the pinned sodium archive added."""
from pathlib import Path
import json, os, re, shlex, subprocess, sys

root=Path.cwd()
profile=sys.argv[1]
work=root/'build/slice7-final-integrated'
evidence=root/'docs/projects/evidence/slice7-final-integrated/linux'
build=work/f'linux-{profile}'
directory=work/'direct-link-controls'/profile
directory.mkdir(parents=True)
clang=root/'.toolchains/llvm-23.1.4/bin/clang'
cache=(build/'CMakeCache.txt').read_text()
host=Path(re.search(r'^VOID_HOST_DEPS_ROOT:[^=]+=([^\n]+)$',cache,re.M)[1])
sodium=host/'lib/libsodium.a'
assert sodium.is_file()
log=evidence/f'{profile}-direct-link-argv.txt'
wrapper=directory/'clang-with-pinned-sodium'
wrapper.write_text('''#!/usr/bin/env bash
set -euo pipefail
for arg in "$@"; do
    if [[ "$arg" == */libvoid_runtime.a ]]; then
        set -- "$@" @SODIUM@
        printf -v row '%q ' @CLANG@ "$@"
        printf '%s\\n' "$row" >> @LOG@
        break
    fi
done
exec @CLANG@ "$@"
'''.replace('@SODIUM@',shlex.quote(str(sodium))).replace('@CLANG@',shlex.quote(str(clang)))
   .replace('@LOG@',shlex.quote(str(log))))
wrapper.chmod(0o700)
names={'ergonomics_sprint6','stage10_fields_aot','stage10_temporal_aot','stage11_globals_aot',
       'stage10_console_aot','stage10_operators_aot','stage10_conversion_aot',
       'application_native_sprint4','text_collections_sprint3'}
lines=[]
found=set()
for line in (build/'CTestTestfile.cmake').read_text().splitlines():
    match=re.match(r'^add_test\(\[=\[(.*?)\]=\]',line)
    if match and match[1] in names:
        assert line.count(str(clang))==1,line
        lines.append(line.replace(str(clang),str(wrapper)))
        lines.append(f'set_tests_properties({match[1]} PROPERTIES TIMEOUT 900 WORKING_DIRECTORY "{build}")')
        found.add(match[1])
assert found==names, (found,names-found)
(directory/'CTestTestfile.cmake').write_text('\n'.join(lines)+'\n')
env=os.environ|{'TMPDIR':(evidence/f'{profile}-temp.txt').read_text().strip(),'PYTHONDONTWRITEBYTECODE':'1'}
env.pop('VOID_STDLIB_ROOT',None)
if profile=='strict':
    env.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
command=['ctest','--test-dir',str(directory),'--parallel','2','--output-on-failure','--output-junit',
         str(evidence/f'{profile}-direct-link-control.xml')]
with (evidence/f'{profile}-direct-link-control.txt').open('wb') as output:
    output.write((json.dumps(command)+'\n').encode())
    output.flush()
    status=subprocess.run(command,stdout=output,stderr=subprocess.STDOUT,env=env,timeout=3600).returncode
sys.exit(status)
