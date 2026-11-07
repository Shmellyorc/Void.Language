"""Actual complete signed A/B packages through Debug/strict application AOT."""
from pathlib import Path
import argparse,json,os,tempfile
from support import ROOT,EVIDENCE,Runner,Service
parser=argparse.ArgumentParser();parser.add_argument('application',type=Path);parser.add_argument('--name',required=True)
options=parser.parse_args();runner=Runner(options.name);app=options.application.resolve()
# Use the private home filesystem, avoiding cumulative retained tmpfs packages.
work=Path(tempfile.mkdtemp(prefix='VOIDUP profile 雪 # % ',dir=Path.home()));root=work/'private'
with Service(ROOT/'build/voidup-fixtures') as server:
 base=[app,'--root',root,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','120000']
 for release in ('engineering-a','engineering-b'):
  runner.run([*base,'install',release,'--activate'],timeout=900)
  runner.run([*base,'verify',release])
  runner.run([*base,'run','default','voidc','--','--version'])
 runner.run([*base,'rollback']);runner.run([*base,'doctor'])
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,application=str(app),root=str(root),actual_complete_signed_a_b=True,hard_stack_bytes=1048576,filesystem='private home (native filesystem recorded separately)'),indent=2)+'\n')
print('Actual signed A/B install, verify, launch, rollback and doctor passed')
