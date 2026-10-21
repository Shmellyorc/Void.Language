"""Compiler-owned text and collection editor assistance."""
import pathlib,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
source='''func entry(input: i32) -> i32:
    var text = "A🎮"
    var bytes = text.to_utf8()
    var decoded = string.from_utf8(bytes)
    var length = text.length
    var list = [1, 2]
    var found = list.get(-1)
    var popped = list.pop()
    var dictionary = {"x": 1}
    var lookup = dictionary.get("x")
    const frozen = [1]
    var fixed: [i32, 2] = [1, 2]
    return length as i32
'''
with tempfile.TemporaryDirectory(prefix='VOID sprint3 lsp ') as temp:
    entry=pathlib.Path(temp).resolve()/'main.void';entry.write_text(source,encoding='utf-8');uri=entry.as_uri()
    with Client(server,entry) as c:
        c.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}});c.notify('initialized',{});c.sync();version=1
        c.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':source}});c.sync()
        def q(method,line,char,**extra):return c.request(method,{'textDocument':{'uri':uri},'position':{'line':line,'character':char},**extra})[0]
        for line,member in [(2,'to_utf8'),(3,'from_utf8'),(4,'length'),(6,'get'),(7,'pop'),(9,'get')]:
            column=source.splitlines()[line].rindex(member)+1
            hover=q('textDocument/hover',line,column)['result'];assert hover and member in hover['contents']['value'],(line,hover)
            if member=='length':assert 'length: i64' in hover['contents']['value'],hover
            assert q('textDocument/definition',line,column)['result'] is None
            assert 'error' in q('textDocument/rename',line,column,newName='changed')
            references=q('textDocument/references',line,column,context={'includeDeclaration':True})['result'];assert references and all(r['uri']==uri for r in references),references
        renamed=q('textDocument/rename',0,12,newName='seed')
        assert 'result' in renamed,renamed
        def replace(text):
            global version
            version+=1;c.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]});c.sync()
        prefix=source[:source.index('    return')]
        for expression,required,absent in [('text.',{'length','byte_length','to_utf8'},{'from_utf8','pop'}),('string.',{'from_utf8'},{'length','to_utf8'}),('list.',{'get','pop','count'},set()),('dictionary.',{'get','contains','count'},set()),('frozen.',{'get','count'},{'pop','push','remove_at','clear'}),('fixed.',{'get','count'},{'pop','push'}),('text[-1].',{'length','byte_length','to_utf8'},set())]:
            text=prefix+'    var probe = '+expression;replace(text);items=q('textDocument/completion',12,len('    var probe = '+expression))['result'];names={i['label'] for i in items};assert required<=names and not(absent&names),(expression,names)
        for expression,name in [('list.get(','index'),('dictionary.get(','key'),('string.from_utf8(','bytes')]:
            text=prefix+'    var probe = '+expression;replace(text);result=q('textDocument/signatureHelp',12,len('    var probe = '+expression))['result'];assert result and name in result['signatures'][0]['parameters'][0]['label'],(expression,result)
print('Sprint 3 text/collection completion, hover, signatures, references and builtin protections passed')
