"""Independent Python sort/bisect oracles, with exact generated-source preservation."""
from pathlib import Path
import bisect
import os
import random
import sys
import tempfile
import support as h
compiler = Path(sys.argv[1]).resolve()
rng = random.Random(0xC011EC7)
header = '''use collections as c
func emit<t>(const input: [t]):
    var ordered = c.sorted(input, c.numeric_less<t>)
    var descending = c.sorted(input, c.numeric_less<t>, true)
    for item in ordered:
        print(item as i64, end = ",")
    print()
    for item in descending:
        print(item as i64, end = ",")
    print()
    for needle in 0..<5:
        print(c.lower_bound(ordered, needle as t, c.numeric_less<t>), c.upper_bound(ordered, needle as t, c.numeric_less<t>), sep = ",")
value struct tagged:
    key: i32
    serial: i32
func less(const left: tagged, const right: tagged) -> bool:
    return left.key < right.key
func main():
'''
lines = []; expected = bytearray()
for kind in ('i8', 'u8', 'i16', 'u16', 'i32', 'u32', 'i64', 'u64'):
    for size in (0, 1, 2, 3, 32, 127):
        data = [rng.randrange(0 if kind.startswith('u') else -8, 9) for _ in range(size)]
        lines.append(f'    emit<{kind}>([' + ','.join(map(str, data)) + '])\n')
        ascending = sorted(data)
        for result in (ascending, sorted(data, reverse=True)):
            expected.extend((''.join(str(item) + ',' for item in result) + '\n').encode())
        for needle in range(5):
            expected.extend(f'{bisect.bisect_left(ascending, needle)},{bisect.bisect_right(ascending, needle)}\n'.encode())
tagged = [(rng.randrange(5), serial) for serial in range(64)]
lines.append('    var tagged_items = [' + ','.join(f'tagged(key = {key}, serial = {serial})' for key, serial in tagged) + ']\n')
for direction in (False, True):
    lines += [f'    for item in c.sorted(tagged_items, less, {str(direction).lower()}):\n', '        print(item.serial, end = ",")\n', '    print()\n']
    expected.extend((''.join(str(serial) + ',' for key, serial in sorted(tagged, key=lambda item:item[0], reverse=direction)) + '\n').encode())
# Invalid comparators have bounded internal loops and preserve the copied values.
lines += ['    var invalid = c.sorted([3, 1, 2, 3], func(const left: i32, const right: i32) -> bool:\n', '        return true\n', '    )\n', '    for item in invalid:\n', '        print(item, end = ",")\n', '    print()\n', '    var flips = false\n', '    var inconsistent = c.sorted([3, 1, 2, 3], func(const left: i32, const right: i32) -> bool:\n', '        flips = not flips\n', '        return flips\n', '    )\n', '    for item in inconsistent:\n', '        print(item, end = ",")\n', '    print()\n']
with tempfile.TemporaryDirectory(prefix='VOID-collections-oracles-') as directory:
    work = Path(directory); source = work / 'oracle.void'; source.write_text(header + ''.join(lines))
    for switches in (['--no-opt'], []):
        result = h.run([compiler, 'run', source, '--no-cache', *switches], sources=[work], small=True)
        output = result.stdout.splitlines(keepends=True)
        try:
            assert b''.join(output[:-2]) == bytes(expected), 'Python stable-sort/bisect oracle mismatch'
            for row in output[-2:]:
                assert sorted(int(x) for x in row.strip().split(b',') if x) == [1, 2, 3, 3]
        except AssertionError as error:
            h.crash.preserve(result.args, h.ROOT, result, source_inputs=[work], provenance_inputs=[Path(__file__)],
                             failure_kind='test_assertion', evidence_context={'oracle': str(error)})
            raise
print('Collections independent oracles: 48 numeric distributions, 240 bounds and stable tagged duplicates; invalid comparators bounded')
