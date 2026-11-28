"""Exercise the controlled signed target-pack channel over native HTTPS."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

from support import ROOT, Service
from package_voidup_fixtures import DOMAIN, encoded, produce
from voidup_release import catalog, package_target


def run(*args, failure=None, env=None):
    command = list(map(str, args))
    result = subprocess.run(command, text=True, capture_output=True, timeout=900, env=env)
    if failure:
        assert result.returncode != 0 and failure in result.stderr, (command, result.stdout, result.stderr)
    else:
        assert result.returncode == 0, (command, result.stdout, result.stderr)
    print(" ".join(command), flush=True)
    return result


def qualify(build, sdk, inputs, work, destination):
    fixtures = work / 'fixtures'
    compiler = build / 'voidc'
    release_id = 'pack-engineering-toolchain'
    release = produce(build, release_id, fixtures, sdk)
    release['selection'] = {'version': '0.1.0', 'channel': 'stable'}
    release_path = fixtures / 'toolchain.json'
    release_path.write_bytes(encoded(release))
    records = []
    for version in ('1.0.0', '1.1.0'):
        source = work / f'pack-{version}'
        shutil.copytree(inputs, source)
        manifest = source / 'target-pack.txt'
        rows = manifest.read_bytes().splitlines(keepends=True)
        target = rows[3].decode().strip().split()[1]
        rows[1] = f'id {target}\n'.encode()
        rows[2] = f'version {version}\n'.encode()
        notice = 'licenses/providers/void/NOTICE.txt'
        notice_bytes = b'Controlled target-pack qualification notice.\n'
        notice_file = source / notice
        notice_file.parent.mkdir(parents=True)
        notice_file.write_bytes(notice_bytes)
        rows.append(f'{hashlib.sha256(notice_bytes).hexdigest()}\t{notice}\n'.encode())
        manifest.write_bytes(b''.join(rows))
        package_target(argparse.Namespace(pack=source, compiler=compiler, output=fixtures))
        records.append(next(fixtures.glob(f'*-{version}.target-pack.json')))

    # Each run enrolls fresh local test keys. The production roots and endpoint
    # are outside this harness, and no private keys are written to disk.
    keys = [Ed25519PrivateKey.generate() for _ in range(3)]

    def sign(raw, signers):
        return encoded({'schema': 1, 'signatures': [
            {'key_id': f'fixture-{index}', 'signature': keys[index - 1].sign(DOMAIN + raw).hex()}
            for index in signers]})

    root = work / 'manager'
    bootstrap = work / 'bootstrap'
    with Service(fixtures, signer=sign) as server:
        config = work / 'test-roots.json'
        config.write_bytes(encoded(dict(schema=1, trust='engineering', catalog=server.url,
            approval='isolated HTTPS target-pack qualification',
            keys=[dict(id=f'fixture-{index}', public=key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex())
                  for index, key in enumerate(keys, 1)])))
        run(sys.executable, ROOT / 'tools/build_voidup.py', '--compiler', compiler, '--prefix', bootstrap,
            '--trust-config', config, '--licenses', fixtures / 'producer' / release_id / 'share/void/licenses')
        manager = bootstrap / 'bin/voidup'
        base = [manager, '--root', root, '--catalog', server.url, '--ca-file', server.ca]
        local = [manager, '--root', root]
        run(*local, 'target', 'list', '--available', failure='a default toolchain must be installed')

        def serve(serial, descriptors, revoked=(), signers=(1, 2)):
            path = work / f'catalog-{serial}.json'
            catalog(argparse.Namespace(public_config=config, previous=None, serial=serial, revoke=list(revoked),
                release=[release_path], target_pack=descriptors, valid_days=1, output=path))
            document = json.loads(path.read_bytes())
            server.catalog(document, signers)
            return document

        first = serve(1, records[:1])
        run(*base, 'install', release_id, '--activate')
        run(*base, 'target', 'update', target, failure='target pack is not installed')
        assert f'{target} 1.0.0' in run(*base, 'target', 'list', '--available').stdout
        server.catalog(first, (1,))
        run(*base, 'target', 'install', target, failure='2-of-3')
        server.catalog(first)
        run(*base, 'target', 'install', target)
        assert f'{target} 1.0.0' in run(*local, 'target', 'verify', target).stdout
        run(*base, 'target', 'install', target)
        assert run(*local, 'target', 'list', env=os.environ | {
            'VOIDUP_TARGET_PACKS': 'forged', 'VOID_TARGET_PACK_ROOT': str(work / 'forged'),
        }).stdout.splitlines() == [target]

        invalid = copy.deepcopy(first)
        invalid['serial'] = 2
        invalid['target_packs']['releases'][0]['compiler'] = '99.0.0'
        server.catalog(invalid)
        run(*base, 'target', 'update', target, failure='no compatible authenticated target pack')
        run(*local, 'target', 'verify', target)
        server.catalog(first)
        run(*base, 'target', 'update', target, failure='serial replay')

        second = serve(3, records)
        latest = second['target_packs']['releases'][1]['payload']
        artifact_path = '/' + latest['artifact_url']
        artifact = (fixtures / 'www' / latest['artifact_url']).read_bytes()
        server.overrides[artifact_path] = (200, bytes([artifact[0] ^ 1]) + artifact[1:], {})
        run(*base, 'target', 'update', target, failure='package SHA-256 mismatch')
        del server.overrides[artifact_path]
        assert f'{target} 1.0.0' in run(*local, 'target', 'verify', target).stdout

        fault = work / 'fsync-fault.so'
        run('cc', '-shared', '-fPIC', ROOT / 'tests/voidup/fsync_fault.c', '-ldl', '-o', fault)
        run(*base, 'target', 'update', target, failure='durability is uncertain',
            env=os.environ | {'LD_PRELOAD': str(fault), 'VOIDUP_TEST_FAIL_SYNC': str(root / 'official-target-packs')})
        assert f'{target} 1.0.0' in run(*local, 'target', 'verify', target).stdout

        # Recover the fully published slot without a second archive request.
        count = server.requests.count(artifact_path)
        run(*base, 'target', 'update', target)
        assert count == server.requests.count(artifact_path)
        assert f'{target} 1.1.0' in run(*local, 'target', 'verify', target).stdout

        selected = root / 'official-target-packs' / ('p-' + latest['inventory_sha256']) / 'target-pack.txt'
        original = selected.read_bytes()
        selected.write_bytes(original.replace(b'version 1.1.0', b'version 9.9.9'))
        run(*local, 'target', 'verify', target, failure='authenticated digest')
        selected.write_bytes(original)

    # The listener is stopped and the default engineering URL is unavailable.
    project = work / 'application'
    launch = [*local, 'run', 'default', 'voidc', '--']
    run(*launch, 'new', 'app', project)
    (project / 'src/app.void').write_text('func main() -> i32:\n    print(37)\n    return 0\n')
    run(*launch, 'publish', '--project', project, '--target', target, '-o', destination)
    run(*launch, 'publish', '--verify', destination)
    assert (destination / notice).read_bytes() == notice_bytes
    with Service(fixtures, signer=sign) as revocation_server:
        base = [manager, '--root', root, '--catalog', revocation_server.url, '--ca-file', revocation_server.ca]
        second['serial'] = 4
        second['revoked'] = [latest['id']]
        revocation_server.catalog(second)
        run(*base, 'target', 'list', '--available')
        run(*local, 'target', 'verify', target, failure='No signed official pack')
        run(*base, 'target', 'update', target, failure='would downgrade')
        run(*local, 'target', 'remove', target)
        assert run(*local, 'target', 'list').stdout == ''
    print(f'Controlled catalog, retry recovery and offline {target} publication passed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('sdk', type=Path)
    parser.add_argument('pack', type=Path)
    parser.add_argument('distribution', type=Path)
    parser.add_argument('--scratch', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='void-target-acquisition-', dir=args.scratch) as scratch:
        qualify(args.build.resolve(), args.sdk.resolve(), args.pack.resolve(),
                Path(scratch).resolve(), args.distribution.resolve())


if __name__ == '__main__':
    main()
