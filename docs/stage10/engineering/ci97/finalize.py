"""Aggregate this bounded repair without promoting local tests to native acceptance."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
initial=read(base/'initial-hashes.json');freeze=read(base/'repair-source-freeze.json')
assert all(sha(root/p)==h for p,h in freeze.items())
changed=[p for p,h in initial.items() if sha(root/p)!=h]
assert set(changed)=={p for p in freeze if p in initial},changed
preserved={p:h for p,h in initial.items() if p.startswith(('.github/','docs/stage10/engineering/step41')) or p in ('CMakeLists.txt','README.md')}
assert all(sha(root/p)==h for p,h in preserved.items())
sdk=root/'.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64';configurations={}
for name in ('debug','san'):
 build=root/'build'/('step42-ci97-'+name);old=root/'build'/('step42-ci96-'+name)
 tests=read(base/(name+'-tests.json'));repeat=read(base/(name+'-keyboard-repeat-records.json'));aot=read(base/(name+'-source-pty-records.json'))
 assert len(tests['tests'])==11 and all(t['returncode']==0 for t in tests['tests'])
 assert len(repeat)==20 and all(t['returncode']==0 for t in repeat)
 assert len(aot)==2 and all(t['returncode']==0 and t['restored_configuration'] for t in aot)
 members=subprocess.check_output([sdk/'bin/llvm-ar','t',old/'libvoid_runtime.a'],text=True).splitlines()
 assert members==subprocess.check_output([sdk/'bin/llvm-ar','t',build/'libvoid_runtime.a'],text=True).splitlines()
 delta=[]
 for member in members:
  before=subprocess.check_output([sdk/'bin/llvm-ar','p',old/'libvoid_runtime.a',member]);after=subprocess.check_output([sdk/'bin/llvm-ar','p',build/'libvoid_runtime.a',member])
  if before!=after:delta.append(member)
 assert delta==['console.cpp.o'],delta
 binaries={str(p.relative_to(root)):sha(p) for p in build.iterdir() if p.is_file() and (p.name.startswith('void_runtime_') or p.suffix=='.a' or p.name=='console.cpp.o')}
 (base/(name+'-final-binary-hashes.json')).write_text(json.dumps(binaries,indent=2)+'\n')
 configurations[name]=dict(status='PASS: focused Linux harness only',suites_passed=11,suites_executed=11,failed=0,skipped=0,native_keyboard_repetitions=20,normal_O2_linked_source_PTY_cases=2,commands=name+'-tests.json',build_commands=name+'-build-commands.json',final_keyboard_build=name+'-final-keyboard-build.json',binary_hashes=name+'-final-binary-hashes.json',changed_runtime_archive_members=delta,ASAN_OPTIONS=tests['ASAN_OPTIONS'],UBSAN_OPTIONS=tests['UBSAN_OPTIONS'])
path=root/'docs/stage10/engineering/step42-results.json';results=read(path)
if 'historical_ci96_local_verification' not in results:results['historical_ci96_local_verification']=results['local_verification']
# Verify the accepted CI96 original build identities were not altered by this harness.
old_binary_mismatches=[]
for name,record in results['historical_ci96_local_verification']['configurations'].items():
 for relative,h in record.get('binary_archive_object_sha256',{}).items():
  p=root/record['build_directory']/relative
  if sha(p)!=h:old_binary_mismatches.append(str(p.relative_to(root)))
assert not old_binary_mismatches,old_binary_mismatches
verification=dict(source_freeze_matches=True,modified_existing_code=changed,preserved_workflows_and_step41_inputs=preserved,original_ci96_binary_archive_object_mismatches=old_binary_mismatches,compiler_frontend_and_stack_protection_changed=False,native_macOS_execution=False,native_Windows_execution=False,full_CMake_or_JIT_acceptance=False)
(base/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
results.update(updated_utc=datetime.now(timezone.utc).isoformat(),status='CI97 native console repairs locally verified; Golden Wrench review and actual macOS verification pending; Step42 INCOMPLETE',step42_complete=False,stage_status='INCOMPLETE',authority='Boss continuation: CI97 native console repairs and focused Linux Debug/strict verification only. No Git mutations, dispatch, workflow changes or new architecture.')
results['local_verification']=dict(status='PASS: Debug11/11 and strict11/11 console suites; native keyboard20/20 per configuration; normal/O2 linked source PTY2/2 each',configurations=configurations,source_freeze='ci97/repair-source-freeze.json',verification='ci97/verification.json',scope='Isolated focused native harness replaces only console.cpp.o in copies of accepted CI96 archives. Compiler used for AOT emission is accepted CI96; not a fresh compiler/JIT or full CMake/matrix acceptance.',stack_protections='Original source limits, stack scripts and Step41 evidence unchanged; earlier accepted genuine1MiB results retained, not rerun here.',native_results='Repaired macOS and Windows unverified')
results['ci97_console_repairs']=dict(baseline_commit='96bac2ea8e4be31d574a5916118feab0ef6b2e7f',local_start_commit='8485e6404596cf73deb55302ad3678a23cc0c6a4',main_run_id=40069548583,main_observation=dict(Windows_Debug='194/194',Windows_Release='194/194',Ubuntu_Debug='199/199',Ubuntu_Release='199/199',Ubuntu_Sanitizers='198/199 native keyboard failed',macOS_Debug='195/199 four native console failures',macOS_Release='195/199 same four failures',evidence='ci97/main-jobs.json; ci97/main-success-excerpts.json; ci97/raw/main-*.log'),ThinLTO2_observation=dict(run_id=40075098927,commit='8485e6404596cf73deb55302ad3678a23cc0c6a4',evidence='ci97/thinlto2-jobs.json; ci97/raw/thinlto2-ubuntu-debug.log',scope='Separate observation, no cache causation or repair claimed; workflows unchanged'),diagnosis='ci97/DIAGNOSIS.md',diff='ci97/repairs.diff',kernel_source='ci97/kernel-sources.json',native_before_after='ci97/host-linux.json; ci97/host-linux.json.flags.json',buffered_reproduction=dict(before='key=4 bytes=1b native_batch=4 clock_ms=31',after='key=5 bytes=1b5b41 native_batch=4 clock_ms=31',raw='ci97/buffered-before.log; ci97/buffered-after.log',strict_before_failure='ci97/regression-before-san-execute.log',mode='Virtual-clock mechanism; not uninstrumented reproduction of hosted scheduling'),PTY_read_batches='ci97/raw/before-read-batches.jsonl',failure_history=dict(LSan_ptrace='ci97/before-san-keyboard.log; accepted untraced runs preserve all settings',compiler='ci97/raw/san-driver-compile-failure.log; ci97/raw/san-build-failure-commands.json; cause unproven, no compiler fix claimed',new_guard_test='ci97/raw/new-guard-test-before-correction.log; corrected max-read assumption to ordered read_char calls'),next='Golden Wrench review; Boss-controlled macOS observer and Debug/Release native regressions, then remaining Step42 acceptance. Stage10 INCOMPLETE; Steps43/44 separate.')
results['platforms']['windows']['status']='CI97 BASELINE ACCEPTED: Debug194/194 and Release194/194; current repair not executed natively'
results['platforms']['windows']['ci97_baseline']={'run_id':40069548583,'commit':'96bac2ea8e4be31d574a5916118feab0ef6b2e7f','Debug':'194/194','Release':'194/194'}
results['platforms']['macos']['status']='CI97 Debug/Release195/199; current console repair native verification pending'
results['baseline']['current_historical_source_manifest_differences']=sorted(set(results['baseline']['current_historical_source_manifest_differences'])|set(freeze))
path.write_text(json.dumps(results,indent=2)+'\n')
print('Verified source freeze, original binaries, workflows and Step41 evidence; wrote Step42 results.')
