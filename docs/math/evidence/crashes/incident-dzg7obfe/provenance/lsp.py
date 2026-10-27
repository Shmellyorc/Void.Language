"""Compiler-owned Math completion, signatures, source navigation and recovery."""
from pathlib import Path
from contextlib import contextmanager
import json
import os
import sys
import tempfile
import bootstrap as h
sys.path.insert(0,str(h.ROOT/'tools'))
from lsp_client import Client
server=Path(sys.argv[1]).resolve();checks=0
public={'abs','min','max','clamp','sign','is_nan','is_infinite','is_finite','floor','ceil','trunc','round','lerp','inverse_lerp','sin','cos','tan','asin','acos','atan','atan2','sqrt','pow','exp','exp2','log','log2','radians','degrees','pi_f32','pi_f64','tau_f32','tau_f64','e_f32','e_f64'}

def check(value,message):
    global checks
    assert value,message
    checks+=1

def point(text,needle,delta=0):
    prefix=text[:text.index(needle)+delta]
    return {'line':prefix.count('\n'),'character':len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}

@contextmanager
def project(directory):
    previous=Path.cwd();os.chdir(directory)
    try:yield
    finally:os.chdir(previous)

with tempfile.TemporaryDirectory(prefix='VOID math LSP ') as directory,project(directory):
    entry=Path(directory)/'main.void'
    imports='use math as m\n'
    base=imports+'func main():\n    var wave = m.sin<f64>(0.5)\n    var angle = m.atan2(1.0, -1.0)\n    var turn = m.tau_f64\n'
    entry.write_text(base)
    with Client(server,entry) as client:
        client.request('initialize',{'capabilities':{'textDocument':{'signatureHelp':{'signatureInformation':{'documentationFormat':['plaintext']}}}}});client.notify('initialized',{});client.sync()
        text=base;version=0
        def replace(content):
            global text,version
            text=content;version+=1
            client.notify('textDocument/didOpen' if version==1 else 'textDocument/didChange',{'textDocument':{'uri':entry.as_uri(),'languageId':'void','version':version,'text':content}} if version==1 else {'textDocument':{'uri':entry.as_uri(),'version':version},'contentChanges':[{'text':content}]})
            return client.sync()
        def query(method,needle,delta=0,**extra):
            response=client.request('textDocument/'+method,{'textDocument':{'uri':entry.as_uri()},'position':point(text,needle,delta),**extra})[0]
            check('error' not in response,response);return response['result']
        notes=replace(base);check(all(not x['diagnostics'] for x in notes),notes)
        for needle,description in [('m.sin','radians'),('m.atan2','quadrant'),('m.tau_f64','full turn')]:
            hover=query('hover',needle,len('m.'))['contents']['value'];check(description.lower() in hover.lower(),hover)
            definition=query('definition',needle,len('m.'));check('math.void' in str(definition),definition)
        for prefix,needle in [('use mat','mat'),('use ma','ma')]:
            replace(prefix);response=query('completion',needle,len(needle));items=response.get('items',[]) if isinstance(response,dict) else response
            check(any('math' in x['label'] for x in items),items)
        replace(imports+'func main():\n    var value = m.')
        response=query('completion','m.',2);items=response.get('items',[]) if isinstance(response,dict) else response;names={x['label']:x for x in items}
        check(public<=names.keys(),sorted(names));check(not any(name.startswith('_') for name in names),names)
        for call,parameters,active in [('m.sin(',['value: t'],0),('m.pow(1.0, ',['base: t','exponent: t'],1),('m.atan2(y = 1.0, x = ',['y: t','x: t'],1),('m.clamp(upper = 1.0, value = ',['value: t','lower: t','upper: t'],0),('m.lerp(0.0, 1.0, ',['start: t','end: t','weight: t'],2),('m.inverse_lerp(0.0, 1.0, ',['start: t','end: t','value: t'],2)]:
            replace(imports+'func main():\n    var value = '+call)
            result=query('signatureHelp',call,len(call),context={'triggerKind':1,'isRetrigger':False});check(bool(result),(call,result));shown=result['signatures'][0]
            labels=[x['label'] for x in shown['parameters']]
            check(labels==parameters or labels==[x.replace(': t', ': f32') for x in parameters],(call,shown));check(result['activeParameter']==active,(call,result))
            check(bool(shown['parameters'][active].get('documentation')),(call,shown))
        replace(base)
        references=query('references','m.sin',len('m.'),context={'includeDeclaration':True});check(bool(references),references)
        denied=client.request('textDocument/rename',{'textDocument':{'uri':entry.as_uri()},'position':point(text,'m.sin',len('m.')),'newName':'renamed'})[0]
        check('read-only' in str(denied),denied)
        # Several malformed immutable document versions followed by a clean result.
        for fragment in ('m.atan2(y = ','m.pow<f64>(', 'm.sin('):
            replace(imports+'func main():\n    var value = '+fragment)
            result=query('signatureHelp',fragment,len(fragment));check(bool(result),(fragment,result))
        notes=replace(imports+'func note(text: string, value: f32) -> f32:\n    return value\nfunc main():\n    var value = note("🔥", m.sin(0.5))\n')
        # UTF-16 span calculation includes the astral symbol before the member.
        hover=query('hover','m.sin',len('m.'));check(bool(hover),hover)
        replace('## Source generic documentation.\n## @param value Source generic parameter.\nfunc echo<t>(value: t) -> t:\n    return value\nfunc main():\n    var value = echo(')
        result=query('signatureHelp','echo(',len('echo('));check(bool(result),result)
        check('Source generic parameter.' in str(result),result)
        notes=replace(base);check(all(not x['diagnostics'] for x in notes),notes)
        client.shutdown()
    evidence=h.ROOT/'docs/math/evidence';evidence.mkdir(parents=True,exist_ok=True)
    (evidence/('lsp-'+server.parent.name+'.json')).write_text(json.dumps({'server':str(server),'checks':checks,'public_members':sorted(public)},indent=2)+'\n')
print(f'Math LSP: {checks} checks passed')
