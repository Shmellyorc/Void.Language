#!/usr/bin/env python3
"""Explicit bootstrap for pinned, pure-Python loopback HTTP/2 test fixtures."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile
import argparse
import os
import platform
import shutil
import subprocess
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--certificates', action='store_true', help='Build the separate pinned Windows certificate fixture tool')
parser.add_argument('--jobs', type=int, default=4)
args = parser.parse_args()

root = Path(__file__).resolve().parents[1]
base = root / ".toolchains/http/test-python"
base.mkdir(parents=True, exist_ok=True)
for pin in json.loads((root / "tests/net/http/python-versions.json").read_text()):
    archive = base / (pin["name"] + "-" + pin["version"] + ".whl")
    if not archive.exists():
        with urllib.request.urlopen(pin["url"], timeout=60) as response:
            archive.write_bytes(response.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit(f"HTTP test dependency digest mismatch: {archive.name}")
    with zipfile.ZipFile(archive) as wheel:
        for item in wheel.infolist():
            target = base / "site-packages" / item.filename
            if not target.resolve().is_relative_to((base / "site-packages").resolve()):
                raise SystemExit("Invalid test wheel path")
        wheel.extractall(base / "site-packages")
    print(f"verified test dependency: {pin['name']} {pin['version']}")

if args.certificates and platform.system() == 'Windows':
    # Test-only generator. Production still links Schannel, with no AWS-LC
    # archive in its native link closure. No test root enters an OS trust store.
    policy = root / 'third_party/http-versions.json'
    pin = next(p for p in json.loads(policy.read_text())['dependencies'] if p['name'] == 'aws-lc')
    archive = root / '.toolchains/http/downloads' / pin['archive']; archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        with urllib.request.urlopen(pin['url'], timeout=120) as response, archive.open('wb') as output:
            shutil.copyfileobj(response, output)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']:
        raise SystemExit('Certificate fixture source checksum mismatch')
    build = root / '.toolchains/http/test-certificates-build'
    source = build / 'source' / pin['source_dir']
    source.parent.mkdir(parents=True, exist_ok=True)
    if source.exists(): shutil.rmtree(source)
    with tarfile.open(archive) as bundle: bundle.extractall(source.parent, filter='data')
    prefix = root / '.toolchains/http/test-certificates'
    cc = root / '.toolchains/llvm-23.1.4/bin/clang-cl.exe'
    report = {'scope': 'certificate fixture tool only; production TLS is Schannel',
              'pins_sha256': hashlib.sha256(policy.read_bytes()).hexdigest(), 'commands': []}
    prefix.mkdir(parents=True, exist_ok=True)
    for command in [
        ['cmake', '-S', source, '-B', build / 'build', '-G', 'Ninja', f'-DCMAKE_C_COMPILER={cc}',
         f'-DCMAKE_CXX_COMPILER={cc}', f'-DCMAKE_INSTALL_PREFIX={prefix}', '-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW',
         '-DBUILD_SHARED_LIBS=OFF', '-DBUILD_TESTING=OFF', '-DBUILD_LIBSSL=ON',
         '-DDISABLE_GO=ON', '-DENABLE_SOURCE_MODIFICATION=OFF'],
        ['cmake', '--build', build / 'build', '--parallel', str(args.jobs)],
        ['cmake', '--install', build / 'build'],
    ]:
        result = subprocess.run(list(map(str, command)), check=False)
        report['commands'].append({'argv': list(map(str, command)), 'exit_code': result.returncode})
        if result.returncode: raise SystemExit('Certificate fixture build failed')
    report['files'] = {str(path.relative_to(prefix)).replace('\\', '/'): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in prefix.rglob('*') if path.is_file() and not path.is_symlink()}
    (prefix / 'VOID-HTTP-TEST-CERTIFICATES.json').write_text(json.dumps(report, indent=2)+'\n')
    print('verified test-only certificate tool:', prefix / 'bin/openssl.exe')
