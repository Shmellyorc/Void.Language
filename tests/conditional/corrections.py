"""G87 review corrections: excluded continuations and owner-configured package advice."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,tempfile
repo=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(repo/'tools'))
from test_output import output_directory
compiler,report=(Path(p).resolve() for p in sys.argv[1:]);records=[];inputs={}
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args,status=0,output=None,error=None):
    args=list(map(str,args));r=subprocess.run(args,capture_output=True,text=True,timeout=90,preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=args,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr);records.append(row)
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576,compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),inputs=inputs,records=records),indent=2)+'\n')
    assert r.returncode==status,row
    if output is not None:assert r.stdout==output,row
    if error:assert error in r.stderr,row
    assert not any(x in r.stderr for x in ('AddressSanitizer','runtime error:','Assertion','Missing owned')),row
    return r
API='''value struct player_state:
    name: string
    health: i32
    speed: f64
func create_player(name: string, health: i32, speed: f64) -> player_state:
    return player_state(name = name, health = health, speed = speed)
func echo(value: string) -> string:
    return value
func arithmetic() -> i32:
    return (
1 +
        (2 * 3)
    )
'''
BODY='''        print(
      "same-close")
        print(
"own-close"
)
        var player = create_player(
      name = "Bob",
  health = 100,
            speed = 5.0
        )
        print(player.name, player.health, player.speed == 5.0)
        var grouped = (
1 +
            (2 * 3)
)
        if (
 grouped == 7
        ):
            print("grouped", arithmetic())
        print(echo(
      "nested"))
'''
OUTPUT='same-close\nown-close\nBob 100 true\ngrouped 7\nnested\n'
MANIFEST="schema=1\n[project]\nid='org.review.app.project'\nname='Review'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.review.app'\n"
LIB="schema=1\n[project]\nid='org.review.api.project'\nname='Review API'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.review.api'\nname='api'\nentry='api.void'\n"
with tempfile.TemporaryDirectory(prefix='g87-corrections-',dir=output_directory()) as directory:
    root=Path(directory)
    # Ordinary multiline calls and parenthesized arithmetic/conditions/returns
    # keep the existing parser: test both ')' placements and cosmetic dedents.
    cases={
        'ordinary':(API+'func main():\n'+''.join(line[4:]+'\n' if line.startswith('    ') else line+'\n' for line in BODY.splitlines()),OUTPUT),
        'selected':(API+'func main():\n    compile if true:\n'+BODY+'    compile else:\n        unavailable $ invalid "\n    print("after")\n',OUTPUT+'after\n'),
        'excluded':(API+'func main():\n    compile if false:\n'+BODY+'    compile else:\n        print("inactive")\n    print("after")\n','inactive\nafter\n'),
        'original_false':('func main():\n    compile if false:\n        print(\n      "value"\n        )\n    print("after")\n','after\n'),
        'original_true':('func main():\n    compile if true:\n        print(\n      "value"\n        )\n    print("after")\n','value\nafter\n'),
        'opaque_recovery':('compile if false:\n    invalid((( $ @ "\n    print(\n"ignored")\nfunc main():\n    print("after")\n','after\n'),
        'comments_strings':('func main():\n    compile if false:\n        print( # ) ignored\n"literal ( ) {{ }}",\n      """multiline\nnot code (\n# text\n"""\n)\n    print("after")\n','after\n'),
        'nested_directives':('func main():\n    compile if false:\n        print(\n            if true:\n                compile if false:\n                    "no"\n                compile else:\n                    "yes"\n            else:\n                "other"\n        )\n    print("after")\n','after\n'),
    }
    # A multiline literal retains its lexical extent in either configuration;
    # source-looking content cannot become a declaration or global initializer.
    quoted='    func unused() -> string:\n        return """text\nfunc initialize() -> i32:\n    print("ghost")\n    return 17\nvar ghost = initialize()\n# """\n'
    for enabled in (True,False):
        cases['quoted_'+str(enabled)]=('compile if '+str(enabled).lower()+':\n'+quoted+'func main():\n    print("after")\n','after\n')
    cases['deep_excluded']=('func main():\n    compile if false:\n        print(\n'+'(\n'*300+'\"deep\"\n'+')\n'*301+'    print(\"after\")\n','after\n')
    for name,(source,expected) in cases.items():
        path=root/(name+'.void');path.write_text(source);inputs[name]=source
        for flags in (['--no-opt'],[]):run([compiler,'run',path,'--no-cache',*flags],output=expected)
        # Qualify native code for the ordinary form, both configurations, and
        # the exact reviewed regression; retain focused source checks for recovery.
        if name in ('ordinary','selected','excluded','original_false','original_true'):
            for mode,flags in [('O0',['--no-opt']),('O2',[])]:
                exe=root/(name+'-'+mode+('.exe' if os.name=='nt' else ''))
                run([compiler,'build',path,'-o',exe,*flags]);run([exe],output=expected)
    # A valid nested directive condition is still checked under an excluded call.
    bad=cases['nested_directives'][0].replace('compile if false:\n                    "no"','compile if feature.missing:\n                    "no"')
    path=root/'unknown.void';path.write_text(bad);inputs['unknown']=bad
    run([compiler,'check',path],1,error="Unknown build condition 'missing'")
    # Metadata advice must use the library's validated features, not consumer
    # settings. All paths are in-project editable sources, without installation.
    for variant in ('plain','comment','identifier','provider'):
        for enabled in (True,False):
            project=root/(variant+'-'+str(enabled));package=project/'deps/api';package.mkdir(parents=True)
            (project/'void.project.toml').write_text(MANIFEST+'[build.conditions]\nextra='+str(not enabled).lower()+'\n')
            (package/'void.project.toml').write_text(LIB+'[build.conditions]\nextra='+str(enabled).lower()+'\n')
            name='native_value' if variant=='identifier' else 'value'
            source=('# native comment only\n' if variant=='comment' else '')+f'compile if feature.extra:\n    public func {name}() -> i32:\n        return 47\ncompile else:\n    public func {name}() -> i32:\n        return 53\n'
            (package/'api.void').write_text(source);inputs[variant+str(enabled)]=source
            (project/'main.void').write_text('use pkg/api as api\nfunc main():\n    print(api.'+name+'())\n')
            if variant=='provider':
                native='compile if feature.extra:\n    native "org.review.provider"\n    func scalar(value: i32) -> i32:\n        pass\ncompile else:\n    public func ordinary() -> i32:\n        return 0\n'
                (package/'provider.void').write_text(native);inputs['provider']=native
            # A nested project owns different conditions and is outside this
            # package's admitted source inventory, including metadata inspection.
            nested=package/'child';nested.mkdir();(nested/'void.project.toml').write_text(LIB.replace("org.review.api'","org.review.child'")+"[build.conditions]\nother=true\n")
            (nested/'unused.void').write_text('# native child\ncompile if feature.other:\n    public func other() -> i32:\n        return 1\n')
            result=run([compiler,'add','api','--project',project,'--path','deps/api'])
            assert ("source declares native provider 'org.review.provider'" in result.stdout)==(variant=='provider' and enabled),result
            assert not (project/'libs').exists(),'in-project sources must remain editable, without installed views'
            run([compiler,'run','--project',project,'--no-cache'],output='47\n' if enabled else '53\n')
print('G87 corrections: '+str(len(records))+' process checks at 1 MiB')
