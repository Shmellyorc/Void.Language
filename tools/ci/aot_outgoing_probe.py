"""Native Linux x64 outgoing-SP witnesses, LLVM 19.1.7 only.

Uses saved focused-fixture optimized IR to compile distinct ordinary controls.
Never links a staged/rejected object. GDB measures caller instructions, stepping
OVER callees, so callee frames/return addresses cannot inflate caller storage.
The machine CFG observation is diagnostic only, not an encoded-CFG certificate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import resource
import subprocess
import struct

HOST = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
static int visits;
int64_t sink0(void *p) { *(char*)p=7; ++visits; return 42; }
int64_t sink7(void *p,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f) {
 assert(a==1001 && b==1002 && c==1003 && d==1004 && e==1005 && f==1006);
 *(char*)p=7; ++visits; return a+b+c+d+e+f;
}
int64_t sink11(void *p,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f,
               int64_t g,int64_t h,int64_t i,int64_t j) {
 assert(g==1007 && h==1008 && i==1009 && j==1010);
 int64_t first=sink7(p,a,b,c,d,e,f); return first+g+h+i+j;
}
struct Large { int64_t values[8]; };
struct Large large(void) { ++visits; return (struct Large){{1,2,3,4,5,6,7,8}}; }
extern int64_t sample(int);
int main(int argc,char **argv) {
 assert(argc==2); int go=atoi(argv[1]); int64_t result=sample(go);
 assert(result==EXPECTED && visits==VISITS);
 puts("ordinary independently linked outgoing control passed"); return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', required=True, type=Path)
    parser.add_argument('--llvm-bin', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if platform.system()!='Linux' or platform.machine()!='x86_64':
        parser.error('native Linux x64 only')
    out=args.output.resolve(); out.mkdir(parents=True,exist_ok=False)
    (out/'complete.json').write_text('{"passed": false}\n')
    commands=[]; results=[]

    def run(argv,label,stack=None):
        def limit():
            resource.setrlimit(resource.RLIMIT_STACK,(stack,stack))
        proc=subprocess.run([str(x) for x in argv],capture_output=True,text=True,
                            encoding='utf-8',errors='strict',timeout=120,
                            preexec_fn=limit if stack else None)
        (out/(label+'.log')).write_text(proc.stdout+proc.stderr)
        commands.append(dict(argv=[str(x) for x in argv],returncode=proc.returncode,stack=stack))
        (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        if proc.returncode:
            raise RuntimeError(label+' failed; see retained log')
        return proc.stdout

    for name in ('llc','clang','llvm-nm'):
        if not re.search(r'version 19\.1\.7(?:\D|$)',run([args.llvm_bin/name,'--version'],name+'-version')):
            raise RuntimeError('LLVM 19.1.7 required')
    host=out/'host.c'; host.write_text(HOST)
    cases=dict(reserved=(42,1), **{'reserved-padding':(6021,1), 'padding':(10055,1),
               'multiple':(16076,2), 'branch':(None,1), 'loop':(10055,3),
               'near-under':(10055,1), 'near-limit':(10055,1), 'hidden-result':(8,1)})
    for mode in ('normal','O2'):
        for case,(expected,visits) in cases.items():
            label=case+'-'+mode; source=args.fixtures/('outgoing-'+label+'.ll')
            cert_path=source.with_suffix('.json')
            cert=json.loads(cert_path.read_text()); profile=json.loads(cert['profile'])
            if profile['llvm']!='19.1.7' or 'linux' not in profile['triple'] or not profile['triple'].startswith('x86_64'):
                raise RuntimeError('unexpected fixture target/profile')
            if profile['ir_pipeline']!=('verify-only' if mode=='normal' else 'default<O2>;PassBuilder(no-TM)'):
                raise RuntimeError('unexpected optimization identity')
            f=cert['functions']['sample']; obs=f['outgoing_sp_observation']
            if not obs['complete_machine_observation'] or obs['final_encoded_cfg_bound']:
                raise RuntimeError('wrong diagnostic/certification classification')
            saved=out/(label+'.ll'); saved.write_bytes(source.read_bytes())
            obj=out/(label+'.ordinary-control.o')
            if obj.resolve()==source.with_suffix('.o').resolve():
                raise RuntimeError('control/candidate collision')
            run([args.llvm_bin/'llc','-O2','-relocation-model=pic','-mcpu=generic','-filetype=obj',saved,'-o',obj],label+'-compile')
            for go in (0,1):
                wanted=(6021 if go else 10055) if expected is None else expected
                exe=out/(label+'-'+str(go)+'.ordinary-control')
                run([args.llvm_bin/'clang','-O2','-std=c11','-Wall','-Wextra','-Werror',
                     '-DEXPECTED='+str(wanted),'-DVISITS='+str(visits),host,obj,'-o',exe],label+'-'+str(go)+'-link')
                for stack in (None,1048576):
                    run([exe,go],label+'-'+str(go)+('-1m' if stack else '-default'),stack)
                symbols=run([args.llvm_bin/'llvm-nm','--print-size','--defined-only',exe],label+'-'+str(go)+'-symbols')
                size=int(re.search(r'^\w+\s+(\w+)\s+[Tt]\s+sample$',symbols,re.M)[1],16)
                witness=out/(label+'-'+str(go)+'.witness.json')
                script=out/(label+'-'+str(go)+'.gdb')
                script.write_text(f'''set pagination off
set confirm off
set disable-randomization off
python
import gdb,json
outcome=[]
gdb.events.exited.connect(lambda event:outcome.append(getattr(event,'exit_code',None)))
b=gdb.Breakpoint('*sample')
gdb.execute('run {go}')
b.delete()
start=int(gdb.parse_and_eval('$pc')); end=start+{size}
entry=int(gdb.parse_and_eval('$rsp')); peak=0; calls=[]; steps=0
while start<=int(gdb.parse_and_eval('$pc'))<end:
    steps+=1
    assert steps<10000
    pc=int(gdb.parse_and_eval('$pc')); sp=int(gdb.parse_and_eval('$rsp'))
    depth=entry-sp; assert depth>=0
    peak=max(peak,depth)
    inst=gdb.selected_frame().architecture().disassemble(pc,count=1)[0]['asm']
    if inst.startswith('call'):
        assert sp%16==0
        calls.append(dict(offset=pc-start,depth=depth,sp_aligned=True))
        gdb.execute('nexti',to_string=True)
        assert int(gdb.parse_and_eval('$rsp'))==sp
    elif inst.startswith('ret'):
        assert depth==0
        gdb.execute('stepi',to_string=True)
        break
    else:
        gdb.execute('stepi',to_string=True)
assert peak<={obs['peak_entry_sp_depth']}
open({str(witness)!r},'w').write(json.dumps(dict(peak=peak,calls=calls,steps=steps),indent=2)+'\\n')
gdb.execute('continue')
assert outcome==[0]
end
quit
''')
                digest=hashlib.sha256(exe.read_bytes()).hexdigest()
                run(['gdb','-nx','-q','-batch','-x',script,exe],label+'-'+str(go)+'-debugger')
                if hashlib.sha256(exe.read_bytes()).hexdigest()!=digest:
                    raise RuntimeError('ordinary control changed')
                result=json.loads(witness.read_text())
                allowed={c['entry_sp_depth'] for c in obs['calls']}
                if any(c['depth'] not in allowed for c in result['calls']):
                    raise RuntimeError('native call depth missing from machine observation')
                result.update(case=case,mode=mode,argument=go,F=f['frame'],
                              machine_peak=obs['peak_entry_sp_depth'],machine_additional=obs['additional_below_F'],
                              actual_additional=max(0,result['peak']-f['frame']),
                              ordinary_control_sha256=digest,staged_candidate_delivered=cert['certified_delivery'],
                              native_stack_runs=[None,1048576],profile=profile)
                witness.write_text(json.dumps(result,indent=2)+'\n'); results.append(result)
    # Both alternatives of every branch are sampled; repeated execution must not
    # accumulate depth. This finite witness corroborates, never proves all paths.
    for mode in ('normal','O2'):
        for case in cases:
            pair=[r for r in results if r['mode']==mode and r['case']==case]
            if max(r['peak'] for r in pair)!=pair[0]['machine_peak']:
                raise RuntimeError('machine peak not reached by native witness: '+case+'/'+mode)
    # A bytes-only negative: changing a conditional target can repeat a frame
    # setup without changing either flattened SP-writer or call list. Do not run
    # the patched file or route it through delivery. This is evidence of the
    # current verifier's information gap, not a compiler-miscompile claim.
    original=out/'branch-normal-0.ordinary-control'
    data=bytearray(original.read_bytes())
    if data[:6]!=b'\x7fELF\x02\x01':
        raise RuntimeError('ELF64 little-endian control required')
    def disassembly(path,label):
        return run([args.llvm_bin/'llvm-objdump','-d','--disassemble-symbols=sample',path],label)
    before=disassembly(original,'branch-original-bytes')
    start=int(re.search(r'^([0-9a-f]+) <sample>:',before,re.M)[1],16)
    lines=[]
    for line in before.splitlines():
        match=re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2} )+)\s*(.*)$',line)
        if match:
            lines.append((int(match[1],16),bytes.fromhex(match[2]),match[3]))
    branch=next((address,code,asm) for address,code,asm in lines
                if (0x70<=code[0]<=0x7f or code[:1]==b'\x0f' and len(code)>1 and 0x80<=code[1]<=0x8f))
    address,code,asm=branch
    width=1 if len(code)==2 else 4
    displacement=start-(address+len(code))
    if not -(1<<(8*width-1))<=displacement<(1<<(8*width-1)):
        raise RuntimeError('negative branch displacement does not fit')
    phoff=struct.unpack_from('<Q',data,32)[0]
    phsize,phcount=struct.unpack_from('<HH',data,54)
    offsets=[]
    for index in range(phcount):
        header=phoff+index*phsize
        kind,flags,offset,vaddr,paddr,filesz,memsz,alignment=struct.unpack_from('<IIQQQQQQ',data,header)
        if kind==1 and vaddr<=address and address+len(code)<=vaddr+filesz:
            offsets.append(offset+address-vaddr)
    if len(offsets)!=1 or data[offsets[0]:offsets[0]+len(code)]!=code:
        raise RuntimeError('cannot uniquely locate branch bytes')
    replacement=code[:-width]+displacement.to_bytes(width,'little',signed=True)
    data[offsets[0]:offsets[0]+len(code)]=replacement
    negative=out/'unsupported-branch-target.bytes-only-negative'
    negative.write_bytes(data)
    after=disassembly(negative,'branch-negative-bytes')
    def flattened(text):
        return [line.split(':',1)[1].split('\t')[-1] for line in text.splitlines()
                if re.search(r'\b(callq?|pushq?|popq?)\b|\b(addq|subq)\s+.*%rsp',line)]
    if before==after or flattened(before)!=flattened(after):
        raise RuntimeError('branch-only negative did not preserve SP/call lists')
    gap=dict(original_sha256=hashlib.sha256(original.read_bytes()).hexdigest(),
             negative_sha256=hashlib.sha256(data).hexdigest(),branch_address=address,
             original_branch=asm,original_bytes=code.hex(),negative_bytes=replacement.hex(),
             new_target=start,flattened_sp_and_calls_unchanged=True,never_executed=True,
             implication='linear SP/call comparison cannot certify encoded CFG path multiplicity')
    (out/'encoded-cfg-gap.json').write_text(json.dumps(gap,indent=2)+'\n')
    (out/'complete.json').write_text(json.dumps(dict(passed=True,results=results,encoded_cfg_gap=gap),indent=2)+'\n')
    print('36 native debugger witnesses and 72 ordinary-control runs passed (36 at 1 MiB)')


if __name__=='__main__':
    main()
