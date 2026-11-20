"""G80 source contracts, successful JIT/AOT O0/O2 and required rejections."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, evidence = (Path(p).resolve() for p in sys.argv[1:])
fixtures = Path(__file__).resolve().parent
records = []
def run(args, expected=0, output=None, error=None):
    command = list(map(str, args))
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    row = dict(command=command, expected=expected, actual=result.returncode, stdout=result.stdout, stderr=result.stderr)
    records.append(row); evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, row
    if output is not None: assert result.stdout == output, row
    if error is not None: assert error in result.stderr, row
    return result
with tempfile.TemporaryDirectory(prefix='VOID G80 ') as directory:
    root = Path(directory)
    def write(name, text):
        file = root / name; file.write_text(text); return file
    def execute(file, output, status=0, error=None):
        run([compiler, 'check', file])
        for switches in [[], ['--no-opt']]:
            run([compiler, 'run', file, '--no-cache', *switches], status, output, error)
            target = root / ('program' + str(len(records)))
            run([compiler, 'build', file, '-o', target, *switches])
            run([target], status, output, error)
    for name, output in [('values', 'static values preserved\n'), ('generics', 'static specializations preserved\n'), ('ownership', 'static roots preserved\n')]:
        execute(fixtures / (name + '.void'), output)
    execute(fixtures.parents[1] / 'examples/learn/static_members.void', 'VOID 4 0 0\n7 Boss\n')
    run([compiler, 'check', fixtures / 'async.void'])
    execute(write('order.void', 'var first = seed("before")\nstruct state:\n    static middle = seed("static")\nvar last = seed("after")\nfunc seed(label: string) -> i32:\n    print(label)\n    return 1\nfunc main():\n    print("entry")\n'), 'before\nstatic\nafter\nentry\n')
    write('library.void', 'struct counter:\n    static value = 1\nstatic func counter.bump():\n    counter.value += 1\n')
    execute(write('imports.void', 'use "./library.void" as a\nuse "./library.void" as b\nfunc main():\n    a.counter.bump()\n    b.counter.value += 3\n    print(a.counter.value, b.counter.value)\n'), '5 5\n')
    execute(write('early.void', 'struct state:\n    static first: i32 = state.read()\n    static later = 7\nstatic func state.read() -> i32:\n    return state.later\nfunc main():\n    print("entry")\n'), '', 1, 'not initialized')
    negatives = {
        'foreign_owner': ('use "./library.void"\nstatic func counter.fake():\n    pass\n', 'owning module'),
        'invented_method_types': ('value struct box<t>:\n    static count = 1\nstatic func box<t>.read() -> i32:\n    return box<t>.count\nfunc main():\n    print(box<i32>.read<string>())\n', 'not generic'),
        'unused_generic_conflict': ('value struct box<t>:\n    item: t\nstatic func box<t>.same():\n    pass\nfunc (self box<t>) same():\n    pass\n', 'share a name'),
        'instance_static': ('struct item:\n    static count = 0\nfunc main():\n    var value = item()\n    print(value.count)\n', 'VOID'),
        'type_instance': ('struct item:\n    count: i32\nfunc main():\n    print(item.count)\n', 'static member'),
        'private': ('struct item:\n    private static count = 0\nfunc main():\n    print(item.count)\n', 'not accessible'),
        'private_method': ('struct item:\n    value: i32\nprivate static func item.read() -> i32:\n    return 7\nfunc main():\n    print(item.read())\n', 'not accessible'),
        'readonly': ('struct item:\n    static const count = 1\nfunc main():\n    item.count = 2\n', 'static const'),
        'readonly_deep': ('struct node:\n    value: i32\nstruct item:\n    static const shared = node(value = 1)\nfunc main():\n    item.shared.value = 2\n', 'readonly'),
        'duplicate': ('struct item:\n    static count = 1\n    count: i32\n', 'conflicts'),
        'method_field': ('struct item:\n    static count = 1\nstatic func item.count():\n    pass\n', 'Duplicate'),
        'no_initializer': ('struct node:\n    value: i32\nstruct item:\n    static shared: node\n', 'initializer'),
        'constant': ('struct item:\n    static count = 1\nconst value = item.count\n', 'compile-time'),
        'default': ('struct item:\n    static count = 1\nfunc read(value: i32 = item.count):\n    pass\n', 'compile-time'),
        'invented_types': ('struct item:\n    static count = 1\nfunc main():\n    print(item<i32>.count)\n', 'generic'),
        'owner': ('use "./library.void" as a\nstatic func a.counter.fake():\n    pass\n', 'VOID'),
        'lifecycle': ('struct item:\n    value: i32\nstatic func item.init():\n    pass\n', 'lifecycle'),
        'worker': ('struct item:\n    static count = 1\nstatic func item.read() -> i32:\n    return item.count\nasync func main():\n    var result = spawn item.read()\n    print(await result)\n', 'Worker'),
    }
    for name, (source, error) in negatives.items():
        run([compiler, 'check', write(name + '.void', source)], 1, error=error)
print(f'G80 static members: {len(records)} checks; JIT/AOT O0/O2, generics, ownership, visibility, startup')
