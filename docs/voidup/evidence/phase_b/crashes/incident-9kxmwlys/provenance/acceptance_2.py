"""Actual signed A/B install, activation, compiler, LSP and relocation tests."""
from pathlib import Path
import argparse, json, os, shutil, tempfile
from support import ROOT,EVIDENCE,Runner,Service

parser=argparse.ArgumentParser()
parser.add_argument('application',type=Path)
parser.add_argument('--name',default='acceptance-1')
parser.add_argument('--fixtures',type=Path,default=ROOT/'build/voidup-fixtures')
options=parser.parse_args()
app=options.application.resolve();runner=Runner(options.name)
work=Path(tempfile.mkdtemp(prefix='VOIDUP real A B 雪 # % '))
root=work/'private root';fixture=work/'arguments.void'
fixture.write_text('use os\nuse encoding/hex as hex\nfunc main() -> i32:\n    if os.args() is Ok(var values):\n        for value in values:\n            print(hex.encode(value.to_utf8()))\n        return 17\n    return 2\n')
with Service(options.fixtures) as server:
 base=[app,'--root',root,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','120000']
 def run(*args,**kwargs):return runner.run([*base,*args],**kwargs)
 runner.run([app,'--help']);runner.run([app,'--version'])
 run('doctor');run('list');run('list','--available')
 run('install','engineering-a','--activate',timeout=900)
 run('verify','engineering-a');run('which','voidc');run('which','voidls')
 run('install','engineering-b','--activate',timeout=900)
 run('verify','engineering-b');run('list');run('doctor')
 forwarded=['','two words','"quote"','tail\\','雪😀','--looks-like-option','$(touch SHOULD_NOT_EXIST)']
 for release in ('engineering-a','engineering-b'):
  version=run('run',release,'voidc','--','--version')
  assert json.loads((root/'toolchains'/release/'toolchain.json').read_bytes())['build_identity'].encode() in version.stdout
  for flags in (['--no-opt'],[]):
   result=run('run',release,'voidc','--','run',fixture,*flags,'--no-cache','--',*forwarded,expected=17)
   assert result.stdout==''.join(value.encode().hex()+'\n' for value in forwarded).encode(),result.stdout
   binary=work/(release+('-normal' if flags else '-o2'))
   run('run',release,'voidc','--','build',fixture,'-o',binary,*flags)
   env=dict(os.environ,LD_LIBRARY_PATH=str(root/'toolchains'/release/'lib/void'))
   outcome=runner.run([binary,*forwarded],cwd=work,expected=17,env=env)
   assert outcome.stdout==result.stdout
  messages=[dict(jsonrpc='2.0',id=1,method='initialize',params=dict(processId=None,rootUri=None,capabilities={})),dict(jsonrpc='2.0',method='initialized',params={}),dict(jsonrpc='2.0',id=2,method='shutdown'),dict(jsonrpc='2.0',method='exit')]
  payload=b''
  for message in messages:
   raw=json.dumps(message).encode();payload+=f'Content-Length: {len(raw)}\r\n\r\n'.encode()+raw
  lsp=run('run',release,'voidls','--',input=payload)
  assert b'"id":1' in lsp.stdout.replace(b' ',b'') and b'"capabilities"' in lsp.stdout,lsp.stdout
 run('rollback');assert json.loads((root/'control.json').read_bytes())['current']=='engineering-a'
 run('run','default','voidc','--','--version');run('default','engineering-b')
 run('install','engineering-b','--activate')
 run('default','absent',expected=1)
 run('run','default','voidc','--','check',work/'absent.void',expected=1)
 assert not (work/'SHOULD_NOT_EXIST').exists()
 record=dict(schema=1,application=str(app),retained_work=str(work),root=str(root),
  real_builds=json.loads((EVIDENCE/'real-packages.json').read_bytes()),requests=server.requests,
  hard_stack_bytes=1048576,normal_o2_jit_and_independent_aot=True,lsp_initialized=True)
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps(record,indent=2))
