"""No-opt AOT must finish a local initializer before stopping on the next line."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory


compiler, dwarf, lldb = (str(Path(p).resolve()) for p in sys.argv[1:4])
sanitized = sys.argv[4:] == ['1']
SOURCE = '''struct board:
    scroll: f32
func (g board) draw_background():
    for row in 0..<2:
        for col in 0..<2:
            var px = (col as f32) * 192f + g.scroll - 192f
            var py = (row as f32) * 192f + g.scroll - 192f
            print(px)
            print(py)
func main():
    var g = board()
    g.draw_background()
'''
EXPECTED = [-192, -192, 0, -192, -192, 0, 0, 0]


def run(*args):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
    return result.stdout


def values(output):
    assert list(map(float, output.splitlines())) == EXPECTED, output


def debug(root, executable, mode):
    commands = ['settings set target.disable-aslr false']
    if sanitized:
        # The untraced run checks for leaks; the LLDB child cannot use ptrace for that check.
        asan_options = os.environ.get('ASAN_OPTIONS', '')
        target_options = ':'.join(filter(None, (asan_options, 'detect_leaks=0')))
        commands.append('settings set target.env-vars ' + json.dumps(f'ASAN_OPTIONS={target_options}'))
    commands += ['breakpoint set -f app.void -l 7', 'run']
    if mode == 'full':
        commands += ['frame variable row col px', 'expression -- px']
    commands += ['next', 'frame info']
    if mode == 'full':
        commands += ['frame variable row col px py', 'expression -- py']
    commands += ['breakpoint disable', 'continue']
    script = root / 'lldb-commands.txt'
    script.write_text('\n'.join(commands) + '\n')
    output = run(lldb, '--batch', '--no-lldbinit', '-s', script, executable)
    print(output)
    before, after = output.split('(lldb) next\n', 1)
    assert re.search(r'frame #0: .*app\.void:7\b', before), before
    assert re.search(r'frame #0: .*app\.void:8\b', after), after
    if mode == 'full':
        assert '(float) px = -192' in before and '(int) row = 0' in before, before
        assert '(float) py = -192' in after and '(int) col = 0' in after, after
        assert re.search(r'\$0 = -192\b', before) and re.search(r'\$1 = -192\b', after), output
    assert 'exited with status = 0' in after, output


with TemporaryDirectory(prefix='VOID debug initializer ') as directory:
    root = Path(directory)
    source = root / 'app.void'
    source.write_text(SOURCE)
    (root / 'void.project.toml').write_text(
        "schema=1\n[project]\nid='org.test.debug.initializers'\nname='Initializers'\n"
        "version='1.0.0'\nkind='application'\n[application]\nid='org.test.debug.initializers.app'\n"
        "[build.debug]\noptimization='none'\ndebug_info='full'\n"
        "[build.release]\noptimization='O2'\ndebug_info='full'\n")
    for profile in ('debug', 'release'):
        selection = ('--project', root, '--profile', profile)
        values(run(compiler, 'run', *selection))
        executable = root / profile
        obj = root / (profile + '.o')
        run(compiler, 'build', *selection, '-o', executable)
        run(compiler, 'emit', *selection, '-o', obj)
        run(dwarf, '--verify', obj)
        values(run(executable))
        if profile == 'debug':
            debug(root, executable, 'full')
        print(f'PASS: {profile} JIT/AOT values and valid emitted DWARF')
    # Line-only source debugging must honor the same no-opt codegen contract.
    executable = root / 'line-only'
    run(compiler, 'build', '--project', root, '--profile', 'debug', '--debug-info', 'line', '-o', executable)
    values(run(executable))
    debug(root, executable, 'line')
    assert source.read_text() == SOURCE
    print('PASS: full/line no-opt source breakpoint 7 -> 8, initialized locals and native Watch evaluation')
