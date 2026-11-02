from pathlib import Path
import json,sys,threading,time,subprocess
root=Path.cwd(); sys.path.insert(0,str(root/'tests/net/http'))
from examples import CancellationHandler
from dns_fixture import DNS
import fixture
class Delayed(DNS):
 def answer(self,data,transport):
  time.sleep(.15);return super().answer(data,transport)
class Producer(CancellationHandler):
 def do_POST(self):
  self.connection.settimeout(5)
  first=self.rfile.read(1024);self.server.events.append(('first',len(first),time.monotonic()-self.server.started));self.server.pending.set()
  second=self.rfile.read(1024);self.server.events.append(('second',len(second),time.monotonic()-self.server.started))
  if second:self.reply(b'ok')
for variant in ('before','after'):
 dns=Delayed();peer=fixture.Server(('127.0.0.1',0),Producer)
 peer.pending,peer.closed,peer.stopping=threading.Event(),threading.Event(),threading.Event();peer.events=[];peer.started=time.monotonic();peer.fail_barrier=False
 threading.Thread(target=peer.serve_forever,daemon=True).start()
 argv=[str(root/f'.toolchains/ci41443618378/control/dns-{variant}'),'--connection-wait',f'http://pending.void.test:{peer.server_port}',dns.endpoint,'producer']
 r=subprocess.run(argv,capture_output=True,timeout=15)
 record=dict(argv=argv,code=r.returncode,stdout=r.stdout.decode(),stderr=r.stderr.decode(),events=peer.events)
 (root/f'.toolchains/ci41443618378/producer-{variant}.json').write_text(json.dumps(record,indent=2));print(json.dumps(record))
 peer.stopping.set();peer.shutdown();peer.server_close();dns.close()
