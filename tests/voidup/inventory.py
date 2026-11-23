"""Execute the real inventory parser for Linux, macOS and Windows contracts.

The native compiler runs on this host. Other platform branches are modeled by
replacing os.platform() only in a disposable model.void copy. Producer install
and dependency-audit commands are modeled; inventory/ZIP generation is real.
No installed tools, trust roots or production application sources are changed.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import package_voidup_fixtures as producer
import voidup_dependencies
import voidup_macos

HOSTS = {
    'linux': ('Linux', 'x86_64', 'linux-x86_64-ubuntu24.04', 'linux',
              ['bin/voidc', 'bin/voidls', 'sdk/bin/clang', 'sdk/bin/ld.lld']),
    'macos': ('Darwin', 'arm64', 'macos-aarch64-14.0', 'darwin',
              ['bin/voidc', 'bin/voidls', 'sdk/bin/clang', 'sdk/bin/ld64.lld']),
    'windows': ('Windows', 'amd64', 'windows-x86_64', 'win32',
                ['bin/voidc.exe', 'bin/voidls.exe', 'sdk/bin/clang.exe', 'sdk/bin/lld-link.exe']),
}
REQUIRED = 'required executable absent from inventory'


def payload(work, platform):
    system, machine, host, python_platform, required = HOSTS[platform]
    sdk = work / 'sdk'
    for name in ['clang-23', 'clang', 'lld', 'ld64.lld', 'clang.exe', 'lld-link.exe']:
        path = sdk / 'bin' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'nonproduction fixture\n')
        path.chmod(0o755)
    (sdk / 'lib/clang/23/include').mkdir(parents=True)
    (sdk / 'lib/clang/23/include/fixture.h').write_bytes(b'fixture\n')

    def install(argv, **kwargs):
        prefix = Path(argv[argv.index('--prefix') + 1])
        files = {name: b'fixture\n' for name in required[:2]}
        files.update({'bin/void-native.txt': b'VOID native 23.1.4 v2\n/producer/clang\n/producer/lld\n../lib/void/runtime.a\n../lib/void/fixture.a\n',
                      'bin/void-native-resources.txt': b'VOID native resources 1\n',
                      'share/void/stdlib/io.void': b'fixture\n'})
        for name, data in files.items():
            path = prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o755 if name in required else 0o644)
        return subprocess.CompletedProcess(argv, 0)

    with patch.object(producer.platform, 'system', return_value=system), \
         patch.object(producer.platform, 'machine', return_value=machine), \
         patch.object(producer.sys, 'platform', python_platform), \
         patch.object(producer.subprocess, 'run', side_effect=install), \
         patch.object(producer.subprocess, 'check_output', return_value='Build identity: ' + 'a' * 64), \
         patch.object(voidup_dependencies, 'audit', return_value={}), \
         patch.object(voidup_macos, 'relocate', return_value={}):
        release = producer.produce(work / 'build', 'contract', work / 'fixtures', sdk, host=host)
    inventory = json.loads((work / 'fixtures/www/inventories/contract.json').read_bytes())
    members = {m['name']: m for m in inventory['members']}
    for name in required:
        assert members[name]['executable'] and not members[name]['directory'], name
    with zipfile.ZipFile(work / 'fixtures/www/packages/contract.zip') as archive:
        for name in required:
            assert archive.getinfo(name).external_attr >> 16 == 0o100755
            assert hashlib.sha256(archive.read(name)).hexdigest() == members[name]['sha256']
    manifest = (work / 'fixtures/producer/contract/bin/void-native.txt').read_text().splitlines()
    assert manifest[2] == '../' + required[-1], manifest
    assert manifest[1] == ('../sdk/bin/void-clang' if platform == 'macos' else '../' + required[-2])
    return inventory


def vectors(inventory, platform):
    required = HOSTS[platform][-1]
    cases = [('valid producer inventory', inventory, '', False)]
    def add(label, change, error=REQUIRED, bad_digest=False):
        document = copy.deepcopy(inventory)
        change(document)
        document['expanded_bytes'] = sum(m['size'] for m in document['members'] if not m['directory'])
        cases.append((label, document, error, bad_digest))
    for name in required:
        add('missing ' + name, lambda d, n=name: d['members'].__setitem__(slice(None), [m for m in d['members'] if m['name'] != n]))
        add('not executable ' + name, lambda d, n=name: next(m for m in d['members'] if m['name'] == n).update(executable=False))
    for wrong in ['sdk/bin/ld.lld', 'sdk/bin/ld64.lld', 'sdk/bin/lld-link.exe']:
        if wrong != required[-1]:
            add('wrong linker ' + wrong, lambda d, n=wrong: next(m for m in d['members'] if m['name'] == required[-1]).update(name=n))
    add('linker is directory', lambda d: next(m for m in d['members'] if m['name'] == required[-1]).update(name=required[-1]+'/', directory=True, executable=False, size=0, sha256=''))
    add('directory executable metadata', lambda d: next(m for m in d['members'] if m['directory']).update(executable=True), 'directory has file metadata')
    add('invalid member digest', lambda d: next(m for m in d['members'] if not m['directory']).update(sha256='invalid'), 'invalid file metadata')
    add('inventory digest mismatch', lambda d: None, 'inventory digest or length mismatch', True)
    add('inventory identity mismatch', lambda d: d.update(host='different-host'), 'inventory identity differs from catalog')
    return cases


DRIVER = '''use "./model.void" as model
use "./support.void" as support

func verify(raw: string, expected: string, bad_digest: bool) -> result<bool, support.up_error>:
    var data = raw.to_utf8()
    var digest = support.up_digest(data)?
    if bad_digest:
        digest = "0000000000000000000000000000000000000000000000000000000000000000"
    var release = model.up_release(id = "contract", host = "HOST", build_identity = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", inventory_bytes = data.count, inventory_sha256 = digest)
    var parsed = model.up_inventory_parse(data, release)
    if parsed is Ok(var document):
        return Ok(expected == "")
    else if parsed is Err(var failure):
        if expected == "" or failure.message != expected or failure.operation != "inventory":
            print("Unexpected inventory failure: {failure.message}")
            return Ok(false)
        return Ok(true)
    return Ok(false)

func main() -> i32:
BODY
    print("PASS PLATFORM: COUNT inventory cases")
    return 0
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--platform', choices=HOSTS)
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    if args.evidence:
        args.evidence.mkdir(parents=True, exist_ok=True)
    for platform in [args.platform] if args.platform else HOSTS:
        with tempfile.TemporaryDirectory(prefix='voidup-inventory-') as temporary:
            work = Path(temporary)
            inventory = payload(work, platform)
            cases = vectors(inventory, platform)
            for source in (ROOT / 'apps/voidup').glob('*.void'):
                shutil.copyfile(source, work / source.name)
            model = work / 'model.void'
            source = model.read_text()
            assert 'os.platform()' in source
            native = {'linux': 'linux', 'darwin': 'macos', 'win32': 'windows'}.get(sys.platform)
            if platform != native:
                model.write_text(source.replace('os.platform()', json.dumps(platform)))
            body = []
            for index, (label, document, error, bad_digest) in enumerate(cases, 1):
                raw = producer.encoded(document).decode().replace('{', '{{').replace('}', '}}')
                body.append(f'    # {label}\n    if verify({json.dumps(raw)}, {json.dumps(error)}, {str(bad_digest).lower()}) is Ok(var valid{index}):\n        if not valid{index}:\n            return {index}\n    else:\n        return {index + 100}')
            driver = DRIVER.replace('HOST', HOSTS[platform][2]).replace('BODY', '\n'.join(body)).replace('PLATFORM', platform).replace('COUNT', str(len(cases)))
            entry = work / 'inventory.void'
            entry.write_text(driver)
            if args.evidence:
                (args.evidence / (platform + '-inventory.json')).write_bytes(producer.encoded(inventory))
                (args.evidence / (platform + '-void-native.txt')).write_bytes((work / 'fixtures/producer/contract/bin/void-native.txt').read_bytes())
            result = subprocess.run([str(compiler), 'run', '--no-cache', str(entry)], capture_output=True, text=True, timeout=120)
            print(result.stdout, end='', flush=True)
            if result.returncode:
                raise AssertionError((platform, result.returncode, result.stderr))


if __name__ == '__main__':
    main()
