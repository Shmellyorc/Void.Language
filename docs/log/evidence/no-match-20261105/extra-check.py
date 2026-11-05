exec(open('/tmp/void-no-match-check.py').read().split("shutil.copy2('/tmp/void-no-match-correctness.cpp'")[0].replace("shutil.copy2(__file__,d/'check.py')","shutil.copy2(__file__,d/'extra-check.py')"))
s=(d/'correctness.cpp').read_text();start=s.index('    for(int fixture=1;fixture<=2;++fixture)');end=s.index('    std::cout<<checks',start);s=s[:start]+s[end:]
needle='    // Pending collection must complete across the no-match operation boundary.'
extra='''    // Deliberately corrupt a separately rooted replacement extent, restore it
    // before collection, and verify Runtime failure/error precedence.
    runtime.string_into(&replacement,"late");
    runtime.array_write(runtime.root_word(&replacements),layout,true,80,&replacement);
    std::uint64_t valid_extent; std::memcpy(&valid_extent,replacement,sizeof(valid_extent));
    auto corrupt_extent=valid_extent+1;std::memcpy(replacement,&corrupt_extent,sizeof(corrupt_extent));
    auto before_output=output;failure={};
    auto corrupt_status=invoke(&output,16,failure);
    std::memcpy(replacement,&valid_extent,sizeof(valid_extent));
    require(failure.message && output==before_output,"corrupt replacement extent accepted or published");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,0,0xffffffffu);
    std::memcpy(replacement,&corrupt_extent,sizeof(corrupt_extent));failure={};
    corrupt_status=invoke(&output,-1,failure);
    std::memcpy(replacement,&valid_extent,sizeof(valid_extent));
    require(!failure.message && corrupt_status==2 && output==before_output,"later replacement masked invalid first key");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,0,1000);
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,1,1000);
    std::memcpy(replacement,&corrupt_extent,sizeof(corrupt_extent));failure={};
    corrupt_status=invoke(&output,-1,failure);
    std::memcpy(replacement,&valid_extent,sizeof(valid_extent));
    require(!failure.message && corrupt_status==2 && output==before_output,"later replacement masked duplicate key");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,1,1001);
    failure={};
'''
s=s.replace(needle,extra+needle).replace('#undef main','#undef main\n#include <cstring>')
(d/'native-extra.cpp').write_text(s)
folder=build/'CMakeFiles/void_text_ownership.dir';f=dict(re.findall(r'^(CXX_\w+) = (.*)$',(folder/'flags.make').read_text(),re.M));link=shlex.split((folder/'link.txt').read_text());flags=[link[0]]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(f[n])
def run(label,args,cwd=build):
 p=h.run(args,cwd=cwd,small=True,sources=[d],timeout=600);(d/(label+'.stdout')).write_bytes(p.stdout);(d/(label+'.stderr')).write_bytes(p.stderr);print(label,p.returncode,flush=True)
run('compile-native-extra',[*flags,'-c',d/'native-extra.cpp','-o',d/'native-extra.o'])
args=link.copy();args[args.index('CMakeFiles/void_text_ownership.dir/tests/text_encoding/ownership.cpp.o')]=str(d/'native-extra.o');args.insert(args.index('libvoid_runtime.a'),str(d/'candidate-plain.o'));args=[x.replace('--dependency-file=CMakeFiles/void_text_ownership.dir/link.d','--dependency-file='+str(d/'native-extra.link.d')) for x in args];args[args.index('-o')+1]=str(d/'native-extra');run('link-native-extra',args)
run('check-native-extra',[d/'native-extra','unused','unused',d])
# The retained strict frontend predates translation; only run the native ABI
# against its sanitized Runtime, with the current translation TU overlaid.
build=root/'build/llvm23-strict';folder=build/'CMakeFiles/void_text_ownership.dir';f=dict(re.findall(r'^(CXX_\w+) = (.*)$',(folder/'flags.make').read_text(),re.M));link=shlex.split((folder/'link.txt').read_text());flags=[link[0]]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(f[n])
for name in ['candidate-plain','native-extra']:run('strict-compile-'+name,[*flags,'-c',d/(name+'.cpp'),'-o',d/(name+'-strict.o')])
args=link.copy();args[args.index('CMakeFiles/void_text_ownership.dir/tests/text_encoding/ownership.cpp.o')]=str(d/'native-extra-strict.o');args.insert(args.index('libvoid_runtime.a'),str(d/'candidate-plain-strict.o'));args=[x.replace('--dependency-file=CMakeFiles/void_text_ownership.dir/link.d','--dependency-file='+str(d/'native-extra-strict.link.d')) for x in args];args[args.index('-o')+1]=str(d/'native-extra-strict');run('strict-link-native-extra',args)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86',LSAN_OPTIONS='exitcode=87')
p=h.run([d/'native-extra-strict','unused','unused',d],cwd=build,env=env,small=True,sources=[d],timeout=600);(d/'strict-check-native-extra.stdout').write_bytes(p.stdout);(d/'strict-check-native-extra.stderr').write_bytes(p.stderr);print('strict-check-native-extra',p.returncode,flush=True)
