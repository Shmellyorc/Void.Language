"""Two retained-input replays and one source-associated compilation; no ptrace."""
from pathlib import Path
import hashlib,json,os,resource,shutil,sys,tempfile,time
ROOT=Path(__file__).resolve().parents[4];OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tests/text_encoding'));import crash
os.environ['VOID_CRASH_EVIDENCE']=str(OUT.parent/'crashes')
H=Path('/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028')
source=H/'flat-builder/performance.void'
work=Path(tempfile.mkdtemp(prefix='VOID LLVM23 DSE acceptance '));records=[]
def limits():
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    os.umask(0o077);resource.setrlimit(resource.RLIMIT_CORE,(resource.RLIM_INFINITY,resource.RLIM_INFINITY))
    Path('/proc/self/coredump_filter').write_text('0x37\n')
def run(name,args,env,inputs,provenance):
    start=time.monotonic()
    p=crash.run(list(map(str,args)),cwd=work,env=env,capture_output=True,text=True,timeout=600,preexec_fn=limits,
                expected_returncode=0,source_inputs=inputs,provenance_inputs=[*provenance,Path(__file__)])
    records.append({'name':name,'argv':list(map(str,args)),'source_claim':'Stage11 retained preopt IR; logging captured diagnostic IR or source-associated workload, not original crashing process bytes',
                    'inputs_sha256':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [*inputs,*provenance]},
                    'status':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'process_wall_seconds':time.monotonic()-start,
                    'hard_stack_bytes':1048576,'working_directory':str(work)})
    (OUT/'dse-results.json').write_text(json.dumps(records,indent=2)+'\n');print(name,p.returncode,flush=True)
    if p.returncode or p.expectation_failed:raise SystemExit('Stopped at first failure; workspace/core and recorder evidence retained.')
base=dict(os.environ)
for name,path in [('stage11-console',ROOT/'build/llvm23-metrics/stage11-console-preopt.ll'),('logging-captured',H/'dse-input-investigation/captures/logging/preopt.bc')]:
    run(name,[ROOT/'build/llvm23-metrics/replay-llvm23',path,'/main.void' if name=='stage11-console' else '/performance.void'],base,[source] if name=='logging-captured' else [],[path,OUT/'replay.cpp'])
env=dict(base,VOID_STDLIB_ROOT=str(H/'flat-builder/stdlib'))
run('logging-source-associated',[ROOT/'build/llvm23-metrics/metrics-llvm23',source,1,1,6,12800,1000],env,[source],[OUT/'metrics.cpp'])
shutil.rmtree(work)
print('3/3 bounded DSE-associated checks passed; no historical cause or repair inferred.')
