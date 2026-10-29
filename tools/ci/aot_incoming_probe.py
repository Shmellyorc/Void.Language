"""Native Linux debugger witness for original production AOT argument storage.

Reads staged reports as measurements, but executes ONLY the distinct original
control executables produced by integration scripts, never a rejected candidate.
No acceptance rule or ABI estimator is implemented here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--corpus', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--llvm-bin', type=Path, required=True)
    a = p.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        p.error('native Linux x64 only')
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out/'complete.json').write_text('{"passed": false}\n')
    records = [json.loads(x) for x in (a.corpus/'records.jsonl').read_text().splitlines()]
    version = subprocess.run([str(a.llvm_bin/'llvm-objdump'),'--version'],check=True,
                             capture_output=True,text=True).stdout
    if not re.search(r'version 23\.1\.4(?:\D|$)',version):
        raise RuntimeError('LLVM 23.1.4 required')
    results = []
    for stem, marker in (('main','63686f6f7365'),('numeric','65786368616e6765')):
        record = next(r for r in records if r['kind']=='integration' and r['mode']=='normal'
                      and r['source'].endswith('/examples/value_structs/'+stem+'.void'))
        original_object = Path(record['original'])
        executable = original_object.with_suffix('')
        if executable.resolve() == Path(record['object']).resolve():
            raise RuntimeError('candidate/control identity collision')
        cert = json.loads(Path(record['certificate']).read_text())
        profile = json.loads(cert['profile'])
        if profile['llvm']!='23.1.4' or profile['ir_pipeline']!='verify-only':
            raise RuntimeError('unexpected witness profile')
        control_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
        caller = next(f for f in cert['functions'] if '3a3a656e747279' in f)
        callee = next(f for f in cert['functions'] if marker in f)
        frame = cert['functions'][caller]['frame']
        incoming = cert['functions'][callee]['incoming']
        for item in incoming:
            objects = [o for o in cert['functions'][callee]['objects'] if o['fixed'] and not o['spill']
                       and o['offset']==item['offset'] and o['size']==item['size']]
            if len(objects)!=1:
                raise RuntimeError('ambiguous incoming machine object')
            item['machine_alignment'] = objects[0]['alignment']
        hidden = None
        if stem=='numeric':
            unmatched = [h for h in cert['functions'][caller]['unmatched_homes']
                         if not h['has_alloca_identity'] and h['physical_bytes']==64]
            homes = [o for o in cert['functions'][caller]['objects']
                     if any(o['index']==h['index'] for h in unmatched)]
            # Numeric's MIR must independently show the result pointer formed
            # from this slot, passed in RDI, and loaded after exchange returns.
            if len(homes)!=1:
                raise RuntimeError('ambiguous measured return home')
            hidden = dict(bytes=homes[0]['size'],entry_offset=homes[0]['offset']+cert['functions'][caller]['E'])
        assembly = subprocess.run([str(a.llvm_bin/'llvm-objdump'),'-d',str(executable)],
                                  check=True,capture_output=True,text=True,encoding='utf-8').stdout
        (out/(stem+'.assembly')).write_text(assembly)
        start = int(re.search(r'^([0-9a-f]+) <'+re.escape(caller)+r'>:',assembly,re.M)[1],16)
        call = re.search(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2} )+)\s*callq\s+.*<'+re.escape(callee)+r'>',assembly,re.M)
        if not call:
            raise RuntimeError('no independently observable original call')
        offset = int(call[1],16)-start
        length = len(call[2].split())
        output = str(out/(stem+'.witness.json'))
        script = f"""set pagination off
set confirm off
set disable-randomization off
python
import gdb,json
outcome=[]
gdb.events.exited.connect(lambda event: outcome.append(getattr(event,'exit_code',None)))
caller={caller!r}
callee={callee!r}
measurements={incoming!r}
hidden={hidden!r}
b=gdb.Breakpoint('*'+caller)
gdb.execute('run 10')
entry=int(gdb.parse_and_eval('$rsp'))
b.delete()
b=gdb.Breakpoint('*'+caller+'+{offset}')
gdb.execute('continue')
call_sp=int(gdb.parse_and_eval('$rsp'))
assert call_sp%16==0
result_pointer=int(gdb.parse_and_eval('$rdi')) if hidden else None
if hidden: assert result_pointer==entry+hidden['entry_offset']
before=[bytes(gdb.selected_inferior().read_memory(call_sp+x['offset'],x['size'])).hex() for x in measurements]
b.delete()
gdb.execute('stepi')
callee_sp=int(gdb.parse_and_eval('$rsp'))
assert callee_sp==call_sp-8
assert int(gdb.parse_and_eval('$pc'))==int(gdb.parse_and_eval('&'+callee))
after=[bytes(gdb.selected_inferior().read_memory(callee_sp+x['callee_entry_sp_offset'],x['size'])).hex() for x in measurements]
assert before==after
for x in measurements: assert (callee_sp+x['callee_entry_sp_offset'])%x['machine_alignment']==0
b=gdb.Breakpoint('*'+caller+'+{offset+length}')
gdb.execute('continue')
assert int(gdb.parse_and_eval('$rsp'))==call_sp
if hidden:
    assert bytes(gdb.selected_inferior().read_memory(result_pointer,1))==bytes([129])
    assert bytes(gdb.selected_inferior().read_memory(result_pointer+56,1))==bytes([3])
gdb.execute('stepi')
resumed=int(gdb.parse_and_eval('$rsp'))
assert resumed==entry-{frame}
result=dict(original_control={str(executable)!r},callee=callee,caller=caller,frame={frame},
            entry_sp=entry,call_sp=call_sp,callee_sp=callee_sp,after_cleanup_sp=resumed,
            outgoing_below_static_frame=entry-{frame}-call_sp,incoming=measurements,
            incoming_bytes_before=before,incoming_bytes_at_entry=after,hidden_result=hidden,
            hidden_result_pointer=result_pointer)
open({output!r},'w').write(json.dumps(result,indent=2)+'\\n')
b.delete()
gdb.execute('continue')
assert gdb.selected_inferior().pid==0 and outcome==[0]
end
quit
"""
        path = out/(stem+'.gdb')
        path.write_text(script)
        run = subprocess.run(['gdb','-nx','-q','-batch','-x',str(path),str(executable)],
                             capture_output=True,text=True,encoding='utf-8',errors='strict',timeout=90)
        (out/(stem+'.log')).write_text(run.stdout+run.stderr)
        if run.returncode or not Path(output).exists():
            raise RuntimeError('native witness failed: '+stem)
        if hashlib.sha256(executable.read_bytes()).hexdigest()!=control_hash:
            raise RuntimeError('original control changed during witness')
        result = json.loads(Path(output).read_text())
        result['original_control_sha256'] = control_hash
        result['llvm_profile'] = profile
        result['normal_exit_verified'] = True
        Path(output).write_text(json.dumps(result,indent=2)+'\n')
        results.append(result)
    (out/'complete.json').write_text(json.dumps(dict(passed=True,controls=results),indent=2)+'\n')
    print('Two native original-control incoming/cleanup witnesses passed')


if __name__ == '__main__':
    main()
