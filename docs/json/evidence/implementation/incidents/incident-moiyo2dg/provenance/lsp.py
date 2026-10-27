from pathlib import Path
import os, sys, tempfile, json, resource
import harness as h
from crash import install
install()
sys.path.insert(0,str(h.ROOT/'tools'))
from lsp_client import Client
checks=0

def check(test,detail):
    global checks
    assert test,detail
    checks+=1

def point(text,needle,delta=0):
    before=text[:text.index(needle)+delta]
    return {'line':before.count('\n'),'character':len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2}

base='''use json as json
use convert
struct data:
    score: i32
func main():
    var encoded = "😀" + convert.unwrap(json.stringify(1))
    var decoded = json.parse_as<data>("{{\\"score\\":1}}")
    if json.parse("0") is Ok(var node):
        var number = node.as_number<u64>()
'''
with tempfile.TemporaryDirectory(prefix='void-json-lsp-') as tmp:
    oldcwd=Path.cwd();os.chdir(tmp)
    try:
        entry=Path(tmp)/'main.void';entry.write_text(base)
        oldstack=resource.getrlimit(resource.RLIMIT_STACK)
        resource.setrlimit(resource.RLIMIT_STACK,(1048576,oldstack[1]))
        client=Client(h.compiler,entry)
        resource.setrlimit(resource.RLIMIT_STACK,oldstack)
        observed=(Path('/proc')/str(client.process.pid)/'limits').read_text()
        check('1048576' in next(x for x in observed.splitlines() if 'Max stack size' in x),observed)
        with client:
            client.request('initialize',{'capabilities':{'textDocument':{'signatureHelp':{'signatureInformation':{'documentationFormat':['plaintext']}}}}})
            client.notify('initialized',{});client.sync();version=0;text=base
            def replace(value):
                global version,text
                text=value;version+=1
                client.notify('textDocument/didOpen' if version==1 else 'textDocument/didChange',{'textDocument':{'uri':entry.as_uri(),'languageId':'void','version':version,'text':value}} if version==1 else {'textDocument':{'uri':entry.as_uri(),'version':version},'contentChanges':[{'text':value}]})
                return client.sync()
            def query(method,needle,delta=0,**extra):
                response=client.request('textDocument/'+method,{'textDocument':{'uri':entry.as_uri()},'position':point(text,needle,delta),**extra})[0]
                check('error' not in response,response)
                return response['result']
            notes=replace(base);check(all(not x['diagnostics'] for x in notes),notes)
            for spelling,description in [('json.stringify','Encode supported typed data'),('json.parse_as','Parse strict JSON'),('node.as_number','Extract a JSON number')]:
                offset=spelling.index('.')+2
                hover=query('hover',spelling,offset)
                check(description in str(hover),(spelling,hover))
                definition=query('definition',spelling,offset)
                check('json.void' in str(definition),definition)
            replace('use json as json\nfunc main():\n    var result = json.')
            items=query('completion','json.',5);items=items.get('items',[]) if isinstance(items,dict) else items
            labels={x['label'] for x in items}
            check({'parse','parse_as','to_value','from_value','stringify','value','limits','null_value'}<=labels,labels)
            check(not any(x.startswith('_') for x in labels),labels)
            for call,description in [('json.parse_as<[i32]>(','Valid VOID Unicode text'),('json.stringify(','Static public data schema')]:
                replace('use json as json\nfunc main():\n    var result = '+call)
                result=query('signatureHelp',call,len(call),context={'triggerKind':1,'isRetrigger':False})
                check(bool(result),(call,result))
                check(description in str(result),(call,result))
                check('_json_' not in str(result),result)
            replace('use json as json\nfunc main():\n    if json.parse("0") is Ok(var node):\n        var result = node.as_number<u64>(')
            result=query('signatureHelp','node.as_number<u64>(',len('node.as_number<u64>('),context={'triggerKind':1,'isRetrigger':False})
            check(bool(result) and 'as_number' in str(result),result)
            replace(base)
            denied=client.request('textDocument/rename',{'textDocument':{'uri':entry.as_uri()},'position':point(text,'json.parse_as',6),'newName':'changed'})[0]
            check('read-only' in str(denied),denied)
            client.shutdown()
    finally:os.chdir(oldcwd)
h.receipt.write_text(json.dumps({'profile':h.profile,'server_sha256':h.digest(h.compiler),'checks':checks,'stack_bytes_observed':1048576,'stdlib_hashes':{str(p):h.digest(p) for p in (h.ROOT/'stdlib').rglob('*.void')}},indent=2)+'\n')
print(f'JSON LSP: {checks} checks passed')
