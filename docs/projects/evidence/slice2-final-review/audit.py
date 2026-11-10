from pathlib import Path
import json,hashlib,subprocess,re,urllib.parse
r=Path.cwd();e=r/'docs/projects/evidence';receipt=json.loads((e/'slice2-final-review.json').read_text());count=0
def check(v,label):
 global count
 count+=1
 assert v,label
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
for n,h in receipt['source_inputs_sha256'].items():check(sha(r/n)==h,n)
for n,h in receipt['starting_files_preserved_sha256'].items():check(sha(r/n)==h,n)
for n,v in receipt['review_amended'].items():check(sha(r/n)==v['final_sha256'],n)
for row in receipt['raw_evidence']+[receipt['report'],receipt['git_inventory']['evidence']]+receipt['patches']:
 check(sha(r/row['path'])==row['sha256'],row['path'])
for p in receipt['linux_profiles']:
 for n,h in p['binaries_sha256'].items():check(sha(r/('build/llvm23-'+p['profile'])/n)==h,n)
 check(p['passed']==36 and p['failed']==0,p['profile'])
for n,h in receipt['windows_native']['source_inputs_sha256'].items():check(sha(r/n)==h,n)
check(receipt['windows_native']['status']=='FAIL','Original Windows harness failure retained')
check(receipt['windows_nested_corrective_supplement']['status']=='PASS','Both Windows corrective runs pass')
for profile,c in zip(['Release','Debug'],receipt['windows_nested_corrective_supplement']['commands']):
 check(c['returncode']==0,profile+' corrected nested run')
 check(c['binary_sha256'] in receipt['windows_native'][profile+'_new_binary_sha256'].values(),profile+' unchanged binary')
actual=[{'status':v[:2],'path':v[3:]} for v in subprocess.check_output(['git','status','--porcelain=v1','-uall','-z']).decode().split('\0') if v]
check(actual==[{'status':v['status'],'path':v['path']} for v in receipt['git_inventory']['rows']],'Complete current Git inventory')
check(subprocess.check_output(['git','rev-parse','HEAD']).decode().strip()==receipt['baseline'],'HEAD')
check(sha(r/'.git/index')==receipt['operational_boundaries']['index_sha256'],'Index unchanged')
check(subprocess.check_output(['git','diff','--cached','--name-only'])==b'','Index empty')
subprocess.run(['git','diff','--check'],check=True,capture_output=True);count+=1
for p in receipt['patches']:
 subprocess.run(['git','apply','--reverse','--check',r/p['path']],check=True,capture_output=True);count+=1
report=r/receipt['report']['path'];links=0
for target in re.findall(r'\]\(([^)]+)\)',report.read_text()):
 if '://' not in target:
  path=(report.parent/urllib.parse.unquote(target.split('#')[0])).resolve();check(path.exists(),str(path));links+=1
result={'status':'PASS','checks':count,'report_links':links,'inventory_counts':receipt['git_inventory']['counts'],'source_changes':list(receipt['review_amended']),'prior_files_unchanged':len(receipt['starting_files_preserved_sha256']),'receipt_sha256':sha(e/'slice2-final-review.json')}
(r/'build/slice2-final-review-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
