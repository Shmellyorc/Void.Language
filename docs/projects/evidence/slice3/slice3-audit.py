from pathlib import Path
import json,hashlib,subprocess,os,re,urllib.parse
r=Path.cwd();e=r/'docs/projects/evidence';receipt=json.loads((e/'slice3-local.json').read_text());count=0;env=dict(os.environ,GIT_OPTIONAL_LOCKS='0')
def check(ok,label):
 global count
 count+=1
 assert ok,label
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git',*args],env=env)
for field in ['source_inputs_sha256','starting_files_preserved_sha256']:
 for n,h in receipt[field].items():check(sha(r/n)==h,n)
for n,v in receipt['starting_files_amended'].items():check(sha(r/n)==v['final_sha256'],n)
for row in receipt['raw_evidence']+receipt['patches']+receipt['logging_test_receipts']+[receipt['report'],receipt['profile_decision'],receipt['git_inventory']['evidence']]:check(sha(r/row['path'])==row['sha256'],row['path'])
for p in receipt['linux_profiles']:
 for n,h in p['final_binary_sha256'].items():check(sha(r/('build/llvm23-'+p['profile'])/n)==h,n)
 check(p['core_cli_matrix']['passed']==42 and p['final_server_matrix']['failed']==0,p['profile'])
for n,h in receipt['windows_final_server_qualification']['source_inputs_sha256'].items():check(sha(r/n)==h,n)
for artifact in receipt['windows_source_archives']:check(sha(artifact['path'])==artifact['sha256'],artifact['path'])
check(receipt['windows_final_server_qualification']['status']=='PASS','Native final server execution')
check(receipt['windows_main_qualification']['status']=='PASS','Native main qualification')
check(receipt['editor']['project_host']['status']=='PASS' and len(receipt['editor']['project_host']['events'])==14,'Actual final editor project host')
check(receipt['installed_compiler_identity']['same_code_sections'],'Installed compiler code identity')
actual=[{'status':v[:2],'path':v[3:]} for v in git('status','--porcelain=v1','-uall','-z').decode().split('\0') if v]
check(actual==[{'status':v['status'],'path':v['path']} for v in receipt['git_inventory']['rows']],'Complete current Git inventory')
check(git('rev-parse','HEAD').decode().strip()==receipt['baseline'],'HEAD')
check(git('diff','--cached','--name-only')==b'','No staged changes')
head=[];index=[]
for row in git('ls-tree','-rz','HEAD').split(b'\0'):
 if row:
  meta,path=row.split(b'\t',1);mode,kind,oid=meta.split();head.append((path,mode,oid))
for row in git('ls-files','--stage','-z').split(b'\0'):
 if row:
  meta,path=row.split(b'\t',1);mode,oid,stage=meta.split();assert stage==b'0';index.append((path,mode,oid))
check(sorted(index)==sorted(head),'All indexed paths/modes/blobs equal HEAD')
check(sha(r/'.git/index')==receipt['operational_boundaries']['index_entry_audit']['final_raw_index_sha256'],'Recorded final raw index metadata checksum')
check(git('diff','--name-only','--','src/runtime','include/void/runtime.hpp','stdlib','.github','editors/vscode/package-lock.json')==b'','Runtime, stdlib, hosted workflows and editor dependency lock untouched')
subprocess.run(['git','diff','--check'],check=True,env=env,capture_output=True);count+=1
for row in receipt['patches']:subprocess.run(['git','apply','--reverse','--check',r/row['path']],check=True,env=env,capture_output=True);count+=1
report=r/receipt['report']['path'];links=0
for target in re.findall(r'\]\(([^)]+)\)',report.read_text()):
 if '://' not in target:
  path=(report.parent/urllib.parse.unquote(target.split('#')[0])).resolve();check(path.exists(),str(path));links+=1
result={'status':'PASS','record_checks':count,'report_links':links,'indexed_entries_compared':len(index),'inventory_counts':receipt['git_inventory']['counts'],'source_changes':len(receipt['patches'][0]['changed_paths']),'prior_files_unchanged':len(receipt['starting_files_preserved_sha256']),'receipt_sha256':sha(e/'slice3-local.json')}
(r/'build/slice3-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
