"""Security-negative qualification checks; no production admission policy."""
import hashlib
import json
import os
from pathlib import Path
import stat

AUTHENTICATE = ('authenticate', 'expected a 2-of-3 signature envelope')
AMBIGUOUS = ('select', 'ambiguous stable version for this host')
UNSUPPORTED = ('select', 'no compatible stable release is available for this host')


def admitted_parent(parent, compiler, run, source_directory):
    # Resolve only the existing, caller-owned fixture parent. New roots are
    # appended afterwards and never canonicalized or created by this helper.
    physical = Path(parent).resolve(strict=True)
    before = physical.stat(follow_symlinks=False)
    assert stat.S_ISDIR(before.st_mode), 'Negative-test parent is not a directory'
    probe = Path(source_directory).resolve(strict=True) / 'negative-parent-admission.void'
    assert not os.path.lexists(probe), 'Parent admission probe must be fresh'
    probe.write_text('use io\nfunc main() -> i32:\n'
        '    var admitted = io.open_directory(' + json.dumps(str(physical), ensure_ascii=False) + ')\n'
        '    if admitted is Err(var failure):\n'
        '        print(failure.message)\n'
        '        return 1\n'
        '    print("VOIDUP negative parent admitted")\n'
        '    return 0\n', encoding='utf-8')
    result = run([compiler, 'run', '--no-cache', probe])
    assert result.stdout.splitlines() == [b'VOIDUP negative parent admitted'], result.stdout
    after = physical.stat(follow_symlinks=False)
    assert (before.st_dev, before.st_ino, before.st_mode) == (after.st_dev, after.st_ino, after.st_mode), 'Negative-test parent changed during admission'
    assert physical.resolve(strict=True) == physical, 'Negative-test parent spelling changed'
    return physical


def fresh_root(parent, name):
    assert name and Path(name).name == name and name not in ('.', '..'), 'Expected a single fresh root name'
    root = Path(parent) / name
    assert not os.path.lexists(root), f'Negative-test root is not fresh: {root}'
    return root


def refusal(result, expected, *, filesystem=False):
    assert result.returncode == 1, result.returncode
    assert result.stdout == b'', 'A refused command unexpectedly announced success'
    records = []
    for line in result.stderr.decode('utf-8', errors='strict').splitlines():
        if line.startswith('VOIDUP_DIRECTORY_DIAGNOSTIC '):
            assert filesystem, 'Filesystem diagnostic cannot satisfy a policy rejection'
            json.loads(line.removeprefix('VOIDUP_DIRECTORY_DIAGNOSTIC '))
        else:
            records.append(json.loads(line))
    assert len(records) == 1 and isinstance(records[0], dict), 'Expected one structured VOIDUP refusal'
    error = records[0]
    assert (error['operation'], error['message']) == expected, error
    assert error['code'] == 1 and not error['published'] and not error['durable'], error
    if not filesystem:
        assert error['kind'] == 'policy' and error['native_code'] == 0 and error['native_domain'] == '', error
    return error


def ordinary(path, directory=False):
    info = Path(path).stat(follow_symlinks=False)
    assert (stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode) and info.st_nlink == 1), f'Unexpected rejection-state object: {path}'


def rejected_state(root, *, catalog=None, raw_catalog=None, raw_signatures=None):
    root = Path(root)
    ordinary(root, True)
    assert {p.name for p in root.iterdir()} == {'toolchains', 'transactions', 'metadata', 'control.json', 'voidup-root.json', 'writer.lock'}, 'Unexpected rejected-root contents'
    for leaf in ('toolchains', 'transactions', 'metadata'):
        ordinary(root / leaf, True)
    assert not list((root / 'toolchains').iterdir()), 'Rejected catalog installed a toolchain'
    assert not list((root / 'transactions').iterdir()), 'Rejected catalog left a transaction'
    assert not os.path.lexists(root / 'bin'), 'Rejected catalog published bin proxies'
    for leaf in ('control.json', 'voidup-root.json', 'writer.lock'):
        ordinary(root / leaf)
    marker = json.loads((root / 'voidup-root.json').read_bytes())
    assert marker == {'schema': 1, 'application': 'VOIDUP-private-v1'}, marker
    control = json.loads((root / 'control.json').read_bytes())
    expected = dict(schema=1, generation=1, serial=0, catalog_digest='', last_time=0,
                    current='', previous='', revoked=[], installed=[])
    if catalog is None:
        assert not list((root / 'metadata').iterdir()), 'Unauthenticated catalog was retained'
    else:
        assert raw_catalog is not None and raw_signatures is not None
        digest = hashlib.sha256(raw_catalog).hexdigest()
        expected.update(generation=2, serial=catalog['serial'], catalog_digest=digest,
                        last_time=control['last_time'], revoked=catalog['revoked'])
        assert catalog['issued'] <= control['last_time'] <= catalog['expires'], control
        assert {p.name for p in (root / 'metadata').iterdir()} == {'c-' + digest}
        proof = root / 'metadata' / ('c-' + digest)
        ordinary(proof, True)
        assert {p.name for p in proof.iterdir()} == {'catalog.json', 'catalog.sig'}
        for name, content in [('catalog.json', raw_catalog), ('catalog.sig', raw_signatures)]:
            ordinary(proof / name)
            assert (proof / name).read_bytes() == content, name
    assert control == expected, control
    return control


def literal_var_parent(physical):
    physical = Path(physical)
    info = Path('/var').stat(follow_symlinks=False)
    assert stat.S_ISLNK(info.st_mode) and info.st_uid == 0, 'Expected the native system /var alias'
    assert os.readlink('/var') in ('private/var', '/private/var'), 'Unexpected system alias target'
    relative = physical.relative_to('/private/var')
    raw = Path('/var') / relative
    assert raw.resolve(strict=True) == physical
    return raw


def filesystem_refusal(result, root):
    error = refusal(result, ('io.create_directory_handle', 'io.create_directory_handle failed; native code 20'), filesystem=True)
    assert error['native_domain'] == 'filesystem' and error['native_code'] == 20 and error['kind'] == 'not_directory', error
    assert not os.path.lexists(root), 'Filesystem-refused root was published'
    return error
