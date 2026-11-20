"""G87: selected graph, original locations, project features, JIT/AOT and cache."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from test_output import output_directory
compiler,report=(Path(p).resolve() for p in sys.argv[1:]);records=[];inputs={}
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args,status=0,output=None,error=None):
    args=list(map(str,args));r=subprocess.run(args,capture_output=True,text=True,timeout=120,preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=args,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr);records.append(row)
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576,compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),inputs=inputs,records=records),indent=2)+'\n')
    assert r.returncode==status,row
    if output is not None:assert r.stdout==output,row
    if error:assert error.lower() in r.stderr.lower() and re.search(r'\.(void|toml):\d+:\d+:',r.stderr),row
    assert not any(x in r.stderr for x in ('AddressSanitizer','runtime error:','Assertion','Missing owned')),row
    return r
MANIFEST="schema=1\n[project]\nid='org.test.conditional'\nname='Conditional'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.conditional.app'\n"
CASES={
'basic':('''compile if true:
    func selected() -> i32:
        return 17
compile else if false:
    func selected() -> string:
        return "wrong"
compile else:
    use "./missing.void"
    use pkg/not_declared
    use lib/absent
    use absent_standard_module
    deliberately $ invalid "
func main():
    print(selected())
''','17\n'),
'nested':('''func main():
    compile if false:
        use "./absent.void"
        compile if true:
            $ invalid
        compile else:
            ?! invalid
    compile else:
        compile if not false and (true or false):
            var shared = 19
        print(shared)
    if true:
        compile if true:
            print("body")
    else:
        print("wrong")
''','19\nbody\n'),
'globals':('''func initialize() -> i32:
    print("initialized")
    return 5
compile if false:
    var hidden = initialize()
    static var bad = missing()
compile else:
    var visible = initialize()
func main():
    print(visible)
''','initialized\n5\n'),
'generics':('''compile if true:
    interface readable<t>:
        func read() -> t
    value struct box<t>:
        item: t
    func (self box<t>) read() -> t:
        compile if true:
            return self.item
        compile else:
            return missing
    func read<t: readable<i32>>(value: t) -> i32:
        return value.read()
compile else:
    value struct box<t>:
        broken: missing
func main():
    var value = box<i32>(item = 23)
    print(read(value))
''','23\n'),
'members':('''struct managed:
    compile if true:
        value: i32
    compile else:
        value: string
    compile if true:
        static const selected = 29
func (self managed) init():
    compile if true:
        self.value = 31
    compile else:
        self.missing = "bad"
enum label:
    compile if true:
        active
    compile else:
        inactive(missing)
func main():
    var m = managed()
    print(m.value, managed.selected, label.active == label.active)
''','31 29 true\n'),
'multiline':('''compile if true:
    func selected() -> string:
        return """literal
compile if made_up:
compile else:
text"""
func main():
    print(selected())
''','literal\ncompile if made_up:\ncompile else:\ntext\n'),
'continuation':('''compile if true:
    func selected() -> string:
        return if true:
            "yes"
        else:
            "no"
func main():
    compile if true:
        print(
"argument",
            selected()
        )
''','argument yes\n'),
}
NEGATIVE={
'located':('compile if true:\n    func main():\n        missing()\n','unknown'),
'unknown':('compile if made_up:\n    pass\n','Unknown compile condition'),
'unknown_inactive':('compile if false:\n    compile if feature.missing:\n        pass\n','Unknown build condition'),
'validate_rhs':('compile if true or feature.typo:\n    pass\n','Unknown build condition'),
'bad_os':('compile if target_os == "linxu":\n    pass\n','Unknown target_os'),
'bad_arch':('compile if target_arch == "arm64":\n    pass\n','Unknown target_arch'),
'orphan':('compile else:\n    pass\n','no matching'),
'double_else':('compile if true:\n    pass\ncompile else:\n    pass\ncompile else:\n    pass\n','cannot follow'),
'empty':('compile if false:\ncompile else:\n    pass\n','requires an indented'),
'empty_eof':('compile if false:\n','requires an indented'),
'missing_colon':('compile if true\n    pass\n','Expected compile if'),
'incomplete':('compile if (true and):\n    pass\n','Unknown compile condition'),
'conflict':('compile if true:\n    func same():\n        pass\ncompile if true:\n    func same():\n        pass\n','duplicate'),
'inactive_hidden':('compile if false:\n    value struct hidden:\n        value: i32\nfunc main():\n    var x = hidden()\n','unknown'),
'bad_indent':('compile if true:\n    pass\n  pass\n','indentation'),
'continued_directive':('func main():\n    print(\n        compile if true:\n            "bad"\n    )\n','source-region boundary'),
}
NEGATIVE['predicate_depth']=('compile if '+'not '*65+'true:\n    pass\n','resource limit')
NEGATIVE['predicate_size']=('compile if '+' and '.join(['true']*513)+':\n    pass\n','resource limit')
NEGATIVE['region_depth']=(''.join('    '*n+'compile if true:\n' for n in range(65))+'    '*65+'pass\n','nesting limit')
CASES['region_limit']=(''.join('    '*n+'compile if true: # bounded region\n' for n in range(64))+'    '*64+'func main():\n'+'    '*65+'print("bounded")\n','bounded\n')
with tempfile.TemporaryDirectory(prefix='conditional-',dir=output_directory()) as directory:
    root=Path(directory)
    for name,(source,expected) in CASES.items():
        path=root/(name+'.void');path.write_text(source);inputs[name]=source
        for flags in (['--no-opt'],[]):run([compiler,'run',path,'--no-cache',*flags],output=expected)
        for profile in ('debug','release'):
            # A standalone build uses --no-opt for O0.
            exe=root/(name+'-'+profile+('.exe' if os.name=='nt' else ''))
            run([compiler,'build',path,'-o',exe,*(['--no-opt'] if profile=='debug' else [])]);run([exe],output=expected)
    for name,(source,error) in NEGATIVE.items():
        path=root/(name+'.void');path.write_text(source);inputs[name]=source
        result=run([compiler,'check',path],1,error=error)
        if name=='bad_os':assert ':1:25:' in result.stderr,result.stderr
        if name=='located':assert ':3:9:' in result.stderr,result.stderr
    # Source comments, CRLF, BOM and tabs keep authored byte locations.
    path=root/'tabs.void';path.write_bytes(b'\xef\xbb\xbf# compile if typo:\r\ncompile if true:\r\n\tfunc main():\r\n\t\tprint("tabs")\r\ncompile else:\r\n\t$ wrong\r\n')
    run([compiler,'run',path,'--no-cache'],output='tabs\n')
    project=root/'project';project.mkdir();manifest=project/'void.project.toml';main=project/'launcher.void'
    source='''compile if feature.extra:
    use "./enabled.void" as api
    func main():
        print(api.value())
compile else:
    use "./disabled.void" as api
    func main():
        print(api.value())
'''
    main.write_text(source);inputs['project']=source
    # Each disabled imported dependency is physically absent.
    for enabled in (True,False,True):
        manifest.write_text(MANIFEST+'[build.conditions]\nextra='+str(enabled).lower()+'\n')
        selected=project/('enabled.void' if enabled else 'disabled.void');other=project/('disabled.void' if enabled else 'enabled.void')
        other.unlink(missing_ok=True);selected.write_text('public func value() -> i32:\n    return '+('41' if enabled else '43')+'\n')
        expected='41\n' if enabled else '43\n'
        for profile in ('debug','release'):
            run([compiler,'run','--project',project,'--profile',profile,'--no-cache'],output=expected)
            exe=root/('project-'+str(enabled)+'-'+profile+('.exe' if os.name=='nt' else ''));run([compiler,'build','--project',project,'--profile',profile,'-o',exe]);run([exe],output=expected)
        # Bootstrap cache inventory, then check deterministic warm-hit behavior.
        run([compiler,'run','--project',project,'--profile','debug'],output=expected)
        cold=run([compiler,'run','--project',project,'--cache'],output=expected)
        warm=run([compiler,'run','--project',project,'--cache'],output=expected)
        if 'Debug cache: unavailable' not in warm.stderr:assert 'Debug cache: hit' in warm.stderr,(cold.stderr,warm.stderr)
    for fields,error in [('extra=1','requires a boolean'),('extra="true"','requires a boolean'),('Extra=true','Invalid build condition'),('extra=true\nextra=false','cannot redefine')]:
        manifest.write_text(MANIFEST+'[build.conditions]\n'+fields+'\n');run([compiler,'check','--project',project],1,error=error)
    # Change only the manifest while preserving identical source bytes and declarations.
    main.write_text('func selected() -> i32:\n    compile if feature.extra:\n        return 59\n    compile else:\n        return 61\nfunc main():\n    print(selected())\n')
    for enabled in (True,False):
        manifest.write_text(MANIFEST+'[build.conditions]\nextra='+str(enabled).lower()+'\n')
        expected='59\n' if enabled else '61\n'
        cold=run([compiler,'run','--project',project,'--cache'],output=expected)
        warm=run([compiler,'run','--project',project,'--cache'],output=expected)
        if 'Debug cache: unavailable' not in warm.stderr:
            assert 'Debug cache: miss' in cold.stderr and 'Debug cache: hit' in warm.stderr,(cold.stderr,warm.stderr)
    # Locked source packages use their own manifest, never the consumer's features.
    package=project/'deps/api';package.mkdir(parents=True);(package/'api.void').write_text('compile if feature.extra:\n    public func value() -> i32:\n        return 47\ncompile else:\n    public func value() -> i32:\n        return 53\n')
    package_manifest="schema=1\n[project]\nid='org.test.packageproject'\nname='Package'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.test.package'\nname='api'\nentry='api.void'\n[build.conditions]\nextra=true\n"
    (package/'void.project.toml').write_text(package_manifest)
    manifest.write_text(MANIFEST+"[build.conditions]\nextra=false\n[dependencies.api]\nid='org.test.package'\npath='deps/api'\nversion='=1.0.0'\n")
    main.write_text('use pkg/api as api\nfunc main():\n    print(api.value())\n')
    run([compiler,'resolve','--project',project]);run([compiler,'run','--project',project,'--no-cache'],output='47\n')
print('G87 source: '+str(len(records))+' process checks at 1 MiB')
