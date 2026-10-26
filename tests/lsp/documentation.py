"""Plain documentation, warning ownership, overlays and typed pattern metadata.
Use the existing real-pipe client. No editor interpretation of documentation.
"""
from pathlib import Path
import sys, tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame
server = Path(sys.argv[1]).resolve()
checks = 0

def point(text, needle, delta=0):
    prefix = text[:text.index(needle)+delta].replace('\r\n', '\n').replace('\r', '\n')
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2}

def check(value, message):
    global checks
    assert value, message
    checks += 1

with tempfile.TemporaryDirectory(prefix='VOID documentation é😀 ') as temp:
    root = Path(temp); entry = root/'main.void'; api = root/'api.void'
    api_text = '''## Save plain_name <value> é😀.
## Mention @param inline inside ordinary prose.
## @param name Name payload é😀.
## @param last Last payload.
## @return Saved value.
func save(name: string, middle: i32 = 1, last: i32 = 2) -> i32:
    return middle + last
## @param wrong Unknown name.
func no_docs(first: i32, second: i32) -> i32:
    return first + second
## @param value Generic payload.
func unused<t>(value: t, missing: t) -> t:
    return value
## @param value
## @param value First nonempty.
## @param value Later ignored.
## @return
## @return First return.
## @return Later ignored.
func duplicate(value: i32) -> i32:
    return value
'''
    api.write_text(api_text, encoding='utf-8')
    base='use "./api.void" as api\nfunc main():\n    var value = api.save("hero")\n'
    entry.write_text(base, encoding='utf-8')
    with Client(server, entry) as c:
        c.request('initialize', {'capabilities': {'textDocument': {'publishDiagnostics': {'versionSupport': True, 'relatedInformation': True}, 'signatureHelp': {'signatureInformation': {'documentationFormat': ['plaintext']}}}}})
        c.notify('initialized', {}); initial=c.sync()
        version=0; text=base
        def replace(new):
            global version, text
            text=new; version+=1
            if version==1:
                c.notify('textDocument/didOpen', {'textDocument': {'uri':entry.as_uri(),'languageId':'void','version':version,'text':text}})
            else:
                c.notify('textDocument/didChange', {'textDocument':{'uri':entry.as_uri(),'version':version},'contentChanges':[{'text':text}]})
            return c.sync()
        def query(method, needle, delta=0, **extra):
            response=c.request('textDocument/'+method, {'textDocument':{'uri':entry.as_uri()},'position':point(text,needle,delta),**extra})[0]
            check('error' not in response, response)
            return response['result']
        notes=replace(base)
        warnings=[d for n in initial+notes if n['uri']==api.as_uri() for d in n['diagnostics']]
        # Each publication is a complete set; inspect the most recent imported set.
        published=[n['diagnostics'] for n in initial+notes if n['uri']==api.as_uri()]
        check(published, initial+notes)
        warnings=published[-1]
        check(sorted(d['code'] for d in warnings)==['VOID2022','VOID2022','VOID2023','VOID2024','VOID2024'], warnings)
        check(all(d['severity']==2 for d in warnings), warnings)
        dup=[d for d in warnings if d['code']=='VOID2024']
        check(all(d['relatedInformation'][0]['location']['uri']==api.as_uri() for d in dup), dup)
        hover=query('hover','api.save',5)['contents']['value']
        check('Save plain_name <value> é😀.' in hover and 'Returns: Saved value.' in hover, hover)
        check('Name payload' not in hover and '@return' not in hover and '@param name' not in hover, hover)
        check('Mention @param inline' in hover, hover)
        for context in ({}, {'context': {'triggerKind':1}}, {'context': {'triggerKind':2,'triggerCharacter':','}}, {'context': {'triggerKind':3,'isRetrigger':True}}):
            for call, active, description in [('api.save(',0,'Name payload é😀.'),('api.save("hero", ',1,None),('api.save("hero", last = ',2,'Last payload.'),('api.save(',0,'Name payload é😀.'),('api.no_docs(',0,None)]:
                replace(base[:base.index('    var value')]+ '    var value = '+call)
                help=query('signatureHelp',call,len(call),**context)
                if description is None:check(help is None,(call,help));continue
                check(help and help['activeParameter']==active,(call,help))
                signature=help['signatures'][0]
                check('documentation' not in signature,signature)
                check(signature['parameters'][active]['documentation']=={'kind':'plaintext','value':description},signature)
                check(all('documentation' not in p for i,p in enumerate(signature['parameters']) if i!=active),signature)
        for call in ['api.duplicate(', 'api.save(last = 4, name = ', 'api.save(api.duplicate(']:
            replace(base[:base.index('    var value')]+'    var value = '+call)
            help=query('signatureHelp',call,len(call))
            expected='First nonempty.' if 'duplicate(' in call else 'Name payload é😀.'
            check(help and help['signatures'][0]['parameters'][help['activeParameter']]['documentation']['value']==expected,help)
        replace(base.replace('api.save("hero")','api.'))
        items=query('completion','var value = api.',len('var value = api.'))
        save=next(i for i in (items['items'] if isinstance(items,dict) else items) if i['label']=='save')
        check('Returns: Saved value.' in save['documentation'] and 'Name payload' not in save['documentation'],save)
        for prefix,expected,absent in [('## Description only.\n','Description only.','Returns:'),('## @return Return only.\n','Returns: Return only.','@return'),('## Description only.\n## @return Return only.\n','Returns: Return only.','@return')]:
            replace(prefix+'func value(x: i32) -> i32:\n    return x\nfunc main():\n    var n = value(1)')
            shown=query('hover','value(1',1)['contents']['value']
            check(expected in shown and absent not in shown,shown)
            check(query('signatureHelp','value(1',len('value(')) is None,'no parameter tags must suppress')
        replace('## Description.\n## @return Ignored return.\nfunc action():\n    pass\nfunc main():\n    action()')
        check('Ignored return.' not in query('hover','action()',1)['contents']['value'],'no-value return prose leaked')
        method='value struct box<t>:\n    value: t\n## @param value Method input.\n## @param last Method last.\nfunc (self box<t>) update(value: t, middle: i32 = 1, last: i32 = 2) -> t:\n    return value\nfunc main():\n    var item = box<i32>(value = 1)\n    var result = item.update('
        for call,active,description in [('item.update(',0,'Method input.'),('item.update(1, ',1,None),('item.update(1, last = ',2,'Method last.')]:
            replace(method[:method.rindex('item.update(')]+call)
            help=query('signatureHelp',call,len(call))
            check((help is None) if description is None else (help and help['activeParameter']==active and help['signatures'][0]['parameters'][active]['documentation']['value']==description),(call,help))
        replace(base)
        edited_api=api_text.replace('Name payload é😀.','Unsaved name é😀.')
        c.notify('textDocument/didOpen',{'textDocument':{'uri':api.as_uri(),'languageId':'void','version':7,'text':edited_api}});c.sync()
        replace(base[:base.index('    var value')]+'    var value = api.save(')
        help=query('signatureHelp','api.save(',len('api.save('))
        check(help['signatures'][0]['parameters'][0]['documentation']['value']=='Unsaved name é😀.',help)
        c.notify('textDocument/didChange',{'textDocument':{'uri':api.as_uri(),'version':6},'contentChanges':[{'text':'broken'}]});c.sync()
        check(query('signatureHelp','api.save(',len('api.save(')) is not None,'stale imported overlay replaced docs')
        c.notify('textDocument/didClose',{'textDocument':{'uri':api.as_uri()}});c.sync()
        help=query('signatureHelp','api.save(',len('api.save('))
        check(help['signatures'][0]['parameters'][0]['documentation']['value']=='Name payload é😀.',help)
        # LF, CRLF and CR attachment and tag ranges use unchanged source bytes.
        for newline in ('\n','\r\n','\r'):
            source='''## Description é😀.
## @param x
## @param x Payload.
## @param unknown_é😀 Unknown.
func local(x: i32, missing: i32) -> i32:
    return x
func main():
    var value = local(1, 2)
'''.replace('\n',newline)
            notes=replace(source)
            current=[n for n in notes if n['uri']==entry.as_uri()]
            check(current and current[-1]['version']==version,current)
            ds=current[-1]['diagnostics'];check(sorted(d['code'] for d in ds)==['VOID2022','VOID2023','VOID2024'],ds)
            check(next(d for d in ds if d['code']=='VOID2024')['range']['start']=={'line':2,'character':3},ds)
            check(next(d for d in ds if d['code']=='VOID2023')['range']['end']=={'line':3,'character':21},ds)
            check('Description é😀.' in query('hover','local(1',1)['contents']['value'],source)
        # Ordinary comments and blank lines break attachment; no docs means no warnings.
        for separator in ('\n','# ordinary\n'):
            notes=replace('## @param wrong Wrong.\n'+separator+'func local(x: i32) -> i32:\n    return x\nfunc main():\n    var value = local(1)')
            check(all(not n['diagnostics'] for n in notes),notes)
            check('Wrong.' not in query('hover','local(1',1)['contents']['value'],separator)
        # Concrete/symbolic result tokens, constructors, all pattern routes and captures.
        patterns='''enum parcel:
    empty
    ## A parcel carrying a result.
    data(item: result<i32, string>)
struct box:
    value: i32
func unused<t>(value: t) -> result<t, string>:
    return Ok(value)
func main():
    var value: result<i32, string> = Ok(3)
    var error: result<i32, string> = Err("bad")
    if value is Ok(var success):
        print(success)
    match value:
        Ok(i32(var matched)):
            print(matched)
        Err(var failure):
            print(failure)
    var nested = parcel.data(value)
    match nested:
        parcel.data(Ok(var inner)):
            print(inner)
        _:
            pass
    var doubly: result<result<i32, string>, string> = Ok(Ok(5))
    match doubly:
        Ok(Ok(var deep)):
            print(deep)
        _:
            pass
    var maybe: box? = box(value = 2)
    if maybe is Ok(var present):
        print(present.value)
'''
        replace(patterns)
        for needle,delta,required,forbidden in [('result<i32',1,'result<i32, string>',None),('result<t',1,'result<t, string>',None),('= Ok(3)',3,'success variant','error variant'),('= Err("bad")',3,'error variant','Ok(null)'),('is Ok(var success)',4,'success variant',None),('Ok(i32',1,'success variant',None),('Err(var failure)',1,'error variant',None),('data(Ok(var inner))',6,'success variant',None),('parcel.data(Ok',8,'parcel carrying a result',None),('is Ok(var present)',4,'present nullable','result<'),('print(matched)',7,'matched: i32','payload'),('print(failure)',7,'failure: string','payload'),('Ok(Ok(var deep))',1,'value: result<i32, string>',None),('Ok(Ok(var deep))',4,'value: i32',None),('print(deep)',7,'deep: i32','payload')]:
            result=query('hover',needle,delta);check(result and required in result['contents']['value'],(needle,result))
            check(not forbidden or forbidden not in result['contents']['value'],(needle,result))
        for variant,required,excluded in [('Ok','success variant','error variant'),('Err','error variant','Ok(null)')]:
            replace(patterns+'    var choice: result<i32, string> = '+variant)
            items=query('completion','var choice: result<i32, string> = '+variant,len('var choice: result<i32, string> = '+variant))
            candidates=items['items'] if isinstance(items,dict) else items
            chosen=next(i for i in candidates if i['label']==variant)
            check(required in chosen['documentation'] and excluded not in chosen['documentation'],chosen)
        replace(patterns)
        check(query('definition','= Ok(3)',3) is None,'builtin fictitious navigation')
        # Stale versions and cancelled queued requests cannot resurrect older docs/diagnostics.
        replace(base)
        c.notify('textDocument/didChange',{'textDocument':{'uri':entry.as_uri(),'version':version-1},'contentChanges':[{'text':'broken'}]});c.sync()
        check(query('hover','api.save',5) is not None,'stale overlay replaced current source')
        c.write(frame({'jsonrpc':'2.0','id':'old-docs','method':'textDocument/signatureHelp','params':{'textDocument':{'uri':entry.as_uri()},'position':point(text,'api.save(',len('api.save('))}})+frame({'jsonrpc':'2.0','method':'$/cancelRequest','params':{'id':'old-docs'}}))
        check(c.response('old-docs')[0]['error']['code']==-32800,'cancelled documentation request')
        c.notify('textDocument/didClose',{'textDocument':{'uri':entry.as_uri()}});c.sync()
        c.shutdown()
print(f'Documentation: {checks} assertions passed')
