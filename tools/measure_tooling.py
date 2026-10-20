"""Measure Stage 7 queries using the existing research fixture/client harness."""
import argparse, hashlib, json, pathlib, platform, sys, tempfile
repo=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(repo/'docs/research/stage7'))
import measure_baseline as base
from lsp_client import Client
p=argparse.ArgumentParser();p.add_argument('--build',default='build/ci-release');p.add_argument('--repetitions',type=int,default=7);a=p.parse_args()
server=(repo/a.build/'voidls').resolve();result={'host':platform.platform(),'server_sha256':hashlib.sha256(server.read_bytes()).hexdigest(),'repetitions':a.repetitions,'workloads':{}}
def point(text,offset):
    return {'line':text[:offset].count('\n'),'character':len(text[:offset].rsplit('\n',1)[-1].encode('utf-16-le'))//2}
with tempfile.TemporaryDirectory(prefix='VOID query measurements ') as directory:
    for name,modules,functions in [('small',2,5),('medium',16,50),('large',32,100)]:
        entry,text=base.fixture(pathlib.Path(directory)/name,modules,functions)
        text=text.replace('    return ','  return ').replace('x: i32','x:i32')
        with Client(server,entry) as c:
            c.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}})
            c.notify('initialized',{});c.sync()
            c.notify('textDocument/didOpen',{'textDocument':{'uri':entry.as_uri(),'languageId':'void','version':1,'text':text}})
            assert all(not n['diagnostics'] for n in c.sync())
            at=text.index('.fn0')+1
            queries={
                'completion':('textDocument/completion',{'textDocument':{'uri':entry.as_uri()},'position':point(text,at)}),
                'signatureHelp':('textDocument/signatureHelp',{'textDocument':{'uri':entry.as_uri()},'position':point(text,at+4)}),
                'hover':('textDocument/hover',{'textDocument':{'uri':entry.as_uri()},'position':point(text,at)}),
                'rename_preflight':('textDocument/rename',{'textDocument':{'uri':(entry.parent/'m0.void').as_uri()},'position':{'line':0,'character':6},'newName':'renamed_fn'}),
                'formatting_preflight':('textDocument/formatting',{'textDocument':{'uri':entry.as_uri()},'options':{'tabSize':4,'insertSpaces':True}}),
            }
            measured={};version=1
            for feature,(method,params) in queries.items():
                samples=[]
                query_text=text[:at]+'\n' if feature=='completion' else text
                version+=1
                c.notify('textDocument/didChange',{'textDocument':{'uri':entry.as_uri(),'version':version},'contentChanges':[{'text':query_text}]});c.sync()
                for _ in range(a.repetitions):
                    elapsed,(response,notes)=base.timed(lambda:c.request(method,params))
                    assert not notes and response.get('result'),(feature,response,notes)
                    if feature=='rename_preflight':assert len(response['result']['documentChanges'])==2
                    if feature=='formatting_preflight':assert len(response['result'])==1
                    samples.append(elapsed)
                measured[feature]=base.summary(samples)
            measured['rss_after_queries_kib']=base.rss(c.process.pid)
            result['workloads'][name]=measured;c.shutdown()
print(json.dumps(result,indent=2))
