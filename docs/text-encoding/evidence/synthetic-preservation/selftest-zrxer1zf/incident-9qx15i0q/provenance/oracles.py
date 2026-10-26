"""Independent Python reference cases; generates persistent, deterministic source."""
from pathlib import Path
import base64
import random
import sys
import unicodedata


def literal(value):
    if isinstance(value, str):
        return '"' + ''.join('\\u{' + format(ord(c), 'x') + '}' for c in value) + '"'
    if isinstance(value, bool):
        return 'true' if value else 'false'
    if isinstance(value, (bytes, list)):
        return '[' + ', '.join(map(literal, value)) + ']'
    return str(value)


def source():
    groups = []
    cases = []

    def direct(expression, expected):
        cases.append([f'    if {expression} != {literal(expected)}:', '        return 1'])

    def result(expression, expected, array=False):
        comparison = f'not same(observed, {literal(expected)})' if array else f'observed != {literal(expected)}'
        cases.append([f'    if {expression} is Ok(var observed):', f'        if {comparison}:',
                      '            return 2', '    else:', '        return 3'])

    def invalid(expression):
        cases.append([f'    if {expression} is Ok(var unexpected):', '        return 4'])

    rng = random.Random(4648)
    for size in [0, 1, 2, 3, 4, 5, 31, 32, 33, 255, 256, 257]:
        raw = bytes(range(256)) if size == 256 else bytes(rng.randrange(256) for _ in range(size))
        result(f'hex.encode({literal(raw)})', raw.hex())
        result(f'hex.encode({literal(raw)}, true)', raw.hex().upper())
        result(f'hex.decode({literal(raw.hex().upper())})', raw, True)
        for url in (False, True):
            for padded in (True, False):
                encoded = (base64.urlsafe_b64encode if url else base64.b64encode)(raw).decode()
                if not padded:
                    encoded = encoded.rstrip('=')
                options = ('base64.alphabet.url_safe' if url else 'base64.alphabet.standard')
                options += ', ' + ('base64.padding.required' if padded else 'base64.padding.omitted')
                result(f'base64.encode({literal(raw)}, {options})', encoded)
                result(f'base64.decode({literal(encoded)}, {options})', raw, True)
    for encoded in ['A', 'AA', 'AAA', 'AA===', 'A===', '====', 'A=AA', 'AA=A', 'AB==', 'AAB=',
                    'AA== ', 'AA==\0', 'AA\nA', 'AA\tA', 'AA雪', '--__']:
        invalid(f'base64.decode({literal(encoded)})')
    for encoded in ['A', 'AB', 'AAB', 'AA==', 'AA=', 'AA A', '+/8', 'AA\0']:
        invalid(f'base64.decode({literal(encoded)}, base64.alphabet.url_safe, base64.padding.omitted)')
    for encoded in ['0', '000', '0x', '0 ', '\0a', 'é', '雪0', 'GG', '0g']:
        invalid(f'hex.decode({literal(encoded)})')

    samples = ['', 'a', '\0', '雪😀e\u0301雪', 'aaaaa', ' x \r\n', 'ßİΣ𐐀', 'abababa']
    for value in samples:
        for needle in ['', 'a', '\0', '雪', '😀', 'aa', 'aba', 'missing']:
            call = literal(value) + ', ' + literal(needle)
            direct(f'text.index_of({call})', value.find(needle))
            direct(f'text.last_index_of({call})', value.rfind(needle))
            direct(f'text.contains({call})', needle in value)
            direct(f'text.starts_with({call})', value.startswith(needle))
            direct(f'text.ends_with({call})', value.endswith(needle))
            if needle:
                result(f'text.split({call})', value.split(needle), True)
                result(f'text.replace({call}, "X")', value.replace(needle, 'X'))
                result(f'text.replace_first({call}, "X")', value.replace(needle, 'X', 1))
        for start in [0, len(value), -len(value)]:
            result(f'text.substring({literal(value)}, {start})', value[start:])
        result(f'text.repeat({literal(value)}, 3)', value * 3)
        result(f'text.pad_left({literal(value)}, {len(value) + 2}, "😀")', '😀😀' + value)
        result(f'text.pad_right({literal(value)}, {len(value) + 2}, "雪")', value + '雪雪')
    for value, expected in [('', ['']), ('\n', ['', '']), ('\r\n', ['', '']),
                            ('a\r\nb\rc\n', ['a', 'b', 'c', '']), ('\r\r\n\n', ['', '', '', ''])]:
        result(f'text.split_lines({literal(value)})', expected, True)
    spaces = [*range(9, 14), 32, 133, 160, 5760, *range(8192, 8203), 8232, 8233, 8239, 8287, 12288]
    for scalar in spaces:
        value = chr(scalar)
        direct(f'text.trim({literal(value + "雪" + value)})', '雪')
        direct(f'text.is_blank({literal(value)})', True)
    for value in ['\0', '\u001c', '\u200b', '\ufeff', 'e\u0301']:
        direct(f'text.is_blank({literal(value)})', False)
        direct(f'text.trim({literal(value)})', value)
    for call in ['substring("x", -2)', 'substring("x", 2)', 'substring("x", 0, -2)',
                 'substring("x", 0, 2)', 'substring("x", 9223372036854775807)',
                 'split("x", "")', 'split("x", "x", 1)', 'split_lines("x\\n", 1)',
                 'split("", "x", 0)', 'replace("x", "", "x")', 'replace_first("x", "", "x")',
                 'repeat("x", -1)', 'repeat("雪", 9223372036854775807)',
                 'pad_left("x", -1)', 'pad_left("x", 1, "")', 'pad_right("x", 1, "ab")',
                 'pad_right("x", 9223372036854775807)']:
        invalid('text.' + call)
    result('text.join([], "x")', '')
    result('text.join(["", "雪", ""], "😀")', '😀雪😀')
    result('text.substring("雪", 1, 0)', '')
    result('text.repeat("", 9223372036854775807)', '')

    assert unicodedata.unidata_version == '16.0.0'
    changed = [chr(cp) for cp in range(0x110000)
               if chr(cp).lower() != chr(cp) or chr(cp).upper() != chr(cp)]
    for offset in range(0, len(changed), 128):
        value = '\0'.join(changed[offset:offset + 128])
        direct(f'text.to_lower({literal(value)})', value.lower())
        direct(f'text.to_upper({literal(value)})', value.upper())
    for value in ['ΟΣ', 'ΟΣΑ', 'Σ', 'AΣ\u0345', 'AΣ\u0345Α', '\u0345Σ',
                  'A\u0345Σ', 'AΣ\u0301', 'AΣ\u0301A', 'İßﬃᾀ', '𐐀\0𐐨']:
        direct(f'text.to_lower({literal(value)})', value.lower())
        direct(f'text.to_upper({literal(value)})', value.upper())

    lines = ['use text', 'use encoding/hex', 'use encoding/base64', 'use encoding/ascii', '',
             'private func same<t>(const left: [t], const right: [t]) -> bool:',
             '    if left.count != right.count:', '        return false',
             '    for index in 0..<left.count:', '        if left[index] != right[index]:',
             '            return false', '    return true', '']
    for offset in range(0, len(cases), 24):
        name = f'group_{offset // 24}'
        groups.append(name)
        lines.append(f'private func {name}() -> i32:')
        for case in cases[offset:offset + 24]:
            lines.extend(case)
            lines.append('')
        lines.extend(['    return 0', ''])
    lines.append('func main() -> i32:')
    for index, name in enumerate(groups):
        lines.extend([f'    if {name}() != 0:', f'        return {index + 1}', ''])
    lines.extend(['    return 0', ''])
    return '\n'.join(lines), len(cases)


if __name__ == '__main__':
    result, count = source()
    Path(sys.argv[1]).write_text(result)
    print(f'{count} independently expected oracle cases generated')
