"""Compiler-owned Sprint 2 numeric and argument assistance; no fake definitions."""
import pathlib,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
source='''## Add an amount.
func add(value: i32, extra: i32 = 2) -> i32:
    return value + extra
func entry(input: i32) -> i32:
    var health: i32 = -20
    var maximum = i32.max_value
    var result = health.abs().clamp(0, 100) + i32.abs(health)
    return add(extra = result, value = input)
func ceiling<t>(value: t = t.max_value) -> t:
    return value
'''
with tempfile.TemporaryDirectory(prefix='VOID sprint2 lsp ') as temp:
    entry=pathlib.Path(temp).resolve()/'main.void';entry.write_text(source);uri=entry.as_uri()
    with Client(server,entry) as c:
        c.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}})
        c.notify('initialized',{});c.sync();version=1
        c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':source}});c.sync()
        def q(method,line,char,**extra):return c.request(method,{'textDocument':{'uri':uri},'position':{'line':line,'character':char},**extra})[0]
        assert '= …' in q('textDocument/hover',8,8)['result']['contents']['value']
        assert 'max_value: i32' in q('textDocument/hover',5,24)['result']['contents']['value']
        assert 'Absolute value' in q('textDocument/hover',6,25)['result']['contents']['value']
        assert q('textDocument/definition',6,25)['result'] is None
        references=q('textDocument/references',6,25,context={'includeDeclaration':True})['result']
        assert len(references)==2 and all(item['uri']==uri for item in references),references
        assert c.request('workspace/symbol',{'query':'abs'})[0]['result']==[]
        assert q('textDocument/rename',6,25,newName='magnitude')['error']['code']==-32602
        # Named label navigation/rename participates in the same parameter binding.
        definition=q('textDocument/definition',7,16)['result'];assert definition['range']['start']['line']==1,definition
        assert 'extra: i32' in q('textDocument/hover',7,16)['result']['contents']['value']
        rename=q('textDocument/rename',7,16,newName='bonus');assert 'result' in rename,rename
        renamed=rename['result']
        edits=renamed['documentChanges'][0]['edits']
        assert len(edits)==3 and {edit['range']['start']['line'] for edit in edits}=={1,2,7},renamed
        def replace(text):
            global version
            version+=1;c.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]});c.sync()
        prefix=source[:source.index('    var maximum')]
        for expression,expected in [('i32.',{'min_value','max_value','abs','clamp'}),('health.',{'abs','clamp'}),('health.abs().',{'abs','clamp'})]:
            text=prefix+'    var probe = '+expression;replace(text)
            items=q('textDocument/completion',5,len('    var probe = '+expression))['result']
            assert {item['label'] for item in items}==expected,(expression,items)
        for expression,expected,count in [('i32.clamp(', 'value: i32',3),('health.clamp(', 'lower: i32',2),('add(extra = ', 'extra: i32',2)]:
            text=prefix+'    var probe = '+expression;replace(text)
            result=q('textDocument/signatureHelp',5,len('    var probe = '+expression))['result'];assert result,result
            signature=result['signatures'][0];assert len(signature['parameters'])==count,signature
            assert expected in signature['parameters'][result['activeParameter']]['label'],result
print('Sprint 2 numeric completion/hover/signatures/readonly and named-label navigation passed')
