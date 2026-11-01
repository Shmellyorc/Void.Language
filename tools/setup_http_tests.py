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
# Recreate imported files from the checked wheels, including on a cache hit.
# Removed files from an older fixture must not remain importable.
if (base / 'site-packages').exists():
    shutil.rmtree(base / 'site-packages')
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
    prefix = root / '.toolchains/http/test-certificates'
    manifest_path = prefix / 'VOID-HTTP-TEST-CERTIFICATES.json'
    identity = {
        'pins_sha256': hashlib.sha256(policy.read_bytes()).hexdigest(),
        'recipe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'sdk_sha256': hashlib.sha256((root / '.toolchains/llvm-23.1.4/void-sdk-identity.json').read_bytes()).hexdigest(),
        'platform': platform.system(), 'architecture': platform.machine().lower(),
        'crt': 'MultiThreaded',
        'VCToolsVersion': os.environ.get('VCToolsVersion', ''),
        'WindowsSDKVersion': os.environ.get('WindowsSDKVersion', ''),
    }
    if manifest_path.exists():
        cached = json.loads(manifest_path.read_text())
        if cached.get('identity') != identity or not cached.get('files'):
            raise SystemExit('Cached certificate fixture identity mismatch')
        for name, digest in cached['files'].items():
            path = prefix / name
            if not path.resolve().is_relative_to(prefix.resolve()) or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise SystemExit(f'Cached certificate fixture digest mismatch: {name}')
        if not (prefix / 'bin/openssl.exe').is_file():
            raise SystemExit('Cached certificate fixture executable missing')
        print('verified cached test-only certificate tool:', prefix / 'bin/openssl.exe')
        raise SystemExit(0)
    # AWS-LC's native x64 Windows assembler is a test-build prerequisite. Keep
    # it separate from production dependencies and never select an ambient copy.
    tools = root / '.toolchains/http/test-certificates-tools'
    tools.mkdir(parents=True, exist_ok=True)
    nasm_archive = tools / 'nasm-2.16.03-win64.zip'
    nasm_url = 'https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip'
    nasm_sha256 = '3ee4782247bcb874378d02f7eab4e294a84d3d15f3f6ee2de2f47a46aa7226e6'
    if not nasm_archive.exists():
        with urllib.request.urlopen(nasm_url, timeout=120) as response, nasm_archive.open('wb') as output:
            shutil.copyfileobj(response, output)
    if hashlib.sha256(nasm_archive.read_bytes()).hexdigest() != nasm_sha256:
        raise SystemExit('Certificate fixture NASM archive checksum mismatch')
    if (tools / 'nasm-2.16.03').exists():
        shutil.rmtree(tools / 'nasm-2.16.03')
    with zipfile.ZipFile(nasm_archive) as bundle:
        for item in bundle.infolist():
            if not (tools / item.filename).resolve().is_relative_to(tools.resolve()):
                raise SystemExit('Invalid NASM archive path')
        bundle.extractall(tools)  # Retains the upstream LICENSE too.
    nasm = tools / 'nasm-2.16.03/nasm.exe'
    nasm_version = subprocess.check_output([nasm, '-v'], text=True).strip()
    if nasm_version.split()[:3] != ['NASM', 'version', '2.16.03']:
        raise SystemExit('Certificate fixture NASM version mismatch')
    build = root / '.toolchains/http/test-certificates-build'
    source = build / 'source' / pin['source_dir']
    source.parent.mkdir(parents=True, exist_ok=True)
    if source.exists(): shutil.rmtree(source)
    with tarfile.open(archive) as bundle: bundle.extractall(source.parent, filter='data')
    cc = root / '.toolchains/llvm-23.1.4/bin/clang-cl.exe'
    report = {'scope': 'certificate fixture tool only; production TLS is Schannel',
              'pins_sha256': hashlib.sha256(policy.read_bytes()).hexdigest(),
              'identity': identity, 'commands': [],
              'assembler': {'version': nasm_version, 'url': nasm_url, 'sha256': nasm_sha256}}
    prefix.mkdir(parents=True, exist_ok=True)
    for command in [
        ['cmake', '-S', source, '-B', build / 'build', '-G', 'Ninja', f'-DCMAKE_C_COMPILER={cc}',
         f'-DCMAKE_CXX_COMPILER={cc}', f'-DCMAKE_INSTALL_PREFIX={prefix}', '-DCMAKE_BUILD_TYPE=Release',
         f'-DNASM_EXECUTABLE={nasm}', f'-DCMAKE_ASM_NASM_COMPILER={nasm}',
         '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW',
         '-DBUILD_SHARED_LIBS=OFF', '-DBUILD_TESTING=OFF', '-DBUILD_LIBSSL=ON',
         '-DDISABLE_GO=ON', '-DENABLE_SOURCE_MODIFICATION=OFF'],
        ['cmake', '--build', build / 'build', '--parallel', str(args.jobs)],
        ['cmake', '--install', build / 'build'],
    ]:
        result = subprocess.run(list(map(str, command)), check=False)
        report['commands'].append({'argv': list(map(str, command)), 'exit_code': result.returncode})
        (build / 'commands.json').write_text(json.dumps(report, indent=2)+'\n')
        if result.returncode: raise SystemExit('Certificate fixture build failed')
    report['files'] = {str(path.relative_to(prefix)).replace('\\', '/'): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in prefix.rglob('*') if path.is_file() and not path.is_symlink()}
    (prefix / 'VOID-HTTP-TEST-CERTIFICATES.json').write_text(json.dumps(report, indent=2)+'\n')
    print('verified test-only certificate tool:', prefix / 'bin/openssl.exe')
