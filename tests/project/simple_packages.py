"""G61 starter, source package and declarative native workflow with independent results."""
from hashlib import sha256
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import tomllib

compiler, git, readobj = (Path(arg).resolve() for arg in sys.argv[1:])
windows = sys.platform == 'win32'
env = dict(os.environ)
env.pop('VOID_NATIVE_CATALOG', None)
env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1',
           GIT_AUTHOR_NAME='G61 fixture', GIT_AUTHOR_EMAIL='fixture@example.invalid',
           GIT_COMMITTER_NAME='G61 fixture', GIT_COMMITTER_EMAIL='fixture@example.invalid')


def run(*args, cwd, status=0, contains=None, exact=None):
    p = subprocess.run(list(map(str, args)), cwd=cwd, env=env, capture_output=True, timeout=90)
    out = p.stdout.decode('utf-8', errors='replace').replace('\r\n', '\n')
    err = p.stderr.decode('utf-8', errors='replace')
    assert p.returncode == status and (contains is None or contains in out + err), (args, p.returncode, out, err)
    assert exact is None or out == exact, (args, out, err)
    assert 'AddressSanitizer' not in err and 'runtime error:' not in err, (args, err)
    return out


def put(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content.encode('utf-8'))


def pin(repository):
    run(git, 'add', '.', cwd=repository)
    run(git, 'commit', '-m', 'test fixture only', cwd=repository)
    return run(git, 'rev-parse', 'HEAD', cwd=repository).strip()


with tempfile.TemporaryDirectory(prefix='void-g61-') as scratch:
    root = Path(scratch)
    if windows:
        env['LOCALAPPDATA'] = str(root/'user-data')
        store = root/'user-data/VOID/packages'
    elif sys.platform == 'darwin':
        env['HOME'] = str(root/'user-home')
        store = root/'user-home/Library/Application Support/VOID/packages'
    else:
        env['XDG_DATA_HOME'] = str(root/'user-data')
        store = root/'user-data/void/packages'
    cli = lambda *args, **kw: run(compiler, *args, **kw)
    listing = cli('new', 'list', cwd=root, contains='Application')
    assert cli('new', cwd=root) == listing
    new_help = cli('new', '--help', cwd=root, contains='voidc new [list]')
    assert 'voidc new install' not in new_help and 'voidc new uninstall' not in new_help
    package_help = cli('package', '--help', cwd=root, contains='voidc package install ALIAS')
    assert 'voidc package uninstall ALIAS' in package_help
    assert not list(root.iterdir())
    cli('new', 'app', 'consumer', cwd=root)
    cli('new', 'lib', 'source', '--package-name', 'arithmetic',
        '--package-id', 'org.example.g61', cwd=root)
    cli('new', '--', 'lib', cwd=root)
    assert tomllib.loads((root/'lib/void.project.toml').read_text())['project']['kind'] == 'application'
    cli('new', 'app', 'consumer', cwd=root, status=1)
    shell = root/'existing'
    put(shell/'main.void', 'func main():\n    print("standalone")\n')
    cli('run', 'main.void', cwd=shell, exact='standalone\n')
    cli('init', cwd=shell)
    cli('run', '.', cwd=shell, exact='standalone\n')

    source = root/'source'
    project = root/'consumer'
    cli('package', 'list', cwd=root)
    assert not store.exists()
    entry = tomllib.loads((source/'void.project.toml').read_text())['package']['entry']
    put(source/entry, 'public func answer() -> i32:\n    return 17\n')
    cli('new', 'install', 'local', '--dir', source, cwd=root,
        status=2, contains='Expected one project destination.')
    assert not store.exists() and not (root/'install').exists()
    # Independent processes race initialization and the same installation.
    command = [compiler, 'package', 'install', 'local', '--dir', source]
    first = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    second = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    results = [process.communicate(timeout=90) for process in (first, second)]
    assert (first.returncode, second.returncode) == (0, 0), results
    assert sum(b'Installed user package local' in out for out, _ in results) == 1, results
    registration = store/'local'
    cached = registration/'libs/local'
    assert (cached/'void.project.toml').is_file() and (registration/'void.lock').is_file()
    cli('package', 'list', cwd=root, contains='Installed: local')
    cli('package', 'install', 'local', '--dir', source, cwd=root, contains='Verified existing')
    cli('package', 'install', 'local', '--dir', root/'lib', cwd=root,
        status=1, contains='different origin or pin')
    cli('remove', 'local', '--project', registration, cwd=project, status=1, contains='user package store')
    cli('resolve', '--project', registration, '--refresh', cwd=project, status=1, contains='user package store')
    cli('add', 'bypass', '--dir', cached, cwd=project, status=1, contains='user package store')
    cli('build', '.', '-o', cached/'unauthorized', cwd=project, status=1, contains='user package store')
    assert not (cached/'unauthorized').exists()
    put(project/'src/app.void', 'use pkg/local\nfunc main():\n    print(local.answer())\n')
    cli('add', 'local', cwd=project)
    cli('run', '.', cwd=project, exact='17\n')
    project_lock = (project/'void.lock').read_bytes()
    saved = (cached/entry).read_bytes()
    put(cached/entry, 'public func answer() -> i32:\n    return 99\n')
    cli('add', 'local', cwd=project, status=1, contains='Stale or tampered void.lock')
    cli('package', 'list', cwd=root, status=1, contains='Stale or tampered void.lock')
    (cached/entry).write_bytes(saved)
    receipt = registration/'.void-registration'
    saved_receipt = receipt.read_bytes()
    receipt.write_bytes(saved_receipt+b'tampered')
    cli('add', 'local', cwd=project, status=1, contains='registration identity mismatch')
    receipt.write_bytes(saved_receipt)
    cli('package', 'install', 'invalid', '--dir', root/'existing', cwd=root,
        status=1, contains='Source must be a VOID library')
    assert not (store/'invalid').exists() and not list((store/'.void').glob('registration-*'))
    cli('new', 'uninstall', 'local', cwd=root, status=2, contains='Expected one project destination.')
    assert registration.is_dir() and not (root/'uninstall').exists()
    cli('package', 'uninstall', 'local', cwd=root)
    assert not registration.exists()
    assert (project/'void.lock').read_bytes() == project_lock
    cli('run', '.', cwd=project, exact='17\n')
    cli('package', 'uninstall', 'local', cwd=root, status=1, contains='No user package')
    cli('remove', 'local', cwd=project)
    assert not (project/'libs/local').exists()
    cli('package', 'install', 'local', '--dir', source, cwd=root)
    cli('add', 'local', cwd=project)
    cli('run', '.', cwd=project, exact='17\n')
    cli('remove', 'local', cwd=project)
    cli('package', 'uninstall', 'local', cwd=root)
    put(project/'src/app.void', 'func main():\n    print("no dependencies")\n')
    if sys.platform.startswith('linux'):
        original_home = env.get('HOME')
        env.update(HOME=str(root/'fallback-home'), XDG_DATA_HOME='relative-data')
        cli('package', 'install', 'fallback', '--dir', source, cwd=root)
        fallback = root/'fallback-home/.local/share/void/packages/fallback'
        assert fallback.is_dir() and not (root/'relative-data').exists()
        cli('package', 'uninstall', 'fallback', cwd=root)
        env['XDG_DATA_HOME'] = str(root/'user-data')
        if original_home is None:
            env.pop('HOME')
        else:
            env['HOME'] = original_home

    git_repo = root/'fixture-repo'
    git_repo.mkdir()
    cli('new', 'lib', 'nested', cwd=git_repo)
    run(git, 'init', '-q', cwd=git_repo)
    revision = pin(git_repo)
    cli('package', 'install', 'fetched', '--git', 'file:fixture-repo',
        '--rev', revision, '--subdir', 'nested', cwd=root)
    registration = store/'fetched'
    assert (registration/'libs/fetched/.void-package').is_file()
    assert not (registration/'libs/nested').exists()
    unavailable = root/'unavailable-fixture'
    git_repo.rename(unavailable)
    cli('package', 'install', 'fetched', '--git', 'file:fixture-repo',
        '--rev', revision, '--subdir', 'nested', cwd=root, contains='Verified existing')
    cli('add', 'fetched', cwd=project)
    lock = tomllib.loads((project/'void.lock').read_text())
    assert lock['package'][0]['rev'] == revision and lock['package'][0]['subdir'] == 'nested'
    assert lock['package'][0]['git'] == 'file:fixture-repo'
    cli('tidy', '--check', cwd=project)
    cli('package', 'uninstall', 'fetched', cwd=root)
    cli('resolve', '--project', project, '--locked', cwd=root)
    cli('remove', 'fetched', cwd=project)
    assert not registration.exists() and (unavailable/'nested').exists()
    unavailable.rename(git_repo)
    print('Automatic storage, concurrent install, exact lock reuse, independent uninstall, tamper and rollback passed', flush=True)

    platform = 'windows' if windows else 'linux'
    target = 'x86_64-pc-windows-msvc msvc-x64' if windows else 'x86_64-unknown-linux-gnu sysv-x64'
    put(source/'native/void.build.toml', 'format = 1\nversion = "1.0.0"\n')
    put(source/'native/provider.h', 'int g61_answer(int input);\n')
    put(source/'native/provider.c', '#include "provider.h"\nint g61_answer(int input) {return input * 3;}\n')
    put(source/'native/NOTICE.txt', 'Independent G61 test fixture. VOID project license applies.\n')
    descriptor = source/f'native/provider-{platform}.descriptor'
    put(descriptor, f'void-c-primitive 4 3\nprovider org.example.g61\ntarget {target}\n'
        f'source-file /src/ffi.void\nthunk {"1"*64}\nfixture {"2"*64}\n'
        f'effects direct-sync nonblocking no-callbacks\ncall c-default\n'
        f'aliases c_int:i32 c_long:{"i32" if windows else "i64"} c_size:u64 c_bool:bool c_char:i8\n'
        'bind answer g61_answer c_int c_int\n')
    cli('native-bind', descriptor, 'provider.h', '-o', source/'src/ffi.void', cwd=root)
    library = tomllib.loads((source/'void.project.toml').read_text())
    put(source/library['package']['entry'], 'use "./ffi.void" as ffi\npublic func answer(n: i32) -> i32:\n    return ffi.answer(n)\n')
    put(source/'prepare.py', f'from pathlib import Path\nPath({str(root/"script-ran")!r}).touch()\n')
    project = root/'consumer'
    put(project/'src/app.void', 'use pkg/cdemo as demo\nfunc main():\n    print(demo.answer(14))\n')
    cli('add', 'cdemo', '--dir', source, cwd=project)
    assert not (root/'script-ran').exists()
    catalog = project/'libs/cdemo/.void/native'
    before = {p.name: (sha256(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns) for p in catalog.iterdir()}
    cli('add', 'cdemo', '--dir', source, cwd=project, contains='unchanged')
    assert before == {p.name: (sha256(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns) for p in catalog.iterdir()}
    cli('run', '.', cwd=project, status=2, contains='--allow-native')
    cli('build', '.', cwd=project, status=2, contains='--allow-native')
    cli('tidy', '--check', cwd=project)
    for profile in ('debug', 'release'):
        cli('run', '.', '--profile', profile, '--allow-native', cwd=project, exact='42\n')
        executable = root/profile/('app.exe' if windows else 'app')
        cli('build', '.', '--profile', profile, '--allow-native', '-o', executable, cwd=project)
        run(executable, cwd=root, exact='42\n')
        if windows:
            run(readobj, '--file-headers', executable, cwd=root, contains='SizeOfStackReserve: 1048576')
    for path, message in (
        (catalog/descriptor.name, 'artifact identity mismatch'),
        (next(catalog.glob('*.dll' if windows else '*.so')), 'artifact identity mismatch'),
        (project/'libs/cdemo/native/provider.c', 'Stale or tampered void.lock'),
    ):
        original = path.read_bytes()
        path.write_bytes(bytes([original[0] ^ 1]) + original[1:])
        cli('run', '.', '--allow-native', cwd=project, status=1, contains=message)
        path.write_bytes(original)
    cli('run', '.', '--allow-native', cwd=project, exact='42\n')
    cli('remove', 'cdemo', cwd=project)
    assert 'native-requirements' not in tomllib.loads((project/'void.project.toml').read_text())
    cli('check', '.', cwd=project, status=1, contains='pkg/cdemo')
    shutil.copytree(source, git_repo/'with-native')
    revision = pin(git_repo)
    cli('package', 'install', 'gitdemo', '--git', 'file:fixture-repo',
        '--rev', revision, '--subdir', 'with-native', cwd=root)
    put(project/'src/app.void', 'use pkg/gitdemo as demo\nfunc main():\n    print(demo.answer(14))\n')
    registered_catalog = store/'gitdemo/libs/gitdemo/.void/native'
    prepared = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in registered_catalog.iterdir()}
    cli('add', 'gitdemo', cwd=project)
    materialized = project/'libs/gitdemo/.void/native'
    assert {p.name:p.read_bytes() for p in materialized.iterdir()} == {name:data[0] for name,data in prepared.items()}
    cli('add', 'gitdemo', cwd=project, contains='unchanged')
    assert {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in registered_catalog.iterdir()} == prepared
    cli('run', '.', cwd=project, status=2, contains='--allow-native')
    cli('tidy', '--check', cwd=project)
    cli('package', 'uninstall', 'gitdemo', cwd=root)
    cli('run', '.', '--allow-native', cwd=project, exact='42\n')
    cli('remove', 'gitdemo', cwd=project)
    assert not (store/'gitdemo').exists() and (git_repo/'with-native').is_dir()
    print('Declarative C package, catalog reuse, explicit consent, tamper rejection, JIT and independent AOT passed', flush=True)
