"""Authenticated hostile metadata/packages and bounded transport failures."""
from pathlib import Path
import argparse, copy, hashlib, io, json, os, tempfile, time, zipfile
from support import ROOT,EVIDENCE,Runner,Service,encoded,sign

parser=argparse.ArgumentParser();parser.add_argument('application',type=Path)
parser.add_argument('--name',default='rejection-1');options=parser.parse_args()
app=options.application.resolve();fixtures=ROOT/'build/voidup-fixtures';runner=Runner(options.name)
work=Path(tempfile.mkdtemp(prefix='VOIDUP rejected 雪 # % '))
original=json.loads((fixtures/'www/catalog.json').read_bytes());cases=[]
with Service(fixtures) as server:
 def attempt(name,document=None,signers=(1,2),overrides=None,command=('install','engineering-a'),floor=0):
  server.overrides.clear();server.catalog(document or original,signers)
  if overrides:server.overrides.update(overrides)
  target=work/name
  args=[app,'--root',target,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','1000',*command]
  result=runner.run(args,expected=1,timeout=30)
  diagnostic=json.loads(result.stderr)
  assert diagnostic['message'] and diagnostic['operation'],diagnostic
  assert not list((target/'toolchains').iterdir())
  control=json.loads((target/'control.json').read_bytes())
  assert control['serial']==floor,(name,control)
  assert not control['current'] and not control['installed']
  cases.append(dict(name=name,diagnostic=diagnostic,retained_serial=control['serial']))
 def changed(**updates):
  document=copy.deepcopy(original);document.update(updates);return document
 attempt('one-signature',signers=(1,))
 duplicate=sign(encoded(original),fixtures/'keys',(1,1))
 attempt('duplicate-key',overrides={'/catalog.json.sig':(200,duplicate,{})})
 attempt('wrong-exact-bytes',overrides={'/catalog.json':(200,encoded(original)+b' ',{})})
 malformed=json.loads(sign(encoded(original),fixtures/'keys'));malformed['signatures'][0]['key_id']='unknown'
 attempt('unknown-key',overrides={'/catalog.json.sig':(200,encoded(malformed),{})})
 attempt('expired',changed(issued=int(time.time())-3600,expires=int(time.time())-1))
 attempt('future',changed(issued=int(time.time())+3600,expires=int(time.time())+7200))
 attempt('unsupported-schema',changed(schema=99))
 attempt('unknown-schema-field',changed(unrecognized=True))
 attempt('minimum-manager',changed(min_manager=999))
 attempt('duplicate-release',changed(releases=[original['releases'][0]]*2))
 attempt('revoked',changed(revoked=['engineering-a']),floor=10)
 document=copy.deepcopy(original);document['releases'][0]['host']='windows-x86_64'
 attempt('wrong-host',document,floor=10)
 attempt('redirect',overrides={'/catalog.json':(302,b'',{'Location':server.url})})
 attempt('metadata-over-limit',overrides={'/catalog.json':(200,b'x'*(8388608+1),{})})
 document=copy.deepcopy(original);document['releases'][0]['artifact_url']='https://127.0.0.1:1/evil'
 attempt('cross-origin',document,floor=10)
 attempt('truncated-package',overrides={'/packages/engineering-a.zip':(200,b'PK',{})},floor=10)
 document=copy.deepcopy(original);document['releases'][0].update(artifact_bytes=22,artifact_sha256='0'*64)
 attempt('wrong-artifact-hash',document,overrides={'/packages/engineering-a.zip':(200,b'x'*22,{})},floor=10)
 bad_inventory=(fixtures/'www/inventories/engineering-a.json').read_bytes()+b' '
 attempt('inventory-digest',overrides={'/inventories/engineering-a.json':(200,bad_inventory,{})},floor=10)
 mismatched=json.loads((fixtures/'www/inventories/engineering-a.json').read_bytes());mismatched['build_identity']='0'*64
 raw_mismatched=encoded(mismatched);document=copy.deepcopy(original)
 document['releases'][0].update(inventory_bytes=len(raw_mismatched),inventory_sha256=hashlib.sha256(raw_mismatched).hexdigest())
 attempt('inventory-build-identity',document,overrides={'/inventories/engineering-a.json':(200,raw_mismatched,{})},floor=10)
 # Small fixtures are rejection probes only; actual acceptance always uses the
 # complete real A/B packages. These cannot execute and must never publish.
 identity=original['releases'][0]['build_identity']
 files={'bin/voidc':b'NEVER EXECUTE','bin/voidls':b'NEVER EXECUTE',
        'sdk/bin/clang':b'NEVER EXECUTE','sdk/bin/ld.lld':b'NEVER EXECUTE',
        'bin/void-native.txt':b'bad','bin/void-native-resources.txt':b'bad',
        'share/void/stdlib/io.void':b'bad','toolchain.json':encoded(dict(schema=1,release_id='engineering-a',host=original['releases'][0]['host'],build_identity=identity,llvm_version='23.1.4'))}
 dirs=sorted({str(parent)+'/' for name in files for parent in Path(name).parents if str(parent)!='.'},key=lambda name:(name.count('/'),name))
 members=[dict(name=name,size=0,sha256='',executable=False,directory=True) for name in dirs]
 members += [dict(name=name,size=len(data),sha256=hashlib.sha256(data).hexdigest(),executable=name in ['bin/voidc','bin/voidls','sdk/bin/clang','sdk/bin/ld.lld'],directory=False) for name,data in files.items()]
 def package_case(name,mutate_inventory=None,mutate_zip=None):
  manifest=dict(schema=1,release_id='engineering-a',host=original['releases'][0]['host'],build_identity=identity,expanded_bytes=sum(map(len,files.values())),members=copy.deepcopy(members))
  if mutate_inventory:mutate_inventory(manifest)
  stream=io.BytesIO()
  with zipfile.ZipFile(stream,'w') as archive:
   for directory in dirs:archive.writestr(directory,b'')
   for member,data in files.items():archive.writestr(member,data)
   if mutate_zip:mutate_zip(archive)
  body=stream.getvalue();inventory=encoded(manifest)
  document=copy.deepcopy(original);release=document['releases'][0]
  release.update(artifact_bytes=len(body),artifact_sha256=hashlib.sha256(body).hexdigest(),inventory_bytes=len(inventory),inventory_sha256=hashlib.sha256(inventory).hexdigest())
  attempt(name,document,overrides={'/packages/engineering-a.zip':(200,body,{}),'/inventories/engineering-a.json':(200,inventory,{})},floor=10)
 package_case('zip-unlisted',mutate_zip=lambda archive:archive.writestr('unlisted',b'x'))
 package_case('zip-traversal',mutate_zip=lambda archive:archive.writestr('../escape',b'x'))
 package_case('zip-duplicate',mutate_zip=lambda archive:archive.writestr('bin/voidc',b'x'))
 package_case('member-hash',mutate_inventory=lambda m:m['members'][-1].update(sha256='0'*64))
 package_case('inventory-traversal',mutate_inventory=lambda m:m['members'][-1].update(name='../escape'))
 package_case('inventory-case-collision',mutate_inventory=lambda m:m['members'].append(dict(m['members'][-1],name='TOOLCHAIN.JSON')))
 package_case('expanded-limit',mutate_inventory=lambda m:m.update(expanded_bytes=4294967297))
 saved_identity=files['toolchain.json']
 bad_identity=json.loads(saved_identity);bad_identity['build_identity']='0'*64
 files['toolchain.json']=encoded(bad_identity)
 members[-1].update(size=len(files['toolchain.json']),sha256=hashlib.sha256(files['toolchain.json']).hexdigest())
 package_case('payload-build-identity')
 files['toolchain.json']=saved_identity
 # A new authenticated floor persists despite a later transfer failure.
 server.overrides.clear();server.catalog(original)
 retained=work/'floor'
 base=[app,'--root',retained,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','1000']
 runner.run([*base,'list','--available'])
 for name,document in [('replay',changed(serial=9)),('equivocation',changed(min_manager=1,expires=original['expires']-1))]:
  server.catalog(document);failure=runner.run([*base,'list','--available'],expected=1)
  assert json.loads((retained/'control.json').read_bytes())['serial']==10
  cases.append(dict(name=name,diagnostic=json.loads(failure.stderr)))
 server.catalog(changed(serial=11,revoked=['engineering-a']))
 runner.run([*base,'list','--available'])
 server.catalog(changed(serial=12))
 failure=runner.run([*base,'list','--available'],expected=1)
 assert json.loads((retained/'control.json').read_bytes())['serial']==11
 cases.append(dict(name='revocation-removal',diagnostic=json.loads(failure.stderr)))
 # An explicit test trust bundle is required; an untrusted local TLS endpoint
 # never reaches application catalog authentication.
 runner.run([app,'--root',work/'tls','--catalog',server.url,'list','--available'],expected=1)
 unknown=work/'unknown';unknown.mkdir(mode=0o700);(unknown/'sentinel').write_bytes(b'KEEP')
 runner.run([app,'--root',unknown,'doctor'],expected=1)
 assert sorted(p.name for p in unknown.iterdir())==['sentinel']
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,cases=cases,retained_work=str(work),requests=server.requests),indent=2)+'\n')
 print(f'{len(cases)} hostile authenticated metadata/package cases rejected; TLS and unknown-root refusal passed')
