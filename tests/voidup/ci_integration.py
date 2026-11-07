"""Focused wiring checks; never dispatch CI or modify a real provider prefix."""
import copy,json,os
from pathlib import Path
import sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/ci'))
import voidup_bootstrap as host

class Integration(unittest.TestCase):
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
