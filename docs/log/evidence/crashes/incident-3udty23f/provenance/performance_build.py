"""Build a separate measurement harness; never edit production for instrumentation.

Usage: performance_build.py OUTPUT_DIRECTORY
The output includes all generated source overlays and compiler/link receipts.
"""
from pathlib import Path
import json, os, re, shlex, shutil, sys
import support as h

out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
build = h.ROOT / 'build/ci-official-release'
library = out / 'stdlib'
if library.exists():
    shutil.rmtree(library)
shutil.copytree(h.ROOT / 'stdlib', library)
(library / 'log.void').write_text('use io/memory\n' + (library / 'log.void').read_text()
    + '\n' + (h.ROOT / 'tests/log/performance_framing.void').read_text())
source = out / 'performance.void'
source.write_text('''use log
async func entry(command: i32) -> i32:
    if command >= 1000:
        return log.perf_setup(command - 1000)
    return log.perf_stage(command)
''')
env = dict(os.environ, VOID_STDLIB_ROOT=str(library))
h.run([build / 'voidc', 'check', source], small=True, env=env, sources=[out])
base = (h.ROOT / 'tests/log/host.cpp').read_text()
base = base.replace('std::make_shared<vd::Runtime>(1', 'std::make_shared<vd::Runtime>(256')
base = base.replace('#include "../managed/probe.hpp"', '#include "' + str(h.ROOT / 'tests/managed/probe.hpp') + '"')
(out / 'performance-base.cpp').write_text(base)
shutil.copy2(h.ROOT / 'tests/log/performance_metrics.hpp', out)
shutil.copy2(h.ROOT / 'tests/log/performance_host.cpp', out)

def recipe(target):
    directory = build / 'CMakeFiles' / (target + '.dir')
    settings = dict(re.findall(r'^(CXX_\w+) = (.*)$', (directory / 'flags.make').read_text(), re.M))
    link = shlex.split((directory / 'link.txt').read_text())
    flags = [link[0]]
    for name in ('CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS'):
        flags += shlex.split(settings[name])
    return flags, link

flags, _ = recipe('void_log_host')
instrumentation = {
    'list.cpp': [('Runtime::list_count(', 'list_count'), ('Runtime::list_read(', 'list_read'), ('Runtime::list_push(', 'list_push')],
    'array.cpp': [('Runtime::list_header(RegisteredRootWord source,const ElementSchemaRef&', 'list_header'),
                  ('Runtime::list_write_bytes(', 'bulk_write'), ('Runtime::array_slice(', 'array_slice'),
                  ('Runtime::string_encode_into(', 'string_encode')],
    'buffer.cpp': [('Runtime::root_word(', 'root_word'), ('Runtime::reference_source(', 'reference_source'),
                   ('Runtime::BufferBuilder::copy_elements(', 'buffer_copy')],
    'coordination.cpp': [('Runtime::collect(', 'collect'), ('Runtime::poll()', 'poll')],
}
objects=[]
for filename, methods in instrumentation.items():
    text = (h.ROOT / 'src/runtime' / filename).read_text()
    for marker, metric in methods:
        assert text.count(marker)==1, marker
        start=text.index('{',text.index(marker))+1
        text=text[:start]+' perf::Timer perf_timer(perf::'+metric+'); '+text[start:]
    text=text.replace('std::memcpy(', 'perf::copy(').replace('std::memmove(', 'perf::move(')
    if filename=='coordination.cpp':
        text=text.replace('const auto& slot = f->slots_[i];','perf::add(perf::gc_slots); const auto& slot = f->slots_[i];')
        text=text.replace('for (auto* h : handles_) if (h->value_) roots.push_back(h->value_);',
                          'for (auto* h : handles_) {perf::add(perf::gc_handles); if (h->value_) roots.push_back(h->value_);}')
    overlay=out / filename
    overlay.write_text('#include "performance_metrics.hpp"\n'+text)
    obj=overlay.with_suffix('.o')
    h.run([*flags, '-c', overlay, '-o', obj], cwd=build, small=True, sources=[out])
    objects.append(str(obj))
probe=(h.ROOT / 'tests/managed/probe.cpp').read_text().replace('#include "probe.hpp"', '#include "'+str(h.ROOT / 'tests/managed/probe.hpp')+'"')
probe=probe.replace('void* operator new(std::size_t size) {', 'void* operator new(std::size_t size) { perf::add(perf::allocation); perf::add(perf::allocation_bytes,size);')
(out / 'probe.cpp').write_text('#include "performance_metrics.hpp"\n'+probe)
h.run([*flags,'-c',out/'probe.cpp','-o',out/'probe.o'],cwd=build,small=True,sources=[out])

for target in ('void_log_host','void_log_probe_optimized'):
    compile_flags, link=recipe(target)
    host_object=out / (target+'.o')
    h.run([*compile_flags,'-c',out/'performance_host.cpp','-o',host_object],cwd=build,small=True,sources=[out])
    link[link.index(f'CMakeFiles/{target}.dir/tests/log/host.cpp.o')]=str(host_object)
    link[link.index(f'CMakeFiles/{target}.dir/tests/managed/probe.cpp.o')]=str(out/'probe.o')
    if target!='void_log_host':
        obj=out/'performance-native.o'
        h.run([build/'void_io_runner','--emit-probe',source,obj,'optimized'],small=True,env=env,sources=[out])
        adapter=out/'performance-adapter.o'
        h.run([*compile_flags,'-c',str(obj)+'.cpp','-o',adapter],cwd=build,small=True,sources=[out])
        link[link.index('CMakeFiles/void_log_probe_optimized.dir/log-probe-optimized.o.cpp.o')]=str(adapter)
        link[link.index('log-probe-optimized.o')]=str(obj)
    for measured in (False,True):
        args=link.copy()
        exe=out / (target+('-instrumented' if measured else '-timing'))
        args[args.index('-o')+1]=str(exe)
        if measured:
            args[args.index('libvoid_runtime.a'):args.index('libvoid_runtime.a')]=objects
        h.run(args,cwd=build,small=True,sources=[out])
        print('Built',exe,flush=True)
(out/'hashes.json').write_text(json.dumps({str(p.relative_to(out)):h.crash.digest(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
