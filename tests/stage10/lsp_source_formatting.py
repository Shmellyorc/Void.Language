"""Source formatting, original diagnostics and dirty revision recovery over real LSP."""
from pathlib import Path
import sys, tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client
checks=0

def check(value,why):
    global checks
    assert value,why
    checks+=1

source=r'''# é😀 comment  
value struct number:
  private secret: i32
  const id: i32=7
  value: i32
  operator +(const left: number,const right: number) -> number:
    return number(value=left.value+right.value)
  operator %(const left: number,const right: number) -> number:
    return left
  operator ==(const left: number,const right: number) -> bool:
    return left.value==right.value
property (self number) amount: i32:
  get:
    return self.value
  set(value):
    self.value=value
func entry(input:i32)->i32:
  var a=number(value=input)
  a+=number(value=2)
  a%=number(value=2)
  var text="é😀 {{brace}} {input:>12|000}"
  var triple="""preserve  
    {(
        input # } is comment
        + 2
    ):000}
\u{1f600} {{raw}}
last"""
  return a.amount # tail  
'''
with tempfile.TemporaryDirectory(prefix='VOID Step39 é😀 ') as directory:
    file=Path(directory)/'main.void'
    file.write_text(source, encoding="utf-8")
    uri=file.as_uri()
    with Client(Path(sys.argv[1]).resolve(),file) as c:
        c.request('initialize',{'capabilities':{'textDocument':{'publishDiagnostics':{'versionSupport':True}}}})
        c.notify('initialized',{});c.sync()
        version=0
        current=source
        def replace(text):
            global version,current
            version+=1;current=text
            method='textDocument/didOpen' if version==1 else 'textDocument/didChange'
            params={'textDocument':{'uri':uri,'version':version}}
            if version==1:params['textDocument'].update(languageId='void',text=text)
            else:params['contentChanges']=[{'text':text}]
            c.notify(method,params)
            notes=c.sync()
            for n in notes:
                if n['uri']==uri:check(n.get('version')==version,('diagnostic revision',n))
            return [d for n in notes if n['uri']==uri for d in n['diagnostics']]
        def position(needle,delta=0):
            at=current.index(needle)+delta;line=current.count('\n',0,at);start=current.rfind('\n',0,at)+1
            return {'line':line,'character':len(current[start:at].encode('utf-16-le'))//2}
        def format_doc():
            response=c.request('textDocument/formatting',{'textDocument':{'uri':uri},'options':{'tabSize':4,'insertSpaces':True}})[0]
            check('error' not in response,response)
            return response['result']
        def query(method,needle,delta=0,**extra):
            response=c.request(method,{'textDocument':{'uri':uri},'position':position(needle,delta),**extra})[0]
            check('error' not in response,response)
            return response.get('result')
        for windows in [False,True]:
            text=source.replace('\n','\r\n') if windows else source
            check(not replace(text),'valid dirty source')
            first=format_doc();check(len(first)==1,first)
            pretty=first[0]['newText']
            check(first[0]['range']['start']=={'line':0,'character':0},first)
            check(pretty!=text and '    var a = number' in pretty,pretty)
            for literal in ['"é😀 {{brace}} {input:>12|000}"', '# é😀 comment  ', '# tail  ']:check(literal in pretty,('changed bytes',literal))
            begin=text.index('"""');end=text.index('"""',begin+3)+3
            check(text[begin:end] in pretty,'changed triple literal')
            check(''.join(ch for ch in text if ch in '\r\n')==''.join(ch for ch in pretty if ch in '\r\n'),'changed line endings')
            check(not replace(pretty),'formatted source invalid')
            check(format_doc()==[],'not idempotent')
            location=query('textDocument/definition','a+=',1)
            check(location and location['range']['start']['line']==5,location)
            check(query('textDocument/definition','input:>12',1)['range']['start']['line']==16,'hole definition span')
            # Two independent errors retain original UTF-16 positions, then clear.
            bad=pretty.replace('input:>12|000','input:0.#0').replace('return a.amount','return missing')
            diagnostics=replace(bad)
            pattern=[d for d in diagnostics if 'Required fraction' in d['message']]
            unknown=[d for d in diagnostics if 'missing' in d['message']]
            check(len(pattern)==1 and pattern[0]['range']['start']==position('0.#0',3),pattern)
            check(len(unknown)==1 and unknown[0]['range']['start']==position('missing'),unknown)
            check(format_doc()==[],'invalid source formatted')
            check(query('textDocument/hover','a+=',1) is None,'stale invalid hover')
            # Outdated notifications cannot repair errors or expose old formatting.
            c.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version-1},'contentChanges':[{'text':pretty}]})
            check(c.sync()==[],'stale diagnostics republished')
            check(format_doc()==[],'outdated source formatting used')
            check(not replace(pretty),'repair failed')
            check(format_doc()==[],'repair lost stable formatting')
            for broken in [pretty.replace('operator +(', 'operator !=('),pretty.replace('{input:>12|000}','{input + }'),pretty.replace('{input:>12|000}','{(input]}')]:
                check(bool(replace(broken)),'malformed declaration/hole accepted')
                check(format_doc()==[],'malformed source formatted')
                check(not replace(pretty),'malformed repair failed')
            # Disk refresh must retain a dirty overlay; closing must restore disk.
            file.write_text('func entry(input: i32) -> i32:\n    return missing_disk\n', encoding="utf-8")
            c.request('void/refreshProject',{});c.sync()
            check(format_doc()==[],'refresh replaced dirty overlay')
            check(query('textDocument/definition','input:>12',1),'refresh lost overlay bindings')
            c.notify('textDocument/didClose',{'textDocument':{'uri':uri}})
            notes=c.sync();check(any('missing_disk' in d['message'] for n in notes for d in n['diagnostics']),'close did not restore disk diagnostics')
            # Reopen with a fresh revision; the existing helper tracks the next edit.
            version+=1;current=pretty
            c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':pretty}})
            check(not [d for n in c.sync() for d in n['diagnostics']],'reopen repair failed')
            check(format_doc()==[],'reopen lost formatter stability')
        # Changing the opening line of this literal would alter hole indentation.
        unsafe='func entry(input: i32) -> i32:\n  var text = "{(func() -> i32:\n    return input\n  )()}"\n  return text.length as i32\n'
        check(not replace(unsafe),'unsafe formatting fixture must originally compile')
        check(format_doc()==[],'unsafe multiline interpolation rewritten')
        temporal='use time\nfunc entry(input:i32)->i32:\n  if make_date(2024,2,29) is Ok(var day):\n    var text="""é😀 {day:yyyy-MM-dd}\n{input:000}"""\n    return text.length as i32\n  return input\n'
        check(not replace(temporal),'temporal formatter fixture')
        first=format_doc();check(len(first)==1,'temporal format edit')
        pretty=first[0]['newText']
        literal='"""é😀 {day:yyyy-MM-dd}\n{input:000}"""'
        check(literal in pretty,'temporal literal bytes changed')
        check(not replace(pretty),'formatted temporal source invalid')
        check(format_doc()==[],'temporal idempotence')
        location=query('textDocument/definition','make_date',2)
        check(location and location['uri'].endswith('/time.void'),'installed temporal identity changed')
        check(query('textDocument/definition','day:yyyy',1)['uri']==uri,'temporal hole binding changed')
        bad=pretty.replace('yyyy-MM-dd','HH')
        diagnostics=replace(bad)
        check(bool(diagnostics),'invalid temporal format accepted')
        check(any(d['range']['start']==position('HH') for d in diagnostics),'temporal original UTF-16 diagnostic')
        check(format_doc()==[],'invalid temporal pattern formatted')
        check(not replace(pretty),'temporal diagnostic repair')
        check(format_doc()==[],'temporal repair stability')
        c.shutdown()
print(f'{checks} source formatting LSP/revision/diagnostic checks passed')
