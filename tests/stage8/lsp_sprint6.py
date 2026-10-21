"""Stage 8 Sprint 6 compiler-owned source positions, recovery and formatting."""
import pathlib,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
source='''value struct component:
    number: i32
struct node:
    using component
func echo(text: string, suffix: string = "!") -> string:
    return "{text}{suffix}"
func entry(input: i32) -> i32:
    var yes = node has component
    var text = echo(
        "🎮{echo("hi")}",
        suffix = "?",
    )
    var result = if yes:
        text
    else:
        "none"
    return result.length as i32
'''
with tempfile.TemporaryDirectory(prefix='VOID sprint6 editor ') as directory:
    file=pathlib.Path(directory)/'main.void';uri=file.as_uri();file.write_text(source, encoding='utf-8')
    with Client(server,file) as c:
        c.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}});c.notify('initialized',{});c.sync();version=0
        def replace(text):
            global version
            version+=1
            if version==1:c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':text}})
            else:c.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]})
            return [d for n in c.sync() for d in n['diagnostics']]
        def q(method,line=0,col=0,**extra):
            return c.request(method,{'textDocument':{'uri':uri},'position':{'line':line,'character':col},**extra})[0]
        assert not replace(source)
        for line,col,name in [(7,16,'node'),(7,25,'component'),(9,13,'echo'),(13,9,'text')]:
            response=q('textDocument/hover',line,col);assert response.get('result'),response
            response=q('textDocument/definition',line,col);assert response.get('result'),(name,response)
        # UTF-16 position after the supplementary scalar points to the embedded call.
        line=source.splitlines()[9];column=len(line[:line.index('echo')+1].encode('utf-16-le'))//2
        assert q('textDocument/definition',9,column)['result']['range']['start']['line']==4
        renamed=q('textDocument/rename',4,7,newName='display')['result'];assert renamed,renamed
        # Completion and signature assistance on a multiline argument line.
        partial=source.replace('        suffix = "?",','        suffix = echo(')
        replace(partial);signature=q('textDocument/signatureHelp',10,len('        suffix = echo('))['result'];assert signature,signature
        partial=source.replace('        suffix = "?",','        suffix = inp')
        replace(partial);items=q('textDocument/completion',10,len('        suffix = inp'))['result'];assert 'input' in {i['label'] for i in items},items
        # Inner substitution tokens remain available to compiler-owned completion.
        replace(source.replace('{text}{suffix}', '{te}{suffix}'))
        items=q('textDocument/completion',5,len('    return "{te'))['result'];assert 'text' in {i['label'] for i in items},items
        messy=source.replace('var text = echo(', 'var text=echo(')
        assert not replace(messy)
        formatted=q('textDocument/formatting',options={'tabSize':4,'insertSpaces':True})['result']
        assert formatted,formatted
        if formatted:
            assert len(formatted)==1,formatted
            pretty=formatted[0]['newText'];assert not replace(pretty)
            assert q('textDocument/formatting',options={'tabSize':4,'insertSpaces':True})['result']==[]
        for invalid in [source.replace('{echo("hi")}','{1}'),source.replace('node has component','node has i32'),source.replace('        "none"','        9')]:
            errors=replace(invalid);assert errors,invalid
            assert not replace(source)
        # Ordinary incomplete edits must not terminate this same server or leak
        # compiler-generated declarations into completion. Repair every state.
        pid=c.process.pid
        fragments=['u','us','unknown','var value =','const value =','if','return (','use','use ','use "','use io/','func','func pending(','func pending() ->','struct pending:','func pending() -> missing:\n    return 1']
        for fragment in fragments:
            errors=replace(fragment+'\n'+source)
            if fragment in ['u','us','unknown']:
                assert any(d['code']=='VOID2002' and fragment in d['message'] for d in errors),(fragment,errors)
            column=len(fragment.splitlines()[0])
            for method in ['textDocument/completion','textDocument/hover','textDocument/signatureHelp']:
                response=q(method,0,column);assert 'error' not in response,(fragment,method,response)
                if method=='textDocument/completion':
                    items=response['result']
                    if isinstance(items,dict):items=items['items']
                    assert isinstance(items,list),response
                    assert all('$' not in item['label'] for item in items),response
            assert c.process.pid==pid and c.process.poll() is None,fragment
            assert not replace(source),fragment
            definition=q('textDocument/definition',7,16)['result'];assert definition and definition['uri']==uri,(fragment,definition)
        # Incomplete function-local expressions and temporary statements share
        # the same recovery machinery, including anonymous/generated functions.
        for fragment in ['u','input +','echo(','var callback = func(', 'var text = "{inp']:
            edited=source[:source.index('    var yes')]+'    '+fragment+'\n'
            replace(edited)
            response=q('textDocument/completion',7,len('    '+fragment));assert 'error' not in response,(fragment,response)
            assert not replace(source)
            assert q('textDocument/definition',7,16)['result']
        # Walk complete editing sequences character by character, including
        # synthetic applications/closures and poisoned declaration signatures.
        sequences=['var callback = func(value: i32) -> i32:\n    return value\n',
            'func pending() -> missing:\n    return 1\n',
            'use "/peer.void" as peer\n',
            'func identity<t>(value: t) -> t:\n    return value\n']
        for sequence in sequences:
            for n in range(1,len(sequence)+1):
                fragment=sequence[:n];replace(fragment)
                line=fragment.count('\n');column=len(fragment.rsplit('\n',1)[-1])
                response=q('textDocument/completion',line,column);assert 'error' not in response,(fragment,response)
                assert c.process.poll() is None,fragment
            assert not replace(source)
            assert q('textDocument/definition',7,16)['result']
        assert c.process.poll() is None and c.process.pid==pid
        c.shutdown()
print('Sprint 6 LSP navigation, UTF-16, hover, rename, completion, signature, diagnostics and formatting passed')
