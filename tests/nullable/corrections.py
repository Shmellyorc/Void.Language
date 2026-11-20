"""G79 review regressions: owned constant extraction and source-facing presentation."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, evidence = (Path(p).resolve() for p in sys.argv[1:])
fixtures = Path(__file__).resolve().parent
records = []

def invoke(command, expected=0, output=None):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=90)
    records.append(dict(command=list(map(str, command)), expected=expected, actual=result.returncode,
                        stdout=result.stdout, stderr=result.stderr))
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, records[-1]
    if output is not None:
        assert result.stdout == output, records[-1]
    return result

with tempfile.TemporaryDirectory(prefix='VOID G79 review ') as directory:
    root = Path(directory)
    for name, output in [('review_constants', 'constants preserved\n'),
                         ('review_display', '0 null false\n[null, 0, 7]\n0|null|false||[]\n')]:
        source = fixtures / (name + '.void')
        invoke([compiler, 'check', source])
        for options in [[], ['--no-opt']]:
            invoke([compiler, 'run', source, '--no-cache', *options], output=output)
            target = root / (name + str(len(options)))
            invoke([compiler, 'build', source, '-o', target, *options])
            invoke([target], output=output)
    prefix = 'struct node:\n    value: i32\nvalue struct carrier:\n    child: node\n'
    sources = {
        'readonly_root': (prefix + 'func need(value: carrier?):\n    pass\nfunc main() -> i32:\n    const held: carrier? = carrier(child = node(value = 7))\n    need(held)\n    return 0\n', 'carrier?.child'),
        'readonly_nested': (prefix + 'value struct holder:\n    optional: carrier?\nfunc need(value: holder):\n    pass\nfunc main() -> i32:\n    const held = holder(optional = carrier(child = node(value = 7)))\n    need(held)\n    return 0\n', 'holder.optional.child'),
        'readonly_generic': ('struct node:\n    value: i32\nvalue struct box<t>:\n    child: t\nfunc need(value: box<node>?):\n    pass\nfunc main() -> i32:\n    const held: box<node>? = box<node>(child = node(value = 7))\n    need(held)\n    return 0\n', 'box<'),
    }
    for name, (source, logical) in sources.items():
        file = root / (name + '.void');file.write_text(source)
        result = invoke([compiler, 'check', file], 1)
        assert 'VOID2007' in result.stderr and logical in result.stderr, records[-1]
        assert all(hidden not in result.stderr for hidden in ['$nullable', '$value', '$array:']), records[-1]
print(f'G79 review corrections: {len(records)} checks; constants, presentation, readonly; JIT/AOT O0/O2')
