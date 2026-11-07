"""Offline engineering producer, never part of VOIDUP's installation path.

Installs actual compiler build outputs, materializes an LLVM subset, inventories
every byte, then signs a private catalog using separately held test keys.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, time, zipfile
from cryptography.hazmat.primitives.serialization import load_pem_private_key, Encoding, PublicFormat

ROOT = Path(__file__).resolve().parents[1]
DOMAIN = b'VOIDUP CATALOG v1\n'
HOST = 'linux-x86_64-ubuntu24.04'

def encoded(value):
    return json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode()

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def sign(catalog_bytes, key_dir, signers=(1, 2)):
    signatures = []
    public_keys = re.findall(r'"([0-9a-f]{64})"', (ROOT/'apps/voidup/trust_keys.void').read_text())
    for number in signers:
        key = load_pem_private_key((key_dir / f'offline-{number}.pem').read_bytes(), None)
        if key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex() != public_keys[number-1]:
            raise RuntimeError('offline key does not match the application test trust root')
        signatures.append(dict(key_id=f'test-{number}', signature=key.sign(DOMAIN + catalog_bytes).hex()))
    return encoded(dict(schema=1, signatures=signatures))

def produce(build, release_id, output, sdk):
    prefix = output / 'producer' / release_id
    if prefix.exists():
        raise RuntimeError(f'refusing to replace producer directory: {prefix}')
    prefix.mkdir(parents=True, mode=0o700)
    for component in ('compiler', 'tooling'):
        subprocess.run(['cmake', '--install', str(build), '--prefix', str(prefix),
                        '--component', component], check=True, stdout=subprocess.DEVNULL)
    # Materialize shared library aliases into distinct regular files: the runtime
    # intentionally rejects symlinks and multiply linked private files.
    for path in list(prefix.rglob('*')):
        if path.is_symlink():
            if path.is_dir():
                resolved = path.resolve()
                path.unlink()
                shutil.copytree(resolved, path, symlinks=False)
                continue
            data = path.read_bytes()
            path.unlink()
            path.write_bytes(data)
    version = subprocess.check_output([str(prefix / 'bin/voidc'), '--version'], text=True)
    identity = re.search(r'Build identity: ([0-9a-f]{64})', version).group(1)
    (prefix / 'sdk/bin').mkdir(parents=True)
    for original, name in [('clang-23', 'clang'), ('lld', 'ld.lld')]:
        shutil.copy2(sdk / 'bin' / original, prefix / 'sdk/bin' / name)
    shutil.copytree(sdk / 'lib/clang/23', prefix / 'sdk/lib/clang/23', symlinks=False)
    native = prefix / 'bin/void-native.txt'
    lines = native.read_text().splitlines()
    assert lines[0] == 'VOID native 23.1.4 v2'
    lines[1:3] = ['../sdk/bin/clang', '../sdk/bin/ld.lld']
    native.write_text('\n'.join(lines) + '\n')
    (prefix / 'toolchain.json').write_bytes(encoded(dict(schema=1, release_id=release_id,
        host=HOST, build_identity=identity, llvm_version='23.1.4')))
    paths = sorted(prefix.rglob('*'), key=lambda p: (not p.is_dir(), len(p.relative_to(prefix).parts), p.as_posix()))
    members = []
    expanded = 0
    artifact = output / 'www/packages' / (release_id + '.zip')
    artifact.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(artifact, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as archive:
        for path in paths:
            assert not path.is_symlink() and (path.is_file() or path.is_dir())
            name = path.relative_to(prefix).as_posix() + ('/' if path.is_dir() else '')
            name.encode('ascii')
            size = 0 if path.is_dir() else path.stat().st_size
            executable = path.is_file() and bool(path.stat().st_mode & 0o111)
            members.append(dict(name=name, size=size, sha256='' if path.is_dir() else sha(path),
                                executable=executable, directory=path.is_dir()))
            expanded += size
            archive.write(path, name)
    inventory = output / 'www/inventories' / (release_id + '.json')
    inventory.parent.mkdir(parents=True, exist_ok=True)
    inventory.write_bytes(encoded(dict(schema=1, release_id=release_id, host=HOST,
        build_identity=identity, expanded_bytes=expanded, members=members)))
    return dict(id=release_id, host=HOST, build_identity=identity,
        artifact_url='packages/' + artifact.name, artifact_bytes=artifact.stat().st_size,
        artifact_sha256=sha(artifact), inventory_url='inventories/' + inventory.name,
        inventory_bytes=inventory.stat().st_size, inventory_sha256=sha(inventory))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-a', required=True, type=Path)
    parser.add_argument('--build-b', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/voidup-fixtures')
    parser.add_argument('--sdk', type=Path, default=ROOT / '.toolchains/llvm-23.1.4')
    options = parser.parse_args()
    output = options.output.resolve()
    releases = [produce(build.resolve(), release_id, output, options.sdk.resolve())
                for build, release_id in [(options.build_a, 'engineering-a'), (options.build_b, 'engineering-b')]]
    assert releases[0]['build_identity'] != releases[1]['build_identity'], 'real compiler builds must differ'
    now = int(time.time())
    catalog = dict(schema=1, domain='voidup-catalog-v1', serial=10, issued=now-60,
                   expires=now+86400, min_manager=1, llvm_version='23.1.4', revoked=[], releases=releases)
    raw = encoded(catalog)
    (output / 'www/catalog.json').write_bytes(raw)
    (output / 'www/catalog.json.sig').write_bytes(sign(raw, output / 'keys'))
    record = dict(schema=1, created=now, builds=[str(options.build_a.resolve()), str(options.build_b.resolve())],
                  catalog_sha256=hashlib.sha256(raw).hexdigest(), releases=releases,
                  sdk_files=['bin/clang-23', 'bin/lld', 'lib/clang/23'],
                  system_prerequisite='Ubuntu 24.04 x86_64 build-essential (C/C++ headers, CRT and system libraries)')
    (ROOT / 'docs/voidup/evidence/phase_b/real-packages.json').write_text(json.dumps(record, indent=2)+'\n')
    print(json.dumps(record, indent=2))

if __name__ == '__main__':
    main()
