"""Optional real package consumer, sharing the existing D3 smart HTTPS fixture.

Run git_https.py with VOID_TEST_RAYLIB_CATALOG and VOID_TEST_RAYLIB_EVIDENCE.
The catalog is built and explicitly approved by the caller, outside Git fetch.
"""
from hashlib import sha256
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from unittest.mock import patch


def qualify(compiler, git, repos, base, origin, environment):
    root = Path(__file__).resolve().parents[2]
    output = Path(os.environ['VOID_TEST_RAYLIB_EVIDENCE']).resolve()
    output.mkdir(parents=True, exist_ok=True)
    catalog = Path(os.environ['VOID_TEST_RAYLIB_CATALOG']).resolve()
    env = dict(environment, VOID_NATIVE_CATALOG=str(catalog))
    # D3 isolates Git's transport scratch. Native linking uses the caller's
    # separately verified private temp root, as in the existing HTTPS AOT test.
    native_env = dict(env)
    for name in ('TMPDIR', 'TEMP', 'TMP'):
        if name in os.environ:
            native_env[name] = os.environ[name]
        else:
            native_env.pop(name, None)
    events = []
    def run(*args, status=0, message=None, cwd=None, use_env=None):
        result = subprocess.run(list(map(str, args)), env=env if use_env is None else use_env,
                                cwd=cwd, capture_output=True, timeout=180)
        text = (result.stdout + result.stderr).decode(errors='replace')
        assert result.returncode == status, (args, result.returncode, text)
        assert message is None or message in text, (args, text)
        assert 'AddressSanitizer' not in text and 'UndefinedBehaviorSanitizer' not in text, text
        return text
    repo = repos / 'raylib.git'
    standalone = os.environ.get('VOID_TEST_RAYLIB_STANDALONE') == '1'
    package = repo if standalone else repo / 'verified-packages/raylib'
    shutil.copytree(root / 'verified-packages/raylib', package, ignore=shutil.ignore_patterns('__pycache__'))
    run(git, 'init', '--quiet', '--object-format=sha1', repo)
    run(git, '-C', repo, 'add', '.')
    run(git, '-C', repo, '-c', 'user.name=VOID Raylib Fixture', '-c', 'user.email=fixture@example.invalid',
        '-c', 'commit.gpgsign=false', 'commit', '--quiet', '--no-verify', '-m', 'Disposable raylib package')
    revision = run(git, '-C', repo, 'rev-parse', 'HEAD').strip()
    consumer = output / 'consumer'
    assert not consumer.exists(), consumer
    shutil.copytree(root / 'examples/raylib-consumer', consumer, ignore=shutil.ignore_patterns('__pycache__', 'void.project.toml'))
    run(sys.executable, consumer / 'configure.py', '--origin', origin + '/raylib.git', '--rev', revision,
        *(['--standalone'] if standalone else []))
    if os.environ.get('VOID_TEST_RAYLIB_ASSETS'):
        assets=Path(os.environ['VOID_TEST_RAYLIB_ASSETS']).resolve()
        shutil.copytree(assets,consumer/'assets')
        manifest=consumer/'void.project.toml'
        names=', '.join('"assets/'+file.name+'"' for file in sorted(assets.iterdir()))
        manifest.write_text(manifest.read_text().replace('[application]\n','[application]\nresources=['+names+']\n'),encoding='utf-8')
    def cli(command, *args, **options):
        if command == 'deps':
            return run(compiler, command, args[0], '--project', consumer, *args[1:], **options)
        if command in ('build', 'package'):
            options['use_env'] = native_env
        if command == 'run':
            options.setdefault('cwd',base)
        return run(compiler, command, '--project', consumer, *args, **options)
    cli('deps', 'fetch')
    events.append('smart HTTPS Git acquisition of exact immutable revision and package subdirectory')
    cli('resolve')
    cli('add', 'raylib', '--git', origin + '/raylib.git', '--rev', revision,
        *([] if standalone else ['--subdir', 'verified-packages/raylib']))
    assert (consumer / 'libs/raylib/src/raylib.void').is_file()
    cli('tidy', '--check')
    events.append('existing manual Git declaration adopted by add into verified libs/raylib; read-only tidy')
    lock = consumer / 'void.lock'
    before = lock.read_bytes(), lock.stat().st_mtime_ns
    cli('resolve', '--locked')
    cli('check')
    run(compiler, 'deps', 'explain', '--project', consumer, '--json')
    assert before == (lock.read_bytes(), lock.stat().st_mtime_ns)
    events.append('source snapshot, native catalog, source/native lock, read-only locked resolve/check/explain')
    # A package cannot self-authorize execution.
    cli('run', status=2, message='--allow-native')
    for profile in ('debug', 'release'):
        text=cli('run', '--allow-native', '--profile', profile, '--no-cache', message='RAYLIB package headless ABI passed')
        assert 'RAYLIB subsystem headless ownership passed' in text,text
        assert 'RAYLIB final typed format/log and file callbacks passed' in text,text
        image = output / ('aot-' + profile) / ('app.exe' if os.name == 'nt' else 'app')
        cli('build', '--allow-native', '--profile', profile, '-o', image)
        run(image, message='RAYLIB package headless ABI passed', cwd=base)
        destination = output / ('package-' + profile)
        cli('package', '--allow-native', '--profile', profile, '-o', destination)
        run(compiler, 'package', '--verify', destination)
        moved = output / ('relocated ' + profile + ' 雪')
        destination.rename(moved)
        run(compiler, 'package', '--verify', moved)
        run(moved / image.name, message='RAYLIB package headless ABI passed', cwd=base)
        events.append(profile + ' JIT, native AOT, D5 inventory verification and relocated execution')
    # The imported helper invokes an operation beyond the historical 64th ordinal.
    events.append('nested camera/matrix/ray/collision records, mixed ownership/spans, images/PNG, WAV decoding, UTF-8/codepoints, strings, file results, compression, hashes and direct calls above ordinal 64')
    events.append('typed TraceLog/TextFormat and all five retained global hooks invoked and released through the Git source package')
    run(sys.executable,root/'tests/project/raylib_guards.py',compiler,catalog,use_env=native_env,cwd=base)
    events.append('private buffer tag and pointer-array alias rejection in normal/O2 native execution')
    original = (consumer / 'void.project.toml').read_text()
    (consumer / 'void.project.toml').write_text(original.replace(revision, '0' * 40))
    cli('resolve', '--locked', status=1, message='Missing Git dependency snapshot')
    (consumer / 'void.project.toml').write_text(original.replace('=0.1.0', '>=0.1.0'))
    cli('resolve', '--locked', status=1, message='Stale or tampered void.lock')
    (consumer / 'void.project.toml').write_text(original)
    suffix = 'src/raylib.void' if standalone else 'verified-packages/raylib/src/raylib.void'
    file, = (consumer / '.void/git-deps').glob('*/*/tree/' + suffix)
    contents = file.read_bytes()
    file.write_bytes(contents + b'\n# tampered source\n')
    cli('check', status=1, message='Git object identity mismatch')
    file.write_bytes(contents)
    provider = catalog / ('void_raylib.dll' if os.name == 'nt' else 'libvoid_raylib.so')
    contents = provider.read_bytes()
    try:
        provider.write_bytes(contents[:-1] + bytes([contents[-1] ^ 1]))
        cli('run', '--allow-native', status=1, message='artifact identity mismatch')
    finally:
        provider.write_bytes(contents)
    cli('resolve', '--locked')
    assert before == (lock.read_bytes(), lock.stat().st_mtime_ns)
    events.append('changed source, mismatched revision, stale lock and changed native provider refused')
    server = compiler.with_name('voidls.exe' if os.name == 'nt' else 'voidls')
    sys.path.insert(0, str(root / 'tools'))
    from lsp_client import Client
    main = consumer / 'src/main.void'
    source = main.read_text()
    line = next(i for i, s in enumerate(source.splitlines()) if 'rl.fade' in s)
    character = source.splitlines()[line].index('rl.fade') + 3
    with patch.dict(os.environ, env, clear=True), Client(server, consumer, project=True) as client:
        assert not any(note['diagnostics'] for note in client.initialize())
        client.notify('textDocument/didOpen', {'textDocument': {'uri': main.as_uri(), 'languageId': 'void', 'version': 1, 'text': source}})
        assert not any(note['diagnostics'] for note in client.sync())
        response, _ = client.request('textDocument/hover', {'textDocument': {'uri': main.as_uri()}, 'position': {'line': line, 'character': character}})
        assert 'fade' in str(response) and 'color' in str(response), response
        incomplete = source + '\nfunc package_completion_probe():\n    rl.get_ray_\n'
        client.notify('textDocument/didChange', {'textDocument': {'uri': main.as_uri(), 'version': 2},
                                                'contentChanges': [{'text': incomplete}]})
        client.sync()
        response, _ = client.request('textDocument/completion', {'textDocument': {'uri': main.as_uri()},
            'position': {'line': len(incomplete.splitlines()) - 1, 'character': 15}})
        assert 'get_ray_collision_box' in str(response), response
        client.shutdown()
    events.append('real language server resolves pinned pkg import and typed completion/hover')
    record = {'status': 'PASS', 'public_github_fetch': 'NOT RUN; final commit does not exist', 'revision': revision,
              'origin': origin + '/raylib.git', 'subdir': '' if standalone else 'verified-packages/raylib', 'events': events,
              'lock_sha256': sha256(lock.read_bytes()).hexdigest()}
    (output / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    print('Verified raylib source package HTTPS consumer passed', revision, flush=True)
