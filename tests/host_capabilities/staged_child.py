"""Explicit Windows test-caller approval of a private executable copy.

The build artifact may live on a runner volume owned by Network Service. That
volume is deliberately outside process.start's trusted ancestry contract. The
native fixture pins and hashes the approved bytes into an exclusive directory
created by the existing private-directory provider on the trusted temp volume.
"""
from pathlib import Path
import hashlib,os,subprocess

def stage_child(executable,work,records):
 if os.name!='nt':return executable
 executable=Path(executable);digest=hashlib.sha256(executable.read_bytes()).hexdigest()
 destination=work/'approved-child'
 def stage(target,expected,success,error=None):
  command=[str(executable),'--stage-executable',str(executable),str(target),expected]
  result=subprocess.run(command,capture_output=True,timeout=120,cwd=work)
  record={'command':command,'status':result.returncode,'stdout':result.stdout.decode(errors='replace'),'stderr':result.stderr.decode(errors='replace'),'expected_success':success}
  records.append(record)
  assert (result.returncode==0)==success,record
  if error is not None:assert error in record['stderr'],record
 stage(work/'wrong-digest-child','0'*64,False,'staged image matches caller-approved digest')
 stage(destination,digest,True)
 child=destination/'child.exe'
 assert hashlib.sha256(child.read_bytes()).hexdigest()==digest
 stage(destination,digest,False,'exclusive trusted staging root') # Never replace an existing root/image.
 assert hashlib.sha256(child.read_bytes()).hexdigest()==digest
 records.append({'approved_source':str(executable),'staged_image':str(child),'sha256':digest,'source_bytes':executable.stat().st_size})
 return child
