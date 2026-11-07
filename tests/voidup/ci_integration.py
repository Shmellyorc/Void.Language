"""Focused wiring checks; never dispatch CI or modify a real provider prefix."""
import copy,hashlib,json,os,subprocess
from pathlib import Path
import sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/ci'))
import voidup_bootstrap as host
import llvm_sdk

class Toolchain(unittest.TestCase):
 def setUp(self):
  self.scratch=tempfile.TemporaryDirectory();self.addCleanup(self.scratch.cleanup)
  self.sdk=Path(self.scratch.name)/'sdk';self.resource=self.sdk/'lib/clang/23'
  identity={'version':llvm_sdk.VERSION,'asset':'fixture','sha256':'fixture'}
  self.create('void-sdk-identity.json',json.dumps(identity))
  self.create('include/llvm/Config/llvm-config.h',f'#define LLVM_VERSION_STRING "{llvm_sdk.VERSION}"')
  for path in ['bin/clang-cl.exe','bin/lld-link.exe','bin/llvm-lib.exe','bin/llvm-readobj.exe','lib/LLVMCore.lib','lib/clang/23/include/stddef.h','lib/clang/23/lib/windows/clang_rt.builtins-x86_64.lib']:self.create(path)
  self.addCleanup(patch.stopall)
  patch.object(llvm_sdk,'archive_identity',return_value=identity).start()
  self.outputs=patch.object(llvm_sdk.subprocess,'check_output',side_effect=self.output).start()
 def create(self,path,text='fixture'):
  path=self.sdk/path;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
 def output(self,command,**kwargs):
  return str(self.resource) if command[-1]=='/clang:-print-resource-dir' else 'LLVM version '+llvm_sdk.VERSION
 def test_selector_is_resource_leaf(self):
  self.assertEqual(llvm_sdk.windows_clangcl(self.sdk)['LLVMToolsVersion'],'23')
  other=self.resource.with_name('23.1.4');self.resource.rename(other);self.resource=other
  self.assertEqual(llvm_sdk.windows_clangcl(self.sdk)['LLVMToolsVersion'],'23.1.4')
  self.assertFalse(any(call.args[0][0].name=='llvm-lib.exe' for call in self.outputs.call_args_list))
 def test_wrong_version_fails(self):
  self.outputs.side_effect=lambda *args,**kwargs:'LLVM version 0.0.0'
  with self.assertRaisesRegex(RuntimeError,'version'):llvm_sdk.windows_clangcl(self.sdk)
 def test_wrong_or_external_resource_fails(self):
  for path in [self.resource.with_name('wrong'),self.sdk.parent/'ambient/lib/clang/23']:
   self.resource=path
   with self.assertRaisesRegex(RuntimeError,'resource directory'):llvm_sdk.windows_clangcl(self.sdk)
 def test_missing_components_fail(self):
  for path in ['bin/clang-cl.exe','bin/lld-link.exe','bin/llvm-lib.exe','bin/llvm-readobj.exe','lib/LLVMCore.lib','lib/clang/23/include/stddef.h','lib/clang/23/lib/windows/clang_rt.builtins-x86_64.lib','include/llvm/Config/llvm-config.h','void-sdk-identity.json']:
   file=self.sdk/path;before=file.read_bytes();file.unlink()
   with self.assertRaises(RuntimeError,msg=path):llvm_sdk.windows_clangcl(self.sdk)
   file.write_bytes(before)
 def test_mismatched_headers_and_receipt_fail(self):
  self.create('include/llvm/Config/llvm-config.h','mismatch')
  with self.assertRaisesRegex(RuntimeError,'headers'):llvm_sdk.windows_clangcl(self.sdk)
  self.create('void-sdk-identity.json','{}')
  with self.assertRaisesRegex(RuntimeError,'identity'):llvm_sdk.windows_clangcl(self.sdk)
 def test_ambient_probe_override_fails_before_execution(self):
  self.outputs.reset_mock()
  with self.assertRaisesRegex(RuntimeError,'Only the pinned'):llvm_sdk.pinned_cxx(self.sdk/'bin/clang-cl.exe')
  self.outputs.assert_not_called()

class Integration(unittest.TestCase):
 def test_approved_host_patch_checkout_bytes(self):
  # Exercise Git's checkout filters even on Unix; Windows must receive the
  # same bytes that the patch policy and corresponding-source notice bind.
  folder='third_party/host-patches/libsodium-1.0.22/'
  policy=json.loads((ROOT/folder/'PROVENANCE.json').read_text())
  for name in ('PROVENANCE.json',policy['patch']):
   path=folder+name
   original=subprocess.check_output(['git','show','HEAD:'+path],cwd=ROOT)
   checkout=subprocess.check_output(['git','-c','core.autocrlf=true','cat-file','--filters','HEAD:'+path],cwd=ROOT)
   self.assertEqual(checkout,original,path)
   self.assertEqual((ROOT/path).read_bytes(),checkout,path)
   if name==policy['patch']:
    self.assertEqual(hashlib.sha256(checkout).hexdigest(),policy['patch_sha256'])
 def test_native_profile_layouts(self):
  for system,arch,bundle in [('Linux','x86_64',''),('Windows','x86_64','windows-x86_64/'),('Darwin','arm64','macos-arm64/')]:
   for configuration,profile in [('Debug','Debug'),('Release','Release'),('Sanitizers','strict')]:
    if configuration=='Sanitizers' and system!='Linux':continue
    plan={'profile':profile,'prefix':'http/'+profile,'identity':{'host':{'system':system,'architecture':arch}}}
    with patch.object(host.http,'make_plan',return_value=plan):actual=host.layout(configuration)
    self.assertEqual(actual['host_prefix'],f'.toolchains/host/prefix/{bundle}{profile}')
    self.assertTrue(actual['zlib'].endswith(('zs.lib' if profile=='Release' else 'zsd.lib') if system=='Windows' else 'libz.a'))
 def test_selected_workflow_order_and_no_credentials(self):
  text=(ROOT/'.github/workflows/ci-link-experiment.yml').read_text()
  self.assertEqual(text.count('- {os:'),7)
  self.assertLess(text.index('http_bootstrap.py bootstrap'),text.index('voidup_bootstrap.py bootstrap'))
  self.assertLess(text.index('voidup_bootstrap.py bootstrap'),text.index('- name: Configure ${{ matrix.configuration }}'))
  self.assertLess(text.index('voidup_bootstrap.py test-plan'),text.index('- name: Execute ${{ matrix.configuration }} tests'))
  for forbidden in ['package_voidup_fixtures','offline-','PRIVATE KEY','tests/voidup/acceptance.py','git add']:
   self.assertNotIn(forbidden,text)
  self.assertIn('ctest --test-dir build/${{ matrix.configuration }} --output-on-failure --output-junit ctest-results.xml',text)
 def test_missing_disabled_and_private_tests_rejected(self):
  plan={'identity':{'host':{'system':'Linux'}}}
  tests=[{'name':name,'command':['native-check']} for name in host.COMMON_TESTS|host.LINUX_TESTS]
  with tempfile.TemporaryDirectory() as tmp:
   def check(rows):
    with patch.object(host.subprocess,'check_output',return_value=json.dumps({'tests':rows})):
     host.test_plan(plan,Path(tmp),Path(tmp))
   check(tests)
   with self.assertRaisesRegex(RuntimeError,'not registered'):check(tests[1:])
   disabled=copy.deepcopy(tests);disabled[0]['properties']=[{'name':'DISABLED','value':True}]
   with self.assertRaisesRegex(RuntimeError,'disabled'):check(disabled)
   with self.assertRaisesRegex(RuntimeError,'Private signed'):check([*tests,{'name':'voidup_private_install','command':['forbidden']}])
 def test_disk_preflight_checks_both_volumes(self):
  plan={'identity':{'host':{'system':'Linux'}}}
  with tempfile.TemporaryDirectory() as tmp,patch.object(host.shutil,'which',return_value='/verified/tool'):
   usage=type('Usage',(),{'free':host.HOST_RESERVE+host.APP_RESERVE})()
   with patch.object(host.shutil,'disk_usage',return_value=usage):host.preflight(plan,Path(tmp))
   usage.free=host.HOST_RESERVE-1
   with patch.object(host.shutil,'disk_usage',return_value=usage):
    with self.assertRaisesRegex(RuntimeError,'headroom'):host.preflight(plan,Path(tmp))

if __name__=='__main__':unittest.main(verbosity=2)
