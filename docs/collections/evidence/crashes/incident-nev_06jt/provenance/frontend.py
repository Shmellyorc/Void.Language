"""Readonly/inference/key/guard negatives through the ordinary source checker."""
from pathlib import Path
import os
import re
import sys
import tempfile
import support as h
compiler = Path(sys.argv[1]).resolve()
base = '''use collections as c
struct node:
    number: i32
func keep(const item: node) -> bool:
    return true
func less(const left: node, const right: node) -> bool:
    return left.number < right.number
'''
cases = {
 'fixed': 'var input: [i32, 2] = [1, 2]\nvar output = c.sum(input)',
 'bool-sum': 'var output = c.sum([true])',
 'string-sum': 'var output = c.sum(["x"])',
 'bool-order': 'var output = c.numeric_less(true, false)',
 'string-order': 'var output = c.numeric_less("a", "b")',
 'list-equality': 'var output = c.contains([[1]], [1])',
 'dict-equality': 'var output = c.contains([{1: 2}], {1: 2})',
 'value-equality': 'var output = c.contains([packet(number = 1)], packet(number = 1))',
 'fixed-equality': 'var input: [[i32, 2]] = [[1, 2]]\nvar needle: [i32, 2] = [1, 2]\nvar output = c.contains(input, needle)',
 'bare-generic': 'var output = c.sorted([1, 2], c.numeric_less)',
 'untyped-lambda': 'var output = c.sorted([1, 2], func(a, b) -> bool:\n    return a < b\n)',
 'map-destination-only': 'var output: [i32] = c.map([1], func(const value: i32) -> i32:\n    return value\n)',
 'map-laundering': 'var output = c.map<node, node>([node()], func(const item: node) -> node:\n    return item\n)',
 'private-helper': 'var output = c._bound([1], 0, 1, c.numeric_less<i32>, false, false)',
 'private-version': 'var output = c._collections_version([1])',
 'wrong-callback': 'var output = c.sorted([1], func(const a: i64, const b: i64) -> bool:\n    return a < b\n)',
 'readonly-filter': 'var output = c.filter([node()], keep)\noutput[0].number = 9',
 'readonly-reverse': 'var output = c.reversed([node()])\noutput[0].number = 9',
 'readonly-sorted': 'var output = c.sorted([node()], less)\noutput[0].number = 9',
 'readonly-find': 'if c.find([node()], keep) is Ok(var found):\n    found.number = 9',
 'readonly-min': 'if c.min([node()], less) is Ok(var found):\n    found.number = 9',
 'readonly-max': 'if c.max([node()], less) is Ok(var found):\n    found.number = 9',
 'readonly-keys': 'var output = c.keys({node(): 1})\noutput[0].number = 9',
 'readonly-values': 'var output = c.values({"key": node()})\noutput[0].number = 9',
 'readonly-copy': 'var output = c.copy_dictionary({"key": node()})\noutput["key"].number = 9',
 'readonly-nested': 'var output = c.reversed([[node()]])\noutput[0].push(node())',
 'float-key': 'var output = c.copy_dictionary({1.0d: 1})',
 'custom-key': 'var output = c.copy_dictionary({custom(number = 1): 1})',
 'private-operator': 'var output = c.contains([hidden(number = 1)], hidden(number = 1))',
 'internal-operator': 'var output = c.contains([internal_eq(number = 1)], internal_eq(number = 1))',
}
base += '''value struct packet:
    number: i32
value struct custom:
    number: i32
    operator ==(const left: custom, const right: custom) -> bool:
        return left.number == right.number
value struct hidden:
    number: i32
    private operator ==(const left: hidden, const right: hidden) -> bool:
        return true
value struct internal_eq:
    number: i32
    internal operator ==(const left: internal_eq, const right: internal_eq) -> bool:
        return true
'''
def diagnostic(result, sources):
    if not re.search(rb'\.void:\d+:\d+: error:', result.stderr) or b'VOID IR:' in result.stderr:
        h.crash.preserve(result.args, h.ROOT, result, source_inputs=sources,
                         failure_kind='test_assertion', evidence_context={'expected': 'source-located diagnostic'})
        raise AssertionError(result.stderr)
with tempfile.TemporaryDirectory(prefix='VOID-collections-negative-') as directory:
    work = Path(directory)
    for name, body in cases.items():
        source = work / (name + '.void')
        source.write_text(base + 'func main():\n' + ''.join('    ' + line + '\n' for line in body.splitlines()))
        result = h.run([compiler, 'check', source], status=1, sources=[work], context={'mode': 'expected_source_diagnostic'})
        diagnostic(result, [work])
    # A project spelling never acquires canonical guard authority.
    source = work / 'collections.void'
    source.write_text('func main():\n    var token = _collections_version([1])\n')
    diagnostic(h.run([compiler, 'check', source], status=1), [work])
    source.write_text('func _collections_version(const items: [i32]) -> i32:\n    return 7\nfunc main():\n    print(_collections_version([1]))\n')
    h.run([compiler, 'run', source, '--no-cache'], expected=b'7\n')
    # Malformed canonical hooks are still type/arity checked, not a raw IR escape.
    library = work / 'stdlib'; library.mkdir()
    (library / 'error.void').write_bytes((h.ROOT / 'stdlib/error.void').read_bytes())
    main = work / 'guard.void'; main.write_text('use collections as c\nfunc main():\n    c.probe()\n')
    original = (h.ROOT / 'stdlib/collections.void').read_text()
    for body in ('_collections_version(1)', '_collections_version([1], 2)',
                 '_collections_check([1], true)', '_collections_check([1])',
                 '_collections_check({1: 2}, 0)', '_collections_version<i32>([1])'):
        (library / 'collections.void').write_text(original + '\nfunc probe():\n    ' + body + '\n')
        result = h.run([compiler, 'check', main], status=1, sources=[work],
                       env=dict(os.environ, VOID_STDLIB_ROOT=str(library)), context={'mode':'expected_guard_diagnostic'})
        diagnostic(result, [work])
print(f'Collections: {len(cases) + 7} source-located negative cases and ordinary same-name function passed')
