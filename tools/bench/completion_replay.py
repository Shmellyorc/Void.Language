"""Replay the unchanged G08 editor sequence with an optional attributed server.

The native CLI and frozen inputs still come from compiler_project.py. Each run
records exact editor result/diagnostic transcripts (including positions), server
identity, raw timings and memory. Trace files are optional developer artifacts.
"""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory
import compiler_project as base


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build',type=Path)
    parser.add_argument('--server',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--trace',type=Path)
    parser.add_argument('--repetitions',type=int,default=20)
    parser.add_argument('--queries',type=int,default=10)
    args=parser.parse_args()
    if min(args.repetitions,args.queries)<1: parser.error('positive sample counts required')
    if args.trace:
        args.trace.resolve().touch(exist_ok=False)
        os.environ['VOID_G19_TRACE']=str(args.trace.resolve())
    else: os.environ.pop('VOID_G19_TRACE',None)
    transcripts=[]
    original=base.Client
    class RecordingClient(original):
        def __init__(self,*a,**kw):
            super().__init__(*a,**kw)
            self.project_root=Path(a[1])
            transcripts.append([])
            self.transcript=transcripts[-1]
        def request(self,method,params=None,**kw):
            response,notes=super().request(method,params,**kw)
            if method.startswith('textDocument/'):
                entry={'method':method,'result':response.get('result'),'error':response.get('error'),'diagnostics':notes}
                # Replays have distinct disposable physical roots. Normalize only
                # that root; retain all source-relative names, bytes and positions.
                encoded=json.dumps(entry,sort_keys=True,ensure_ascii=False)
                encoded=encoded.replace(self.project_root.as_uri(),'PROJECT_URI').replace(self.project_root.as_posix(),'PROJECT')
                self.transcript.append(json.loads(encoded))
            return response,notes
        def shutdown(self):
            if os.name=='nt':
                import ctypes as c
                from ctypes import wintypes as w
                class Counters(c.Structure):
                    _fields_=[('cb',w.DWORD),('faults',w.DWORD)]+[(n,c.c_size_t) for n in
                        ['peak_working','working','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile']]
                kernel=c.WinDLL('kernel32',use_last_error=True)
                psapi=c.WinDLL('psapi',use_last_error=True)
                kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
                kernel.CloseHandle.argtypes=[w.HANDLE]
                psapi.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Counters),w.DWORD]
                handle=kernel.OpenProcess(0x1000,False,self.process.pid)
                if not handle: raise c.WinError(c.get_last_error())
                try:
                    counters=Counters();counters.cb=c.sizeof(counters)
                    if not psapi.GetProcessMemoryInfo(handle,c.byref(counters),c.sizeof(counters)):raise c.WinError(c.get_last_error())
                    benchmark.samples['memory.WorkingSet.KiB'].append(counters.working/1024)
                    benchmark.samples['memory.PeakWorkingSet.KiB'].append(counters.peak_working/1024)
                finally:kernel.CloseHandle(handle)
            return super().shutdown()
    base.Client=RecordingClient
    with args.output.open('x',encoding='utf-8') as receipt, TemporaryDirectory(prefix='VOID G19 ') as temp:
        benchmark=base.Benchmark(args.build.resolve())
        if args.server: benchmark.server=args.server.resolve()
        benchmark.metadata.update(server_sha256=base.digest(benchmark.server),server=str(benchmark.server),
                                  repetitions=args.repetitions,queries=args.queries,lane='editor',trace=bool(args.trace))
        template=shutil.copytree(base.FIXTURE,Path(temp)/'template')
        try:
            benchmark.cli(template,'resolve')
            benchmark.metadata['lock_sha256']=base.digest(template/'void.lock')
            for i in range(args.repetitions):
                print(f'G19 editor replay {i+1}/{args.repetitions}',flush=True)
                with TemporaryDirectory(prefix='editor ',dir=temp) as iteration:
                    root=shutil.copytree(template,Path(iteration)/'project')
                    benchmark.editor(root,args.queries)
        finally:
            report=benchmark.report()
            report['editor_transcripts']=transcripts
            report['transcript_sha256']=[sha256(json.dumps(t,sort_keys=True,ensure_ascii=False).encode()).hexdigest() for t in transcripts]
            json.dump(report,receipt,indent=2,sort_keys=True);receipt.write('\n')


if __name__=='__main__': main()
