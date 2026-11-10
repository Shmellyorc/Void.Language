from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback, zipfile

share = Path(r'\\vboxsvr\VM\slice7-g5\compiler-final')
local = Path.home() / 'VoidS7-G5-Compiler'
packet = local / 'source-packet-final'
baseline = Path.home() / 'VoidCI42377496702-Native'
source = baseline
sdk = baseline / '.toolchains/llvm-23.1.4/bin'
cmake = Path(r'C:\gcc\bin\cmake.exe')
ctest = cmake.with_name('ctest.exe')
assert sys.getwindowsversion().build == 19045
assert shutil.disk_usage(local).free > 12 * 1024**3
packet.mkdir(exist_ok=True)
hashes = json.loads((share/'hashes.json').read_text())
archive_path = local/'source-final.zip'
shutil.copyfile(share/'source.zip', archive_path)
with zipfile.ZipFile(archive_path) as archive:
    assert set(archive.namelist()) == set(hashes)
    for name, digest in hashes.items():
        data = archive.read(name)
        assert hashlib.sha256(data).hexdigest() == digest, name
        path = packet/name
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_bytes(data)

def qualify(profile):
    build = local/profile
    temp = local/(profile+'-tmp')
    temp.mkdir(exist_ok=True)
    record = dict(status='running', profile=profile, platform=platform.platform(), build=sys.getwindowsversion().build,
                  free_bytes_before=shutil.disk_usage(local).free, commands=[])
    def save():
        (share/(profile+'-result.json')).write_text(json.dumps(record,indent=2)+'\n')
    env = dict(os.environ, TEMP=str(temp), TMP=str(temp), VOID_CLANG=str((sdk/'clang.exe').resolve(strict=True)),
               PATH=str(sdk)+os.pathsep+os.environ['PATH'], VOID_G4_EVIDENCE_ROOT=str(local/(profile+'-g4-final-artifacts')),
               VOID_G5_EVIDENCE_ROOT=str(local/(profile+'-g5-final-artifacts')))
    def run(label, args, timeout=3600):
        argv = list(map(str,args))
        record['stage'] = label
        save()
        start = time.monotonic()
        with (share/(profile+'-'+label+'.log')).open('wb') as log:
            process = subprocess.run(argv, cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        record['commands'].append(dict(label=label, argv=argv, returncode=process.returncode, seconds=time.monotonic()-start))
        save()
        assert process.returncode == 0, (label, process.returncode)
    try:
        cache=(share.parent.parent/'project-slice3'/(profile+'-CMakeCache.txt')).read_text()
        lookup=lambda key:re.search(r'^'+re.escape(key)+r':[^=]+=(.*)$',cache,re.M)[1]
        keys=('CMAKE_AR','CMAKE_CXX_COMPILER','CMAKE_C_COMPILER','CMAKE_LINKER','CMAKE_MAKE_PROGRAM',
              'CMAKE_MT','CMAKE_RC_COMPILER','LLVM_DIR','ZLIB_INCLUDE_DIR','ZLIB_LIBRARY_RELEASE','zstd_DIR','LibXml2_DIR',
              'VOID_HOST_DEPS_ROOT','VOID_HTTP_DEPS_ROOT')
        options=['-D'+key+'='+lookup(key) for key in keys]
        clang=Path(lookup('VOID_CLANG')).resolve(strict=True)
        assert hashlib.sha256(clang.read_bytes()).digest() == hashlib.sha256((sdk/'clang.exe').read_bytes()).digest()
        env['VOID_CLANG']=str(clang)
        run('configure',[cmake,'-S',source,'-B',build,'-G','Ninja',*options,
            '-DVOID_CLANG='+str(clang),'-DCMAKE_BUILD_TYPE='+profile,'-DBUILD_TESTING=ON','-DVOID_SANITIZERS=OFF',
            '-DVOID_LLVM_RESEARCH=OFF','-DPython3_EXECUTABLE='+sys.executable])
        targets=['voidc','void_g5_reload','void_runtime_callbacks','void_tests','void_g1_tests','void_g1_aot_normal','void_g1_aot_optimized','void_g1_aot_mismatch',
                 'void_g2_tests','void_g2_aot_normal','void_g2_aot_optimized',
                 'void_g3_tests','void_g3_aot_normal','void_g3_aot_optimized','void_project_manifest']
        run('build',[cmake,'--build',build,'--parallel','2','--target',*targets])
        xml = share/(profile+'-regression.xml')
        test_args = [ctest,'--test-dir',build,'--output-on-failure','-j','1','-R',
            r'^(ffi_g[12345]_.*|runtime_callbacks|gc|reload|analysis|project_manifest|project_cli|cli_contract)$',
            '--output-junit',xml]
        for attempt in (1, 2):
            try:
                run('regression', test_args)
                break
            except AssertionError:
                import xml.etree.ElementTree as ET
                failed = [case.get('name') for case in ET.parse(xml).getroot().findall('testcase')
                          if case.find('failure') is not None]
                failure = (share/(profile+'-regression.log')).read_text(encoding='utf-8',errors='replace')
                if attempt == 2 or not (failed == ['cli_contract'] and 'PermissionError: [WinError 5]' in failure
                                        or failed == ['analysis'] and 'concurrent call/lease failure' in failure):
                    raise
                shutil.copyfile(xml, share/(profile+'-parallel-first.xml'))
                shutil.copyfile(share/(profile+'-regression.log'), share/(profile+'-parallel-first.log'))
                shutil.copyfile(build/'Testing/Temporary/LastTest.log', share/(profile+'-parallel-first-LastTest.log'))
        record['status']='PASS'
    except BaseException:
        record['status']='FAIL'
        record['error']=traceback.format_exc()
    finally:
        last = build/'Testing/Temporary/LastTest.log'
        if record['stage'] == 'regression' and last.is_file():
            shutil.copyfile(last, share/(profile+'-LastTest.log'))
        record['free_bytes_after']=shutil.disk_usage(local).free
        save()
    return record['status']

times_path = share/'reusable-input-times.json'
prior_times = json.loads(times_path.read_text()) if times_path.is_file() else {}
reused_times = []
originals = {}
created_dirs = []
backup = local/'backup-final'
backup.mkdir()
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def save_overlay(status, **extra):
    (share/'overlay.json').write_text(json.dumps(dict(status=status, originals=originals, **extra), indent=2)+'\n')
try:
    for name, incoming in hashes.items():
        origin = packet/name
        assert digest(origin) == incoming
        target = source/name
        assert target.is_relative_to(source) and '..' not in Path(name).parts
        assert not any(p.is_symlink() or p.is_junction() for p in [target, *target.parents] if p.is_relative_to(source))
        previous = digest(target) if target.exists() else None
        if previous == incoming:
            continue
        if previous is not None:
            preserved = backup/name
            preserved.parent.mkdir(exist_ok=True, parents=True)
            shutil.copy2(target, preserved)
            assert digest(preserved) == previous
        originals[name] = previous
        parent = target.parent
        pending = []
        while not parent.exists():
            pending.append(parent)
            parent = parent.parent
        for parent in reversed(pending):
            parent.mkdir()
            created_dirs.append(parent)
        shutil.copyfile(origin, target)
        assert digest(target) == incoming
        if name in prior_times and prior_times[name]['sha256'] == incoming:
            time_ns = prior_times[name]['mtime_ns']
            os.utime(target, ns=(target.stat().st_atime_ns, time_ns))
            assert target.stat().st_mtime_ns == time_ns
            reused_times.append(name)
    save_overlay('active', input_count=len(hashes))
    with ThreadPoolExecutor(2) as executor:
        status=list(executor.map(qualify, ('Release','Debug')))
finally:
    restored = {}
    for name, previous in reversed(list(originals.items())):
        target = source/name
        if previous is None:
            assert digest(target) == hashes[name]
            target.unlink()
            assert not target.exists()
        else:
            assert digest(target) == hashes[name]
            shutil.copy2(backup/name, target)
            assert digest(target) == previous
        restored[name] = previous
    for parent in reversed(created_dirs):
        if not any(parent.iterdir()):
            parent.rmdir()
    save_overlay('restored', restored=restored, reused_times=reused_times, free_bytes=shutil.disk_usage(local).free)
sys.exit(0 if all(s=='PASS' for s in status) else 1)
