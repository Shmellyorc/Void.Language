"""Native Windows qualification from a verified packet and existing pinned SDK."""
from pathlib import Path
from hashlib import sha256
import json, os, platform, re, shutil, subprocess, sys, tempfile, time, traceback, zipfile

share = Path(r'\\vboxsvr\VM\slice7-final-integrated')
local = Path.home() / 'VS7I2'
baseline = Path.home() / 'VoidCI42377496702-Native'
source = local / 'src'
sdk = baseline / '.toolchains/llvm-23.1.4/bin'
cmake = Path(r'C:\gcc\bin\cmake.exe')
ctest = cmake.with_name('ctest.exe')
assert sys.getwindowsversion().build == 19045
assert local.is_dir()

def digest(path):
    with path.open('rb') as stream:
        return __import__('hashlib').file_digest(stream, 'sha256').hexdigest()

def save(name, value):
    (share/name).write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8')

packet = local/'source.zip'
expected = json.loads((share/'source-inputs.json').read_text())
assert digest(packet) == (share/'source-archive.txt').read_text().split()[0]
assert all(digest(source/path) == hashed for path, hashed in expected.items())
assert (local/'attested-include.cmake').is_file()

selection = re.compile(
    r'^(project_.*|cli_.*|target_triple|application_workflow_stage9|native_output_paths_ci88|'
    r'long_physical_paths_ci89|toolchain_stop_token|core|jit|reload|gc|gc_.*|native|native_source|'
    r'native_aot_(main|integrated|cleanup_failure)_.*|runtime_.*|analysis|frontend|modules|'
    r'numeric.*|sessions.*|tooling.*|ffi_g[12345]_.*|aot|.*_cli|'
    r'fixed_arrays_source|growable_arrays_source|arrays_(source|foundation)|generics_.*|'
    r'collections_source|dictionaries_source|properties_source|patterns_source|advanced_source|'
    r'async_(ir|source|lifecycle|parallel|syntax_source|aot_resources_.*)|native_workers|io_native_jobs|'
    r'local_libraries|lsp_(positions|protocol|sprint[2346]|stage8|stage11_globals)|'
    r'documentation_execution|stage6_integrated|stage10_(fields.*|operator_reload|operator_resolution|'
    r'operator_integration|tooling|display_reload)|os_(frontend|errors)|binary_ownership|'
    r'http_(native|reload|source|source_negative|source_faults|ir|install)|url_(native|ir|source))$'
)

def qualify(profile):
    build = local/profile
    scratch = Path(tempfile.mkdtemp(prefix=profile+'-', dir=local))
    record = dict(profile=profile, status='running', commands=[], free_bytes_before=shutil.disk_usage(local).free)
    output = lambda suffix: share/(profile+'-'+suffix)
    def save_record():
        save(profile+'-result.json', record)
    env = dict(os.environ, TEMP=str(scratch), TMP=str(scratch), PYTHONDONTWRITEBYTECODE='1',
               PATH=str(sdk)+os.pathsep+os.environ['PATH'])
    for key in ('VOID_STDLIB_ROOT','VOID_G4_EVIDENCE_ROOT','VOID_G5_EVIDENCE_ROOT'):
        env.pop(key,None)
    def run(label, args, timeout=5400, check=True):
        argv = list(map(str,args))
        record['stage'] = label
        save_record()
        start = time.monotonic()
        with output(label+'.txt').open('wb') as log:
            process = subprocess.run(argv, cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        record['commands'].append(dict(label=label, argv=argv, returncode=process.returncode, seconds=time.monotonic()-start))
        save_record()
        if check:
            assert process.returncode == 0, (label, process.returncode)
        return process.returncode
    try:
        identity = subprocess.check_output(['whoami.exe'],text=True).strip()
        run('scratch-acl',['icacls.exe',scratch,'/grant:r',identity+':(OI)(CI)F','/inheritance:r'])
        run('scratch-acl-verified',['icacls.exe',scratch])
        cache=(share.parent/'project-slice3'/(profile+'-CMakeCache.txt')).read_text()
        lookup=lambda key:re.search(r'^'+re.escape(key)+r':[^=]+=(.*)$',cache,re.M)[1]
        keys=('CMAKE_AR','CMAKE_CXX_COMPILER','CMAKE_C_COMPILER','CMAKE_LINKER','CMAKE_MAKE_PROGRAM',
              'CMAKE_MT','CMAKE_RC_COMPILER','LLVM_DIR','ZLIB_INCLUDE_DIR','ZLIB_LIBRARY_RELEASE','zstd_DIR','LibXml2_DIR',
              'VOID_HOST_DEPS_ROOT','VOID_HTTP_DEPS_ROOT')
        options=['-D'+key+'='+lookup(key) for key in keys]
        clang=Path(lookup('VOID_CLANG')).resolve(strict=True)
        assert digest(clang)==digest(sdk/'clang.exe')
        env['VOID_CLANG']=str(clang)
        cache_text=(build/'CMakeCache.txt').read_text()
        assert 'VOID_LLVM_ORC_RUNS:INTERNAL=1' in cache_text
        assert 'CMAKE_PROJECT_INCLUDE:UNINITIALIZED=' in cache_text
        shutil.copy2(build/'CTestTestfile.cmake',output('CTestTestfile.txt'))
        lines = (build/'CTestTestfile.cmake').read_text(encoding='utf-8').splitlines()
        targets={'voidc','voidls','voidup','void_native_inventory'}
        selected=[]
        others=[]
        for line in lines:
            match = re.match(r'^\s*add_test\((?:\[(=*)\[(.*?)\]\1\]|"([^"]+)"|([^\s)]+))',line)
            if not match:
                continue
            name = next(group for group in match.groups()[1:] if group is not None)
            if not selection.fullmatch(name):
                others.append(name)
                continue
            selected.append(name)
            for token in re.findall(r'"([^"]+)"',line):
                needle = build.as_posix().lower()+'/'
                token = token.replace('\\','/')
                offset = token.lower().find(needle)
                if offset >= 0 and token.lower().endswith('.exe'):
                    targets.add(Path(token[offset:]).stem)
        # Some CMake scripts consume binaries through non-positional -D options.
        # Build the selected references plus the install targets explicitly.
        save(profile+'-selection.json',dict(selected=selected, outside_scope=others, targets=sorted(targets)))
        required = {'ffi_g5_callbacks','project_creation_process','cli_install','gc','analysis','reload'}
        assert len(selected) >= 100 and required <= set(selected),(len(selected),sorted(required-set(selected)))
        run('build',[cmake,'--build',build,'--parallel','3','--target',*sorted(targets)])
        test_xml=output('regression.xml')
        ctest_args=[ctest,'--test-dir',build,'--parallel','1','--output-on-failure',
                    '-R',selection.pattern,'--output-junit',test_xml]
        status=run('regression',ctest_args,check=False)
        shutil.copy2(build/'Testing/Temporary/LastTest.log',output('LastTest.txt'))
        record['ctest_exit']=status
        record['status']='PASS' if status == 0 else 'TEST FAIL'
        install=local/(profile+'-installed')
        run('install',[cmake,'--install',build,'--prefix',install,'--config',profile,'--component','compiler'])
        record['installed_compiler']=str(install/'bin/voidc.exe')
        record['installed_sha256']=digest(install/'bin/voidc.exe')
    except BaseException:
        record['status']='ERROR'
        record['error']=traceback.format_exc()
    finally:
        remaining=sorted(p.relative_to(scratch).as_posix() for p in scratch.rglob('*'))
        shutil.rmtree(scratch)
        assert not scratch.exists()
        record['cleanup']=dict(path=str(scratch), removed=True, remaining_before_removal=remaining)
        record['free_bytes_after']=shutil.disk_usage(local).free
        save_record()
    return record['status']

results={p:qualify(p) for p in ('Release','Debug')}
save('done.json',results)
sys.exit(0 if all(s=='PASS' for s in results.values()) else 1)
