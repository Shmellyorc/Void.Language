"""Offline engineering producer, never part of VOIDUP's installation path.

Installs actual compiler build outputs, materializes an LLVM subset, inventories
every byte, then signs a private catalog using separately held test keys.
"""
from pathlib import Path
import argparse, hashlib, json, os, platform, re, shutil, subprocess, sys, time, zipfile
from cryptography.hazmat.primitives.serialization import load_pem_private_key, Encoding, PublicFormat
from engineering_storage import evidence_directory

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

def write_member(archive,path,name,epoch,executable=False,directory=False):
    info=zipfile.ZipInfo(name,time.gmtime(epoch)[:6]);info.create_system=3
    info.external_attr=((0o40755 if directory else 0o100755 if executable else 0o100644)<<16)|(0x10 if directory else 0)
    # Native curated ZIP directories have zero sizes/CRC. Deflating an empty
    # directory produces two compressed bytes and correctly fails admission.
    archive.writestr(info,b'' if directory else path.read_bytes(),compress_type=zipfile.ZIP_STORED if directory else zipfile.ZIP_DEFLATED,compresslevel=1)

def sign(catalog_bytes, key_dir, signers=(1, 2)):
    signatures = []
    public_keys = re.findall(r'"([0-9a-f]{64})"', (ROOT/'apps/voidup/trust_keys.void').read_text())
    for number in signers:
        key = load_pem_private_key((key_dir / f'offline-{number}.pem').read_bytes(), None)
        if key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex() != public_keys[number-1]:
            raise RuntimeError('offline key does not match the application test trust root')
        signatures.append(dict(key_id=f'test-{number}', signature=key.sign(DOMAIN + catalog_bytes).hex()))
    return encoded(dict(schema=1, signatures=signatures))

def produce(build, release_id, output, sdk, host=HOST, configuration=None):
    expected={('Linux','x86_64'):HOST,('Windows','amd64'):'windows-x86_64',('Windows','x86_64'):'windows-x86_64',('Darwin','arm64'):'macos-aarch64-14.0'}.get((platform.system(),platform.machine().lower()))
    if host!=expected:raise ValueError('Toolchain host differs from the native producer')
    prefix = output / 'producer' / release_id
    if prefix.exists():
        raise RuntimeError(f'refusing to replace producer directory: {prefix}')
    prefix.mkdir(parents=True, mode=0o700)
    for component in ('compiler', 'tooling'):
        subprocess.run(['cmake', '--install', str(build), '--prefix', str(prefix),
                        '--component', component, *(['--config',configuration] if configuration else [])], check=True, stdout=subprocess.DEVNULL)
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
    suffix='.exe' if host=='windows-x86_64' else ''
    version = subprocess.check_output([str(prefix / 'bin'/('voidc'+suffix)), '--version'], text=True)
    identity = re.search(r'Build identity: ([0-9a-f]{64})', version).group(1)
    (prefix / 'sdk/bin').mkdir(parents=True)
    tools=[('clang.exe','clang.exe'),('lld-link.exe','lld-link.exe')] if suffix else [('clang','clang'),('ld64.lld','ld64.lld')] if sys.platform=='darwin' else [('clang-23','clang'),('lld','ld.lld')]
    for original, name in tools:
        shutil.copy2(sdk / 'bin' / original, prefix / 'sdk/bin' / name)
    shutil.copytree(sdk / 'lib/clang/23', prefix / 'sdk/lib/clang/23', symlinks=False)
    native = prefix / 'bin/void-native.txt'
    lines = native.read_text().splitlines()
    assert lines[0] == 'VOID native 23.1.4 v2'
    lines[1:3] = ['../sdk/bin/clang.exe', '../sdk/bin/lld-link.exe'] if suffix else ['../sdk/bin/clang', '../sdk/bin/ld.lld']
    if suffix:lines.append('-fuse-ld=lld')
    native_audit={}
    if sys.platform=='darwin':
        from voidup_macos import relocate,relocate_manifest,driver_wrapper
        lines=relocate_manifest(lines)
        wrapper=prefix/'sdk/bin/void-clang';wrapper.write_text(driver_wrapper());wrapper.chmod(0o755)
        origins={prefix/'bin'/name:prefix/'bin'/name for name in ['voidc','voidls']}
        origins.update({prefix/'sdk/bin'/name:sdk/'bin'/original for original,name in tools})
        native_audit=relocate(prefix,origins,[prefix,sdk,build,ROOT/'.toolchains'])
    native.write_text('\n'.join(lines) + '\n')
    if sys.platform!='darwin':
        from voidup_dependencies import audit
        images=[prefix/'bin'/('voidc'+suffix),prefix/'bin'/('voidls'+suffix)]+[prefix/'sdk/bin'/name for _,name in tools]
        if not suffix:images+=[p for p in (prefix/'lib/void').glob('*.so*') if p.is_file()]
        native_audit=audit(prefix,sdk,images)
    (prefix / 'toolchain.json').write_bytes(encoded(dict(schema=1, release_id=release_id,
        host=host, build_identity=identity, llvm_version='23.1.4')))
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
            executable = path.is_file() and (bool(path.stat().st_mode & 0o111) or bool(suffix and path.suffix.lower()=='.exe'))
            members.append(dict(name=name, size=size, sha256='' if path.is_dir() else sha(path),
                                executable=executable, directory=path.is_dir()))
            expanded += size
            epoch=int(os.environ.get('SOURCE_DATE_EPOCH','315532800'))
            if not 315532800<=epoch<=4354819199:raise ValueError('SOURCE_DATE_EPOCH must fit the ZIP timestamp range')
            write_member(archive,path,name,epoch,executable,path.is_dir())
    inventory = output / 'www/inventories' / (release_id + '.json')
    inventory.parent.mkdir(parents=True, exist_ok=True)
    inventory.write_bytes(encoded(dict(schema=1, release_id=release_id, host=host,
        build_identity=identity, expanded_bytes=expanded, members=members)))
    (output/(release_id+'.package.json')).write_bytes(encoded(dict(schema=1,host=host,
        build_identity=identity,archive_bytes=artifact.stat().st_size,expanded_bytes=expanded,
        native_dependency_audit=native_audit,source_date_epoch=epoch)))
    return dict(id=release_id, host=host, build_identity=identity,
        artifact_url='packages/' + artifact.name, artifact_bytes=artifact.stat().st_size,
        artifact_sha256=sha(artifact), inventory_url='inventories/' + inventory.name,
        inventory_bytes=inventory.stat().st_size, inventory_sha256=sha(inventory))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-a', required=True, type=Path)
    parser.add_argument('--build-b', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=evidence_directory('voidup-fixtures'))
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
    (evidence_directory('voidup-qualification') / 'real-packages.json').write_text(json.dumps(record, indent=2)+'\n')
    print(json.dumps(record, indent=2))

if __name__ == '__main__':
    main()
