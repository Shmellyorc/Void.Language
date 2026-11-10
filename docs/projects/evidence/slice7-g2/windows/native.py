from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback, zipfile

out=Path(r'\\vboxsvr\VM\project-slice7-g2')
prior=out.parent/'project-slice3'
home=Path.home()
baseline=home/'VoidCI42377496702-Native'
packet=home/'VoidS7-G2-source'
source=baseline
backup=home/'VoidS7-G2-backup'
sdk=baseline/'.toolchains/llvm-23.1.4'
cmake=Path(r'C:\gcc\bin\cmake.exe')
ctest=cmake.with_name('ctest.exe')
ninja=cmake.with_name('ninja.exe')
def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def save(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n')
def options(profile):
    cache=(prior/(profile+'-CMakeCache.txt')).read_text()
    lookup=lambda key:re.search(r'^'+re.escape(key)+r':[^=]+=(.*)$',cache,re.M)[1]
    keys=('CMAKE_AR','CMAKE_CXX_COMPILER','CMAKE_C_COMPILER','CMAKE_LINKER','CMAKE_MAKE_PROGRAM',
          'CMAKE_MT','CMAKE_RC_COMPILER','LLVM_DIR','ZLIB_INCLUDE_DIR','ZLIB_LIBRARY_RELEASE','zstd_DIR','LibXml2_DIR',
          'VOID_HOST_DEPS_ROOT','VOID_HTTP_DEPS_ROOT','VOID_CLANG')
    return ['-D'+key+'='+lookup(key) for key in keys]+['-DCMAKE_BUILD_TYPE='+profile,'-DBUILD_TESTING=ON','-DVOID_SANITIZERS=OFF','-DVOID_LLVM_RESEARCH=OFF','-DPython3_EXECUTABLE='+sys.executable]
def qualify(profile):
    # Release and Debug have separate output and temp roots.
    record={'status':'running','profile':profile,'platform':platform.platform(),'os_build':sys.getwindowsversion().build,'commands':[]}
    build=home/('VoidS7-G2-'+profile)
    temp=home/('VoidS7-G2-tmp-'+profile)
    filename=profile+'-result.json'
    def run(label,args,timeout=1800):
        argv=list(map(str,args));record['stage']=label;save(filename,record);start=time.monotonic()
        with (out/(profile+'-'+label+'.log')).open('wb') as log:
            process=subprocess.run(argv,cwd=source,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
        record['commands'].append({'label':label,'argv':argv,'returncode':process.returncode,'seconds':time.monotonic()-start})
        save(filename,record)
        assert process.returncode==0,(label,process.returncode)
    try:
        temp.mkdir(exist_ok=True)
        env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'),TEMP=str(temp),TMP=str(temp))
        env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH']
        env['VOID_CRASH_EVIDENCE']=str(temp/'crashes')
        run('configure',[cmake,'-S',source,'-B',build,'-G','Ninja',*options(profile)])
        targets=['voidc','voidls','void_tests','void_numeric_enum_ir','void_native_inventory',
                 'void_project_application','void_project_sessions','void_project_selection','void_project_manifest','void_project_creation',
                 'void_fixed_arrays_tests','void_async_tests','void_stage10_fields','void_project_header',
                 'void_g1_tests','void_g1_aot_normal','void_g1_aot_optimized','void_g1_aot_mismatch','void_native_tests',
                 'void_g2_tests','void_g2_aot_normal','void_g2_aot_optimized',
                 'void_native_aot_main_normal','void_native_aot_main_optimized','void_native_aot_terminated_nul_normal',
                 'void_native_aot_closed_optimized','void_native_aot_duplicate_normal']
        run('build',[cmake,'--build',build,'--parallel','2','--target',*targets])
        tests=r'^(ffi_g[12]_.*|native_source|native_aot_(main_(normal|optimized)|terminated_nul_normal|closed_optimized|duplicate_normal)|stage6_integrated|stage6_cache_processes|numeric|numeric_cli|numeric_enum_.*|fixed_arrays_source|async_ir|stage10_fields|frontend|modules|gc|reload|sessions|analysis|project_(manifest_header|manifest_no_authority|manifest_provenance|application|application_process|creation|creation_process|sessions|selection|profiles|protocol|manifest|manifest_legacy|discovery_cli|nested_cli))$'
        xml=out/(profile+'-ctest.xml')
        run('ctest',[ctest,'--test-dir',build,'--output-on-failure','-j','2','-R',tests,'--output-junit',xml],1200)
        for name in ('numeric-enum-process.json','numeric-enum-lsp.json','project-creation-evidence.json','project-application-evidence.json','project-protocol-evidence.json','project-discovery-evidence.json','project-profiles-evidence.json'):
            path=build/name
            if path.exists():shutil.copyfile(path,out/(profile+'-'+name))
        record['binaries']={n:sha(build/(n+'.exe')) for n in ['voidc','voidls','void_tests','void_numeric_enum_ir','void_project_application','void_project_sessions','void_project_creation']}
        run('g1-direct',[build/'void_g1_tests.exe',build/'ffi-g1/primitives.descriptor',
                         build/'ffi-g1/primitives.void',build/'ffi-g1/tests'])
        run('g2-direct',[build/'void_g2_tests.exe',build/'ffi-g2/records.descriptor',source/'tests/ffi/records.void'])
        for mode in ('normal','optimized'):
            run('g2-aot-'+mode,[sys.executable,source/'tests/ffi/records_process.py',build/('void_g2_aot_'+mode+'.exe')])
        for mode in ('normal','optimized','mismatch'):
            subprocess.run([str(build/('void_g1_aot_'+mode+'.exe')), '7'], cwd=build, env=env,
                stdout=(out/(profile+'-g1-aot-'+mode+'.log')).open('wb'), stderr=subprocess.STDOUT, timeout=30)
        file_names=['ffi-g1/primitives.descriptor','ffi-g1/primitives.void','ffi-g1/application-normal.obj',
                    'ffi-g1/application-optimized.obj','void_g1_tests.exe','void_g1_aot_normal.exe',
                    'void_g1_aot_optimized.exe','void_g1_aot_mismatch.exe']
        file_names += [str(x.relative_to(build)) for n in ('void_g1_thunk','void_g1_fixture','void_g1_wrong_fixture')
                       for x in (build/'CMakeFiles'/(n+'.dir')).rglob('*.obj')]
        record['g1_artifacts']={name:sha(build/name) for name in file_names}
        assert len(record['g1_artifacts'])==11
        file_names=['ffi-g2/records.descriptor','ffi-g2/application-normal.obj','ffi-g2/application-optimized.obj',
                    'void_g2_tests.exe','void_g2_aot_normal.exe','void_g2_aot_optimized.exe']
        file_names += [str(x.relative_to(build)) for n in ('void_g2_thunk','void_g2_fixture')
                       for x in (build/'CMakeFiles'/(n+'.dir')).rglob('*.obj')]
        record['g2_artifacts']={name:sha(build/name) for name in file_names}
        assert len(record['g2_artifacts'])==8
        shutil.copyfile(build/'ffi-g2/records.descriptor',out/(profile+'-records.descriptor'))
        abi = []
        fixture_hash = next(line.split()[1] for line in (build/'ffi-g2/records.descriptor').read_text().splitlines()
                            if line.startswith('fixture '))
        for part, definition in (('fixture', '-DG2_FIXTURE_IDENTITY="'+fixture_hash+'"'), ('thunk','-DG2_LLP64=1')):
            c_input=source/'tests/ffi'/('records_'+part+'.c')
            abi_out=build/'ffi-g2'/('records-'+part+'.ll')
            argv=[str(sdk/'bin/clang.exe'),'-target','x86_64-pc-windows-msvc','-std=c11','-O0','-S','-emit-llvm',
                  definition,'-I'+str(build/'ffi-g2'),str(c_input),'-o',str(abi_out)]
            run('g2-abi-'+part,argv)
            lines=[line for line in abi_out.read_text().splitlines() if
                   (line.startswith(('declare ','define ')) and '@g2_' in line) or line.startswith('target triple')]
            assert any('sret' in line and '@g2_packet' in line for line in lines)
            abi.append({'argv':argv,'C_IR_signatures':lines})
        save(profile+'-g2-c-abi.json',abi)
        for name in ('primitives.descriptor','primitives.void'):
            shutil.copyfile(build/'ffi-g1'/name,out/(profile+'-'+name))
        commands=json.loads((build/'compile_commands.json').read_text())
        save(profile+'-g1-c-compile-commands.json',[command for command in commands
            if Path(command['file']).name in ('primitives_thunk.c','primitives_fixture.c')])
        save(profile+'-g2-c-compile-commands.json',[command for command in commands
            if Path(command['file']).name in ('records_thunk.c','records_fixture.c')])
        record['status']='PASS'
    except BaseException:record['status']='FAIL';record['error']=traceback.format_exc()
    finally:
        for name in ('numeric-enum-process.json','numeric-enum-lsp.json','project-creation-evidence.json'):
            item=build/name
            if item.exists():shutil.copyfile(item,out/(profile+'-'+name))
        save(filename,record)
    return record

summary={'status':'running','free_bytes_before':shutil.disk_usage(home).free,'platform':platform.platform(),'scope':'Native Windows 10 targeted builds at the provider-attested source root. Original source files are backed up and restored. Build outputs and evidence use isolated paths.'}
save('native-results.json',summary)
overlays={}
created_dirs=[]
def parents(path):
    needed=[]
    while not path.exists():
        needed.append(path)
        path=path.parent
    for directory in reversed(needed):
        directory.mkdir()
        created_dirs.append(directory)

def alias(path):
    return path.is_symlink() or path.is_junction()

try:
    accepted=json.loads((out.parent/'project-slice7-g1/native-results.json').read_text())
    assert accepted['status']=='PASS' and accepted['restored_original_source']
    assert sys.getwindowsversion().build==19045
    assert not json.loads((out/'storage.json').read_text())['active_builds']
    hashes=json.loads((out/'source-packet-sha256.json').read_text())
    packet.mkdir(exist_ok=True)
    with zipfile.ZipFile(out/'source-packet.zip') as archive:
        assert set(archive.namelist())==set(hashes)
        for name,digest in hashes.items():
            target=packet/name
            assert target.is_relative_to(packet) and '..' not in Path(name).parts
            data=archive.read(name)
            assert hashlib.sha256(data).hexdigest()==digest,name
            assert not alias(target) and not any(alias(p) for p in target.parents if p.is_relative_to(packet)),name
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(data)
            assert sha(target)==digest,name

    backup.mkdir()
    assert shutil.disk_usage(home).free >= 12*1024**3, 'Need 12 GiB before starting both Windows builds'
    for name,digest in hashes.items():
        incoming=packet/name
        assert sha(incoming)==digest,name
        target=source/name
        assert not alias(target) and not any(alias(p) for p in target.parents if p.is_relative_to(source)),name
        originals=accepted['before_overlay_sha256']
        if name in originals:
            assert (sha(target) if target.exists() else None)==originals[name],name
        elif name in accepted['source_inputs_sha256']:
            assert sha(target)==accepted['source_inputs_sha256'][name],name
        else:
            assert not target.exists(),name
        if target.exists():
            assert target.is_file(),name
            previous=sha(target)
            if previous==digest:continue
            preserved=backup/name
            preserved.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(target,preserved)
            assert sha(preserved)==previous,name
        else:previous=None
        overlays[name]=previous
        parents(target.parent)
        shutil.copyfile(incoming,target)
        assert sha(target)==digest,name
    summary['source_inputs_sha256']=hashes
    summary['before_overlay_sha256']=overlays
    summary['source_packet_sha256']=sha(out/'source-packet.zip')
    summary['free_bytes_latest']=shutil.disk_usage(home).free
    save('native-results.json',summary)
    with ThreadPoolExecutor(max_workers=2) as pool:summary['profiles']=list(pool.map(qualify,('Release','Debug')))
    summary['status']='PASS' if all(p['status']=='PASS' for p in summary['profiles']) else 'FAIL'
except BaseException:summary['status']='FAIL';summary['error']=traceback.format_exc()
finally:
    try:
        for name,digest in reversed(list(overlays.items())):
            target=source/name
            if digest is None:target.unlink(missing_ok=True)
            else:
                shutil.copy2(backup/name,target)
                assert sha(target)==digest,name
        for directory in reversed(created_dirs):
            if not any(directory.iterdir()):directory.rmdir()
        summary['restored_original_source']=True
    except BaseException:
        summary['status']='FAIL'
        summary['restoration_error']=traceback.format_exc()
    summary['free_bytes_latest']=shutil.disk_usage(home).free
    save('native-results.json',summary)
