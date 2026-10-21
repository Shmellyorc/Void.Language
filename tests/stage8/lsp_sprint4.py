"""Application analysis stays nonexecuting and native imports stay untrusted."""
import pathlib,sys,tempfile
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/'tools'))
from lsp_client import Client
server=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID sprint4 editor ') as directory:
    entry=pathlib.Path(directory)/'main.void';uri=entry.as_uri()
    source='var numbers: [i32] = []\nvar text = "A🎮"\nreturn numbers[0]\n'
    entry.write_text(source,encoding='utf-8')
    with Client(server,entry) as client:
        client.request('initialize',{'capabilities':{'workspace':{'workspaceEdit':{'documentChanges':True,'failureHandling':'textOnlyTransactional'}}}});client.notify('initialized',{});client.sync()
        def replace(text,version):
            if version==1: client.notify('textDocument/didOpen',{'textDocument':{'uri':uri,'languageId':'void','version':version,'text':text}})
            else: client.notify('textDocument/didChange',{'textDocument':{'uri':uri,'version':version},'contentChanges':[{'text':text}]})
            notifications=client.sync()
            return [diagnostic for note in notifications for diagnostic in note['diagnostics']]
        assert not replace(source,1)
        # Compiler-owned body names are not written declarations or edit targets.
        result,_=client.request('textDocument/hover',{'textDocument':{'uri':uri},'position':{'line':0,'character':0}})
        assert result.get('result') is None,result
        result,_=client.request('textDocument/rename',{'textDocument':{'uri':uri},'position':{'line':0,'character':0},'newName':'unwritten'})
        assert result.get('result') is None or 'error' in result,result
        result,_=client.request('textDocument/rename',{'textDocument':{'uri':uri},'position':{'line':0,'character':5},'newName':'values'})
        assert result.get('result'),result
        result,_=client.request('textDocument/hover',{'textDocument':{'uri':uri},'position':{'line':1,'character':5}})
        assert result.get('result') and 'string' in result['result']['contents']['value'],result
        assert not replace('func main() -> i32:\n    return 7\n',2)
        errors=replace('func main() -> i32:\n    pass\n',3)
        assert any('Missing return' in d['message'] for d in errors),errors
        errors=replace('use "/native_sink.void" as sink\nreturn sink.echo(7)\n',4)
        assert errors,errors
        # A conflicting application body is diagnosed through the same compiler.
        errors=replace('func main():\n    pass\nreturn 1\n',5)
        assert any('compete' in d['message'] for d in errors),errors
print('Sprint 4 nonexecuting application analysis and native trust checks passed')
