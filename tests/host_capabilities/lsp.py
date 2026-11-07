from pathlib import Path
import sys,tempfile
import support as h
sys.path.insert(0,str(h.ROOT/'tools'))
from lsp_client import Client
server=Path(sys.argv[1]).resolve()
source='''use os as system
use crypto as crypt
use archive as archives
use process as children
use io as files
use convert
func main():
    var operands = system.args()
    var digest = convert.unwrap(crypt.sha256())
    var updated = digest.update([])
    var hashed = digest.finish()
    var released = digest.close()
    var signature = crypt.verify_ed25519([], [], [])
    var root = convert.unwrap(files.create_private_directory("/tmp/new"))
    var admitted = files.open_private_directory("/tmp/owned")
    var nested = root.create_directory("stage")
    var opened = root.open_directory("stage")
    var handle = convert.unwrap(root.create_file("data"))
    var existing = root.open_file("data")
    var permission = handle.private_mode(false)
    var reservation = convert.unwrap(root.lock("writer.lock"))
    var synchronized = root.sync()
    var publication = root.publish(root, "published")
    var record = root.publish_file(handle, "data", root, "record", true)
    var reader = convert.unwrap(archives.open_zip(handle))
    var header = reader.next()
    var chunk = reader.read(10)
    var zip_closed = reader.close()
    var running = convert.unwrap(children.start("/bin/true", [], [], "/tmp"))
    var termination = running.wait()
    var child_closed = running.close()
    var lock_closed = reservation.close()
    var root_closed = root.close()
    digest.deinit()
    reader.deinit()
    running.deinit()
    reservation.deinit()
    root.deinit()
'''
needles=['system.args','crypt.sha256','digest.update','digest.finish','digest.close','crypt.verify_ed25519','files.create_private_directory','files.open_private_directory','root.create_directory','root.open_directory','root.create_file','root.open_file','handle.private_mode','root.lock','root.sync','root.publish(','root.publish_file','archives.open_zip','reader.next','reader.read','reader.close','children.start','running.wait','running.close','reservation.close','root.close','digest.deinit','reader.deinit','running.deinit','reservation.deinit','root.deinit']
with tempfile.TemporaryDirectory(prefix='VOID host LSP ') as directory:
 entry=Path(directory)/'main.void';entry.write_text(source)
 with Client(server,entry) as client:
  client.request('initialize',{'capabilities':{}});client.notify('initialized',{});client.sync()
  client.notify('textDocument/didOpen',{'textDocument':{'uri':entry.as_uri(),'languageId':'void','version':1,'text':source}})
  notes=client.sync();assert all(not n['diagnostics'] for n in notes),notes
  for needle in needles:
   offset=source.index(needle)+needle.index('.')+1;prefix=source[:offset];position={'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
   params={'textDocument':{'uri':entry.as_uri()},'position':position}
   hover=client.request('textDocument/hover',params)[0];definition=client.request('textDocument/definition',params)[0]
   assert hover.get('result') and len(hover['result']['contents']['value'])>40,(needle,hover)
   assert '@stdlib' in str(definition) or 'stdlib' in str(definition),(needle,definition)
print(f'{len(needles)} new API declarations: compiler-owned hover and navigation passed')
