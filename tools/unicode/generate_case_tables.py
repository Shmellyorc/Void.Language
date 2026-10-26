"""Regenerate pinned Unicode 16.0 casing; requires Python UCD 16 and Perl UCD 16.

Python supplies full unconditional mappings; Perl's generated inversion lists
supply Cased and Case_Ignorable for the locale-independent Final_Sigma rule.
This developer tool is not a build or runtime dependency.
"""
from pathlib import Path
import argparse
import hashlib
import json
import unicodedata

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--unicore', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
assert unicodedata.unidata_version == '16.0.0'
assert (args.unicore / 'version').read_text().strip() == '16.0.0'


def ranges(name):
    raw = (args.unicore / 'lib' / name / 'Y.pl').read_text()
    body = raw.split("return <<'END';\n", 1)[1].split('\nEND', 1)[0].splitlines()
    assert body[0].startswith('V')
    points = list(map(int, body[1:]))
    assert len(points) % 2 == 0
    return list(zip(points[::2], points[1::2]))


lines = ['// Generated Unicode 16.0.0 full default mappings. Do not edit by hand.',
         '// See tools/unicode/generate_case_tables.py and docs/text-encoding/evidence/unicode.json.',
         'struct Mapping {', '    std::uint32_t scalar;', '    std::string_view bytes;', '};', '',
         'struct Range {', '    std::uint32_t first;', '    std::uint32_t limit;', '};', '']
counts = {}
for operation in ('lower', 'upper'):
    lines.append(f'constexpr Mapping {operation}_mappings[] = {{')
    count = 0
    for scalar in range(0x110000):
        char = chr(scalar)
        mapped = getattr(char, operation)()
        if mapped != char:
            encoded = ''.join(f'\\{byte:03o}' for byte in mapped.encode('utf-8'))
            lines.append(f'    {{0x{scalar:X}, "{encoded}"}},')
            count += 1
    counts[operation] = count
    lines.extend(['};', ''])
for prop, name in (('Cased', 'cased_ranges'), ('CI', 'ignorable_ranges')):
    lines.append(f'constexpr Range {name}[] = {{')
    data = ranges(prop)
    lines.extend(f'    {{0x{first:X}, 0x{limit:X}}},' for first, limit in data)
    counts[prop] = len(data)
    lines.extend(['};', ''])
args.output.write_text('\n'.join(lines))
print(json.dumps({'unicode': '16.0.0', 'counts': counts, 'inputs': {
    name: hashlib.sha256((args.unicore / name).read_bytes()).hexdigest()
    for name in ('version', 'SpecialCasing.txt', 'lib/Cased/Y.pl', 'lib/CI/Y.pl')
}, 'output_sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()}, indent=2))
