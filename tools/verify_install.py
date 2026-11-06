#!/usr/bin/env python3
"""Verify installed compiler, stdlib, Runtime/native resources and LSP externally.

Uses an automatically removed project outside the checkout. Does not modify the
installation. This is a source-install check, not clean-machine qualification.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
from ci.install_smoke import frames, unframe

HELLO = 'use console\n\nfunc main() -> i32:\n    print("Hello, world!")\n    return 0\n'


def verify(prefix):
    suffix = '.exe' if os.name == 'nt' else ''
    compiler = prefix / 'bin' / ('voidc' + suffix)
    server = prefix / 'bin' / ('voidls' + suffix)
    for name in ('bin/void-native.txt', 'bin/void-native-resources.txt',
                 'share/void/stdlib/console.void', 'share/void/licenses/VOID-MIT-LICENSE.txt'):
        if not (prefix / name).is_file():
            raise RuntimeError('Incomplete installation: missing ' + name)
    # Verify every installed archive/shared dependency before exercising linking.
    inventory = (prefix / 'bin/void-native-resources.txt').read_text(encoding='utf-8').splitlines()
    if not inventory or inventory[0] != 'VOID native resources 1' or len(inventory) < 3:
        raise RuntimeError('Invalid installed native resource inventory')
    for line in inventory[1:]:
        expected, name = line.split('\t', 1)
        resource = Path(name.replace('@VOID_NATIVE_DIR@', str(prefix / 'bin'))).resolve()
        if not name.startswith('@VOID_NATIVE_DIR@/../lib/void/') or not resource.is_relative_to(prefix / 'lib/void'):
            raise RuntimeError('Native resource does not belong to the installation: ' + name)
        with resource.open('rb') as stream:
            actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if actual != expected:
            raise RuntimeError('Installed resource checksum mismatch: ' + str(resource))
    environment = os.environ.copy()
    environment.pop('VOID_STDLIB_ROOT', None)
    environment['PATH'] = str(prefix / 'bin') + os.pathsep + environment.get('PATH', '')

    def execute(argv, *, data=None, stdout=None):
        result = subprocess.run(list(map(str, argv)), cwd=project, env=environment,
                                input=data, capture_output=True, timeout=120)
        if result.returncode or (stdout is not None and result.stdout != stdout):
            raise RuntimeError(f'{argv}: status {result.returncode}\n{result.stdout!r}\n{result.stderr!r}')
        return result.stdout

    with tempfile.TemporaryDirectory(prefix='VOID installed hello é ') as scratch:
        project = Path(scratch)
        entry = project / 'main.void'
        entry.write_text(HELLO, encoding='utf-8')
        print(execute([compiler, '--version']).decode(), end='')
        execute([compiler, 'check', entry], stdout=('Source checked successfully' + os.linesep).encode())
        for flags in ([], ['--no-opt', '--no-cache']):
            execute([compiler, 'run', project, *flags], stdout=b'Hello, world!\n')
        output = project / ('hello' + suffix)
        execute([compiler, 'build', entry, '-o', output])
        execute([output], stdout=b'Hello, world!\n')
        wire = frames([
            {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'capabilities': {}}},
            {'jsonrpc': '2.0', 'method': 'initialized', 'params': {}},
            {'jsonrpc': '2.0', 'id': 2, 'method': 'shutdown'},
            {'jsonrpc': '2.0', 'method': 'exit'},
        ])
        replies = unframe(execute([server, '--entry', entry], data=wire))
        by_id = {item.get('id'): item for item in replies if 'id' in item}
        initialized = by_id.get(1, {}).get('result', {})
        if (initialized.get('serverInfo', {}).get('name') != 'VOID'
                or initialized.get('capabilities', {}).get('experimental', {}).get('voidToolingProtocol') != 1
                or by_id.get(2, {}).get('result', 'missing') is not None):
            raise RuntimeError('Installed language server did not complete initialize/shutdown')
    print('PASS: installed stdlib check, optimized/Debug run, native build/run, resource hashes and LSP handshake')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    args = parser.parse_args()
    verify(args.prefix.expanduser().resolve())
