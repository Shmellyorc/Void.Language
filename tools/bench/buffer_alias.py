#!/usr/bin/env python3
"""Measure existing Linux output guard; no production instrumentation or Git writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import statistics
import subprocess
ROOT = Path(__file__).resolve().parents[2]
SYMBOL = '_ZNK2vd4Heap23validate_output_addressEPPv'

def run(command):
    return subprocess.check_output([str(x) for x in command], text=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--iterations', type=int, default=64)
    parser.add_argument('--counts', type=int, nargs='+', default=[0,1000,10000,100000,500000])
    parser.add_argument('--cpu', type=int)
    args = parser.parse_args()
    if platform.system() != 'Linux':
        parser.error('ELF production-symbol wrapper is Linux-only; not native Windows/macOS evidence')
    if args.samples < 1 or args.iterations < 1 or any(n < 0 for n in args.counts):
        parser.error('positive samples/iterations and nonnegative counts required')
    if args.cpu is not None:
        os.sched_setaffinity(0, {args.cpu})
    build=args.build.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    cache={}
    for line in (build/'CMakeCache.txt').read_text().splitlines():
        if '=' in line and not line.startswith(('#','//')):
            key,value=line.split('=',1);cache[key.split(':')[0]]=value
    if cache.get('CMAKE_BUILD_TYPE') != 'Release' or cache.get('VOID_SANITIZERS') != 'OFF':
        parser.error('use an equivalent non-sanitized Release runtime build')
    compiler=cache['CMAKE_CXX_COMPILER'];linker=cache.get('VOID_LINKER')
    commands=json.loads((build/'compile_commands.json').read_text())
    runtime=next(c for c in commands if Path(c['file']).name=='buffer.cpp' and '/src/runtime/' in c['file'])
    flags=runtime.get('arguments') or shlex.split(runtime['command'])
    rtti='-fno-rtti' if '-fno-rtti' in flags else '-frtti'
    binary=output/'buffer-alias'
    command=[compiler,'-std=c++20','-O3','-DNDEBUG',rtti,'-pthread','-I'+str(ROOT/'include'),
             ROOT/'tools/bench/buffer_alias.cpp',build/'libvoid_runtime.a',
             '-Wl,--wrap='+SYMBOL,'-o',binary]
    if linker:command.insert(1,'--ld-path='+linker)
    subprocess.run([str(x) for x in command],check=True)
    raw=[]
    for sample in range(args.samples):
        counts=args.counts if sample%2==0 else list(reversed(args.counts))
        for count in counts:
            raw.append({'sample':sample,'count':count,'rows':json.loads(run([binary,count,args.iterations]))})
    summary=[]
    for count in args.counts:
        for buffers in (0,1,16):
            rows=[row for item in raw if item['count']==count for row in item['rows'] if row['buffers']==buffers]
            entry={'ordinary_objects':count,'buffers':buffers}
            for metric in ('wall_ns_per_output','guard_lock_service_ns_per_scan','guard_lock_service_max_ns'):
                values=[r[metric] for r in rows]
                entry[metric]={'median':statistics.median(values),'min':min(values),'max':max(values)}
            summary.append(entry)
    files=[ROOT/'tools/bench/buffer_alias.cpp',ROOT/'tools/bench/buffer_alias.py',build/'libvoid_runtime.a',build/'CMakeCache.txt',ROOT/'.github/workflows/ci.yml']
    files += [p for base in ('include/void','src/runtime') for p in sorted((ROOT/base).rglob('*')) if p.is_file()]
    result={'head':run(['git','-C',ROOT,'rev-parse','HEAD']).strip(),'platform':platform.platform(),
            'compiler':run([compiler,'--version']),'llvm_dir':cache['LLVM_DIR'],'rtti':rtti,
            'cpu_affinity':sorted(os.sched_getaffinity(0)),'command':[str(x) for x in command],
            'samples':args.samples,'iterations':args.iterations,
            'sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
            'notes':'Guard timer runs inside caller mutex; excludes wait and surrounding critical work. Two scans/output. Includes clock/wrapper overhead. No subtraction, latency bound or production instrumentation.',
            'raw':raw,'summary':summary}
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
