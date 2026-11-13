"""Bounded G19 authority controls on disposable copies of the frozen G08 project.

Record full responses to compare production and isolated candidate servers.
Disk-only local edits are observed separately from watched/refreshed edits;
editor revision alone must never authorize modified locked dependencies.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory
import compiler_project as base


def run(compiler,server):
    results={}
    scenarios=['repeat','referenced','unreferenced','manifest','dependency','lock',
               'entry','dirty','closed','removed','imports','visibility']
    with TemporaryDirectory(prefix='VOID G19 authority ') as temp:
        for scenario in scenarios:
            root=shutil.copytree(base.FIXTURE,Path(temp)/scenario)
            subprocess.run([str(compiler),'resolve','--project',str(root)],check=True,capture_output=True)
            stock=root/'src/stock.void';text=stock.read_text()
            pos=base.position(text,text.index('item.units')+len('item.'))
            with base.Client(server,root,project=True,cwd=root) as c:
                c.initialize()
                c.notify('textDocument/didOpen',{'textDocument':{'uri':stock.as_uri(),'version':1,'languageId':'void','text':text}});c.sync()
                def query():return c.request('textDocument/completion',{'textDocument':{'uri':stock.as_uri()},'position':pos})[0]['result']
                def state():return c.request('void/projectState')[0]['result']
                before=query();assert {'units','unit_price'} <= {x['label'] for x in before}
                revision=state()['selectionRevision']
                notes=[]
                if scenario in ['repeat','referenced','unreferenced','manifest','dependency','lock','removed','visibility']:
                    paths={'referenced':root/'src/model.void','unreferenced':root/'src/draft.void',
                           'manifest':root/'void.project.toml','dependency':root/'deps/rates/src/api.void',
                           'lock':root/'void.lock','removed':root/'src/model.void','visibility':root/'src/model.void'}
                    if scenario!='repeat':
                        p=paths[scenario];saved=p.read_bytes()
                        if scenario=='removed':p.unlink()
                        elif scenario=='dependency':base.update(p,'return 2','return 3')
                        elif scenario=='lock':p.write_text('invalid lock\n')
                        elif scenario=='manifest':p.write_text('invalid manifest\n')
                        elif scenario=='visibility':p.write_text(p.read_text().replace('    unit_price: i32','    private unit_price: i32'))
                        else:p.write_bytes(saved+b'\n# G19 disk mutation\n')
                    immediate=query()
                    assert state()['selectionRevision']==revision
                    if scenario in ['dependency','lock','manifest']:assert immediate==[],(scenario,immediate)
                    if scenario=='repeat':assert immediate==before
                    if scenario!='repeat':
                        response,notes=c.request('void/refreshProject')
                        assert 'result' in response,response
                        refreshed=query()
                        p.write_bytes(saved)
                        c.request('void/refreshProject')
                    else:refreshed=immediate
                elif scenario=='entry':
                    _,notes=c.request('void/reselectProject',{'entry':'src/audit.void'})
                    immediate=refreshed=query()
                elif scenario=='closed':
                    c.notify('textDocument/didClose',{'textDocument':{'uri':stock.as_uri()}});notes=c.sync()
                    immediate=refreshed=query()
                else:
                    if scenario=='dirty':changed=text.replace('item.units','item.unit_price')
                    else:changed=text.replace('./model.void','./missing.void')
                    c.notify('textDocument/didChange',{'textDocument':{'uri':stock.as_uri(),'version':2},'contentChanges':[{'text':changed}]});notes=c.sync()
                    immediate=refreshed=query()
                    if scenario=='imports':assert immediate==[]
                    if scenario=='dirty':assert {'units','unit_price'}<={x['label'] for x in immediate}
                    assert stock.read_text()==text
                    c.notify('textDocument/didChange',{'textDocument':{'uri':stock.as_uri(),'version':1},'contentChanges':[{'text':'invalid'}]});c.sync()
                    assert query()==immediate,'stale edit changed completion'
                record={'before':before,'immediate':immediate,'refreshed':refreshed,'diagnostics':notes}
                encoded=json.dumps(record,ensure_ascii=False,sort_keys=True).replace(root.as_uri(),'PROJECT_URI').replace(root.as_posix(),'PROJECT')
                results[scenario]=json.loads(encoded)
                c.shutdown()
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('compiler',type=Path);p.add_argument('server',type=Path);p.add_argument('--output',required=True,type=Path)
    args=p.parse_args()
    with args.output.open('x') as f:json.dump(run(args.compiler.resolve(),args.server.resolve()),f,indent=2,sort_keys=True);f.write('\n')
