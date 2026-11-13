"""G26 CLI, D3 views, editable sources and interrupted publication qualification."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import tomllib

compiler = Path(sys.argv[1]).resolve()
interrupt = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
checks = []


def run(*args, status=0, contains=None, cwd=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, timeout=90)
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    if contains:
        assert contains.encode() in result.stdout + result.stderr, (contains, result.stdout, result.stderr)
    return result


def cli(root, command, *args, **kwargs):
    def complete_state():
        return {str(p.relative_to(root)): (p.stat().st_mtime_ns, p.read_bytes() if p.is_file() else None)
                for p in root.rglob('*')}
    before = complete_state() if command == 'tidy' and '--check' in args else None
    result = run(compiler, command, '--project', root, *args, **kwargs)
    if before is not None:
        assert complete_state() == before, 'tidy --check modified project or administrative state'
    return result


def write(root, relative, data):
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(data, encoding='utf-8')
    return target


def state(root):
    # Administrative caches may be warmed by an explicit fetch that later fails.
    # Published manifests, lock, source and installed trees must remain unchanged.
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and '.void' not in p.relative_to(root).parts}


def failure(root, *args, contains=None, status=1):
    before = state(root)
    cli(root, *args, status=status, contains=contains)
    assert state(root) == before, args
    assert not (root / '.void/package-edit').exists(), args


def new(root, library=False):
    root.parent.mkdir(parents=True, exist_ok=True)
    args = ['--nested', '--project-id', 'org.example.' + root.name + '.project']
    if library:
        args += ['--kind', 'library', '--package-id', 'org.example.' + root.name, '--package-name', root.name]
    else:
        args += ['--app-id', 'org.example.' + root.name + '.app']
    run(compiler, 'new', root, *args)
    if library:
        assert tomllib.loads((root / 'void.project.toml').read_text())['package']['entry'] == 'src/' + root.name + '.void'


def qualification(base):
    # G27: the generated manifests compose without entry/dependency repairs.
    geometry, consumer, transitive = (base / name for name in ('geometry', 'consumer', 'transitive'))
    new(geometry, True); new(consumer, True); new(transitive)
    cli(consumer, 'add', 'geometry', '--path', '../geometry')
    write(consumer, 'src/consumer.void', 'use pkg/geometry as g\npublic func answer() -> i32:\n    return g.answer() + 7\n')
    cli(consumer, 'tidy', '--check'); cli(consumer, 'check')
    before = state(consumer)
    cli(consumer, 'add', 'geometry', '--path', '../geometry')
    assert state(consumer) == before
    cli(transitive, 'add', 'consumer', '--path', '../consumer')
    write(transitive, 'src/app.void', 'use pkg/consumer as c\nfunc main() -> i32:\n    return c.answer() - 49\n')
    cli(transitive, 'tidy', '--check'); cli(transitive, 'resolve', '--locked')
    for flags in [('--no-opt',), ()]:
        cli(transitive, 'run', '--no-cache', *flags)
    binary = base / ('transitive-native.exe' if os.name == 'nt' else 'transitive-native')
    cli(transitive, 'build', '-o', binary); run(binary)
    output, relocated = base / 'transitive-package', base / 'transitive-relocated'
    cli(transitive, 'package', '-o', output)
    run(compiler, 'package', '--verify', output)
    shutil.move(output, relocated)
    # Execution after moving both source projects proves the D5 source boundary.
    shutil.move(transitive, base / 'parked-transitive')
    shutil.move(consumer, base / 'parked-consumer')
    shutil.move(geometry, base / 'parked-geometry')
    run(compiler, 'package', '--verify', relocated)
    executables = list(relocated.glob('*.exe')) if os.name == 'nt' else [p for p in relocated.iterdir() if p.is_file() and os.access(p, os.X_OK)]
    assert len(executables) == 1, executables
    run(executables[0])
    for root in (transitive, consumer, geometry):
        shutil.move(base / ('parked-' + root.name), root)
    # Equal IDs at independently copied editable origins must still conflict.
    failure(transitive, 'add', 'geometry', '--path', '../geometry', contains='VOID5007')
    write(transitive, 'src/app.void', 'use pkg/geometry as g\nfunc main() -> i32:\n    return g.answer()\n')
    failure(transitive, 'tidy', '--check', contains='Undeclared direct package')
    failure(transitive, 'check', contains='Undeclared direct package')
    write(transitive, 'src/app.void', 'use pkg/consumer as c\nfunc main() -> i32:\n    return c.answer() - 49\n')
    checks.append('generated library add/tidy/check; transitive application JIT/O2/AOT/D5 after source moves; copied-origin VOID5007; no undeclared re-export')

    app = base / 'app'
    new(app)
    assert not (app / 'libs').exists()
    write(app, 'libs/math/math.void', 'public func answer() -> i32:\n    return 0\n')
    write(app, 'src/app.void', 'use lib/math as m\nfunc main() -> i32:\n    return m.answer()\n')
    cli(app, 'run', '--no-cache', '--no-opt')
    before = state(app)
    cli(app, 'tidy'); cli(app, 'tidy', '--check')
    assert state(app) == before and not (app / 'void.lock').exists()
    write(app, 'libs/math/math.void', 'public func answer() -> i32:\n    return 7\n')
    cli(app, 'run', '--no-cache', status=7)
    cli(app, 'tidy', '--check')
    checks.append('ordinary manifest-free lib/math; tidy read-only; source edits observed')
    write(app, 'src/app.void', 'use lib/missing\nfunc main() -> i32:\n    return 0\n')
    failure(app, 'tidy', contains='Cannot find')
    write(app, 'src/app.void', 'use pkg/missing\nfunc main() -> i32:\n    return 0\n')
    failure(app, 'tidy', '--check', contains='Undeclared direct package')
    write(app, 'src/app.void', 'func main() -> i32:\n    return 0\n')

    local = base / 'local'
    new(local, True)
    # Preserve source comments, unrelated table order and CRLF exactly.
    manifest = app / 'void.project.toml'
    original = manifest.read_bytes().replace(b'\n', b'\r\n') + b'\r\n# Boss comment\r\n'
    manifest.write_bytes(original)
    cli(app, 'add', 'local', '--path', '../local', contains='Editable local source')
    assert manifest.read_bytes().startswith(original)
    assert b'\n' not in manifest.read_bytes().replace(b'\r\n', b'')
    dependency = tomllib.loads(manifest.read_text())['dependencies']['local']
    assert dependency['id'] == 'org.example.local' and dependency['path'] == 'libs/local'
    write(app, 'src/app.void', 'use pkg/local as p\nfunc main() -> i32:\n    return p.answer()\n')
    cli(app, 'run', '--no-cache', status=42)
    write(app, 'libs/local/src/local.void', 'public func answer() -> i32:\n    return 0\n')
    failure(app, 'tidy', '--check', contains='requires reconciliation')
    cli(app, 'tidy'); cli(app, 'run', '--no-cache')
    before = state(app)
    cli(app, 'add', 'local', '--path', '../local')
    cli(app, 'tidy'); cli(app, 'tidy', '--check')
    assert state(app) == before
    checks.append('external editable source copy; exact TOML prefix/CRLF; local lock refresh; repeat idempotence')

    # In-project path sources retain live path identity and edits.
    live = app / 'vendor/live'
    new(live, True)
    cli(app, 'add', 'live', '--path', 'vendor/live')
    assert tomllib.loads(manifest.read_text())['dependencies']['live']['path'] == 'vendor/live'
    cli(app, 'tidy', '--check', contains="Unused direct dependency 'live' retained")

    repo = app / 'vendor/repo'
    new(repo, True)
    run('git', 'init', '-q', repo)
    run('git', '-C', repo, 'add', '.')
    run('git', '-C', repo, '-c', 'user.name=G26 fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'immutable source fixture')
    revision = run('git', '-C', repo, 'rev-parse', 'HEAD').stdout.decode().strip()
    cli(app, 'add', 'source', '--git', 'file:vendor/repo', '--rev', revision)
    assert (app / 'libs/source/src/repo.void').is_file()
    lock = tomllib.loads((app / 'void.lock').read_text())
    locked = next(p for p in lock['package'] if p['id'] == 'org.example.repo')
    assert locked['rev'] == revision and locked['git'] == 'file:vendor/repo' and 'path' not in locked
    write(app, 'src/app.void', 'use pkg/source as p\nfunc main() -> i32:\n    return p.answer() - 42\n')
    run(compiler, 'deps', 'explain', '--project', app); cli(app, 'resolve', '--locked')
    cli(app, 'tidy', '--check')
    cli(app, 'run', '--no-cache', '--no-opt'); cli(app, 'run', '--no-cache')
    binary = app / ('build/native.exe' if os.name == 'nt' else 'build/native')
    cli(app, 'build', '-o', binary); run(binary)
    output = base / 'distribution'
    cli(app, 'package', '-o', output)
    run(compiler, 'package', '--verify', output)
    relocated = base / 'relocated'
    shutil.move(output, relocated)
    run(compiler, 'package', '--verify', relocated)
    executables = list(relocated.glob('*.exe')) if os.name == 'nt' else [p for p in relocated.iterdir() if p.is_file() and os.access(p, os.X_OK)]
    assert len(executables) == 1, executables
    run(executables[0])
    before = state(app)
    cli(app, 'add', 'source', '--git', 'file:vendor/repo', '--rev', revision)
    cli(app, 'tidy'); cli(app, 'tidy')
    assert state(app) == before
    checks.append('immutable D3 Git view; explain/locked; normal/O2 JIT; native AOT; D5 verification/relocation; idempotence')
    # Library consumers use the identical D3 pin and publication path. The app
    # already declares this same immutable origin; no physical local IDs merge.
    gitapp = base / 'gitapp'
    new(gitapp)
    shutil.copytree(repo, gitapp / 'vendor/repo')
    cli(gitapp, 'add', 'source', '--git', 'file:vendor/repo', '--rev', revision)
    gitconsumer = gitapp / 'vendor/gitconsumer'
    new(gitconsumer, True)
    shutil.copytree(repo, gitconsumer / 'vendor/repo')
    cli(gitconsumer, 'add', 'source', '--git', 'file:vendor/repo', '--rev', revision)
    write(gitconsumer, 'src/gitconsumer.void', 'use pkg/source as p\npublic func answer() -> i32:\n    return p.answer()\n')
    cli(gitconsumer, 'tidy', '--check'); cli(gitconsumer, 'check')
    cli(gitconsumer, 'resolve', '--locked')
    cli(gitapp, 'add', 'gitconsumer', '--path', 'vendor/gitconsumer')
    write(gitapp, 'src/app.void', 'use pkg/gitconsumer as p\nfunc main() -> i32:\n    return p.answer() - 42\n')
    cli(gitapp, 'tidy', '--check'); cli(gitapp, 'run', '--no-cache')
    # Leave the original fixture's later pin-update assertions independent.
    checks.append('library adds full-pin Git dependency; locked/tidy/check; application consumes shared immutable transitive origin')
    installed = app / 'libs/source/src/repo.void'
    content = installed.read_bytes()
    installed.write_bytes(content + b'// tamper\n')
    failure(app, 'tidy', '--check', contains='Modified or unowned')
    failure(app, 'tidy', contains='Modified or unowned')
    failure(app, 'check', contains='Modified or unowned')
    installed.write_bytes(content)
    write(app, 'src/app.void', 'use lib/source/src/repo\nfunc main() -> i32:\n    return 0\n')
    failure(app, 'check', contains='project boundary')
    failure(app, 'tidy', contains='project boundary')
    write(app, 'src/app.void', 'use "/libs/source/src/repo.void"\nfunc main() -> i32:\n    return 0\n')
    failure(app, 'check', contains='project boundary')
    write(app, 'src/app.void', 'func main() -> i32:\n    return 0\n')
    checks.append('installed edits rejected by check/tidy; lib and quoted imports cannot bypass pkg boundary')

    failure(app, 'add', 'source', '--git', 'file:vendor/repo', '--rev', 'a' * 40)
    failure(app, 'add', 'bad', '--git', 'file:vendor/repo', '--rev', 'short')
    failure(app, 'add', 'bad', '--git', 'file:vendor/absent', '--rev', revision)
    failure(app, 'add', 'local', '--path', 'vendor/live', contains='Conflicting')
    failure(app, 'add', 'source', status=2)
    write(app, 'libs/collision/owned.void', '// developer-owned\n')
    failure(app, 'add', 'collision', '--git', 'file:vendor/repo', '--rev', revision, contains='already exists')
    failure(app, 'add', 'broken', '--path', 'absent')
    bad = base / 'bad'
    new(bad, True)
    write(bad, 'void.project.toml', 'not valid TOML !')
    failure(app, 'add', 'broken', '--path', '../bad')
    # Resolution failure after publishing a staged local directory rolls back.
    broken = base / 'broken'
    new(broken, True)
    with (broken / 'void.project.toml').open('a') as f:
        f.write('\n[dependencies.nope]\nid="org.example.nope"\npath="absent"\nversion="=0.1.0"\n')
    failure(app, 'add', 'broken', '--path', '../broken')
    if os.name != 'nt':
        (app / 'libs/link').symlink_to(local, target_is_directory=True)
        failure(app, 'add', 'link', '--path', '../local')
        (app / 'libs/link').unlink()
    # A deliberately changed pin is reconciled only after an explicit D3 fetch;
    # the previous installed view must still match its old verified snapshot.
    write(repo, 'README.txt', 'second immutable revision\n')
    run('git', '-C', repo, 'add', '.')
    run('git', '-C', repo, '-c', 'user.name=G26 fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'second explicit pin')
    updated = run('git', '-C', repo, 'rev-parse', 'HEAD').stdout.decode().strip()
    manifest.write_text(manifest.read_text().replace(revision, updated))
    failure(app, 'tidy')  # No implicit acquisition.
    run(compiler, 'deps', 'fetch', '--project', app)
    failure(app, 'tidy', '--check', contains='requires reconciliation')
    cli(app, 'tidy'); cli(app, 'tidy', '--check')
    assert (app / 'libs/source/README.txt').read_text() == 'second immutable revision\n'
    revision = updated
    locked = next(p for p in tomllib.loads((app / 'void.lock').read_text())['package'] if p['id'] == 'org.example.repo')
    shutil.rmtree(app / 'libs/source')
    before = state(app)
    cli(app, 'tidy', '--check', contains='Optional installed source view missing')
    assert state(app) == before
    cli(app, 'tidy'); cli(app, 'tidy', '--check')
    assert (app / 'libs/source/README.txt').is_file()
    # Immutable lock claims cannot be refreshed into apparently clean state.
    lock_path = app / 'void.lock'
    lock_bytes = lock_path.read_bytes()
    lock_path.write_bytes(lock_bytes.replace(locked['tree'].encode(), b'0' * 64))
    failure(app, 'tidy', contains='Stale or tampered')
    lock_path.write_bytes(lock_bytes)
    write(app, 'libs/unreferenced/broken.void', 'intentionally invalid unreferenced source !')
    cli(app, 'tidy', '--check')
    checks.append('explicit pin change and verified old-view replacement; manual D3 fetch; optional view reconstruction; immutable lock tamper rejected; unreferenced library excluded')
    declaration = manifest.read_bytes()
    manifest.write_bytes(declaration.replace(b'org.example.local"', b'org.example.wrong"'))
    failure(app, 'tidy', contains='Wrong dependency package ID')
    manifest.write_bytes(declaration)
    manifest.write_bytes(declaration.replace(b'version = "=0.1.0"', b'version = "=2.0.0"', 1))
    failure(app, 'tidy', contains='version conflict')
    manifest.write_bytes(declaration)
    native = b'\n[native-requirements.raylib]\nid="org.voidlang.raylib"\nversion="=6.0.0"\nabi=3\n'
    manifest.write_bytes(declaration + native)
    failure(app, 'tidy', contains='catalog')
    manifest.write_bytes(declaration)
    if os.name == 'nt':
        run('cmd', '/d', '/c', 'mklink', '/J', app / 'libs/link', local)
        failure(app, 'add', 'link', '--path', '../local')
        os.rmdir(app / 'libs/link')
    checks.append('failed acquisition, invalid SHA, conflicting alias, owned collision, malformed source, missing transitive path and symlink rollback')

    if interrupt:
        for library, phase in [(library, phase) for library in (False, True)
                               for phase in ['before', 'after', 'replace-displaced', 'replace-before', 'replace-after']]:
            recovery = base / ('recovery' + ('library' if library else 'app') + phase.replace('-', ''))
            new(recovery, library)
            if phase.startswith('replace'):
                write(recovery, 'libs/interrupted/owned.txt', 'previous source')
            before = state(recovery)
            run(interrupt, '--package-interrupt', recovery, phase, status=86)
            assert (recovery / '.void/package-edit').is_dir()
            snapshot = state(recovery)
            cli(recovery, 'tidy', '--check', status=1, contains='Interrupted package edit')
            cli(recovery, 'check', status=1, contains='Interrupted package edit')
            assert state(recovery) == snapshot
            cli(recovery, 'tidy')
            assert state(recovery) == before
            cli(recovery, 'tidy', '--check')
        checks.append('five process-interruption phases for applications and libraries including displaced previous view and before/after manifest/lock publication; read-only gate and exact recovery')
    editor = os.environ.get('VOID_G26_EDITOR_PROJECT')
    if editor:
        destination = Path(editor)
        shutil.copytree(app, destination)
        write(destination, 'src/app.void', 'use pkg/source as p\nuse lib/math as m\nfunc main() -> i32:\n    return p.answer() - 42\n')
    evidence = os.environ.get('VOID_G26_EVIDENCE')
    if evidence:
        Path(evidence).write_text(json.dumps({'status': 'PASS', 'revision': revision, 'source': locked, 'manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(), 'lock_sha256': hashlib.sha256((app / 'void.lock').read_bytes()).hexdigest(), 'checks': checks}, indent=2) + '\n')


with tempfile.TemporaryDirectory(prefix='VOID G26 maintenance ') as temporary:
    qualification(Path(temporary))
print('\n'.join(checks))
print('G26_PACKAGE_MAINTENANCE_PASS')
