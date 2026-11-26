"""Real CLI streams/exits, timing schema, TTY gating, phase and publication contracts."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
schema = json.loads((Path(__file__).resolve().parents[2] / 'schemas/cli-timings-v1.schema.json').read_text())
checks = 0


def validate(value, spec):
    # Validate this deliberately small JSON Schema vocabulary without adding a
    # Python package dependency. These are actual emitted reports, not samples.
    if 'enum' in spec:
        assert any(value == v and type(value) is type(v) for v in spec['enum']), (value, spec)
    if 'type' in spec:
        kinds = spec['type'] if isinstance(spec['type'], list) else [spec['type']]
        types = {'object': dict, 'array': list, 'string': str, 'integer': int, 'boolean': bool, 'null': type(None)}
        assert any(type(value) in ((int,float) if k == 'number' else (types[k],)) for k in kinds), (value,spec)
    if type(value) in (int,float) and 'minimum' in spec:
        assert value >= spec['minimum']
    if isinstance(value,dict):
        assert set(spec.get('required',[])) <= value.keys(), value
        for key,item in value.items():
            if key in spec.get('properties',{}):validate(item,spec['properties'][key])
    if isinstance(value,list) and 'items' in spec:
        for item in value:validate(item,spec['items'])


def run(args, status=0, **kwargs):
    global checks
    result = subprocess.run([str(compiler), *map(str,args)], capture_output=True, timeout=90, **kwargs)
    checks += 1
    assert result.returncode == status, (args,result)
    return result


with tempfile.TemporaryDirectory(prefix='VOID G75 雪 ') as folder:
    root = Path(folder)
    sources = root/'sources'; sources.mkdir()
    app = sources/'main.void'; app.write_text('func main() -> i32:\n    return 7\n')
    legacy = sources/'legacy.void'; legacy.write_text('func entry(value: i32) -> i32:\n    return value\n')
    bad = sources/'bad.void'; bad.write_text('func main() -> i32:\n    return unknown\n')
    reports = root/'reports'; reports.mkdir()
    assert json.loads(run(['--capabilities']).stdout)['statusPlain'] is True
    default = run(['check',app]); assert default.stdout == ('Source checked successfully'+os.linesep).encode() and not default.stderr
    for mode in ('auto','off'):
        current=run(['check',app,'--status',mode]);assert current.stdout==default.stdout and current.stderr==default.stderr
    plain=run(['check',app,'--status','plain'])
    assert plain.stdout==default.stdout and b'Checking' in plain.stderr and b'Finished' in plain.stderr and b'\x1b' not in plain.stderr
    for flags in (['--status','bad'],['--status'],['--status','off','--status','auto'],['--timings','--timings'],
                  ['--timings-format','text'],['--timings','--timings-format','xml'],['--timings','--timings-format','json'],
                  ['--timings-output',reports/'unused.json'],['--timings','--timings-format','json','--timings-output',app],
                  ['--timings','--timings-format','json','--timings-output',reports/'missing'/'report.json']):
        run(['check',app,*flags],2)

    serial=0
    def timed(args,status=0):
        global serial
        serial+=1;path=reports/f'{serial}.json'
        flags=['--timings','--timings-format','json','--timings-output',path]
        r=run([*args,*flags],status)
        raw=path.read_bytes();assert b'\x1b' not in raw and raw.endswith(b'\n')
        v=json.loads(raw);validate(v,schema)
        assert v['process_exit_code']==status
        phases={item['id']:item for item in v['phases']};assert len(phases)==len(v['phases'])
        for phase in phases.values():
            if phase['parent_id']:
                assert phase['parent_id'] in phases
                assert phase['duration_us'] is None or phase['duration_us']<=phases[phase['parent_id']]['duration_us']
        return r,v,phases

    _,v,p=timed(['check',app]);assert v['build']['outcome']=='success' and p['lex_parse']['duration_us'] is not None
    _,v,p=timed(['check',bad],1);assert v['build']['outcome']=='failure' and p['frontend']['state']=='partial' and p['llvm_ir']['duration_us'] is None
    r,v,p=timed(['run',app,'--no-cache'],7)
    assert r.stdout==b'' and not r.stderr and v['application']['exit_code']==7 and v['application']['legacy_result'] is None
    assert p['typed_ir']['count']==1 and p['jit_materialization']['count']==1
    _,v,p=timed(['run',legacy,'7','--no-cache'])
    assert v['application']['entry_contract']=='legacy_i32' and v['application']['legacy_result']==7 and v['application']['exit_code'] is None
    obj=root/'source.o'
    _,v,p=timed(['emit',app,'-o',obj,'--no-opt'])
    assert obj.is_file() and p['llvm_optimization']['state']=='skipped' and p['native_object_emission']['duration_us'] is not None
    exe=root/('app.exe' if os.name=='nt' else 'app')
    _,v,p=timed(['build',app,'-o',exe])
    assert p['native_link_driver']['count']==1 and p['publication']['count']==1
    assert subprocess.run([str(exe)]).returncode==7
    project=root/'packaged-project'
    run(['new','app',project])
    # Schema-valid defaults must not pretend that a project profile resolved.
    malformed=root/'malformed-project';malformed.mkdir()
    (malformed/'void.project.toml').write_text('[=invalid manifest]\n')
    r,v,p=timed(['check','--project',malformed],1)
    assert b'VOID5001' in r.stderr
    assert v['configuration']=={'project_mode':True,'profile':None,'optimization':None,'debug_info':None},v
    r,v,p=timed(['run','--project',project,'--profile','release','--cache'],2)
    assert b'--cache requires the debug profile' in r.stderr
    assert v['configuration']=={'project_mode':True,'profile':None,'optimization':None,'debug_info':None},v
    default_debug_info='none' if sys.platform=='darwin' else 'full'
    for profile,opt,debug in [('debug','none',default_debug_info),('release','O2','none')]:
        _,v,p=timed(['check','--project',project,'--profile',profile])
        assert v['configuration']=={'project_mode':True,'profile':profile,'optimization':opt,'debug_info':debug},v
    _,v,p=timed(['check','--project',project,'--profile','release','--no-opt'])
    assert v['configuration']['optimization']=='none' and v['configuration']['profile']=='release'
    for flags,opt in [([], 'O2'),(['--no-opt'],'none')]:
        _,v,p=timed(['check',app,*flags])
        assert v['configuration']=={'project_mode':False,'profile':None,'optimization':opt,'debug_info':'none'},v
    _,v,p=timed(['run','--project',project,'--cache'])
    run_debug_info='none' if default_debug_info=='none' else 'unavailable'
    assert v['configuration']=={'project_mode':True,'profile':'debug','optimization':'none','debug_info':run_debug_info},v
    distribution=root/'distribution'
    if sys.platform=='darwin':
        r,v,p=timed(['package','--project',project,'-o',distribution],1)
        assert b'Application packaging requires Linux x86-64 GNU or Windows x64 MSVC.' in r.stderr
        assert not v['build']['complete'] and v['build']['outcome']=='failure' and not distribution.exists()
        assert p['package_assembly']['count']==1 and p['package_assembly']['state']=='partial',p
        for phase in ('native_link_driver','publication'):
            assert p[phase]['count']==0 and p[phase]['state']=='not_reached',p
    else:
        _,v,p=timed(['package','--project',project,'-o',distribution])
        assert v['build']['complete'] and v['build']['outcome']=='success'
        assert p['package_assembly']['count']==1 and p['package_assembly']['state']=='complete'
        if os.name=='nt':
            # Windows copies into the reserved package stage and publishes the outer
            # directory inside package_assembly, bypassing native_build::publish.
            assert p['publication']=={'id':'publication','parent_id':None,'kind':'unavailable',
                                     'state':'not_reached','duration_us':None,'count':0,'reason':'not_applicable'},p
        else:
            assert p['publication']['count']>0 and p['publication']['state']=='complete'
        run(['package','--verify',distribution])
        assert subprocess.run([str(distribution/('app.exe' if os.name=='nt' else 'app'))]).returncode==0
    run(['run',app,'--cache'],7)
    r,v,p=timed(['run',app,'--cache'],7)
    if os.name!='nt':
        assert v['cache']['status']=='hit' and p['typed_ir']['state']=='skipped' and p['jit_materialization']['count']==1
    else:assert v['cache']['status']=='unavailable'
    text=run(['run',app,'--timings','--no-cache'],7)
    assert b'Build wall time:' in text.stderr and b'Application wall time:' in text.stderr
    failed=run(['check',bad,'--status','plain'],1)
    assert b'Finished' not in failed.stderr and failed.stderr.index(b'error:')<failed.stderr.index(b'Failed')

    # A late report collision cannot overwrite another file or replace app exit.
    blocking=sources/'blocking.void';blocking.write_text('use console\nfunc main() -> i32:\n    print("ready")\n    var answer = console.stdin.read_line()\n    return 7\n')
    path=reports/'late.json'
    child=subprocess.Popen([str(compiler),'run',str(blocking),'--no-cache','--timings','--timings-format','json','--timings-output',str(path)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    assert child.stdout.readline()==b'ready\n'
    path.write_bytes(b'retained')
    out,err=child.communicate(b'continue\n',timeout=60)
    assert child.returncode==7 and path.read_bytes()==b'retained' and b'Timing report failed:' in err,(out,err)
    # A report collision cannot invalidate a successfully published executable.
    import time
    late_build=reports/'late-build.json'
    published=root/('published.exe' if os.name=='nt' else 'published')
    child=subprocess.Popen([str(compiler),'build',str(app),'-o',str(published),'--timings','--timings-format','json',
                            '--timings-output',str(late_build)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    deadline=time.monotonic()+10
    while not list(reports.glob('timing-*')) and child.poll() is None and time.monotonic()<deadline:
        time.sleep(0.001)
    assert child.poll() is None, 'build completed before report collision control'
    late_build.write_bytes(b'preserved build report destination')
    out,err=child.communicate(timeout=60)
    assert child.returncode==0 and published.is_file() and b'Timing report failed:' in err,(out,err)
    assert subprocess.run([str(published)]).returncode==7
    assert late_build.read_bytes()==b'preserved build report destination'
    # Existing destinations and aliases are rejected before compilation.
    run(['check',app,'--timings','--timings-format','json','--timings-output',path],2)
    if os.name=='nt':
        # Real Win32 executions only. The Unicode scratch root is intentional.
        assert root.drive and len(root.drive)==2, 'drive-letter fixture requires a local Windows volume'
        def drive_case(value,upper):
            text=str(value.absolute());return (text[0].upper() if upper else text[0].lower())+text[1:]
        for source_upper in (False,True):
            for report_upper in (False,True):
                rejected=sources/f'drive-{source_upper}-{report_upper}.json'
                run(['check',drive_case(app,source_upper),'--timings','--timings-format','json',
                     '--timings-output',drive_case(rejected,report_upper)],2)
                assert not rejected.exists()
        # Extended DOS spellings cannot disguise a source or artifact location.
        extended='\\\\?\\'+drive_case(sources/'extended.json',False)
        run(['check',app,'--timings','--timings-format','json','--timings-output',extended],2)
        assert not (sources/'extended.json').exists()
        artifact=root/'drive-artifact.exe'
        run(['build',app,'-o',drive_case(artifact,True),'--timings','--timings-format','json',
             '--timings-output',drive_case(artifact,False)],2)
        assert not artifact.exists()
        for upper in (False,True):
            accepted=reports/f'drive-outside-{upper}.json'
            run(['check',app,'--timings','--timings-format','json','--timings-output',drive_case(accepted,upper)])
            validate(json.loads(accepted.read_bytes()),schema)
        junction=root/'report-junction'
        created=subprocess.run(['cmd','/d','/c','mklink','/J',str(junction),str(reports)],capture_output=True,timeout=30)
        if created.returncode==0:
            try:
                run(['check',app,'--timings','--timings-format','json','--timings-output',junction/'junction.json'],2)
                assert not (reports/'junction.json').exists()
            finally:os.rmdir(junction)
        else:print(f'Windows junction fixture unavailable: {created.stderr!r}')
        long_sources=root/('long-'+'a'*120)/('b'*120)
        try:long_sources.mkdir(parents=True)
        except OSError as error:print(f'Windows long-path fixture unavailable: {error}')
        else:
            long_app=long_sources/'main.void';long_app.write_text(app.read_text())
            rejected=long_sources/'report.json'
            run(['check',long_app,'--timings','--timings-format','json','--timings-output',drive_case(rejected,False)],2)
            assert not rejected.exists()
    if os.name!='nt':
        alias=root/'alias';alias.symlink_to(reports,target_is_directory=True)
        run(['check',app,'--timings','--timings-format','json','--timings-output',alias/'new.json'],2)
        read,write=os.pipe();os.close(read)
        result=subprocess.run([str(compiler),'check',str(app),'--status','plain'],stdout=subprocess.PIPE,stderr=write,timeout=30)
        os.close(write);assert result.returncode==0 and result.stdout==default.stdout
        import pty
        import select
        for extra,expected in [({},True),({'CI':'1'},False),({'TERM':'dumb'},False),
                               ({'TERM':'unknown'},False),({'TERM':None},False),
                               ({'TERMINFO':'/void-g75-missing-terminfo','TERMINFO_DIRS':'/void-g75-missing-terminfo'},True)]:
            master,slave=pty.openpty()
            environment=os.environ|{'TERM':'xterm'};environment.pop('CI',None)
            for key,value in extra.items():
                if value is None:environment.pop(key,None)
                else:environment[key]=value
            output=b''
            try:
                assert os.isatty(slave)
                result=subprocess.run([str(compiler),'check',str(app)],stdout=subprocess.PIPE,stderr=slave,env=environment,timeout=30)
                # Drain the terminal while the slave is still open.
                while select.select([master],[],[],0)[0] and (chunk:=os.read(master,4096)):
                    output+=chunk
            finally:
                os.close(slave)
                os.close(master)
            assert result.returncode==0 and (b'Finished' in output)==expected,(extra,output)
    assert not list(reports.glob('timing-*')), list(reports.iterdir())
print(f'presentation: {checks} process checks, actual schema/phase/exit/publication contracts passed on {sys.platform}')
