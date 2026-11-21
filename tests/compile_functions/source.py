"""G93 real CLI: required constants, runtime parity, ownership and finite work."""
from pathlib import Path
import json, os, platform, subprocess, sys, tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from test_output import output_directory
compiler, report = (Path(p).resolve() for p in sys.argv[1:])
records = []
def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
def run(args, expected=0, output=None, contains=None):
    p = subprocess.run(list(map(str, args)), capture_output=True, timeout=90,
                       preexec_fn=stack if os.name == 'posix' else None)
    row = dict(argv=list(map(str, args)), status=p.returncode, stdout=p.stdout.decode(), stderr=p.stderr.decode())
    row['sources'] = {str(p): p.read_text() for p in map(Path, map(str, args)) if p.suffix == '.void' and p.is_file()}
    records.append(row); report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576, records=records), indent=2) + '\n')
    assert p.returncode == expected, row
    if output is not None: assert row['stdout'] == output, row
    if contains: assert contains.lower() in row['stderr'].lower(), row
    return row
negative = {
    'ordinary': ('func f() -> i32:\n    return 7\nconst result = f()\n', 'explicitly marked'),
    'ordinary_nested': ('func f() -> i32:\n    return 7\ncompile func g() -> i32:\n    return f()\n', 'explicitly marked'),
    'io_unused': ('compile func f():\n    print("forbidden")\n', 'runtime-only'),
    'io_unreached': ('compile func f() -> i32:\n    if false:\n        print("forbidden")\n    return 2\n', 'runtime-only'),
    'global_read': ('var state = 1\ncompile func f() -> i32:\n    return state\n', 'runtime-only'),
    'global_write': ('var state = 1\ncompile func f():\n    state = 3\n', 'external state'),
    'managed': ('struct cell:\n    value: i32\ncompile func f() -> cell:\n    return cell(value = 2)\n', 'Runtime identity'),
    'receiver_write': ('value struct cell:\n    value: i32\ncompile func (c cell) change() -> i32:\n    c.value += 1\n    return c.value\n', 'readonly'),
    'list_write': ('compile func f(items: [i32]):\n    items[0] = 3\n', 'fixed-array'),
    'list_push': ('compile func f(items: [i32]):\n    items.push(3)\n', 'runtime-only'),
    'nominal_interpolation': ('value struct box:\n    item: i32\nfunc (b box) string() -> string:\n    return "shown"\ncompile func render() -> string:\n    return "value {box(item = 1)}"\nconst folded = render()\n', 'runtime-only'),
    'dictionary_lookup': ('compile func lookup(mapping: {i32: i32}) -> i32:\n    return mapping[0]\nconst answer = lookup({1: 7, 0: 9})\n', 'dictionary lookup'),
    'enum_custom_equality': ('value struct item:\n    number: i32\n    operator ==(const left: item, const right: item) -> bool:\n        return left.number == right.number\nenum reading:\n    present(value: item)\ncompile func compare(left: reading, right: reading) -> bool:\n    return left == right\n', 'custom equality'),
    'collection_custom_equality': ('value struct item:\n    number: i32\n    operator ==(const left: item, const right: item) -> bool:\n        return left.number == right.number\ncompile func compare(left: [item], right: [item]) -> bool:\n    return left == right\n', 'custom equality'),
    'deferred': ('compile func f():\n    defer print("bad")\n', 'deferred'),
    'async': ('compile async func f():\n    pass\n', 'Expected compile if'),
    'compile_const': ('compile const answer = 2\n', 'Expected compile if'),
    'compile_var': ('compile var answer = 2\n', 'Expected compile if'),
    'compile_struct': ('compile value struct cell:\n    value: i32\n', 'Expected compile if'),
    'cycle': ('compile func f() -> i32:\n    return answer\nconst answer = f()\n', 'Circular'),
    'default_cycle': ('compile func f(value: i32 = answer) -> i32:\n    return value\nconst answer = f()\n', 'Circular'),
    'enum_cycle': ('compile func f() -> i32:\n    return code.answer.value\nenum code(i32):\n    answer = f()\n', 'Circular'),
    'zero': ('compile func divide(value: i32) -> i32:\n    return 8 / value\nconst answer = divide(0)\n', 'zero'),
    'argument': ('compile func f(value: i32) -> i32:\n    return value\nconst answer = f("wrong")\n', 'Expected i32'),
    'missing_argument': ('compile func f(value: i32) -> i32:\n    return value\nconst answer = f()\n', 'missing required argument'),
    'invalid_conversion': ('compile func f(value: i32) -> i8:\n    return value as i8\nconst answer = f(128)\n', 'conversion'),
    'bounds_read': ('compile func f(index: i32) -> i32:\n    const values: [i32, 2] = [1, 2]\n    return values[index]\nconst answer = f(-3)\n', 'out of bounds'),
    'bounds_write': ('compile func f(index: i32) -> i32:\n    var values: [i32, 2] = [1, 2]\n    values[index] = 3\n    return values[0]\nconst answer = f(2)\n', 'out of bounds'),
    'bounds_iterable': ('compile func f(index: i32) -> i32:\n    var rows: [[i32, 2], 2] = [[1, 2], [3, 4]]\n    for item in rows[index]:\n        return item\n    return 0\nconst answer = f(-3)\n', 'out of bounds'),
    'recursion': ('compile func f(value: i32) -> i32:\n    return f(value + 1)\nconst answer = f(0)\n', 'limit'),
    'specialization': ('compile func expand<t>(value: t) -> i32:\n    return expand<[t]>([value])\nconst answer = expand<i32>(1)\n', 'limit'),
    'work': ('compile func f() -> i32:\n    for:\n        pass\n    return 0\nconst answer = f()\n', 'work budget'),
    'memory': ('compile func f() -> string:\n    var text = "x"\n    for:\n        text += text\n    return text\nconst answer = f()\n', 'budget'),
    'private': ('use "./owner.void" as owner\nconst answer = owner.hidden(3)\n', 'private'),
    'duplicate_discriminant': ('compile func f() -> i32:\n    return 7\nenum code(i32):\n    a = f()\n    b = f()\n', 'Duplicate numeric'),
}
with tempfile.TemporaryDirectory(prefix='g93-source-', dir=output_directory()) as directory:
    root = Path(directory); source = root / 'main.void'
    (root / 'owner.void').write_text('public compile func calculate(value: i32) -> i32:\n    return value * 3\nprivate compile func hidden(value: i32) -> i32:\n    return value\n')
    body = Path(__file__).with_name('calculations.void').read_text()
    # Same declarations execute with runtime inputs; compare whole independent outputs.
    runtime = '''func runtime_probe() -> i32:
    var number = 7
    var element_values: [i32, 3] = [1, 2, 3]
    var rectangle = dimensions(width = 20, height = 10)
    print(square(number + 1), rectangle.area(), factorial(number), sum_to(number + 3), reverse_sum(), identity<i64>(42), box<i32>(value = 17).read(), box<i32>(value = 17).echo<string>("echo"))
    print(checked_divide(right = 3, left = 21), checked_divide(20), unpack(5), greeting("VOID"), array_total(element_values), 2, 2, 1)
    print(shifted(3, -1), wrapped(127), platform_name(), 64, 3, 4)
    print(defaults().value, default_parameter(), square(number + 1), rectangle.area(), factorial(number))
    return 0
'''
    # Select either main without a textual macro in the compiler.
    constant_body = body
    runtime_body = body.replace('func main() -> i32:', 'func constant_probe() -> i32:') + runtime.replace('func runtime_probe()', 'func main()')
    expected = '64 200 5040 33 2345 42 17 echo\n7 10 11 hello VOID 13 2 2 1\n-128 -128 ' + ('Linux' if platform.system() == 'Linux' else 'Other') + ' 64 3 4\n25 36 64 200 5040\n'
    for label, text in [('constant', constant_body), ('runtime', runtime_body)]:
        source.write_text(text)
        run([compiler, 'check', source])
        for mode, flags in [('O0', ['--no-opt']), ('O2', [])]:
            run([compiler, 'run', source, '--no-cache', *flags], output=expected)
            exe = root / (label + mode + ('.exe' if os.name == 'nt' else ''))
            run([compiler, 'build', source, '-o', exe, *flags])
            run([exe], output=expected)
    for name, (text, reason) in negative.items():
        source.write_text(text); row = run([compiler, 'check', source], 1, contains=reason)
        assert '^' in row['stderr'] and ': error:' in row['stderr'], row
        if name == 'zero':
            assert ':2:14:' in row['stderr'] and 'While evaluating compile func' in row['stderr'], row
    # Fixed-array iteration captures computed values once, freezes selected
    # indexes, and refreshes live fixed storage without retaining stale children.
    source.write_text(Path(__file__).with_name('corrections.void').read_text())
    expected_capture = '3 3 21 21\n21 21 21 21 21 21\n3 3 10 10\n3 3 21 21 3 3\n'
    for flags in (['--no-opt'], []):
        run([compiler, 'run', source, '--no-cache', *flags], output=expected_capture)
        exe = root / ('capture-' + str(len(flags)) + ('.exe' if os.name == 'nt' else ''))
        run([compiler, 'build', source, '-o', exe, *flags]);run([exe], output=expected_capture)
    # Compose nesting with recursion instead of checking either limit alone.
    def nested_recursion(depth, count, control='if true'):
        return ('compile func calculate(n: i32) -> i32:\n' +
                ''.join('    ' * (i + 1) + control + ':\n' for i in range(depth)) +
                '    ' * (depth + 1) + 'if n == 0:\n' +
                '    ' * (depth + 2) + 'return 0\n' +
                '    ' * (depth + 1) + 'return calculate(n - 1)\n' +
                '    return 0\nconst answer = calculate(' + str(count) + ')\n')
    for control in ('if true', 'for true'):
        for depth in (12, 24):
            source.write_text(nested_recursion(depth, 1, control))
            run([compiler, 'check', source])
            source.write_text(nested_recursion(depth, 60, control))
            row = run([compiler, 'check', source], 1, contains='combined expression/statement nesting limit')
            assert 'VOID9001' in row['stderr'] and '^' in row['stderr'] and 'While evaluating compile func' in row['stderr'], row
    source.write_text(nested_recursion(1, 63))
    run([compiler, 'check', source])
    source.write_text(nested_recursion(1, 64))
    run([compiler, 'check', source], 1, contains='recursive call depth limit of 64')
    source.write_text('compile func size() -> i32:\n    return 3\nconst values: [i32, size()] = [1, 2, 3]\nfunc main():\n    print(values.count)\n')
    run([compiler, 'run', source, '--no-cache'], output='3\n')
    source.write_text('compile func count_to(limit: i32) -> i32:\n    for index in 1..limit:\n        pass\n    return limit\n' + ''.join('const result_' + str(n) + ' = count_to(60000)\n' for n in range(75)))
    run([compiler, 'check', source], 1, contains='8388608 per analysis')
    source.write_text('use "./owner.void" as owner\nconst answer = owner.calculate(14)\nfunc main():\n    print(answer)\n')
    run([compiler, 'run', source, '--no-cache'], output='42\n')
    source.write_text('compile func float_value(input: f32) -> f32:\n    return (input * 0.1 + 0.2) / 2.0\ncompile func ieee_zero(input: f32) -> f32:\n    return input / input\ncompile func describe(value: i32) -> string:\n    return "value {value}"\ncompile func same_list(sequence: [i32]) -> bool:\n    return sequence == [1, 2]\nenum reading<t>:\n    empty\n    present(value: t)\ncompile func make_reading<t>(input: t) -> reading<t>:\n    return reading<t>.present(input)\nconst quarter = float_value(3.0)\nconst nan = ieee_zero(0.0)\nconst equal = same_list([1, 2])\nconst described = describe(7)\nconst sample = make_reading<i32>(7)\nfunc main():\n    var input: f32 = 3.0\n    print(quarter == float_value(input), nan != nan, equal, same_list([1, 2]), sample is reading.present(7), described == describe(7))\n')
    for flags in (['--no-opt'], []):
        run([compiler, 'run', source, '--no-cache', *flags], output='true true true true true true\n')
        exe = root / ('float-' + str(len(flags)) + ('.exe' if os.name == 'nt' else ''))
        run([compiler, 'build', source, '-o', exe, *flags]);run([exe], output='true true true true true true\n')
    source.write_text('compile func to_code(value: i32) -> code:\n    return value as code\nenum other(i32):\n    first = to_code(7).value\nenum code(i32):\n    first = 7\nfunc main():\n    print(other.first.value)\n')
    run([compiler, 'run', source, '--no-cache'], output='7\n')
    source.write_text('enum code<t>(i32):\n    first = 7\nfunc cast_code<t>(value: i32) -> code<t>:\n    return value as code<t>\nfunc main():\n    var result = cast_code<i32>(7)\n    print(result.value)\n')
    for flags in (['--no-opt'], []):
        run([compiler, 'run', source, '--no-cache', *flags], output='7\n')
        exe = root / ('enum-' + str(len(flags)) + ('.exe' if os.name == 'nt' else ''))
        run([compiler, 'build', source, '-o', exe, *flags]);run([exe], output='7\n')
    source.write_text(Path(__file__).with_name('extents.void').read_text())
    run([compiler, 'run', source, '--no-cache'], output='4 4\n')
    source.write_text('value struct point:\n    x: i32\nfunc (p point) init(x: i32 = 3):\n    p.x = x\nvalue struct holder:\n    child: point\nfunc main():\n    print(holder().child.x)\n')
    run([compiler, 'run', source, '--no-cache'], output='3\n')
    # A locked local package retains its own conditions and source owner.
    project = root / 'consumer'; project.mkdir(); package = project / 'deps/api'; package.mkdir(parents=True)
    (package / 'api.void').write_text('compile if feature.extra:\n    public compile func calculate(value: i32) -> i32:\n        return value * 3\ncompile else:\n    public compile func calculate(value: i32) -> i32:\n        return value * 4\n')
    manifest = "schema=1\n[project]\nid='org.test.g93project'\nname='G93'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.g93.app'\n"
    (project / 'void.project.toml').write_text(manifest + "[build.conditions]\nextra=false\n[dependencies.api]\nid='org.test.g93package'\npath='deps/api'\nversion='=1.0.0'\n")
    (package / 'void.project.toml').write_text("schema=1\n[project]\nid='org.test.g93packageproject'\nname='G93Package'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.test.g93package'\nname='api'\nentry='api.void'\n[build.conditions]\nextra=true\n")
    (project / 'main.void').write_text('use pkg/api as api\nconst answer = api.calculate(14)\nfunc main():\n    print(answer)\n')
    run([compiler, 'resolve', '--project', project])
    run([compiler, 'check', '--project', project])
    run([compiler, 'run', '--project', project, '--no-cache'], output='42\n')
    # No implicit execution of ordinary calls or startup effects when checking.
    source.write_text('func never() -> i32:\n    print("must not execute")\n    for:\n        pass\n    return 0\nfunc main():\n    print(never())\n')
    run([compiler, 'check', source], output='Source checked successfully\n')
    example = Path(__file__).resolve().parents[2] / 'examples/compile_functions/main.void'
    run([compiler, 'run', example, '--no-cache'], output='64 200 5050 81\n')
print('G93 source:', len(records), 'process checks, JIT/AOT O0/O2, 1 MiB stack')
