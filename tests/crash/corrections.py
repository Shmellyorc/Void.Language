"""Focused G90 review regressions. Uses the existing native crash harness.
Windows-only cases are native tests, not Linux simulations of Windows support.
"""
from pathlib import Path
import json
import os
import signal
import sys
import tempfile
from diagnostics import PROBE, COMPILER, EVIDENCE, run, reports, read_latest


def main(root):
    env = dict(os.environ, VOID_CRASH_DIAGNOSTICS='1')
    completed = []
    if os.name == 'nt':
        windows_main(root, env, completed)
        preserve(completed)
        print('G90 Windows handler, exit preservation and supported sanitizer checks passed (native host only)')
        return
    for index, suffix in enumerate(('', '/', '/.', '////', '/./')):
        for exists in (False, True):
            leaf = root / f'private reports 雪 {index} {exists}'
            if exists:
                leaf.mkdir(mode=0o700)
            result = run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(leaf)+suffix), stack=True)
            assert result.returncode != 0
            assert len(reports(leaf)) == 1, (leaf, suffix, result.stderr)
            assert read_latest(leaf)['classification'] in ('fatal_signal', 'sanitizer_finding')
            completed.append(leaf)
    for name in ('relative', str(root / 'missing-parent' / 'leaf'), str(root.anchor)):
        result = run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=name), stack=True)
        assert result.returncode != 0 and b'VOID crash report unavailable' in result.stderr
    file = root / 'ordinary-file'; file.write_text('not a report directory')
    result = run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(file)), stack=True)
    assert result.returncode != 0 and file.read_text() == 'not a report directory'
    if os.name != 'nt':
        target = root / 'private-target'; target.mkdir(mode=0o700)
        link = root / 'symlink'; link.symlink_to(target, target_is_directory=True)
        for suffix in ('', '/', '/.', '/./'):
            result = run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(link)+suffix), stack=True)
            assert result.returncode == -signal.SIGABRT and not reports(target)
        public = root / 'public'; public.mkdir(mode=0o755)
        result = run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(public)+'/.'), stack=True)
        assert result.returncode == -signal.SIGABRT and not reports(public)
        context = root / 'fork-context'
        result = run([PROBE, 'fork-context'], dict(env, VOID_CRASH_DIRECTORY=str(context)), stack=True)
        assert result.returncode == -signal.SIGABRT
        data = read_latest(context)
        assert data['classification'] == 'fatal_signal' and data['recent_operations'][-1] == 'frontend'
        completed.append(context)
        if os.environ.get('VOID_G90_STRICT') == '1':
            for mode in ('fork-sanitizer', 'fork-sanitizer-nested'):
                leaf = root / mode
                for enabled in ('0', '1'):
                    result = run([PROBE, mode], dict(env, VOID_CRASH_DIRECTORY=str(leaf), VOID_CRASH_DIAGNOSTICS=enabled), stack=True)
                    assert result.returncode == 0 and b'AddressSanitizer' in result.stderr, result
                    assert b'parent normal' in result.stdout and not leaf.exists(), (mode, enabled)
            leaf = root / 'fork-sanitizer-parent-crash'
            result = run([PROBE, 'fork-sanitizer-parent-crash'], dict(env, VOID_CRASH_DIRECTORY=str(leaf)), stack=True)
            assert result.returncode == -signal.SIGABRT and b'AddressSanitizer' in result.stderr
            data = read_latest(leaf)
            assert data['classification'] == 'fatal_signal' and data['termination_code'] == signal.SIGABRT
            completed.append(leaf)
            leaf = root / 'genuine-host-sanitizer'
            result = run([PROBE, 'sanitizer'], dict(env, VOID_CRASH_DIRECTORY=str(leaf)), stack=True)
            assert result.returncode != 0 and read_latest(leaf)['classification'] == 'sanitizer_finding'
            completed.append(leaf)
        child_dir = root / 'genuine-native-child'
        result = run([PROBE, 'child'], dict(env, VOID_CRASH_DIRECTORY=str(child_dir)), stack=True)
        assert result.returncode == 0
        data = read_latest(child_dir)
        assert data['process'] == 'native-tool' and data['classification'] == 'child_crash' and data['pid'] is None
        completed.append(child_dir)
    preserve(completed)
    print('G90 corrections: process ownership, equivalent/private paths and native-host classifications passed on ' + os.name)


def preserve(completed):
    if EVIDENCE:
        for leaf in completed:
            destination = EVIDENCE / leaf.name; destination.mkdir(exist_ok=True)
            for path in reports(leaf): (destination / path.name).write_bytes(path.read_bytes())


def windows_main(root, env, completed):
    leaf = root / 'ordinary-windows-exits'
    ordinary = dict(env, VOID_CRASH_DIRECTORY=str(leaf))
    for code in (0, 1, 17, 86, 87, 139, -2, -1073741819, -1073741571, -2147483645):
        for args in ([PROBE, 'exit', str(code)], [COMPILER, 'crashes', 'run', PROBE, 'exit', str(code)]):
            result = run(args, ordinary)
            assert result.returncode & 0xffffffff == code & 0xffffffff, (args, code, result)
            assert not leaf.exists(), (args, result.stderr)
    args = [COMPILER, 'crashes', 'run', PROBE, 'args', 'two words', '"quote"', 'tail\\']
    result = run(args, ordinary)
    assert result.returncode == 0 and result.stdout.splitlines() == [b'two words', b'"quote"', b'tail\\'], result
    assert not result.stderr and not leaf.exists(), result
    for args in ([PROBE, 'handled'], [PROBE, 'exit-process'],
                 [COMPILER, 'crashes', 'run', PROBE, 'handled'],
                 [COMPILER, 'crashes', 'run', PROBE, 'exit-process']):
        result = run(args, ordinary)
        assert result.returncode & 0xffffffff == 0xC0000005 and not leaf.exists()
    source = root / 'normal-negative.void'
    source.write_text('func main() -> i32:\n    return -1073741819\n')
    for flags in ([], ['--no-opt']):
        result = run([COMPILER, 'run', str(source), *flags], ordinary)
        assert result.returncode & 0xffffffff == 0xC0000005 and not leaf.exists()
    # Neither unmarked host exceptions nor AOT exceptions authorize reports.
    for mode in ('raw-abort', 'abort', 'teardown-exception', 'custom-exception', 'fail-fast', 'stack', 'stack-thread'):
        for index, args in enumerate(([PROBE, mode], [COMPILER, 'crashes', 'run', PROBE, mode])):
            incident = root / f'windows-{mode}-{index}'
            result = run(args, dict(env, VOID_CRASH_DIRECTORY=str(incident)))
            assert result.returncode != 0, (args, result.stderr)
            if incident.exists():
                assert os.environ.get('VOID_G90_STRICT') == '1'
                assert read_latest(incident)['classification'] == 'sanitizer_finding'
                completed.append(incident)
    fixture = str(Path(sys.argv[4]).resolve())
    for enabled in ('0', '1'):
        handler_env = dict(ordinary, VOID_CRASH_DIAGNOSTICS=enabled)
        for args in ([fixture], [COMPILER, 'crashes', 'run', fixture],
                     [PROBE, 'top-level-filter'], [COMPILER, 'crashes', 'run', PROBE, 'top-level-filter']):
            result = run(args, handler_env)
            assert result.returncode == 17 and not leaf.exists(), (args, result.stderr)
    if os.environ.get('VOID_G90_STRICT') == '1':
        host = root / 'windows-host-sanitizer'
        result = run([PROBE, 'sanitizer'], dict(env, VOID_CRASH_DIRECTORY=str(host)))
        assert result.returncode != 0 and b'AddressSanitizer' in result.stderr
        data = read_latest(host)
        assert data['classification'] == 'sanitizer_finding' and data['process'] == 'probe'
        completed.append(host)
    # LLVM's unobserved child status is not sufficient Windows exception
    # evidence. Keep it out of host/native-crash classification entirely.
    leaf = root / 'unobserved-windows-child'
    result = run([PROBE, 'child'], dict(env, VOID_CRASH_DIRECTORY=str(leaf)))
    assert result.returncode == 0 and not leaf.exists()


if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='VOID G90 corrections 雪 ') as temporary:
        main(Path(temporary))
