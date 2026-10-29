"""Compiler-owned source docs, alias navigation, incomplete calls and protected stdlib rename."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
from crash import install
install()
sys.path.insert(0,str(h.ROOT/'tools'))
from lsp_client import Client
server=Path(sys.argv[1]).resolve();checks=0
def check(yes,message):
    global checks
    assert yes,message
    checks+=1
def point(text,needle,delta=0):
    prefix=text[:text.index(needle)+delta]
    return {'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}
with tempfile.TemporaryDirectory(prefix='VOID OS LSP ') as directory:
    entry=Path(directory)/'main.void'
    base='use os as system\nfunc main():\n    var face = "😀"\n    var location = system.data_dir("com.game")\n    var process = system.process_id()\n'
    entry.write_text(base, encoding='utf-8')
    with Client(server,entry) as client:
        client.request('initialize',{'capabilities':{'textDocument':{'signatureHelp':{'signatureInformation':{'documentationFormat':['plaintext']}}}}})
        client.notify('initialized',{});client.sync()
        text=base;version=0
        def replace(value):
            global text,version
            text=value;version+=1
            if version==1:client.notify('textDocument/didOpen',{'textDocument':{'uri':entry.as_uri(),'languageId':'void','version':version,'text':value}})
            else:client.notify('textDocument/didChange',{'textDocument':{'uri':entry.as_uri(),'version':version},'contentChanges':[{'text':value}]})
            return client.sync()
        def query(method,needle,delta=0,**extra):
            r=client.request('textDocument/'+method,{'textDocument':{'uri':entry.as_uri()},'position':point(text,needle,delta),**extra})[0]
            check('error' not in r,r)
            return r.get('result')
        notes=replace(base);check(all(not n['diagnostics'] for n in notes),notes)
        for symbol,phrase in [('data_dir','validated application'),('process_id','compiler/embedding host PID')]:
            hover=query('hover','system.'+symbol,8)['contents']['value']
            check(phrase in hover,hover)
            check('os.void' in str(query('definition','system.'+symbol,8)),'definition')
        refs=query('references','system.data_dir',8,context={'includeDeclaration':True});check(bool(refs),refs)
        denied=client.request('textDocument/rename',{'textDocument':{'uri':entry.as_uri()},'position':point(text,'system.data_dir',8),'newName':'renamed'})[0]
        check('read-only' in str(denied),denied)
        replace('use os\nfunc main():\n    var item = os.')
        response=query('completion','os.',3)
        items=response.get('items',[]) if isinstance(response,dict) else response
        names={item['label'] for item in items}
        check({'data_dir','config_dir','cache_dir','state_dir','home_dir','temp_dir','executable_path','resource_dir','env','platform','architecture','process_id','is_bundle','error','error_kind'}<=names,names)
        check(not any(name.startswith('_') for name in names),names)
        replace('use os\nfunc main():\n    var issue = os.error(kind = os.error_kind.invalid_argument)\n    var item = issue.')
        response=query('completion','issue.',6)
        items=response.get('items',[]) if isinstance(response,dict) else response
        fields={item['label'] for item in items}
        check({'message','path','code','operation','kind','os_code','native_domain'}<=fields,fields)
        replace('use os\nfunc main():\n    var item = os.error_kind.')
        response=query('completion','os.error_kind.',14)
        items=response.get('items',[]) if isinstance(response,dict) else response
        kinds={item['label'] for item in items}
        check({'not_found','invalid_argument','invalid_path','invalid_utf8','permission_denied','unavailable','unsupported','system_failure'}<=kinds,kinds)
        for function,param in [('data_dir','app_id'),('config_dir','app_id'),('cache_dir','app_id'),('state_dir','app_id'),('env','name')]:
            call='system.'+function+'('
            replace('use os as system\nfunc main():\n    var item = '+call)
            signature=query('signatureHelp',call,len(call))
            check(bool(signature),signature)
            shown=signature['signatures'][0]
            check(shown['parameters'][0]['label']==param+': string',shown)
            check(bool(shown['parameters'][0].get('documentation')),shown)
        replace('use o')
        response=query('completion','o',1)
        items=response.get('items',[]) if isinstance(response,dict) else response
        check(any(item['label']=='os' or item['label']=='os.void' for item in items),items)
        # Unsaved Unicode line places a query after an astral scalar on the same line.
        unicode='use os as system\nfunc main():\n    print("😀", system.process_id())\n'
        replace(unicode)
        check('Native getpid' in query('hover','system.process_id',9)['contents']['value'],'UTF-16 position')
        # Repair the incomplete overlay without changing disk or restarting the server.
        notes=replace(base);check(all(not n['diagnostics'] for n in notes),notes)
        client.shutdown()
print(f'OS LSP: {checks} checks passed')
