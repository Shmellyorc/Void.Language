"""Real project selection, legacy compatibility and independent native execution."""
from pathlib import Path
import json, os, subprocess, sys, tempfile

compiler=Path(sys.argv[1]).resolve()
evidence=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
checks=0
commands=[]
app="schema=1\n[project]\nid='org.example.sources'\nname='Example'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.example.app'\n[sources]\nroots=['src']\n"
lib="schema=1\n[project]\nid='org.example.sources'\nname='Example'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.example.library'\nname='example'\n[sources]\nroots=['src']\n"
main="func main() -> i32:\n    return 23\n"

def legacy(value):
    return str(value).encode()+ (b'\r\n' if os.name=='nt' else b'\n')

def run(args,status=0,stdout=None,contains=None,cwd=None):
    global checks
    p=subprocess.run(list(map(str,args)),capture_output=True,cwd=cwd,timeout=120)
    commands.append({'argv':list(map(str,args)),'cwd':str(cwd) if cwd else None,'status':p.returncode,'stdout':p.stdout.decode('utf-8','replace'),'stderr':p.stderr.decode('utf-8','replace')})
    assert p.returncode==status,(args,status,p.returncode,p.stdout,p.stderr)
    if stdout is not None:assert p.stdout==stdout,(args,p.stdout)
    if contains is not None:assert contains in p.stderr,(args,p.stderr)
    checks+=1
    return p

try:
 with tempfile.TemporaryDirectory(prefix='VOID project execution 雪 ') as temporary:
    root=Path(temporary)
    def write(name,text):
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,encoding='utf-8');return p
    manifest=write('void.project.toml',app)
    selected=write('src/app.void',main)
    # P01-P03: names have no priority; normal/O2 JIT and independent AOT.
    for name in ['app.void','program.void','game.void','features/launcher.void']:
        target=root/'src'/name;target.parent.mkdir(parents=True,exist_ok=True)
        if target!=selected:selected.rename(target);selected=target
        run([compiler,'check','--project',root])
        for flags in [[],['--no-opt']]:
            run([compiler,'run',root,'--no-cache',*flags],23,b'')
        run([compiler,'run','--no-cache'],23,b'',cwd=root)
        exe=root/'native'/(selected.stem+('.exe' if os.name=='nt' else ''))
        for flags in [[],['--no-opt']]:
            run([compiler,'build','--project',root,'-o',exe,*flags])
            run([exe],23,b'',cwd=root)
    # Rooted imports and selected-entry root scope; unrelated D semantics/globals.
    helper=write('src/helper.void','var local = 2\nfunc read() -> i32:\n    return root_number + local\n')
    write('src/unrelated.void','var number = initialize()\nfunc initialize() -> i32:\n    print("UNRELATED")\n    return 9\nfunc semantically_bad() -> i32:\n    return false\n')
    write('libs/helper/helper.void','func main() -> i32:\n    return 99\nfunc value() -> i32:\n    return 0\n')
    selected.write_text('use "/src/helper.void" as helper\nuse lib/helper as library\nconst root_number = 21\nfunc main() -> i32:\n    return helper.read() + library.value()\n')
    run([compiler,'run','--project',root,'--no-cache'],23,b'')
    exe=root/('graph-program.exe' if os.name=='nt' else 'graph-program')
    run([compiler,'build','--project',root,'-o',exe]);run([exe],23,b'')
    # P04/P08: an import does not remove another independently eligible seed.
    other=write('src/main.void',main)
    ambiguity=run([compiler,'run',root],1,contains=b'Ambiguous project entry')
    assert ambiguity.stderr.find(b'/src/features/launcher.void')<ambiguity.stderr.find(b'/src/main.void')
    selected.write_text('use "/src/main.void"\n'+main)
    run([compiler,'check',root],1,contains=b'Ambiguous project entry')
    # Explicit mode bypasses both D ambiguity and unrelated syntax errors.
    write('src/unrelated.void','func broken(:\n')
    run([compiler,'run','--project',root,'--entry','src/main.void','--no-cache'],23,b'')
    run([compiler,'run','--project',root,'--entry','src/missing.void'],1)
    run([compiler,'check','--project',root,'--entry','../outside.void'],1)
    run([compiler,'check','--project',root,'--entry','src/main.void',selected],2)
    run([compiler,'check',selected,'--entry','src/main.void'],2)
    other.unlink();selected.write_text(main);write('src/unrelated.void','func helper() -> i32:\n    return false\n')
    run([compiler,'run',root,'--no-cache'],23,b'')
    # P10/P11: only E gets semantic validation, D still gets syntax validation.
    selected.write_text('use "/src/unrelated.void"\n'+main)
    run([compiler,'check',root],1,contains=b'VOID2001')
    selected.write_text(main);write('src/unrelated.void','func broken(:\n')
    run([compiler,'check',root],1,contains=b'VOID1102')
    (root/'src/unrelated.void').unlink()
    helper.write_text('print("MUST NOT INITIALIZE")\n');selected.write_text('use "/src/helper.void"\n'+main)
    run([compiler,'check',root],1,contains=b'VOID4001');selected.write_text(main)
    # Excluded imports and unimported eager globals remain ordinary E behavior.
    manifest.write_text(app+"exclude=['src/scratch']\n")
    write('src/scratch/helper.void','var count = initialize()\nfunc initialize() -> i32:\n    print("IMPORTED")\n    return 23\nfunc main() -> i32:\n    return 99\n')
    run([compiler,'run',root,'--no-cache'],23,b'')
    selected.write_text('use "/src/scratch/helper.void" as helper\nfunc main() -> i32:\n    return helper.count\n')
    for flags in [[],['--no-opt']]:run([compiler,'run',root,'--no-cache',*flags],23,b'IMPORTED\n')
    run([compiler,'build','--project',root,'-o',exe]);run([exe],23,b'IMPORTED\n')
    # Default output belongs to the manifest root, not the nested entry directory.
    run([compiler,'build',root]);default=root/'build'/('launcher.exe' if os.name=='nt' else 'launcher');assert default.is_file();run([default],23,b'IMPORTED\n')
    # Nested malformed manifests still exclude children and reject E crossings.
    write('src/child/void.project.toml','[=roject]');write('src/child/child.void',main)
    run([compiler,'check',root]);run([compiler,'run','--project',root,'--entry','src/child/child.void'],1,contains=b'VOID5009')
    selected.write_text('use "/src/child/child.void"\n'+main)
    run([compiler,'check',root],1,contains=b'VOID5009')
    selected.write_text(main)
    # Explicit project scripts/legacy entries, argument forwarding and output.
    write('tools/script.void','print("SCRIPT")\n')
    for flags in [[],['--no-opt']]:run([compiler,'run','--project',root,'--entry','tools/script.void','--no-cache',*flags],0,b'SCRIPT\n')
    write('tools/legacy.void','func entry(value: i32) -> i32:\n    return value + 2\n')
    run([compiler,'run','--project',root,'--entry','tools/legacy.void','--','9'],0,legacy(11))
    write('tools/args.void','use os\nfunc main():\n    if os.args() is Ok(var values):\n        for arg in values:\n            print(arg)\n')
    run([compiler,'run','--project',root,'--entry','tools/args.void','--','--app','two words'],0,b'--app\ntwo words\n')
    # Manifest override wins over scanning, CLI override wins over authored entry.
    manifest.write_text(app.replace("kind='application'","kind='application'\nentry='tools/script.void'"))
    run([compiler,'run',root,'--no-cache'],0,b'SCRIPT\n')
    run([compiler,'run','--project',root,'--entry','tools/legacy.void','--','5'],0,legacy(7))
    manifest.write_text(app.replace("kind='application'","kind='application'\nentry='tools/missing.void'"))
    run([compiler,'run',root],1)
    # Automatic missing main fails even check; bare explicit check stays valid.
    (root/'src/scratch/helper.void').unlink()
    manifest.write_text(app);selected.write_text('func helper():\n    pass\n')
    for command in ['check','run','build']:run([compiler,command,root],1,contains=b'No eligible main')
    run([compiler,'check',selected])
    # Library check union has no implicit root privilege and no execution entry.
    manifest.write_text(lib);helper.write_text('func helper():\n    pass\n')
    run([compiler,'check','--project',root])
    for command in ['run','build','emit']:
        options=['-o',root/'library.o'] if command=='emit' else []
        run([compiler,command,'--project',root,*options],1,contains=b'unsupported')
    run([compiler,'check','--project',root,'--entry','src/helper.void'],2)
    # Cached selection must revalidate D before any hit or application execution.
    manifest.write_text(app);selected.write_text(main)
    cold=run([compiler,'run','--project',root,'--cache'],23,b'')
    warm=run([compiler,'run','--project',root,'--cache'],23,b'')
    cache_supported=b'Debug cache: unavailable' not in cold.stderr
    if cache_supported:assert b'Debug cache: miss' in cold.stderr and b'Debug cache: hit' in warm.stderr
    else:print('UNQUALIFIED: native persistent disk cache; project selection rejection is still exercised')
    other=write('src/second.void',main);run([compiler,'run','--project',root,'--cache'],1,contains=b'Ambiguous')
    other.unlink();manifest.write_text(app.replace("name='Example'","name='Changed'"));changed=run([compiler,'run','--project',root,'--cache'],23,b'')
    if cache_supported:assert b'Debug cache: miss' in changed.stderr
    manifest.write_text(app+"[build.debug]\ndebug_info='line'\n");changed=run([compiler,'run','--project',root,'--cache'],23,b'')
    if cache_supported:assert b'Debug cache: miss' in changed.stderr
    manifest.write_text(app.replace("id='org.example.sources'","id='org.example.other_sources'"));changed=run([compiler,'run','--project',root,'--cache'],23,b'')
    if cache_supported:assert b'Debug cache: miss' in changed.stderr
    # Bare explicit file ignores adjacent invalid manifest; directory fails closed.
    manifest.write_text('[=roject]')
    run([compiler,'run',selected,'--no-cache'],23,b'')
    run([compiler,'check',root],1,contains=b'VOID5001')
    wrong_case=root/'Void.Project.toml';manifest.rename(wrong_case);write('main.void',main)
    run([compiler,'check',root],1,contains=b'VOID5009');wrong_case.rename(manifest)
    manifest.unlink();write('main.void',main)
    run([compiler,'check','--project',root],1,contains=b'VOID5009')
    run([compiler,'run',root,'--no-cache'],23,b'')
    (root/'main.void').unlink();run([compiler,'run',root],1,contains=b'main.void')
    run([compiler,'run',selected,'--no-cache'],23,b'')
    # Existing explicit legacy precedence stays exact beside project context.
    write('tools/legacy.void','func main() -> i32:\n    return 99\nfunc entry(value: i32) -> i32:\n    return value+1\n')
    run([compiler,'run',root/'tools/legacy.void','3'],0,legacy(4))
    write('void.project.toml',app.replace("kind='application'","kind='application'\nentry='tools/legacy.void'"))
    run([compiler,'run',root,'4'],0,legacy(5))
    manifest.write_text(app);selected.write_text('func main() -> i32:\n    return 99\nfunc entry(value: i32) -> i32:\n    return value+1\n')
    run([compiler,'run','--project',root,'--','4'],0,legacy(5))
 print(f'project_discovery_cli: {checks} process checks passed')
finally:
 if evidence:
    evidence.parent.mkdir(parents=True,exist_ok=True)
    evidence.write_text(json.dumps({'platform':sys.platform,'checks_completed':checks,'commands':commands},indent=2)+'\n')
