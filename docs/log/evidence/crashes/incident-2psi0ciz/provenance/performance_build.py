"""Build a separate measurement harness; never edit production for instrumentation.

Usage: performance_build.py OUTPUT_DIRECTORY
The output includes all generated source overlays and compiler/link receipts.
"""
from pathlib import Path
import json, os, re, shlex, shutil, sys
import support as h

out = Path(sys.argv[1]).resolve()
asynchronous = '--async' in sys.argv[2:]
out.mkdir(parents=True, exist_ok=True)
build = h.ROOT / 'build/ci-official-release'
(out/'production-source-hashes.json').write_text(json.dumps({str(p.relative_to(h.ROOT)):h.crash.digest(p)
    for directory in ('src','include','stdlib') for p in (h.ROOT/directory).rglob('*') if p.is_file()},indent=2)+'\n')
library = out / 'stdlib'
if library.exists():
    shutil.rmtree(library)
shutil.copytree(h.ROOT / 'stdlib', library)
if not asynchronous:
    log_source = (library / 'log.void').read_text()
    if 'use io/memory\n' not in log_source:
        log_source = 'use io/memory\n' + log_source
    (library / 'log.void').write_text(log_source + '\n' + (h.ROOT / 'tests/log/performance_framing.void').read_text())
source = out / 'performance.void'
source.write_text('''use log
async func entry(command: i32) -> i32:
    if command >= 1000:
        return log.perf_setup(command - 1000)
    return log.perf_stage(command)
''')
if asynchronous:
    source.write_text((h.ROOT/'tests/log/performance_async.void').read_text())
env = dict(os.environ, VOID_STDLIB_ROOT=str(library))
h.run([build / 'voidc', 'check', source], small=True, env=env, sources=[out])
base = (h.ROOT / 'tests/log/host.cpp').read_text()
base = base.replace('std::make_shared<vd::Runtime>(1', 'std::make_shared<vd::Runtime>(256')
base = base.replace('#include "../managed/probe.hpp"', '#include "' + str(h.ROOT / 'tests/managed/probe.hpp') + '"')
(out / 'performance-base.cpp').write_text(base)
shutil.copy2(h.ROOT / 'tests/log/performance_metrics.hpp', out)
shutil.copy2(h.ROOT / 'tests/log' / ('performance_async_host.cpp' if asynchronous else 'performance_host.cpp'), out/'performance_host.cpp')

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
    'coordination.cpp': [('Runtime::collect(', 'collect'), ('Runtime::poll()', 'poll'), ('Runtime::Handle::~Handle()', 'handle_destroy')],
    'heap.cpp': [('Heap::validate_output_address(', 'heap_alias')],
}
objects=[]
for filename, methods in instrumentation.items():
    text = (h.ROOT / 'src/runtime' / filename).read_text()
    if filename=='list.cpp' and 'Runtime::list_read_index(' in text:
        methods=[('Runtime::list_read_index(' if marker=='Runtime::list_read(' else marker,metric) for marker,metric in methods]
    for marker, metric in methods:
        assert text.count(marker)==1, marker
        start=text.index('{',text.index(marker))+1
        text=text[:start]+' perf::Timer perf_timer(perf::'+metric+'); '+text[start:]
    text=text.replace('std::memcpy(', 'perf::copy(').replace('std::memmove(', 'perf::move(')
    if filename=='coordination.cpp':
        text=text.replace('const auto& slot = f->slots_[i];','perf::add(perf::gc_slots); const auto& slot = f->slots_[i];')
        text=text.replace('for (auto* h : handles_) if (h->value_) roots.push_back(h->value_);',
                          'for (auto* h : handles_) {perf::add(perf::gc_handles); if (h->value_) roots.push_back(h->value_);}')
    if filename=='heap.cpp':
        text=text.replace('for(const auto& [payload,object]:objects_)if(object.kind==TypeLayout::Kind::buffer)',
                          'for(const auto& [payload,object]:perf::visit(objects_,perf::heap_alias_visits))if(object.kind==TypeLayout::Kind::buffer)')
        text=text.replace(';buffer;buffer=buffer->next)', ';perf::test(buffer,perf::heap_alias_visits);buffer=buffer->next)')
    overlay=out / filename
    overlay.write_text('#include "performance_metrics.hpp"\n'+text)
    obj=overlay.with_suffix('.o')
    h.run([*flags, '-c', overlay, '-o', obj], cwd=build, small=True, sources=[out])
    objects.append(str(obj))

common_objects=[]
if asynchronous:
    text=(h.ROOT/'src/runtime/async.cpp').read_text()
    def region(text,marker,timer,visits=None):
        assert text.count(marker)==1,marker
        first=text.index('{',text.index(marker));depth=1;last=first+1
        while depth:
            depth+=(text[last]=='{')-(text[last]=='}');last+=1
        body=text[first+1:last-1]
        if visits:
            body=re.sub(r':\s*(tasks|s\.tasks)\s*\)',r':perf::visit(\1,perf::'+visits+'))',body)
            body=re.sub(r';\s*(task|child)\s*;',r';perf::test(\1,perf::'+visits+');',body)
        return text[:first+1]+' perf::Timer perf_timer(perf::'+timer+'); '+body+text[last-1:]
    for marker,timer,visits in [
        ('Task& find(', 'async_find_time','async_find'),
        ('void receive()', 'async_receive_time','async_receive'),
        ('bool pending(', 'async_pending_time','async_pending'),
        ('void collect(Task&', 'async_collect_time','async_collect'),
        ('void cancel_children(', 'async_children_time','async_children'),
        ('void finish_native_children(', 'async_children_time','async_children'),
        ('Task* child_failure(', 'async_failure_time','async_failure'),
        ('void wait_work()', 'async_wait','async_work'),
        ('~Scope()', 'async_cleanup','async_scan'),
        ('void vd_async_new(', 'async_new',None),
        ('void async_submit_native(', 'async_new',None),
    ]:
        text=region(text,marker,timer,visits)
    text=re.sub(r':\s*(tasks|s\.tasks)\s*\)',r':perf::visit(\1,perf::async_scan))',text)
    text=re.sub(r';\s*(task|child)\s*;',r';perf::test(\1,perf::async_scan);',text)
    text=text.replace('auto found=task_index.find(address);','perf::add(perf::async_index_lookups);auto found=task_index.find(address);')
    text=text.replace('while(previous && previous->sequence>task.sequence)', 'while(perf::test(previous,perf::async_activation_visits) && previous->sequence>task.sequence)')
    text=text.replace('s.register_task(std::move(item));','s.register_task(std::move(item));perf::add(perf::async_tasks);')
    text=text.replace('executor.register_task(std::move(item));','executor.register_task(std::move(item));perf::add(perf::async_tasks);')
    text=text.replace('s.tasks.push_back(std::move(item));','s.tasks.push_back(std::move(item));perf::add(perf::async_tasks);')
    text=text.replace('executor.tasks.push_back(std::move(item));','executor.tasks.push_back(std::move(item));perf::add(perf::async_tasks);')
    text=text.replace('perf::Timer perf_timer(perf::async_cleanup);','perf::Timer perf_timer(perf::async_cleanup);perf::add(perf::retained_tasks,tasks.size());')
    text=text.replace('next->resume(next->frame.get(),&result);','{perf::Timer timed_resume(perf::async_resume);next->resume(next->frame.get(),&result);}')
    text=text.replace('task->work->native->execute(task->cancel_requested, &failure);','{perf::Timer native_execution(perf::native_execute);task->work->native->execute(task->cancel_requested, &failure);}')
    (out/'async.cpp').write_text('#include "performance_metrics.hpp"\n'+text)
    h.run([*flags,'-c',out/'async.cpp','-o',out/'async.o'],cwd=build,small=True,sources=[out])
    objects.append(str(out/'async.o'))
    clock=(h.ROOT/'src/runtime/temporal_abi.cpp').read_text()
    assert clock.count('auto now=t::now_utc();')==1
    clock=clock.replace('auto now=t::now_utc();','auto now=t::now_utc();volatile auto sampled_seconds=now.seconds;(void)sampled_seconds;now.seconds=1609459200;now.nanosecond=123000000;')
    clock=clock.replace('case 8:{','case 8:{perf::Timer clock_timer(perf::clock_sample);')
    clock=clock.replace('case 15:text=t::format(instant(),strings[2]);break;', 'case 15:{perf::Timer clock_timer(perf::clock_format);text=t::format(instant(),strings[2]);break;}')
    (out/'temporal_abi.cpp').write_text('#include "performance_metrics.hpp"\n'+clock)
    h.run([*flags,'-c',out/'temporal_abi.cpp','-o',out/'temporal_abi.o'],cwd=build,small=True,sources=[out])
    common_objects.append(str(out/'temporal_abi.o'))
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
        args[args.index('libvoid_runtime.a'):args.index('libvoid_runtime.a')]=common_objects
        h.run(args,cwd=build,small=True,sources=[out])
        print('Built',exe,flush=True)
(out/'hashes.json').write_text(json.dumps({str(p.relative_to(out)):h.crash.digest(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
