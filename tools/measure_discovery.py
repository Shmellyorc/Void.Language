"""Complement the void_core probe with real LSP transport cost and response sizes."""
import json,pathlib,sys,tempfile,statistics,time
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID discovery measure ') as d:
    root=pathlib.Path(d);entry=root/'main.void';entry.write_text('func entry(x: i32) -> i32:\n    return x\n')
    for count in (10,1000,10000):
        folder=root/str(count);folder.mkdir()
        for n in range(count):(folder/f'file{100000+n}.void').write_text('not parsed\n')
        with Client(server,entry) as c:
            c.initialize();uri=entry.as_uri();prefix=f'use "{count}/'
            c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':1,'text':prefix}});c.sync()
            times=[];sizes=[]
            for _ in range(7):
                start=time.perf_counter();r=c.request('textDocument/completion',{'textDocument':{'uri':uri},'position':{'line':0,'character':len(prefix)}})[0]['result'];times.append((time.perf_counter()-start)*1000);sizes.append(len(json.dumps(r,separators=(',',':'),ensure_ascii=False).encode()))
                assert len(r['items'])==min(count,256) and r['isIncomplete']==(count>256)
            protocol=[]
            for _ in range(7):
                start=time.perf_counter();c.sync();protocol.append((time.perf_counter()-start)*1000)
            rss=None
            try:rss=next(int(x.split()[1]) for x in pathlib.Path(f'/proc/{c.process.pid}/status').read_text().splitlines() if x.startswith('VmHWM:'))
            except (OSError,StopIteration):pass
            print(json.dumps({'siblings':count,'lsp_first_ms':times[0],'lsp_median_ms':statistics.median(times),'lsp_max_ms':max(times),'barrier_median_ms':statistics.median(protocol),'bytes':sizes[0],'peak_rss_kib':rss}))
