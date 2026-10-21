"""Sprint 1 fixtures through the existing CLI, native engines and Debug cache."""
import argparse,json,os,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('build');p.add_argument('--compiler',default='clang++-19');p.add_argument('--output',required=True);a=p.parse_args();build=pathlib.Path(a.build).resolve()
with tempfile.TemporaryDirectory(prefix='VOID nested engine fixtures ') as d:
    root=pathlib.Path(d);project=root/'project';standard=root/'standard'
    (project/'math').mkdir(parents=True);(standard/'math').mkdir(parents=True)
    (standard/'math/calc.void').write_text('use math/detail as detail\nfunc twice(x: i32) -> i32:\n    return detail.twice(x)\n')
    detail=standard/'math/detail.void';detail.write_text('internal func twice(x: i32) -> i32:\n    return x * 2\n')
    (project/'math/calc.void').write_text('func add(x: i32) -> i32:\n    return x + 1\n')
    (project/'main.void').write_text('use math/calc as standard\nuse "math/calc.void" as local\nfunc entry(x: i32) -> i32:\n    return standard.twice(x) + local.add(x)\n')
    env=dict(os.environ,VOID_STDLIB_ROOT=str(standard))
    def run(command):return subprocess.run([str(x) for x in command],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,check=True)
    run([build/'voidc','check',project]);results={}
    for optimized in (False,True):
        flags=[] if optimized else ['--no-opt'];mode='optimized' if optimized else 'normal'
        result=run([build/'voidc','run',project,'5',*flags]);assert result.stdout.strip()=='16',result
        obj=root/(mode+'.o');exe=root/mode
        run([build/'voidc','emit',project,'-o',obj,*flags]);run([a.compiler,obj,build/'libvoid_runtime.a',build/'libvoid_native_fixture.a','-o',exe])
        assert run([exe,'5']).stdout.strip()=='16'
        results[mode]={'jit':16,'aot':16}
    cache=[]
    for expected,status in [(16,'miss'),(16,'hit')]:
        result=run([build/'voidc','run',project,'5','--cache']);assert result.stdout.strip()==str(expected) and f'Debug cache: {status}' in result.stderr,(result.stdout,result.stderr);cache.append(status)
    detail.write_text('internal func twice(x: i32) -> i32:\n    return x * 3\n')
    result=run([build/'voidc','run',project,'5','--cache']);assert result.stdout.strip()=='21' and 'Debug cache: miss' in result.stderr,(result.stdout,result.stderr);cache.append('stdlib-byte-edit miss:21')
    results['cache']=cache
pathlib.Path(a.output).write_text(json.dumps(results,indent=2)+'\n');print('Nested standard/project identities: normal/O2 JIT/AOT and standard-source cache invalidation passed')
