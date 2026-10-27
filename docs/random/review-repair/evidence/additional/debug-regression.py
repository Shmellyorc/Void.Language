"""Compiler-owned random discovery, arity presentation and navigation through aliases."""
from pathlib import Path
from contextlib import contextmanager
import os
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
    if not value:
        evidence=Path('/tmp/void-review-lsp')
        (evidence/'baseline-main.void').write_bytes(entry.read_bytes())
        (evidence/'baseline-conflict.void').write_bytes((entry.parent/'conflict.void').read_bytes())
        (evidence/'baseline-stdin.bin').write_bytes(client.process.stdin.captured())
        (evidence/'baseline-overlay.void').write_text(text)
        client.shutdown()
        (evidence/'baseline-server-exit.txt').write_text(str(client.process.returncode)+'\n')
    assert value,message
    checks+=1

def point(text,needle,delta=0):
    prefix=text[:text.index(needle)+delta]
    return {'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1])}

@contextmanager
def project_directory(directory):
    previous=Path.cwd()
    os.chdir(directory)
    try:
        yield
    finally:
        os.chdir(previous)

with tempfile.TemporaryDirectory(prefix='VOID random LSP ') as directory, project_directory(directory):
    entry=Path(directory)/'main.void'
    # The working directory also lets the crash harness retain this imported fixture.
    (Path(directory)/'conflict.void').write_text('''## CONFLICT module documentation.
## @param maximum CONFLICT maximum documentation.
## @param lower CONFLICT lower documentation.
## @param upper CONFLICT upper documentation.
func rand_i32(maximum: i32, lower: i32, upper: i32) -> i32:
    return maximum
value struct generator:
    value: i32
## CONFLICT receiver documentation.
## @param maximum CONFLICT maximum documentation.
## @param lower CONFLICT lower documentation.
## @param upper CONFLICT upper documentation.
func (self generator) rand_f32(maximum: f32, lower: f32, upper: f32) -> f32:
    return maximum
## CONFLICT checked receiver documentation.
## @param maximum CONFLICT maximum documentation.
## @param lower CONFLICT lower documentation.
## @param upper CONFLICT upper documentation.
func (self generator) try_rand_f32(maximum: f32, lower: f32, upper: f32) -> f32:
    return maximum
''')
    imports='use random as rng\nuse "./conflict.void" as conflict\n'
    base=imports+'func main():\n    var engine = rng.generator(seed = 42)\n    var a = rng.rand_i32(10)\n    var b = engine.rand_f32(1.0, 2.0)\n    var unrelated = conflict.rand_i32(3, 0, 4)\n'
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
        for needle,description in [('rng.rand_i32','Draw i32 from the shared Runtime default'),('engine.rand_f32','Draw f32: zero arguments sample the precise [0, 1) unit grid.')]:
            delta=needle.index('.')+2
            hover=query('hover',needle,delta)['contents']['value']
            check('_arity_' not in hover and 'private ' not in hover,hover)
            check('maximum' in hover or 'lower' in hover,hover)
            check(description in hover and 'CONFLICT' not in hover,hover)
            definition=query('definition',needle,delta)
            check('random.void' in str(definition),definition)
        unrelated=query('hover','conflict.rand_i32',len('conflict.'))['contents']['value']
        check('CONFLICT module documentation.' in unrelated,unrelated)
        replace('use random\nfunc main():\n    var v = random.')
        response=query('completion','random.',7)
        items=response.get('items',[]) if isinstance(response,dict) else response
        names={x['label']:x for x in items}
        check({'rand_i32','try_rand_f64','generator','seed','choose','shuffled'}<=names.keys(),names)
        check(not any(x.startswith(('_','below_','between_')) for x in names),names)
        check('maximum' in names['rand_i32']['detail'] and 'lower' in names['rand_i32']['detail'],names['rand_i32'])
        for call,expected,active,description,cursor in [
            ('rng.rand_i32(',['maximum: i32'],0,'Exclusive positive upper endpoint; finite for floats.',None),
            ('rng.rand_i32(1, ',['lower: i32','upper: i32'],1,'Exclusive upper endpoint; finite for floats.',None),
            ('rng.rand_i32(lower, 2)',['lower: i32','upper: i32'],0,'Inclusive lower endpoint; finite for floats.',len('rng.rand_i32(lo')),
            ('engine.rand_f32(',['maximum: f32'],0,'Exclusive positive upper endpoint; finite for floats.',None),
            ('engine.try_rand_f32(lower = 1.0, upper = ',['lower: f32','upper: f32'],1,'Exclusive upper endpoint; finite for floats.',None),
        ]:
            prefix=imports+'func main():\n    var engine = rng.generator(seed = 42)\n    var lower: i32 = 1\n    var x = '
            replace(prefix+call)
            result=query('signatureHelp',call,len(call) if cursor is None else cursor,context={'triggerKind':1,'isRetrigger':False})
            check(bool(result),(call,result))
            shown=result['signatures'][0]
            check('_arity_' not in shown['label'],shown)
            check([x['label'] for x in shown['parameters']]==expected,(call,shown))
            check(result['activeParameter']==active,(call,result))
            check(shown['parameters'][active]['documentation']=={'kind':'plaintext','value':description},(call,shown))
            check('CONFLICT' not in str(shown),shown)
        # Ordinary same-name functions still use their own module documentation.
        replace(imports+'func main():\n    var x = conflict.rand_i32(')
        unrelated=query('signatureHelp','conflict.rand_i32(',len('conflict.rand_i32('))
        check(unrelated['signatures'][0]['parameters'][0]['documentation']=={'kind':'plaintext','value':'CONFLICT maximum documentation.'},unrelated)
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
