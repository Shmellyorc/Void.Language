"""Compiler-owned random discovery, arity presentation and navigation through aliases."""
from pathlib import Path
import sys
import tempfile
import harness as h
from crash import install
install()
sys.path.insert(0,str(h.ROOT/'tools'))
from lsp_client import Client
server=Path(sys.argv[1]).resolve();checks=0

def check(value,message):
    global checks
    assert value,message
    checks+=1

def point(text,needle,delta=0):
    prefix=text[:text.index(needle)+delta]
    return {'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1])}

with tempfile.TemporaryDirectory(prefix='VOID random LSP ') as directory:
    entry=Path(directory)/'main.void'
    base='use random as rng\nfunc main():\n    var engine = rng.generator(seed = 42)\n    var a = rng.rand_i32(10)\n    var b = engine.rand_f32(1.0, 2.0)\n'
    entry.write_text(base)
    with Client(server,entry) as client:
        client.request('initialize',{'capabilities':{'textDocument':{'signatureHelp':{'signatureInformation':{'documentationFormat':['plaintext']}}}}})
        client.notify('initialized',{});client.sync()
        text=base;version=0
        def replace(value):
            global text,version
            text=value;version+=1
            client.notify('textDocument/didOpen' if version==1 else 'textDocument/didChange',
                          {'textDocument':{'uri':entry.as_uri(),'languageId':'void','version':version,'text':value}} if version==1 else {'textDocument':{'uri':entry.as_uri(),'version':version},'contentChanges':[{'text':value}]})
            return client.sync()
        def query(method,needle,delta=0,**extra):
            response=client.request('textDocument/'+method,{'textDocument':{'uri':entry.as_uri()},'position':point(text,needle,delta),**extra})[0]
            check('error' not in response,response)
            return response['result']
        notes=replace(base)
        check(all(not x['diagnostics'] for x in notes),notes)
        for needle in ('rng.rand_i32','engine.rand_f32'):
            delta=needle.index('.')+2
            hover=query('hover',needle,delta)['contents']['value']
            check('_arity_' not in hover,hover)
            check('maximum' in hover or 'lower' in hover,hover)
            definition=query('definition',needle,delta)
            check('random.void' in str(definition),definition)
        replace('use random\nfunc main():\n    var v = random.')
        response=query('completion','random.',7)
        items=response.get('items',[]) if isinstance(response,dict) else response
        names={x['label']:x for x in items}
        check({'rand_i32','try_rand_f64','generator','seed','choose','shuffled'}<=names.keys(),names)
        check(not any(x.startswith(('_','below_','between_')) for x in names),names)
        check('maximum' in names['rand_i32']['detail'] and 'lower' in names['rand_i32']['detail'],names['rand_i32'])
        for call,expected,active in [('rng.rand_i32(',['maximum: i32'],0),('rng.rand_i32(1, ',['lower: i32','upper: i32'],1),('engine.rand_f32(',['maximum: f32'],0),('engine.try_rand_f32(lower = 1.0, upper = ',['lower: f32','upper: f32'],1)]:
            prefix='use random as rng\nfunc main():\n    var engine = rng.generator(seed = 42)\n    var x = '
            replace(prefix+call)
            result=query('signatureHelp',call,len(call),context={'triggerKind':1,'isRetrigger':False})
            check(bool(result),(call,result))
            shown=result['signatures'][0]
            check('_arity_' not in shown['label'],shown)
            check([x['label'] for x in shown['parameters']]==expected,(call,shown))
            check(result['activeParameter']==active,(call,result))
        replace('use ran')
        response=query('completion','ran',3)
        items=response.get('items',[]) if isinstance(response,dict) else response
        check(any('random' in x['label'] for x in items),items)
        replace(base)
        refs=query('references','rng.rand_i32',5,context={'includeDeclaration':True})
        check(bool(refs),refs)
        denied=client.request('textDocument/rename',{'textDocument':{'uri':entry.as_uri()},'position':point(text,'rng.rand_i32',5),'newName':'renamed'})[0]
        check('read-only' in str(denied),denied)
        client.shutdown()
print(f'Random LSP: {checks} checks passed')
