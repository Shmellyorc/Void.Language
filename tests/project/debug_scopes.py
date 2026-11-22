"""Source locals in nested scopes must exist, evaluate, and disappear at scope exit."""
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, dwarf, lldb = map(lambda p: str(Path(p).resolve()), sys.argv[1:4])
source = '''func compute(input: i32) -> i32:
    var total = input
    for index in 0..<2:
        var dt: f32 = 0.25
        if index == 0:
            var branch = 7
            total += branch
        else:
            var branch = 9
            total += branch
        total += dt as i32
    var finished = total
    return finished
func main() -> i32:
    return compute(1)
'''
def run(argv, **kw):
    p = subprocess.run(argv, capture_output=True, text=True, timeout=90, **kw)
    assert p.returncode == 0, (argv, p.stdout, p.stderr)
    return p.stdout
with tempfile.TemporaryDirectory(prefix='VOID debug lexical ') as directory:
    root = Path(directory)
    (root/'main.void').write_text(source)
    (root/'void.project.toml').write_text("schema=1\n[project]\nid='org.test.scopes'\nname='Scopes'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.scopes.app'\n[build.debug]\noptimization='O2'\ndebug_info='none'\n")
    exe = root/'app'
    run([compiler, 'build', '--project', str(root), '--profile', 'debug', '--no-opt', '--debug-info', 'full', '-o', str(exe)])
    run([dwarf, '--verify', str(exe)])
    info = run([dwarf, '--debug-info', str(exe)])
    for name in ['dt', 'index', 'branch', 'finished']:
        assert f'("{name}")' in info, name
    assert 'DW_TAG_lexical_block' in info
    commands = ['settings set target.disable-aslr false']
    for line in [7, 10, 11, 12, 13]:
        commands += [f'breakpoint set -f main.void -l {line}']
    commands += ['run', 'frame variable', 'expression -- branch', 'bt', 'continue',
                 'frame variable', 'expression -- dt', 'continue', 'frame variable',
                 'continue', 'frame variable', 'continue', 'frame variable', 'continue',
                 'frame variable', 'continue']
    # LLDB batch stops on an unexpected error; no error suppression is used.
    script = root/'commands';script.write_text('\n'.join(commands)+'\n')
    output = run([lldb, '--batch', '--no-lldbinit', '-s', str(script), str(exe)])
    print(output)
    assert '(float) dt = 0.25' in output, output
    assert '(int) branch = 7' in output and '(int) branch = 9' in output, output
    stops = output.split('(lldb) frame variable\n')[1:]
    # Stop 1 branch 7; stop 2 loop tail; stop 3 branch 9; stop 4
    # loop tail; stop 5 after loop; stop 6 initialized finished.
    assert len(stops)==6, output
    assert ' branch =' not in stops[1].split('(lldb) continue')[0], output
    assert ' dt =' not in stops[4].split('(lldb) continue')[0], output
    assert ' index =' not in stops[4].split('(lldb) continue')[0], output
    assert '(int) finished = 17' in stops[5], output
    assert 'main.void:15' in output and 'exited with status = 17' in output, output
    print('PASS: nested f32/i32, loop binder, sibling names, scope exit, expressions, call stack; debug override on O2/none profile')

    # Check distinct scalar widths/encodings through the actual expression
    # evaluator, including an i8 whose normal display is a character.
    (root/'main.void').write_text('func main() -> i32:\n    var enabled = true\n    var large: u64 = 4294967296\n    var fraction: f64 = 1.5\n    var tiny: i8 = -7\n    var wide: i64 = -42\n    return fraction as i32\n')
    run([compiler, 'build', '--project', str(root), '--profile', 'debug', '--no-opt', '--debug-info', 'full', '-o', str(exe)])
    script.write_text('settings set target.disable-aslr false\nbreakpoint set -f main.void -l 7\nrun\nframe variable\nexpression -- enabled\nexpression -- large\nexpression -- fraction\nexpression -- (int)tiny\nexpression -- wide\ncontinue\n')
    scalars = run([lldb, '--batch', '--no-lldbinit', '-s', str(script), str(exe)])
    print(scalars)
    for value in ['enabled = true', 'large = 4294967296', 'fraction = 1.5', 'wide = -42']:
        assert value in scalars, scalars
    assert '$3 = -7' in scalars and 'exited with status = 1' in scalars, scalars
    print('PASS: bool/u64/f64/i8/i64 locals and native expression evaluation')
