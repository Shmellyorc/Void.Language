"""Read-only Git audit and explicit VOIDUP staging inventory. Never stages files."""
from pathlib import Path
import hashlib,json,subprocess

ROOT=Path(__file__).resolve().parents[1]
EXACT={'.github/workflows/ci-link-experiment.yml','CMakeLists.txt','cmake/CurlProvider.cmake',
 'cmake/HostDependencies.cmake','cmake/host-sodium.targets','cmake/InstallLicenses.cmake','cmake/NativeBuild.cmake',
 'docs/release/NOTICE_INVENTORY.json','docs/release/REDISTRIBUTION_NOTICES.md',
 'include/void/application.hpp','include/void/filesystem.hpp','include/void/filesystem_abi.hpp',
 'include/void/host_abi.hpp','include/void/host_ops.hpp','include/void/ir.hpp','include/void/runtime.hpp','include/void/runtime_abi.hpp',
 'src/backend/llvm/backend.cpp','src/backend/llvm/host.inc','src/frontend/compiler.cpp','src/frontend/host.inc',
 'src/ir/module.cpp','src/ir/host.inc','src/main.cpp','src/runtime/abi.cpp','src/runtime/application.cpp',
 'src/runtime/filesystem.cpp','src/runtime/filesystem_abi.cpp','src/runtime/host_abi.cpp','src/runtime/host_archive.cpp',
 'src/runtime/host_crypto.cpp','src/runtime/host_directory.cpp','src/runtime/host_directory_windows.cpp',
 'src/runtime/host_error.cpp','src/runtime/host_internal.hpp','src/runtime/host_posix.hpp','src/runtime/host_process.cpp',
 'src/runtime/host_process_windows.cpp','src/runtime/host_windows.hpp',
 'stdlib/archive.void','stdlib/crypto.void','stdlib/io.void','stdlib/os.void','stdlib/process.void','stdlib/system_error.void',
 'third_party/host-versions.json','tools/build_voidup.py','tools/package_voidup_fixtures.py',
 'tools/setup_host_deps.py','tools/setup_host_deps_native.py','tools/verify_host_deps.py',
 'tools/ci/voidup_bootstrap.py','tools/ci/linux_disk.py','tools/install.py','tools/voidup_integration_inventory.py'}
TREES=('apps/voidup/','tests/host_capabilities/','tests/voidup/','third_party/host-licenses/',
       'docs/voidup/','docs/os/evidence/crashes/incident-2cmxfrao/')
EXCLUDED={'docs/net/TCP_UDP_HANDOFF.md','website/dist/assets/style.css','website/dist/getting-started/index.html',
          'website/public/assets/style.css','website/public/getting-started/index.html',
          'website/scripts/check.py','website/scripts/test_check.py'}
GENERATED={'docs/voidup/COMMIT_PATHS.txt','docs/voidup/COMMIT_PATHS.nul','docs/voidup/COMMIT_INVENTORY.json',
           'docs/voidup/evidence/integration/INDEX.json'}

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
def generated_cache(name):return '/.void/' in ('/' + name)
def member(name):return (name in EXACT or name.startswith(TREES)) and not generated_cache(name)
def main():
 evidence=ROOT/'docs/voidup/evidence/integration'
 visible={x.decode() for args in [('diff','--name-only','-z'),('diff','--cached','--name-only','-z'),
           ('ls-files','--others','--exclude-standard','-z')] for x in git(*args).split(b'\0') if x}
 unknown=sorted(name for name in visible if not member(name) and not generated_cache(name) and name not in EXCLUDED)
 if unknown:raise RuntimeError('Unclassified changed paths (review before expanding scope): '+repr(unknown))
 paths={name for name in visible if member(name)}
 # Exact evidence includes ignored .log files, not only Git-visible untracked names.
 for name in TREES:
  for path in (ROOT/name).rglob('*'):
   if path.is_file() and '.void' not in path.parts and '__pycache__' not in path.parts and path.suffix!='.pyc':paths.add(path.relative_to(ROOT).as_posix())
 paths |= GENERATED
 for name in paths-GENERATED:
  path=ROOT/name
  if path.is_symlink() or path.suffix.lower() in {'.pem','.key','.exe','.dll','.so','.a','.o','.obj','.zip','.gz','.xz','.pyc'}:
   raise RuntimeError('Unexpected key/build/archive/symlink candidate: '+name)
  with path.open('rb') as stream:head=stream.read(32)
  if head.startswith((b'\x7fELF',b'MZ',b'-----BEGIN PRIVATE KEY')):raise RuntimeError('Unexpected executable/private key: '+name)
 baseline=json.loads((evidence/'baseline.json').read_bytes());changed=[]
 for name,before in baseline.items():
  if before is not None and (not (ROOT/name).is_file() or sha(ROOT/name)!=before):changed.append(name)
 if set(changed)!={'CMakeLists.txt','tests/host_capabilities/portable.py'}:
  raise RuntimeError('Unexpected modification to initial dirty/untracked files: '+repr(changed))
 original=json.loads((ROOT/'docs/voidup/evidence/phase_b/INDEX.json').read_bytes())
 for record in original['files']:
  assert sha(ROOT/'docs/voidup/evidence/phase_b'/record['path'])==record['sha256']
 assert sha(ROOT/'docs/voidup/PHASE_B_REPORT.md')==original['report_sha256']
 for folder,index_name in [('docs/voidup/evidence/portability','INDEX.json'),('docs/voidup/evidence','portability/INDEX-before.json')]:
  record=json.loads((ROOT/folder/index_name).read_bytes())
  for name,digest in record['evidence_files'].items():assert sha(ROOT/folder/name)==digest,name
 ordered=sorted(paths)
 (ROOT/'docs/voidup/COMMIT_PATHS.txt').write_text('\n'.join(ordered)+'\n')
 (ROOT/'docs/voidup/COMMIT_PATHS.nul').write_bytes(b'\0'.join(name.encode() for name in ordered)+b'\0')
 ignored=subprocess.run(['git','check-ignore','--stdin','-z'],cwd=ROOT,input=b'\0'.join(name.encode() for name in ordered)+b'\0',capture_output=True)
 if ignored.returncode not in (0,1):raise RuntimeError(ignored.stderr.decode())
 ignored_names=sorted(x.decode() for x in ignored.stdout.split(b'\0') if x)
 inventory=dict(schema=1,intended_paths=ordered,intended_count=len(ordered),ignored_but_intended=ignored_names,
  excluded_visible_paths=sorted(visible&EXCLUDED),
  excluded_build_and_toolchain_roots=['build*/','.toolchains/','**/__pycache__/'],
  excluded_private_signing_keys=[f'build/voidup-fixtures/keys/offline-{n}.pem' for n in (1,2,3)],
  baseline_changes=changed,all_other_baseline_hashes_unchanged=True,phase_a_b_evidence_hashes_unchanged=True,
  file_hashes={name:sha(ROOT/name) for name in ordered if name not in GENERATED},
  generated_inventory_hashes_omitted='Avoid circular self hashes; integration INDEX separately binds the inventory files',
  suggested_title='Integrate VOIDUP host capabilities, manager and native CI bootstrap [skip ci]',
  staging_command='git --literal-pathspecs add -f --pathspec-from-file=docs/voidup/COMMIT_PATHS.nul --pathspec-file-nul',
  staging_performed=False)
 (ROOT/'docs/voidup/COMMIT_INVENTORY.json').write_text(json.dumps(inventory,indent=2)+'\n')
 files=[dict(path=path.relative_to(evidence).as_posix(),bytes=path.stat().st_size,sha256=sha(path)) for path in sorted(evidence.rglob('*')) if path.is_file() and path.name!='INDEX.json']
 (evidence/'INDEX.json').write_text(json.dumps(dict(schema=1,files=files,report_sha256=sha(ROOT/'docs/voidup/INTEGRATION_REPORT.md'),
   inventory_sha256=sha(ROOT/'docs/voidup/COMMIT_INVENTORY.json'),paths_sha256=sha(ROOT/'docs/voidup/COMMIT_PATHS.nul')),indent=2)+'\n')
 print(f'{len(ordered)} exact intended paths; {len(ignored_names)} ignored evidence paths explicitly allowlisted; {len(visible&EXCLUDED)} unrelated paths excluded. No staging performed.')

if __name__=='__main__':main()
