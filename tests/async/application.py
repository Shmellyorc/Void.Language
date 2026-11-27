"""Run asynchronous applications through the ordinary JIT and native CLI entry."""
from pathlib import Path
import os
import subprocess
import sys
from tempfile import TemporaryDirectory

compiler = Path(sys.argv[1]).resolve()


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(*command, status=0, output=None, error=None):
    result = subprocess.run([str(part) for part in command], capture_output=True, timeout=60,
                            preexec_fn=stack if os.name == 'posix' else None)
    stdout = result.stdout.decode('utf-8').replace('\r\n', '\n')
    stderr = result.stderr.decode('utf-8')
    assert result.returncode == status, (command, result.returncode, stdout, stderr)
    if output is not None:
        assert stdout == output, (command, stdout, stderr)
    if error is not None:
        assert error.lower() in stderr.lower(), (command, stdout, stderr)
    return result


cases = {
    'zero': ('async func main():\n    print("normal")\n', 0, 'normal\n', None),
    'drain': ('''var count = 6
async func child():
    defer print("child cleaned")
    count += 1
async func main() -> i32:
    defer {
        print("main cleaned", count)
    }
    var pending = child()
    return count
''', 6, 'child cleaned\nmain cleaned 7\n', None),
    'failure': ('''async func child(input: i32) -> i32:
    defer print("child cleaned")
    return input / (input - input)
async func main() -> i32:
    defer print("main cleaned")
    return await child(5)
''', 1, 'child cleaned\nmain cleaned\n', 'division'),
    'cancel': ('''async func pause():
    pass
async func job():
    defer print("job cleaned")
    await pause()
    print("job finished")
async func stop<t>(pending: t):
    pending.cancel()
async func main():
    defer print("main cleaned")
    var pending = job()
    var canceller = stop(pending)
    await pending
''', 1, 'job cleaned\nmain cleaned\n', 'cancelled'),
}
with TemporaryDirectory(prefix='void-async-main-') as temporary:
    root = Path(temporary)
    project = root / 'manifest'
    run(compiler, 'new', project, '--source', 'src/start.void')
    manifest = project / 'void.project.toml'
    # Exercise automatic discovery, independent of the file's basename.
    manifest.write_text(''.join(line for line in manifest.read_text().splitlines(keepends=True)
                                if not line.startswith('entry =')))
    project_source = project / 'src/start.void'
    project_source.write_text(cases['zero'][0])
    run(compiler, 'run', '--project', project, '--no-cache', output='normal\n')

    source = root / 'program.void'
    suffix = '.exe' if os.name == 'nt' else ''
    for mode in ([], ['--no-opt']):
        for case, (body, status, output, error) in cases.items():
            executable = root / f'{case}-{len(mode)}{suffix}'
            source.write_text(body)
            run(compiler, 'run', source, '--no-cache', *mode, status=status, output=output, error=error)
            run(compiler, 'build', source, '-o', executable, *mode)
            run(executable, status=status, output=output, error=error)

    source.write_text('async func main():\n    pass\nprint("competing")\n')
    run(compiler, 'check', source, status=1, error='compete with main')
print('Async main passed ordinary JIT and AOT entry, exit, drain, failure and cancellation checks')
