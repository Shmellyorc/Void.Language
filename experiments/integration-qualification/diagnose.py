import qualify as h
import json,subprocess,shlex,re
# Fixed campaign: one full original Stage 7 replay per retained baseline/current/strict.
# No timing-based retries or acceptance criteria. Each replay contains 12 processes.
for label,build in [('editor-baseline','experiments/internal-performance/evidence/baseline'),('editor-integrated','build/llvm23-release')]:
 h.run(label,['python3',h.AREA/'editor_capture.py',label,str(h.ROOT/build)])
# Unchanged original HTTP IO fixture; preserve all nine mode samples, not a new performance cohort.
for label,host in [('http-baseline','experiments/internal-performance/evidence/io-control-baseline'),('http-integrated','build/llvm23-release/void_http_io_performance')]:
 path=h.ROOT/host
 if path.exists():h.run(label,['python3',h.ROOT/'tests/net/http/io_performance.py',path,'--evidence',h.E/label])
 else:(h.E/(label+'-unavailable.json')).write_text(json.dumps({'missing':str(path)}))
# Recompile only the historical advanced test translation unit against current ABI.
source=h.E/'advanced-original.cpp';source.write_bytes(subprocess.check_output(['git','show','HEAD:tests/advanced/tests.cpp'],cwd=h.ROOT))
# Preserve relative include structure without changing the original test bytes.
(source.parent/'managed').mkdir(exist_ok=True)
# The source expects ../managed/probe.hpp; use a source folder beneath evidence.
folder=h.E/'advanced-original';folder.mkdir();source=folder/'tests.cpp';source.write_bytes((h.E/'advanced-original.cpp').read_bytes())
(h.E/'managed/probe.hpp').write_bytes((h.ROOT/'tests/managed/probe.hpp').read_bytes())
build=h.ROOT/'build/llvm23-release';target=build/'CMakeFiles/void_advanced_tests.dir';s=(target/'flags.make').read_text();flags=[re.search(r'^# compile CXX with (.*)$',s,re.M)[1]]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(re.search('^'+n+' = (.*)$',s,re.M)[1])
obj=h.E/'advanced-original.o'
h.run('advanced-original-compile',[*flags,'-c',source,'-o',obj],small=False,check=True)
link=shlex.split((target/'link.txt').read_text());link=[str(obj) if x=='CMakeFiles/void_advanced_tests.dir/tests/advanced/tests.cpp.o' else x for x in link];link[link.index('-o')+1]=str(h.E/'advanced-original-host')
h.run('advanced-original-link',link,cwd=build,small=False,check=True)
h.run('advanced-original-current-runtime',[h.E/'advanced-original-host',h.ROOT])
