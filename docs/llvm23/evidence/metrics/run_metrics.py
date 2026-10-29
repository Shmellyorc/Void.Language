"""Bounded local LLVM migration measurements; use only after acceptance is idle."""
from pathlib import Path
import hashlib,json,os,platform,resource,shutil,statistics,sys,tempfile,time
ROOT=Path(__file__).resolve().parents[4]
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
import crash
os.environ['VOID_CRASH_EVIDENCE']=str(OUT.parent/'crashes')

def limits():
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    os.umask(0o077)
    resource.setrlimit(resource.RLIMIT_CORE,(resource.RLIM_INFINITY,resource.RLIM_INFINITY))
    Path('/proc/self/coredump_filter').write_text('0x37\n')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
records=[]
metadata={'platform':platform.platform(),'machine':platform.machine(),'cpu':Path('/proc/cpuinfo').read_text().split('model name\t: ')[1].splitlines()[0],
          'hard_stack_bytes':1048576,'samples_per_compilation':3,'uncached_compilations_per_case_and_mode':1,
          'measurement_host_sha256':{v:sha(ROOT/f'build/llvm23-metrics/metrics-llvm{v}') for v in ['19','23']},
          'aslr_sysctl':Path('/proc/sys/kernel/randomize_va_space').read_text().strip(),
          'load_average_start':Path('/proc/loadavg').read_text().strip(),
          'stdlib_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sorted((ROOT/'stdlib').rglob('*.void')) if p.is_file()},
          'analysis_contract':'Same sources and current stdlib; preserved LLVM19 Release libraries versus migrated LLVM23 Release libraries. Full module emission, persistent cache disabled. Wall compilation and in-process execution measured separately.'}
(OUT/'environment.json').write_text(json.dumps(metadata,indent=2)+'\n')
cases=json.loads((ROOT/'build/llvm23-metrics/cases.json').read_text())
work=Path(tempfile.mkdtemp(prefix='VOID LLVM metrics '))
metadata['working_directory']=str(work)
(OUT/'environment.json').write_text(json.dumps(metadata,indent=2)+'\n')
if True:
    for case in cases:
        for mode in ['normal','O2']:
            # Alternate version order to avoid always giving one version the warm filesystem.
            versions=['19','23'] if mode=='normal' else ['23','19']
            for version in versions:
                exe=ROOT/f'build/llvm23-metrics/metrics-llvm{version}'
                args=[str(exe),case['source'],str(int(mode=='O2')),str(int(case['async'])),str(case['input']),str(case['expected'])]
                env=dict(os.environ,VOID_STDLIB_ROOT=str(ROOT/'stdlib'))
                t=time.monotonic()
                p=crash.run(args,cwd=work,env=env,capture_output=True,text=True,timeout=600,preexec_fn=limits,
                            expected_returncode=0,source_inputs=[Path(case['source'])],provenance_inputs=[OUT/'metrics.cpp',Path(__file__)])
                row={'case':case['name'],'mode':mode,'llvm':version,'argv':args,'source_sha256':sha(case['source']),
                     'status':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'process_wall_seconds':time.monotonic()-t}
                if p.returncode==0:
                    line=next(line[7:] for line in p.stdout.splitlines() if line.startswith('METRIC '))
                    row['metric']=json.loads(line)
                    row['execution_median_seconds']=statistics.median(s['wall_seconds'] for s in row['metric']['samples'])
                records.append(row)
                (OUT/'results.json').write_text(json.dumps(records,indent=2)+'\n')
                print(case['name'],mode,version,p.returncode,flush=True)
                if p.returncode or p.expectation_failed:raise SystemExit('Bounded measurement stopped after the first failed child; evidence retained.')
shutil.rmtree(work)
print('20/20 bounded representative measurements passed their independent checksums.')
