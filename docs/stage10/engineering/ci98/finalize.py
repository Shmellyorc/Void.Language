"""Verify bounded test repairs and preserve earlier source/acceptance records."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,shutil,subprocess
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
initial=read(base/'initial-hashes.json');freeze=read(base/'repair-source-freeze.json')
assert all(sha(root/p)==h for p,h in freeze.items())
changed=[p for p,h in initial.items() if sha(root/p)!=h]
assert set(changed)==set(freeze),changed
assert subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()=='9a4985f89f40c56b8eaa984775faf18c52b69f19'
configurations={};progress_records={};old_binary_mismatches=[]
for name in ('debug','san'):
 build=root/'build'/('step42-ci98-'+name)
 tests=read(base/(name+'-tests.json'));repeated=read(base/(name+'-repeat-records.json'));repro=read(base/(name+'-reproduction-records.json'))
 assert len(tests['tests'])==11 and all(r['returncode']==0 for r in tests['tests'])
 assert len(repeated)==40 and all(r['returncode']==0 for r in repeated)
 assert all(sum(r['fixture']==stem for r in repeated)==20 for stem in ('console_terminal','console_transfers'))
 assert repro['original_native_fixture']['rows']==['1 2002 2 ','0 80 24','3 80 24','']
 assert bytes.fromhex(repro['same_process_later_input']['stderr_hex']).decode()=='input_kind=3 query_error=3 first_error=4 first_bytes=0 later=7265646972656374656400ff\n'
 assert bytes.fromhex(repro['admission_before_write']['stdout_hex']).decode()=='admitted=1 completed=0 error=6\n'
 lines=(base/(name+'-test-console_transfers-native.log')).read_text().splitlines()
 cases=json.loads(next(l for l in lines if l.startswith('{')))['native_cases']
 progress_records[name]={r['case']:r['native_progress'] for r in cases if r['case'] in ('cancel-write','closed-input-while-output')}
 assert len(progress_records[name])==2
 for case,record in progress_records[name].items():
  assert record['initial_poll_result']==1 and record['initial_poll_revents']&4
  assert record['poll_result']==0 and record['poll_revents']==0
  assert 0<record['completed']<1024*1024 and record['error']==6
  assert record['missing_input']==(case=='closed-input-while-output')
 terminal=read(base/(name+'-step29-native-terminal-results.json'))
 assert [r['report'][-1] for r in terminal['records'] if r['case']=='redirected-input']==['7265646972656374656400ff']
 shutil.copy2(build/'step29-native-terminal-results.json',base/(name+'-repeat-last-terminal-results.json'))
 for p,h in read(root/'docs/stage10/engineering/ci97'/(name+'-final-binary-hashes.json')).items():
  if sha(root/p)!=h:old_binary_mismatches.append(p)
 binaries={str(p.relative_to(root)):sha(p) for p in build.glob('void_runtime_console*') if p.is_file()}
 (base/(name+'-final-binary-hashes.json')).write_text(json.dumps(binaries,indent=2)+'\n')
 configurations[name]=dict(status='PASS: focused Linux test drivers',suites_passed=11,suites_executed=11,failed=0,skipped=0,repetitions_per_repaired_native_suite=20,tests=name+'-tests.json',repeats=name+'-repeat-records.json',reproduction=name+'-reproduction-records.json',build=name+'-build.json',binary_hashes=name+'-final-binary-hashes.json',ASAN_OPTIONS=tests['ASAN_OPTIONS'],UBSAN_OPTIONS=tests['UBSAN_OPTIONS'])
assert not old_binary_mismatches,old_binary_mismatches
source_diff=subprocess.check_output(['git','diff','--',*sorted(freeze)])
(base/'repairs.diff').write_bytes(source_diff)
check=subprocess.run(['git','apply','--reverse','--check',str(base/'repairs.diff')],capture_output=True,text=True)
assert check.returncode==0,check.stderr
verification=dict(baseline='9a4985f89f40c56b8eaa984775faf18c52b69f19',source_freeze_matches=True,modified_test_files=changed,production_changed=False,workflows_changed=False,stack_protections_changed=False,original_CI97_binary_mismatches=old_binary_mismatches,preserved_input_count=len(initial)-len(changed),preserved_Step41_and_CI97_evidence=True,patch_sha256=sha(base/'repairs.diff'),patch_reverse_check=0,raw_progress=progress_records,native_macOS_execution=False,full_CMake_JIT_AOT_acceptance=False)
(base/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
results_path=root/'docs/stage10/engineering/step42-results.json';results=read(results_path)
if 'historical_ci97_local_verification' not in results:results['historical_ci97_local_verification']=results['local_verification']
results.update(updated_utc=datetime.now(timezone.utc).isoformat(),status='CI98/ThinLTO3 macOS Release fixture synchronization repairs locally verified; Golden Wrench review and native Release verification pending; Step42 INCOMPLETE',step42_complete=False,stage_status='INCOMPLETE',authority='Boss authorizes bounded investigation/repair of the two macOS Release failures. Test-only changes; no Git mutation, CI dispatch, workflow edit or new architecture.')
results['local_verification']=dict(status='PASS: Debug11/11 and strict11/11; each repaired native suite20/20 repetitions per configuration',configurations=configurations,source_freeze='ci98/repair-source-freeze.json',verification='ci98/verification.json',scope='Only C++ transfer test driver rebuilt; production Runtime archives and other focused test binaries are unchanged accepted CI97 artifacts. No full CMake/JIT/AOT/native macOS acceptance claimed.',stack_protections='Original Step41 evidence and genuine1MiB protections unchanged; not rerun in this test-only repair.',native_results='Repaired macOS Release test synchronization not yet executed natively')
results['ci98_final_macos_investigation']=dict(baseline_commit='9a4985f89f40c56b8eaa984775faf18c52b69f19',main98=dict(run_id=40091828693,jobs_passed=6,jobs_total=7,macOS_Release='198/199; runtime_console_native_terminal redirected-input empty bytes',jobs='ci98/main98-jobs.json',raw='ci98/raw/main98-macos-release.log'),ThinLTO3=dict(run_id=40091853317,jobs_passed=6,jobs_total=7,macOS_Release='198/199; runtime_console_native_transfers closed-input-while-output prefix assertion',jobs='ci98/thinlto3-jobs.json',raw='ci98/raw/thinlto3-macos-release.log'),classification=dict(redirected_input='Fixture delivery/poll ordering. Same Context later reads all12 original bytes; no production input loss or classification defect demonstrated.',cancelled_prefix='Fixture cancellation/progress ordering. Admission alone permits legal0-byte cancellation; new initial-writable to non-writable native pipe boundary proves progress before close.'),production_repairs=[],diagnosis='ci98/DIAGNOSIS.md',diff='ci98/repairs.diff',native_progress=progress_records,hosted_numeric_prefix_count='Not recorded in original failure log; zero count is inferred/model-reproduced, not claimed as a native macOS measurement.',remaining_native_verification='Golden Wrench review, then Boss-controlled macOS Release rebuild of transfer test and both original native fixture reruns/repetitions, raw terminal/progress reports and full Release acceptance. Other six green configurations retained as baseline. Stage10 INCOMPLETE; no Steps43/44 work.')
results['platforms']['macos']['status']='CI98 and ThinLTO3 macOS Debug accepted; Release198/199 in each with different fixture synchronization failures. Current test-only repair native Release verification pending.'
results['platforms']['macos']['ci98_baseline']={'commit':'9a4985f89f40c56b8eaa984775faf18c52b69f19','main_run':40091828693,'experiment_run':40091853317,'Debug':'Both jobs green','Release':'Each198/199; independent native terminal/transfer failures'}
results['platforms']['windows']['status']='CI98/ThinLTO3 baseline Debug/Release green; no current native rerun or production change'
results['baseline']['current_historical_source_manifest_differences']=sorted(set(results['baseline']['current_historical_source_manifest_differences'])|set(freeze))
results_path.write_text(json.dumps(results,indent=2)+'\n')
print('Verified all frozen production/workflow/historical inputs and original binaries; source patch reverse-check passes; Step42 results updated.')
