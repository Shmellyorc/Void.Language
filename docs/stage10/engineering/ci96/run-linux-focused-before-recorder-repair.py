"""Sequential focused regression commands; preserves accepted Step41 build dirs."""
from pathlib import Path
import hashlib,json,os,subprocess,time,sys
root=Path(__file__).resolve().parents[4]
os.chdir(root)
evidence=Path('docs/stage10/engineering/ci96')
sdk=root/'.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64'
tests_only="--tests-only" in sys.argv
san_only="--san-only" in sys.argv or tests_only
phase_file=evidence/("linux-untraced-commands.json" if san_only else "linux-commands.json")
phases=[]
inputs=['CMakeLists.txt','cmake/LLVMPlatform.cmake','cmake/probes/StopToken.cpp','src/temporal.cpp','tests/formatting/temporal.cpp','tests/stage10/operator_reload.cpp','tests/stage10/console.py','tests/stage10/lsp_source_formatting.py']
freeze={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(evidence/'repair-source-freeze.json').write_text(json.dumps(freeze,indent=2)+'\n')
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86')
def run(label,cmd):
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==s for p,s in freeze.items()),'Repair source changed during verification'
    start=time.monotonic()
    with (evidence/(label+'.log')).open('wb') as log:
        result=subprocess.run([str(a) for a in cmd],stdout=log,stderr=subprocess.STDOUT,env=env)
    phases.append(dict(label=label,command=list(map(str,cmd)),returncode=result.returncode,seconds=time.monotonic()-start,log=label+'.log'))
    phase_file.write_text(json.dumps(phases,indent=2)+'\n')
    print(label,result.returncode,round(phases[-1]['seconds'],2),flush=True)
    if result.returncode:raise SystemExit(result.returncode)
targets=['voidc','voidls','void_operator_reload','void_temporal','void_temporal_ir','void_format_zones','void_runtime_console','void_runtime_console_transfers','void_runtime_console_input','void_runtime_console_keyboard','void_runtime_console_terminal','void_runtime_console_wide','void_console_source','void_native_fixture','void_tests']
tests=['stage10_operator_reload','format_temporal','stage10_temporal_ir','format_zones','stage10_console_aot','lsp_stage10_source_formatting','stage10_console_source','runtime_console','runtime_console_transfers','runtime_console_input','runtime_console_keyboard','runtime_console_terminal','runtime_console_wide','runtime_console_endpoints','runtime_console_native_transfers','runtime_console_native_input','runtime_console_native_keyboard','runtime_console_native_terminal']
configurations=[('san','RelWithDebInfo','ON')] if san_only else [('debug','Debug','OFF'),('san','RelWithDebInfo','ON')]
for name,config,san in configurations:
    tag=('untraced-' if san_only else '')+name
    build=Path('build')/('step42-ci96-'+name)
    if not tests_only:
        run(tag+'-configure',['cmake','-S','.', '-B',build,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE='+config,'-DCMAKE_C_COMPILER='+str(sdk/'bin/clang'),'-DCMAKE_CXX_COMPILER='+str(sdk/'bin/clang++'),'-DLLVM_DIR='+str(sdk/'lib/cmake/llvm'),'-DVOID_CLANG='+str(sdk/'bin/clang'),'-DVOID_LINKER='+str(sdk/'bin/ld.lld'),'-DBUILD_TESTING=ON','-DVOID_SANITIZERS='+san,'-DVOID_LLVM_RESEARCH=OFF','-DVOID_RETENTION_BASELINE=OFF'])
        run(tag+'-build',['cmake','--build',build,'--target',*targets,'--parallel','3'])
    run(tag+'-tests',['ctest','--test-dir',build,'--output-on-failure','--output-junit',root/evidence/(tag+'-junit.xml'),'-R','^('+'|'.join(tests)+')$'])
    if name=='debug':
        import resource
        def small_stack():resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
        for label,cmd in [('frontend-1m',['ctest','--test-dir',build,'-R','^frontend$','--output-on-failure']),('semantic-depth-1m',['python3','tests/integration/semantic_depth.py',str(build/'voidc')])]:
            start=time.monotonic()
            with (evidence/(label+'.log')).open('wb') as log:result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,env=env,preexec_fn=small_stack)
            phases.append(dict(label=label,command=cmd,returncode=result.returncode,seconds=time.monotonic()-start,stack_soft=1048576,stack_hard=1048576,log=label+'.log'))
            phase_file.write_text(json.dumps(phases,indent=2)+'\n')
            print(label,result.returncode,flush=True)
            if result.returncode:raise SystemExit(result.returncode)
