"""Paired retained-payload measurement and permanent semantic/frame regression."""
import argparse,json,pathlib,subprocess
parser=argparse.ArgumentParser()
parser.add_argument('--current');parser.add_argument('--baseline');parser.add_argument('--aot')
parser.add_argument('--sources',type=pathlib.Path,default=pathlib.Path(__file__).parent)
parser.add_argument('--output',type=pathlib.Path,required=True)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
results=[]
def invoke(executable,workload,mode,count,baseline=False,aot=False):
    command=[executable,str(count)] if aot else [executable,str(args.sources/(workload+'.void')),mode,str(count)]
    p=subprocess.run(command,capture_output=True,text=True,timeout=90)
    name=f'{"baseline" if baseline else "current"}-{workload}-{mode}-{count}'
    (args.output/(name+'.stdout')).write_text(p.stdout);(args.output/(name+'.stderr')).write_text(p.stderr)
    assert p.returncode==0,(command,p.returncode,p.stdout[-1000:],p.stderr)
    if aot:assert p.stdout.splitlines()[-1]=='7',p.stdout
    records=[json.loads(line) for line in p.stdout.splitlines() if line.startswith('{')]
    assert records and records[-1]['phase']=='root_drained_gc',records
    if aot:workload=records[0]['workload']
    end=records[-1];assert end['objects']==end['payload_bytes']==end['tasks']==end['persistent_handles']==0,end
    awaited=[r for r in records if r['phase']=='awaited_gc']
    assert len(awaited)==5,awaited
    # Fixed-value schemas are admitted lazily on first use; compare settled group exits.
    catalog={(r['nominal_types'],r['schemas'],r['witnesses']) for r in records if r['phase']=='group_exit_gc'}
    assert len(catalog)==1,catalog
    for r in records:
        if r['phase'] not in ['awaited_gc','group_exit_gc']:continue
        assert r['tasks']==r['completed_records']+1,r
        assert r['native_heap_execution_frames']==0,r
        if baseline:assert r['rooted_frames']==r['tasks'],r
        else:
            assert r['rooted_frames']==1,r
            # Completed task results are now owned by ordinary source roots.
            # Their native ordering records no longer keep them all alive.
            # Bound this scalar-result fixture using the actual generated frame
            # reference count, including every conservatively live temporary.
            assert r['rooted_task_results']==1 and r['worker_records']==0,r
            assert r['persistent_handles']==2,r
            assert r['objects']<=r['rooted_frames']+r['rooted_task_results']+r['rooted_frame_references'],r
            assert r['payload_bytes']<=r['rooted_frame_bytes']+r['rooted_task_payload_bytes']+r['rooted_frame_references']*r['max_task_result_bytes'],r
            # A retained source task may also retain its native ancestry. In
            # this fixed-depth fixture, bound bookkeeping by actual live heap
            # objects/frame references, independently of total completed work.
            assert r['tasks']<=r['objects']+r['rooted_frame_references'],r
            assert r['groups']<=r['tasks'],r
    if workload=='growth':
        running=[r for r in records if r['phase']=='child_running_gc']
        assert running and all(r['rooted_frames']>=2 for r in running),running
    if baseline:assert awaited[-1]['completed_records']==4*count+(workload=='growth'),awaited[-1]
    if not baseline:
        # Compare the same settled source frame after each complete batch.
        # Both result payload and native bookkeeping remain bounded. Exact
        # address reuse may conservatively retain a few additional records.
        assert len({(r['payload_bytes'],r['persistent_handles']) for r in awaited[1:]})==1,awaited
    results.append({'command':command,'records':records})
    return awaited[-1]
if args.aot:
    for count in [0,8,32]:invoke(args.aot,'aot','linked',count,aot=True)
else:
    assert args.current
    for workload in ['growth','workers']:
        for mode in ['normal','optimized']:
            for count in [0,8,32,128]:
                after=invoke(args.current,workload,mode,count)
                if args.baseline:
                    before=invoke(args.baseline,workload,mode,count,baseline=True)
                    assert after['tasks']<=before['tasks']
                    assert after['frame_allocations']==before['frame_allocations']
                    if count:assert after['payload_bytes']<before['payload_bytes']
(args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'Sprint 5 retention: {len(results)} successful process measurements; no suite-count multiplication')
