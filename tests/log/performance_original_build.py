"""Build only the original benchmark host with production GC policy.

Does not execute or replace any benchmark results. Build required CMake targets
first, including AOT probes when passing their target names.
"""
from pathlib import Path
import json,re,shlex,sys
import support as h
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
build=h.ROOT/'build/ci-official-release'
(out/'production-source-hashes.json').write_text(json.dumps({str(p.relative_to(h.ROOT)):h.crash.digest(p)
    for directory in ('src','include','stdlib') for p in (h.ROOT/directory).rglob('*') if p.is_file()},indent=2)+'\n')
source=(h.ROOT/'tests/log/host.cpp').read_text()
assert source.count('std::make_shared<vd::Runtime>(1')==4
source=source.replace('std::make_shared<vd::Runtime>(1','std::make_shared<vd::Runtime>(256')
source=source.replace('#include "../managed/probe.hpp"','#include "'+str(h.ROOT/'tests/managed/probe.hpp')+'"')
overlay=out/'original-host.cpp';overlay.write_text(source)
for target in sys.argv[2:] or ['void_log_host']:
    files=build/'CMakeFiles'/(target+'.dir')
    definitions=dict(re.findall(r'^(CXX_\w+) = (.*)$',(files/'flags.make').read_text(),re.M))
    link=shlex.split((files/'link.txt').read_text())
    flags=[link[0]]
    for name in ('CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS'):
        flags+=shlex.split(definitions[name])
    obj=out/(target+'.o');exe=out/target
    h.run([*flags,'-c',overlay,'-o',obj],cwd=build,small=True,sources=[out])
    link[link.index(f'CMakeFiles/{target}.dir/tests/log/host.cpp.o')]=str(obj)
    link[link.index('-o')+1]=str(exe)
    h.run(link,cwd=build,small=True,sources=[out])
    print('Built original-workload host',exe,flush=True)
(out/'hashes.json').write_text(json.dumps({str(p.relative_to(out)):h.crash.digest(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
