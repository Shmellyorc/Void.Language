"""Real host executables, automatic dispatch, transactional output and private install."""
import os, pathlib, subprocess, sys, tempfile, time
from windows_process import windows_application_path
compiler=pathlib.Path(sys.argv[1]).resolve()
build=pathlib.Path(sys.argv[2]).resolve()
configuration=sys.argv[3]
checks=0

def run(args,status=0,stdout=None,cwd=None,diagnostic=None,env=None,executable=None):
    global checks
    p=subprocess.run([str(a) for a in args],cwd=cwd,capture_output=True,text=True,encoding='utf-8',timeout=90,env=env,executable=executable)
    checks+=1
    assert p.returncode==status,(args,p.returncode,p.stdout,p.stderr)
    if stdout is not None: assert p.stdout==stdout,(args,p.stdout,p.stderr)
    if diagnostic: assert diagnostic in p.stderr,(args,p.stdout,p.stderr)
    return p

if os.name=='nt':
    # Native helper shares the production implementation and must run in both
    # Windows configurations; the existing 160-suite inventory is retained.
    assert len(sys.argv)==5, 'Windows staging probe missing'
    # Retain the native rename/identity observations in workflow evidence.
    print(run([sys.argv[4]]).stdout,end='')

with tempfile.TemporaryDirectory(prefix='VOID Stage9 é 🎮 # % ') as temporary:
    root=pathlib.Path(temporary);project=root/'game # % é';project.mkdir()
    unrelated=root/'unrelated';unrelated.mkdir();source=project/'main.void'
    suffix='.exe' if os.name=='nt' else ''
    def write(text):source.write_text(text,encoding='utf-8')
    def clean():assert not list(project.rglob('.void-build*'))
    samples=[('return 7\n',7),('func main():\n    pass\n',0),
      ('func main() -> i32:\n    var xs: [string] = ["A🎮", "yes"]\n    if xs[-1] == "yes":\n        return 9\n    return 4\n',9),
      ('struct node:\n    text: string\nfunc main() -> i32:\n    var x = node(text = "yes")\n    var map: {string:i32} = {"yes": 13}\n    return map[x.text]\n',13)]
    for text,status in samples:
        write(text)
        for mode in [[],['--no-opt']]:
            jit=run([compiler,'run',source,*mode],status,'',unrelated)
            run([compiler,'build',project,*mode],cwd=unrelated)
            executable=project/'build'/('main'+suffix)
            native=run([executable],status,'',unrelated)
            assert native.stderr==jit.stderr==''
            run([executable,'1'],2,'')
    write('return 7\n')
    run([compiler,'run'],7,'',project);run([compiler,'run','.'],7,'',project)
    run([compiler,'build'],cwd=project);run([compiler,'build','.'],cwd=project)
    alternate=project/'game.void';alternate.write_text('return 11\n',encoding='utf-8')
    run([compiler,'run',alternate],11,'',unrelated)
    run([compiler,'build',alternate],cwd=unrelated)
    run([project/'build'/('game'+suffix)],11,'')
    # Caller-relative override and a percent-containing output filename.
    output=project/('output # % é'+suffix)
    run([compiler,'build','--no-opt',source,'-o',pathlib.Path('..')/project.name/output.name],cwd=unrelated)
    run([output],7,'')
    # Native applications depend on Runtime/platform libraries, never LLVM.
    if sys.platform.startswith('linux'):
        dependencies=run(['ldd',output]).stdout
        assert 'LLVM' not in dependencies,dependencies
    elif sys.platform=='darwin':
        dependencies=run(['otool','-L',output]).stdout
        assert 'LLVM' not in dependencies,dependencies
    # Physical case semantics are verified on both case-sensitive and insensitive hosts.
    run([compiler,'run',project/'MAIN.void'],1)
    exact=project/('CaseArtifact'+suffix);exact.write_bytes(b'original case artifact')
    other=project/('caseartifact'+suffix)
    if other.exists():
        run([compiler,'build',source,'-o',other],1,diagnostic='incorrect spelling')
    else:
        run([compiler,'build',source,'-o',other]);run([other],7,'')
    assert exact.read_bytes()==b'original case artifact'
    # Observe a real build's intermediates, proving percent parents are retained.
    child=subprocess.Popen([str(compiler),'build',str(source),'-o',str(output)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
    seen=False
    while child.poll() is None:
        for stage in project.glob('.void-build-*'):
            assert stage.parent==output.parent,(stage,output)
            seen=True
        time.sleep(.001)
    out,err=child.communicate(timeout=10);checks+=1
    assert child.returncode==0 and seen,(child.returncode,seen,out,err)
    before=output.read_bytes()
    for target in [source,alternate]:
        saved=target.read_bytes()
        run([compiler,'build',source,'-o',target],1,diagnostic='collision')
        run([compiler,'emit',source,'-o',target],1,diagnostic='collision')
        assert target.read_bytes()==saved
    # Preserve project-local import identities through the new native command.
    local=project/'libs/raylib';local.mkdir(parents=True)
    (local/'raylib.void').write_text('func value() -> i32:\n    return 7\n',encoding='utf-8')
    write('use lib/raylib as local\nreturn local.value()\n')
    run([compiler,'build',source,'-o',output]);run([output],7,'')
    run([compiler,'build',source,'-o',local/'raylib.void'],1,diagnostic='collision')
    library=project/'library.void';library.write_text('func value() -> i32:\n    return 7\n',encoding='utf-8')
    write('use "library.void" as library\nfunc main() -> i32:\n    return library.value()\n')
    run([compiler,'build',source,'-o',library],1,diagnostic='collision')
    alias=project/'source-alias'
    os.link(library,alias)
    run([compiler,'build',source,'-o',alias],1,diagnostic='collision')
    write('func main() -> i32:\n    var xs: [i32] = []\n    return xs[0]\n')
    for mode in [[],['--no-opt']]:
        jit=run([compiler,'run',source,*mode],1,'')
        run([compiler,'build',source,'-o',output,*mode])
        native=run([output],1,'')
        assert native.stderr==jit.stderr,(native.stderr,jit.stderr)
        assert 'main.void:' in native.stderr and 'index' in native.stderr
    # Source failure preserves an existing executable and object byte for byte.
    if os.name=='nt':
        import ctypes, concurrent.futures
        from ctypes import wintypes
        # Control the real linker root, retaining Unicode/spaces/#/% literally.
        linkroot=root/'native link é # %';linkroot.mkdir()
        environment=dict(os.environ,TMP=str(linkroot),TEMP=str(linkroot))
        def native_build(index):
            target=project/('parallel-'+str(index)+'.exe')
            run([compiler,'build',source,'-o',target],env=environment)
            run([target],7,'')
        write('return 7\n')
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(native_build,range(4)))
        assert not list(linkroot.iterdir()), 'short linking directories leaked'
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,
                                    wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
        kernel.CreateFileW.restype=wintypes.HANDLE
        kernel.CloseHandle.argtypes=[wintypes.HANDLE]
        locked=kernel.CreateFileW(str(output),0x80000000,1,None,3,0,None)
        assert locked!=ctypes.c_void_p(-1).value, ctypes.get_last_error()
        before=output.read_bytes()
        try:
            run([compiler,'build',source,'-o',output],1,diagnostic='Cannot publish',env=environment)
            assert output.read_bytes()==before
        finally: assert kernel.CloseHandle(locked)
        assert not list(linkroot.iterdir()), 'failed publication leaked link directory'
        clean()
        import _winapi
        junction=root/'temporary-junction'
        _winapi.CreateJunction(str(linkroot),str(junction))
        try:
            redirected=dict(environment,TMP=str(junction),TEMP=str(junction))
            run([compiler,'build',source,'-o',output],1,diagnostic='reparse point',env=redirected)
            assert output.read_bytes()==before and not list(linkroot.iterdir())
            clean()
        finally: junction.rmdir()
        # Reject a complete root that cannot fit a short leaf plus program.exe.
        longenv=dict(environment,TMP=str(project),TEMP=str(project))
        too_long=project
        while len(str(too_long).encode('utf-16-le'))//2<240:
            too_long=too_long/('long-temp-'+'x'*25)
        too_long.mkdir(parents=True)
        longenv.update(TMP=str(too_long),TEMP=str(too_long))
        run([compiler,'build',source,'-o',output],1,diagnostic='too long',env=longenv)
        assert output.read_bytes()==before and not list(too_long.iterdir())
        clean()
    saved=output.read_bytes();obj=project/'saved.o';obj.write_bytes(b'previous object')
    write('func main() -> i32:\n    if false:\n        return 1\n')
    for command,target in [('build',output),('emit',obj)]:
        run([compiler,command,source,'-o',target],1,diagnostic='Missing return')
    assert output.read_bytes()==saved and obj.read_bytes()==b'previous object'
    # Replacement behavior is exercised while the destination executable runs.
    write('for true:\n    pass\n')
    run([compiler,'build',source,'-o',output])
    running_bytes=output.read_bytes()
    process=subprocess.Popen([str(output)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        write('return 7\n')
        replacement=subprocess.run([str(compiler),'build',str(source),'-o',str(output)],capture_output=True,text=True,encoding='utf-8',timeout=90);checks+=1
        assert replacement.returncode in ([0,1] if os.name=='nt' else [0]),replacement.stderr
        if replacement.returncode:
            assert 'Cannot publish' in replacement.stderr,replacement.stderr
            assert output.read_bytes()==running_bytes
        else:run([output],7,'')
    finally:
        process.terminate();process.wait(timeout=20)
    run([compiler,'build',source,'-o',output]);run([output],7,'')
    # Ordinary non-source symlink destinations must not redirect publication.
    sentinel=project/'sentinel';sentinel.write_bytes(b'keep me')
    symlink=project/'redirect'
    try:symlink.symlink_to(sentinel)
    except OSError as error:
        if os.name!='nt' or getattr(error,'winerror',None)!=1314:raise
        print('Windows symlink creation unavailable: privilege 1314; existing reparse gates remain mandatory.')
    else:
        run([compiler,'build',source,'-o',symlink],1,diagnostic='ordinary file')
        assert sentinel.read_bytes()==b'keep me'
    legacy='func main() -> i32:\n    return 99\nfunc entry(x: i32) -> i32:\n    return x + 2\n'
    write(legacy);run([compiler,'build',source,'-o',output])
    for value in [0,5,-7]:
        run([compiler,'run',source,value],0,str(value+2)+'\n')
        run([output,value],0,str(value+2)+'\n')
    for bad in ['nope',2147483648,-2147483649]:
        run([compiler,'run',source,bad],2);run([output,bad],2)
    for command in ['check','run','emit','build']:
        run([compiler,command,source,'--app'],2,diagnostic='removed')
    for args in [['build','--cache'],['emit',source],['build','-o'],['run',source,'--what'],['check'],['run',source,'--no-opt','--no-opt']]:
        run([compiler,*args],2)
    write('return 7\n');run([compiler,'run',source,'4'],2,diagnostic='do not accept')
    write('func helper() -> i32:\n    return 4\n');run([compiler,'check',source]);run([compiler,'build',source],1)
    write('use "library.void" as library\nreturn library.value()\n')
    library.write_text('return 1\n',encoding='utf-8');run([compiler,'build',source],1,diagnostic='Imported modules')
    library.write_text('func main() -> i32:\n    return 6\n',encoding='utf-8')
    write('use "library.void" as library\n');run([compiler,'check',source]);run([compiler,'build',source],1)
    denied=pathlib.Path(__file__).resolve().parents[2]/'examples/native_sink/main.void'
    run([compiler,'build',denied,'-o',output],1)
    # Real long physical paths stay in the selected project, without a clean-dir fallback.
    deep=project
    for n in range(6):deep=deep/('nested-'+str(n)+'-'+'x'*36)
    deep.mkdir(parents=True)
    longsource=deep/'main.void';longsource.write_text('return 7\n',encoding='utf-8')
    assert len(str(longsource)) > 260, longsource
    run([compiler,'build',longsource],cwd=unrelated)
    deep_executable=deep/'build'/('main'+suffix)
    application=None
    if os.name=='nt':
        assert len(str(deep_executable).encode('utf-16-le'))//2>260,deep_executable
        application=windows_application_path(deep_executable)
        assert os.path.samefile(deep_executable,application),(deep_executable,application)
    run([deep_executable],7,'',executable=application)
    if os.name=='nt':
        print('Deep executable launched via explicit extended application:',ascii(application))
    # Real staged install, then relocation. Native config must use its own archives.
    prefix=root/'private install # % é'
    run(['cmake','--install',build,'--config',configuration,'--component','compiler','--prefix',prefix])
    moved=root/'relocated # % é';prefix.rename(moved)
    installed=moved/'bin'/compiler.name;config=installed.parent/'void-native.txt'
    lines=config.read_text(encoding='utf-8').splitlines()
    assert lines[3].startswith('../lib/void/') and lines[4].startswith('../lib/void/')
    write('return 7\n');run([installed,'build',source,'-o',output]);run([output],7,'')
    saved=output.read_bytes()
    for row,change,diagnostic in [(1,str(root/'absent clang'),'resource missing'),(3,str(root/'absent runtime'),'resource missing'),(0,'invalid','resources missing'),(1,str(installed),'version check failed')]:
        broken=lines.copy();broken[row]=change;config.write_text('\n'.join(broken)+'\n',encoding='utf-8')
        run([installed,'build',source,'-o',output],1,diagnostic=diagnostic,env=environment if os.name=='nt' else None)
        assert output.read_bytes()==saved
        if os.name=='nt': assert not list(linkroot.iterdir())
    # Deliberate real linker failure; old output and temporary cleanup survive.
    config.write_text('\n'.join(lines+['-lvoid_stage9_deliberately_missing'])+'\n',encoding='utf-8')
    run([installed,'build',source,'-o',output],1,diagnostic='Native linker failed',env=environment if os.name=='nt' else None)
    if os.name=='nt': assert not list(linkroot.iterdir())
    assert output.read_bytes()==saved
    if os.name=='nt':
        fake=lines.copy();fake[1]=str(pathlib.Path(sys.argv[4]).resolve())
        for flags,diagnostic in [([], 'Invalid native executable'),
                (['--void-test-copy-collision='+str(project)], 'Cannot create native artifact copy')]:
            config.write_text('\n'.join(fake+flags)+'\n',encoding='utf-8')
            run([installed,'build',source,'-o',output],1,diagnostic=diagnostic,env=environment)
            assert output.read_bytes()==saved
            assert not list(linkroot.iterdir())
            clean()
    config.unlink();run([installed,'build',source,'-o',output],1,diagnostic='resources missing')
    assert output.read_bytes()==saved
    clean()
print(f'Stage 9 real workflow checks passed: {checks}')
