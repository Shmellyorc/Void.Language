"""Bounded correctness replay of unchanged Stage 7 graph workloads, preserving exits."""
import qualify as h
import sys,resource,json,shutil,time,os,inspect
sys.path.insert(0,str(h.ROOT/'docs/research/stage7'));import measure_baseline as base
Original=base.Client;label=sys.argv[1];build=sys.argv[2];out=h.E/(label+'-capture');out.mkdir();clients=[]
def limits():
 h.stack()
class RecordingClient(Original):
 def __init__(self,executable,entry,**kw):
  folder=out/('client-'+str(len(clients)));folder.mkdir();shutil.copytree(entry.parent,folder/'source')
  super().__init__(executable,entry,preexec_fn=limits);self.capture=folder;clients.append(self)
  (folder/'limits.txt').write_text(open('/proc/'+str(self.process.pid)+'/limits').read())
 def preserve(self,name):
  self.logger.join(timeout=1)
  (self.capture/(name+'.json')).write_text(json.dumps({'returncode':self.process.poll(),'stderr':self.logs.decode(errors='replace'),'time':time.time()},indent=2))
 def receive(self,*args,**kw):
  try:return super().receive(*args,**kw)
  except BaseException:
   try:self.process.wait(timeout=2)
   except Exception:pass
   self.preserve('failure');raise
 def finish(self,*args,**kw):
  try:return super().finish(*args,**kw)
  finally:self.preserve('final')
base.Client=RecordingClient
original_timed=base.timed
counter=0
def timed(action):
 global counter
 elapsed,result=original_timed(action);frame=inspect.currentframe().f_back
 data={'index':counter,'elapsed_ms':elapsed,'function':frame.f_code.co_name,'line':frame.f_lineno};counter+=1
 with (out/'completed-actions.jsonl').open('a') as f:f.write(json.dumps(data)+'\n')
 return elapsed,result
base.timed=timed
sys.argv=['measure_baseline.py','--build',build,'--repetitions','3'];os.chdir(out)
try:base.main()
finally:
 for c in clients:
  c.preserve('at-exit')
  if c.process.poll() is None:
   c.process.terminate()
   try:c.process.wait(timeout=3)
   except Exception:c.process.kill();c.process.wait()
   c.preserve('terminated-after-failure')
