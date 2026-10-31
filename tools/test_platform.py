"""Platform contracts for native acceptance fixtures."""
import os
import json
from pathlib import Path


def native_manifest(compiler):
    """Read the same v1/v2 link manifest used by the native CLI."""
    directory = Path(compiler).resolve().parent
    lines = (directory / 'void-native.txt').read_text(encoding='utf-8').splitlines()
    if not lines or lines[0] not in ('VOID native 23.1.4 v1', 'VOID native 23.1.4 v2'):
        raise ValueError('Unsupported native link manifest')
    return [line.replace('@VOID_NATIVE_DIR@', str(directory)) for line in lines]


def native_text(text):
    """Expected UTF-8 CRT text; never apply this to captured or wire bytes."""
    return text.replace('\n', os.linesep).encode('utf-8')


def small_stack():
    """Restrict the native child before exec, not the Python launcher's stack."""
    import resource
    before = resource.getrlimit(resource.RLIMIT_STACK)
    target = 1048576
    try:
        resource.setrlimit(resource.RLIMIT_STACK, (target, target))
    except (OSError, ValueError) as error:
        raise RuntimeError(f'Cannot enforce 1 MiB stack; inherited limits={before}') from error
    observed = resource.getrlimit(resource.RLIMIT_STACK)
    assert observed == (target, target), (before, observed)
    os.write(2, ('VOID_TEST_STACK ' + json.dumps({'inherited': before, 'observed': observed}) + '\n').encode('ascii'))


def report_stack(logs):
    """Retain the child's queried limits in passing CTest output too."""
    prefix = b'VOID_TEST_STACK '
    records = [json.loads(line[len(prefix):]) for line in bytes(logs).splitlines() if line.startswith(prefix)]
    assert len(records) == 1 and records[0]['observed'] == [1048576, 1048576], records
    print('VOID_TEST_STACK ' + json.dumps(records[0]))
