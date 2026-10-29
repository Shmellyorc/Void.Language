"""Read-only Linux x64 encoded-CFG adversarial experiment; no code publication.

Requires current compiler fixtures and Part 4 ordinary controls. Never executes a
negative, never changes its input, and never treats CFG success as certification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--audit',required=True,type=Path)
    p.add_argument('--fixtures',required=True,type=Path)
    p.add_argument('--controls',required=True,type=Path)
    p.add_argument('--llvm-bin',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    (out/'complete.json').write_text('{"passed":false}\n');results=[]

    def verify(obj,name,expected,needle='',cert=None,mode='normal'):
        before=hashlib.sha256(obj.read_bytes()).hexdigest();report=out/(name+'.json')
        run=subprocess.run([str(a.audit),'--research-cfg',str(obj),'sample',str(cert) if cert else '-',str(report),mode],
                           capture_output=True,text=True,encoding='utf-8',errors='strict',timeout=90)
        (out/(name+'.log')).write_text(run.stdout+run.stderr)
        data=json.loads(report.read_text())
        if run.returncode!=(0 if expected else 2) or data['supported']!=expected:
            raise RuntimeError(name+': unexpected result '+str(data.get('error')))
        if needle and needle not in data.get('error',''):
            raise RuntimeError(name+': wrong rejection '+str(data.get('error')))
        if before!=hashlib.sha256(obj.read_bytes()).hexdigest() or data['object_sha256'].lower()!=before:
            raise RuntimeError(name+': original bytes/hash changed')
        if data['delivery_credit'] or data['abi_ownership_proved'] or data['physical_memory_bound_proved']:
            raise RuntimeError(name+': CFG proof granted other credit')
        results.append(dict(name=name,supported=expected,error=data.get('error'),object_sha256=before))
        return data

    # Exact retained Part 4 mandatory pair, with fresh ordered machine observations.
    original=a.controls/'branch-normal-0.ordinary-control'
    negative=a.controls/'unsupported-branch-target.bytes-only-negative'
    gap=json.loads((a.controls/'encoded-cfg-gap.json').read_text())
    assert hashlib.sha256(original.read_bytes()).hexdigest()==gap['original_sha256']
    assert hashlib.sha256(negative.read_bytes()).hexdigest()==gap['negative_sha256']
    assert gap['flattened_sp_and_calls_unchanged'] and gap['never_executed']
    flattened=[]
    for file,label in ((original,'original'),(negative,'retarget')):
        decoded=subprocess.run([str(a.llvm_bin/'llvm-objdump'),'-d','--disassemble-symbols=sample',str(file)],
                               check=True,capture_output=True,text=True,encoding='utf-8',errors='strict').stdout
        (out/('mandatory-'+label+'.disassembly')).write_text(decoded)
        events=[]
        for line in decoded.splitlines():
            match=re.match(r'^\s*[0-9a-f]+:\s*(?:[0-9a-f]{2} )+\s*(.*)$',line)
            if match and re.search(r'^callq?\b|^pushq?\b|^popq?\b|^(?:addq|subq)\s+.*%rsp',match[1]):events.append(match[1])
        flattened.append(events)
    assert flattened[0] and flattened[0]==flattened[1]
    (out/'mandatory-flattened-equality.json').write_text(json.dumps(dict(equal=True,events=flattened[0]),indent=2)+'\n')
    cert=a.fixtures/'outgoing-branch-normal.json'
    verify(original,'part4-original',True,cert=cert)
    verify(negative,'part4-retarget',False,'inconsistent join/growing stack cycle',cert=cert)
    verify(original,'part4-original-without-machine',True)
    verify(negative,'part4-retarget-without-machine',False,'inconsistent join/growing stack cycle')
    for mode in ('normal','O2'):
        for case in ('reserved','reserved-padding','padding','multiple','branch','loop','near-under','near-limit','hidden-result'):
            current=a.fixtures/('outgoing-'+case+'-'+mode+'.json')
            for go in (0,1):
                verify(a.controls/(case+'-'+mode+'-'+str(go)+'.ordinary-control'),case+'-'+mode+'-'+str(go),True,cert=current,mode=mode)
            verify(a.controls/(case+'-'+mode+'.ordinary-control.o'),case+'-'+mode+'-relocatable',True,cert=current,mode=mode)
    verify(original,'profile-mismatch',False,'profile mismatch',cert=cert,mode='O2')

    # Machine observations are comparison evidence, never authority over bytes.
    original_certificate=json.loads(cert.read_text())
    for alteration in ('missing-events','changed-depth','missing-edge','missing-delta'):
        changed=json.loads(json.dumps(original_certificate))
        observation=changed['functions']['sample']['outgoing_sp_observation']
        blocks=observation['blocks']
        if alteration=='missing-events':
            blocks[0].pop('ordered_events')
        elif alteration=='changed-depth':
            event=next(e for b in blocks for e in b['ordered_events'] if e['kind']=='sp')
            event['depth']+=8
        elif alteration=='missing-delta':
            event=next(e for b in blocks for e in b['ordered_events'] if e['kind']=='sp')
            event.pop('delta')
        else:
            block=next(b for b in blocks if len(b['successors'])==2)
            block['successors'].pop()
        path=out/(alteration+'.machine.json');path.write_text(json.dumps(changed))
        verify(original,alteration,False,'machine',cert=path)

    malformed=out/'malformed.machine.json';malformed.write_text('{')
    verify(original,'malformed-machine-JSON',False,'invalid machine certificate',cert=malformed)

    # Regression failures may overwrite ONLY disposable copies, never the retained inputs.
    collision_object=out/'collision-input.ordinary-copy';collision_object.write_bytes(original.read_bytes())
    collision_certificate=out/'collision-input.machine.json';collision_certificate.write_bytes(cert.read_bytes())
    for evidence in (collision_object,collision_certificate):
        before=evidence.read_bytes()
        run=subprocess.run([str(a.audit),'--research-cfg',str(collision_object),'sample',str(collision_certificate),str(evidence),'normal'],
                           capture_output=True,text=True,encoding='utf-8',errors='strict',timeout=90)
        if run.returncode!=1 or 'must not replace input evidence' not in run.stderr or evidence.read_bytes()!=before:
            raise RuntimeError('research report/input collision damaged evidence')
        results.append(dict(name='input-report-collision-'+evidence.name,supported=False,error='report aliases input',object_sha256=hashlib.sha256(before).hexdigest()))

    # Replace ONLY the selected ordinary control's function interval in copies.
    symbols=subprocess.run([str(a.llvm_bin/'llvm-nm'),'-S','--defined-only',str(original)],
                           check=True,capture_output=True,text=True).stdout
    match=re.search(r'^(\w+)\s+(\w+)\s+[Tt]\s+sample$',symbols,re.M)
    address,size=(int(x,16) for x in match.groups());raw=original.read_bytes()
    phoff=struct.unpack_from('<Q',raw,32)[0];phsize,phcount=struct.unpack_from('<HH',raw,54)
    places=[]
    for k in range(phcount):
        kind,flags,offset,vaddr,paddr,filesz,memsz,alignment=struct.unpack_from('<IIQQQQQQ',raw,phoff+k*phsize)
        if kind==1 and vaddr<=address and address+size<=vaddr+filesz:places.append(offset+address-vaddr)
    assert len(places)==1
    def synthetic(name,prefix,expected,needle='',last=b'\xc3'):
        body=prefix+b'\x90'*(size-len(prefix)-len(last))+last
        data=bytearray(raw);data[places[0]:places[0]+size]=body
        path=out/(name+'.never-executed');path.write_bytes(data)
        return verify(path,name,expected,needle)
    synthetic('balanced-conditional',bytes.fromhex('4883ec0885ff7401904883c408'),True)
    synthetic('growing-cycle',bytes.fromhex('4883ec0885ff74f84883c408'),False,'inconsistent join/growing stack cycle')
    synthetic('middle-of-instruction',bytes.fromhex('4883ec0885ff74f94883c408'),False,'middle of instruction')
    synthetic('unknown-instruction',bytes.fromhex('0f0b'),False,'unsupported instruction')
    synthetic('indirect-jump',bytes.fromhex('ffe0'),False,'unsupported instruction')
    synthetic('indirect-call',bytes.fromhex('ffd0'),False,'unsupported instruction')
    wide=synthetic('wide-signed-push',bytes.fromhex('68ffffffff58'),True)
    narrow=synthetic('narrow-signed-push',bytes.fromhex('6aff58'),True)
    assert wide['event_graph']==narrow['event_graph'] # Stack effect only, no production encoding credit.
    synthetic('unknown-SP-write',bytes.fromhex('4889fc'),False,'unknown SP writer')
    synthetic('unbalanced-return',bytes.fromhex('4883ec08'),False,'unbalanced return')
    synthetic('missing-fallthrough',b'',False,'missing fallthrough',last=b'\x90')
    synthetic('conditional-without-fallthrough',b'',False,'missing fallthrough',last=bytes.fromhex('75fc'))
    synthetic('truncated-instruction',b'',False,'undecodable',last=bytes.fromhex('0f'))
    synthetic('unequal-join',bytes.fromhex('85ff74044883ec08'),False,'inconsistent join')
    synthetic('out-of-function',bytes.fromhex('eb80'),False,'out-of-function')
    synthetic('unreachable-bytes',b'\xe9'+(size-6).to_bytes(4,'little',signed=True),False,'incomplete reachable code coverage')
    synthetic('event-free-cycle',b'',False,'event-free CFG cycle',last=b'\xe9'+(-size).to_bytes(4,'little',signed=True))
    for name,location,value in (('foreign-OS-ABI',7,9),('foreign-machine',18,183),('unknown-ELF-flags',48,1)):
        changed=bytearray(raw);changed[location]=value
        path=out/(name+'.never-executed');path.write_bytes(changed)
        verify(path,name,False,'unsupported ELF')
    # Unknown call relocation, wrong addend and relocated branch edge.
    source=a.controls/'branch-normal.ordinary-control.o';obj=source.read_bytes()
    shoff=struct.unpack_from('<Q',obj,40)[0];shsize,shcount=struct.unpack_from('<HH',obj,58)
    relocation_sections=[]
    for k in range(shcount):
        header=struct.unpack_from('<IIQQQQIIQQ',obj,shoff+k*shsize)
        if header[1]==4 and header[5]>=24:relocation_sections.append(header)
    text_rela=next(h for h in relocation_sections if h[7] and struct.unpack_from('<IIQQQQIIQQ',obj,shoff+h[7]*shsize)[2]&4)
    at=text_rela[4];offset,info,addend=struct.unpack_from('<QQq',obj,at)
    for name,new_offset,new_info,new_addend,needle in (
        ('unknown-relocation',offset,(info&~0xffffffff)|1,addend,'relocation'),
        ('wrong-call-addend',offset,info,0,'call relocation'),
        ('branch-relocation',10,info,addend,'unsupported relocation'),
        ('function-end-relocation',size-1,info,addend,'relocation overlaps function end')):
        changed=bytearray(obj);struct.pack_into('<QQq',changed,at,new_offset,new_info,new_addend)
        path=out/(name+'.never-executed.o');path.write_bytes(changed)
        verify(path,name,False,needle)
    version=subprocess.run([str(a.llvm_bin/'llvm-mc'),'--version'],check=True,capture_output=True,text=True).stdout
    if not re.search(r'version 23\.1\.4(?:\D|$)',version):raise RuntimeError('LLVM 23.1.4 assembler required')
    for count,expected in ((4095,True),(4096,False)):
        source=out/('resource-'+str(count)+'.s');path=source.with_suffix('.never-executed.o')
        source.write_text('.text\n.globl sample\n.type sample,@function\nsample:\n.rept '+str(count)+'\nnop\n.endr\nret\n.size sample,.-sample\n')
        subprocess.run([str(a.llvm_bin/'llvm-mc'),'-triple=x86_64-pc-linux-gnu','-filetype=obj',str(source),'-o',str(path)],check=True)
        verify(path,'instruction-resource-'+str(count),expected,'' if expected else 'instruction resource bound')
    (out/'complete.json').write_text(json.dumps(dict(passed=True,tests=results,negatives_never_executed=True),indent=2)+'\n')
    print(str(len(results))+' encoded-CFG positive/negative checks passed; no input objects modified or executed')


if __name__=='__main__':main()
