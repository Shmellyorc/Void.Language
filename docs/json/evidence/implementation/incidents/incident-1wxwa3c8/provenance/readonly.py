"""Shared readonly prerequisite: execution, native linking and authority negatives."""
from pathlib import Path
import sys, os, json, hashlib, tempfile, resource, time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/random'))
import harness as h
os.environ['VOID_CRASH_EVIDENCE']=str(ROOT/'docs/json/evidence/implementation/incidents')
compiler=Path(sys.argv[1]).resolve()
profile=sys.argv[2] if len(sys.argv)>2 else 'adhoc'
records=[]

def run(args,src,expected=None,status=0):
    result=h.run(args,src.parent,expected=expected,status=status,sources=[src],small=True,timeout=120)
    records.append({'command':list(map(str,args)),'status':result.returncode,'stdout':result.stdout.decode(),'stderr':result.stderr.decode(),'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'stack_bytes':1048576})
    return result

expectations={'readonly_identity':'same identity\n43\n','direct_const_get':'direct readonly result\n','readonly_result_corrected':'readonly result accepted\n','readonly_reduced':'readonly child\n','readonly_safe_route':'readonly child\n','readonly_paths':'child 42\nlist 0: 42\nsnapshot 42\ndict leaf: 42\nget 42\n'}
with tempfile.TemporaryDirectory(prefix='void-json-readonly-') as tmp:
    tmp=Path(tmp)
    for name,expected in expectations.items():
        src=ROOT/'tests/json'/(name+'.void')
        for opt in [False,True]:
            flags=[] if opt else ['--no-opt']
            run([compiler,'run',src,*flags],src,expected.encode())
            output=tmp/(name+('-O2' if opt else '-O0'))
            run([compiler,'build',src,'-o',output,*flags],src)
            run([output],src,expected.encode())
    header=(ROOT/'tests/json/readonly_paths.void').read_text().split('func main():')[0]
    setup='    var leaf = node(children = [], count = 42)\n    const root = node(children = [leaf])\n'
    negatives={
      'result_mutation':'    if forward(root) is Ok(var child):\n        child.count = 1\n',
      'propagated_mutation':'    if propagate(root) is Ok(var child):\n        child.children.push(leaf)\n',
      'result_escape':'    if forward(root) is Ok(var child):\n        var escaped = node(children = [child])\n',
      'mutable_parameter':'    if forward(root) is Ok(var child):\n        mutate(child)\n',
      'iteration_mutation':'    for child in root.children:\n        child.count = 1\n',
      'snapshot_mutation':'    for child in root.snapshot():\n        child.count = 1\n',
      'list_replacement':'    root.children[0] = leaf\n',
      'pattern_narrowing':'    if root is node(var alias):\n        alias.count = 1\n',
      'get_mutation':'    if root.children.get(0) is Ok(var child):\n        child.count = 1\n',
      'parameter_escape':'    escape(root)\n',
    }
    extra='func mutate(input: node):\n    input.count = 7\nfunc escape(const input: node) -> node:\n    return node(children = [input])\n'
    # Include the invalid escape helper only in its own test.
    for name,body in negatives.items():
        src=tmp/(name+'.void')
        helper=extra if name=='parameter_escape' else extra.split('func escape')[0]
        src.write_text(header+helper+'func main():\n'+setup+body)
        r=run([compiler,'check',src],src,status=1)
        assert b'VOID IR:' not in r.stderr and b'readonly' in r.stderr.lower(),r.stderr
src=ROOT/'tests/json/pattern_recovery_invalid.void'
r=run([compiler,'check',src],src,status=1)
assert b'VOID2001' in r.stderr and b'VOID IR:' not in r.stderr,r.stderr
report={'profile':profile,'compiler':str(compiler),'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'checks':len(records),'records':records,'environment':{k:os.environ[k] for k in ['ASAN_OPTIONS','UBSAN_OPTIONS'] if k in os.environ}}
(ROOT/'docs/json/evidence/implementation'/('readonly-'+profile+'-'+str(time.time_ns())+'.json')).write_text(json.dumps(report,indent=2)+'\n')
print(f'readonly {profile}: {len(records)}/{len(records)} checks passed')
