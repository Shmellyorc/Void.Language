"""Rebuild only the corrected Windows tampering fixture against qualified product libraries."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import traceback
import xml.etree.ElementTree as ET

out = Path(r'\\vboxsvr\VM\project-slice5')
receipt = out / 'fixture'
home = Path.home()
sdk = home / 'VoidCI42377496702-Native/.toolchains/llvm-23.1.4/bin'
packet = home / 'VoidS5-source-final'
cmake = Path(r'C:\gcc\bin\cmake.exe')
ctest = cmake.with_name('ctest.exe')
summary = {'status': 'running', 'profiles': {}, 'commands': []}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save():
    (receipt / 'rerun.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')


def run(profile, name, args, cwd, env):
    argv = list(map(str, args))
    with (receipt / (profile + '-' + name + '.log')).open('wb') as log:
        process = subprocess.run(argv, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=400)
    summary['commands'].append({'profile': profile, 'label': name, 'argv': argv, 'cwd': str(cwd), 'status': process.returncode})
    save()
    assert process.returncode == 0, (profile, name, process.returncode)


def wait(path, valid):
    for _ in range(240):
        try:
            data = json.loads(path.read_text())
            if valid(data):
                return data
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        time.sleep(5)
    raise TimeoutError(path)


try:
    source = receipt / 'creation.cpp'
    expected = (receipt / 'creation.sha256').read_text().strip()
    assert sha(source) == expected
    summary['fixture_sha256'] = expected
    summary['free_bytes_before'] = shutil.disk_usage(home).free
    save()
    for profile in ('Release', 'Debug'):
        result = wait(out / 'final' / (profile + '-result.json'), lambda d: d['status'] != 'running')
        assert result['stage'] == 'ctest' and result['os_build'] == 19045, result
        original_xml = out / 'final' / (profile + '-ctest.xml')
        suite = ET.parse(original_xml).getroot()
        failures = [node.get('name') for node in suite.findall('testcase') if node.find('failure') is not None]
        assert failures == ['project_creation'] and suite.get('tests') == '14', (failures, suite.attrib)
        assert 'Expected failure' in (out / 'final' / (profile + '-ctest.log')).read_text()
        build = home / ('VoidS5-Final-' + profile)
        scratch = build / 'slice5-fixture'
        scratch.mkdir()
        local = scratch / 'creation.cpp'
        shutil.copyfile(source, local)
        assert sha(local) == expected
        temp = home / ('VoidS5-tmp-' + profile)
        env = dict(os.environ, TEMP=str(temp), TMP=str(temp))
        binaries = ('voidc', 'voidls', 'void_project_application', 'void_project_sessions', 'void_project_creation')
        before = {name: sha(build / (name + '.exe')) for name in binaries}
        before_core = sha(build / 'void_core.lib')
        flags = ['/nologo', '/std:c++20', '/EHsc', '/MT', '/GR-', '/W4', '/permissive-',
                 '/D_ITERATOR_DEBUG_LEVEL=0', '/I' + str(packet / 'include')]
        flags += ['/O2', '/DNDEBUG'] if profile == 'Release' else ['/Od', '/Zi']
        obj = scratch / 'creation.obj'
        run(profile, 'compile', [sdk / 'clang-cl.exe', *flags, '/c', local, '/Fo' + str(obj),
                                 '/Fd' + str(scratch / 'creation.pdb')], build, env)
        ninja = (build / 'build.ninja').read_text()
        block = re.search(r'^build void_project_creation\.exe:.*?(?=^#|^build |\Z)', ninja, re.M | re.S)[0]
        libraries = re.search(r'^  LINK_LIBRARIES = (.*)$', block, re.M)[1]
        libraries = re.sub(r'\$([ $:])', lambda match: match[1], libraries)
        link_flags = re.search(r'^  LINK_FLAGS = (.*)$', block, re.M)[1]
        rc = re.search(r'^CMAKE_RC_COMPILER:[^=]+=(.*)$', (build / 'CMakeCache.txt').read_text(), re.M)[1]
        executable = scratch / 'void_project_creation.exe'
        response = scratch / 'link.rsp'
        response.write_text(f'"{obj}" {link_flags} {libraries} /out:"{executable}" /pdb:"{scratch / "linked.pdb"}"')
        intermediate = scratch / 'link'
        intermediate.mkdir()
        run(profile, 'link', [cmake, '-E', 'vs_link_exe', '--msvc-ver=1944', '--intdir=' + str(intermediate),
                              '--rc=' + rc, '--mt=' + str(sdk / 'llvm-mt.exe'), '--manifests',
                              '--', sdk / 'lld-link.exe', '@' + str(response)], build, env)
        run(profile, 'direct', [executable], build, env)
        previous = scratch / 'void_project_creation-original.exe'
        target = build / 'void_project_creation.exe'
        shutil.copyfile(target, previous)
        assert sha(previous) == before['void_project_creation']
        os.replace(executable, target)
        xml = receipt / (profile + '-corrected.xml')
        run(profile, 'ctest', [ctest, '--test-dir', build, '--output-on-failure', '-R', '^project_creation$',
                               '--output-junit', xml], build, env)
        run(profile, 'invalid-id', [sys.executable, receipt / 'invalid-id.py', build / 'voidc.exe'], build, env)
        after = {name: sha(build / (name + '.exe')) for name in binaries}
        assert sha(build / 'void_core.lib') == before_core
        assert all(before[name] == after[name] for name in binaries if name != 'void_project_creation')
        assert before['void_project_creation'] != after['void_project_creation']
        summary['profiles'][profile] = {'status': 'PASS', 'profile': profile, 'binaries': after,
                                        'prior_fixture_sha256': before['void_project_creation'],
                                        'unchanged_void_core_sha256': before_core, 'distinct_tests_passed': 14}
        save()
    replay = wait(out / 'final/native-results.json', lambda d: d['status'] != 'running')
    assert replay['restored_original_source']
    summary['status'] = 'PASS'
except BaseException:
    summary['status'] = 'FAIL'
    summary['error'] = traceback.format_exc()
finally:
    summary['free_bytes_after'] = shutil.disk_usage(home).free
    save()
