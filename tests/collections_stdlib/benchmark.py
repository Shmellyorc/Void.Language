"""Reuse the existing generic channel benchmark host; generate collection workloads."""
from pathlib import Path
import json
import sys
import tempfile
import support as h
host = Path(sys.argv[1]).resolve()
target = Path(tempfile.mkdtemp(prefix='benchmark-' + host.parent.name + '-', dir=h.EVIDENCE))
common = '''use collections as c
use console as con
func require(ok: bool):
    if not ok:
        var zero = 0
        var failed = 1 / zero
var callbacks = 0
'''
results = []
def measure(name, declarations, setup, operation, iterations):
    source = target / (name + '.void')
    source.write_text(common + declarations + '\nfunc entry(iterations: i32) -> i32:\n    if iterations == 0:\n' + ''.join('        ' + line + '\n' for line in setup.splitlines()) + '        return 0\n    callbacks = 0\n    var checksum = 0\n    for repetition in 0..<iterations:\n' + ''.join('        ' + line + '\n' for line in operation.splitlines()) + '        checksum += 7\n    print(callbacks, file = con.stderr)\n    return checksum\n')
    result = h.run([host, source, str(iterations), '7', '3'], cwd=target, sources=[source], timeout=900,
                   context={'mode': 'benchmark', 'scenario': name})
    try:
        record = json.loads(result.stdout)
        record.update(scenario=name, source=str(source), source_sha256=h.crash.digest(source),
                      callback_totals=list(map(int, result.stderr.splitlines())))
        assert len(record['samples']) == 3 and len(record['callback_totals']) == 4
    except (AssertionError, ValueError) as error:
        h.crash.preserve(result.args, target, result, source_inputs=[source], provenance_inputs=[Path(__file__)],
                         failure_kind='test_assertion', evidence_context={'benchmark_receipt':str(error)})
        raise
    results.append(record)
    (target / 'measurements.json').write_text(json.dumps({'host': str(host), 'host_sha256': h.crash.digest(host),
        'cache': 'disabled', 'note': 'Execution includes result checks and one callback-count stderr write per invocation; setup and compilation are separate. RSS is process-wide high water, not algorithm allocation.', 'measurements':results}, indent=2) + '\n')
    print(name, record['samples'][1]['wall'], flush=True)
for size in (0, 1, 32, 1024, 16384):
    for distribution in ('ordered', 'reverse', 'duplicates', 'mixed'):
        formula = {'ordered':'index', 'reverse':f'{size} - 1 - index', 'duplicates':'index % 5', 'mixed':'(index * 17 + 3) % 997'}[distribution]
        data = [index if distribution=='ordered' else size-1-index if distribution=='reverse' else index%5 if distribution=='duplicates' else (index*17+3)%997 for index in range(size)]
        checks = f'require(output.count == {size})'
        if size:
            checks += f'\nrequire(output[0] == {min(data)} and output[-1] == {max(data)})'
        declarations = 'var items: [i32] = []\nfunc less(const left: i32, const right: i32) -> bool:\n    callbacks += 1\n    return left < right\n'
        setup = f'for index in 0..<{size}:\n    items.push({formula})'
        measure(f'sort-{size}-{distribution}', declarations, setup, 'var output = c.sorted(items, less)\n' + checks, 20 if size<=32 else 2 if size<=1024 else 1)
size = 1024
setup = 'for index in 0..<1024:\n    items.push(index)'
declarations = 'var items: [i32] = []\nfunc less(const left: i32, const right: i32) -> bool:\n    callbacks += 1\n    return left < right\n'
measure('bounds-1024', declarations, setup, 'for needle in 0..<100:\n    require(c.lower_bound(items, needle, less) == (needle as i64))', 10)
for operation in ('filter', 'map', 'fold', 'filter-loop', 'map-loop', 'fold-loop'):
    if operation=='filter':
        code='var output = c.filter(items, func(const value: i32) -> bool:\n    callbacks += 1\n    return value % 2 == 0\n)\nrequire(output.count == 512 and output[-1] == 1022)'
    elif operation=='map':
        code='var output = c.map<i32, i32>(items, func(const value: i32) -> i32:\n    callbacks += 1\n    return value * 2\n)\nrequire(output.count == 1024 and output[-1] == 2046)'
    elif operation=='fold':
        code='var total = c.fold(items, 0, func(total: i32, const value: i32) -> i32:\n    callbacks += 1\n    return total + value\n)\nrequire(total == 523776)'
    elif operation=='filter-loop':
        code='var output: [i32] = []\nfor value in items:\n    if value % 2 == 0:\n        output.push(value)\nrequire(output.count == 512 and output[-1] == 1022)'
    elif operation=='map-loop':
        code='var output: [i32] = []\nfor value in items:\n    output.push(value * 2)\nrequire(output.count == 1024 and output[-1] == 2046)'
    else:
        code='var total = 0\nfor value in items:\n    total += value\nrequire(total == 523776)'
    measure(operation, declarations, setup, code, 5)
for operation in ('keys', 'values', 'copy_dictionary'):
    measure('dictionary-' + operation, 'var items: {i32: i32} = {}\n', 'for index in 0..<1024:\n    items[index] = index', f'var output = c.{operation}(items)\nrequire(output.count == 1024)', 3)
measure('sort-text', 'var items: [string] = []\nfunc less(const left: string, const right: string) -> bool:\n    callbacks += 1\n    return c.text_less(left, right)\n', 'for index in 0..<32:\n    items.push("common-雪-{31 - index:000}")', 'var output = c.sorted(items, less)\nrequire(output[0] == "common-雪-000" and output[-1] == "common-雪-031")', 3)
measure('sort-references', 'struct node:\n    key: i32\nvar items: [node] = []\nfunc less(const left: node, const right: node) -> bool:\n    callbacks += 1\n    return left.key < right.key\n', 'for index in 0..<128:\n    items.push(node(key = 127 - index))', 'var output = c.sorted(items, less)\nrequire(output[0].key == 0 and output[-1].key == 127)', 3)
measure('sort-fixed-256KiB', 'var items: [[i32, 65536]] = []\nfunc less(const left: [i32, 65536], const right: [i32, 65536]) -> bool:\n    callbacks += 1\n    return left[65535] < right[65535]\n', 'for index in 0..<4:\n    var item: [i32, 65536]\n    item[65535] = 3 - index\n    items.push(item)', 'var output = c.sorted(items, less)\nrequire(output[0][65535] == 0 and output[-1][65535] == 3)', 1)
print(target)
