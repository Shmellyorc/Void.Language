"""Deterministic image data, CLI rejection and independent LLVM inspection.

Never executes the ELF/PE/Mach-O images. Only the compiler and audit harness run.
"""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
import tempfile

auditor, compiler, readobj = map(lambda x: Path(x).resolve(), sys.argv[1:4])
evidence = Path(sys.argv[4]) if len(sys.argv) > 4 else None
rows = []
checks = 0


def check(value, message):
    global checks
    assert value, message
    checks += 1


def command(args, cwd=None):
    p = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, timeout=30)
    rows.append(dict(argv=list(map(str, args)), status=p.returncode,
                     stdout=p.stdout.decode(errors='replace'), stderr=p.stderr.decode(errors='replace')))
    return p


def put(b, offset, fmt, *values):
    struct.pack_into('<' + fmt, b, offset, *values)


def elf():
    b = bytearray(0x1000)
    b[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    put(b, 16, 'HHIQQQIHHHHHH', 2, 62, 1, 0, 64, 0x900, 0, 64, 56, 3, 64, 4, 1)
    put(b, 64, 'IIQQQQQQ', 1, 5, 0, 0x400000, 0x400000, len(b), len(b), 0x1000)
    interpreter = b'/lib64/ld-linux-x86-64.so.2\0'
    b[0x200:0x200+len(interpreter)] = interpreter
    put(b, 120, 'IIQQQQQQ', 3, 4, 0x200, 0x400200, 0, len(interpreter), len(interpreter), 1)
    strings = b'\0libc.so.6\0/producer/absolute/lib\0GLIBC_2.42\0'
    b[0x400:0x400+len(strings)] = strings
    entries = [(5, 0x400400), (10, len(strings)), (1, strings.index(b'libc')),
               (29, strings.index(b'/producer')), (0x6ffffffe, 0x400500), (0x6fffffff, 1), (0, 0)]
    for i, pair in enumerate(entries):
        put(b, 0x300+i*16, 'QQ', *pair)
    put(b, 176, 'IIQQQQQQ', 2, 4, 0x300, 0x400300, 0, len(entries)*16, len(entries)*16, 8)
    put(b, 0x500, 'HHIII', 1, 1, strings.index(b'libc'), 16, 0)
    put(b, 0x510, 'IHHII', 0, 0, 2, strings.index(b'GLIBC'), 0)
    names = b'\0.shstrtab\0.debug_info\0.symtab\0fixture\0'
    b[0x600:0x600+len(names)] = names
    for index, values in enumerate([
        (names.index(b'.shstrtab'), 3, 0, 0, 0x600, len(names), 0, 0, 1, 0),
        (names.index(b'.debug_info'), 1, 0, 0, 0x700, 1, 0, 0, 1, 0),
        (names.index(b'.symtab'), 2, 0, 0, 0x800, 48, 1, 1, 8, 24),
    ], 1):
        put(b, 0x900+index*64, 'IIQQQQIIQQ', *values)
    put(b, 0x818, 'IBBHQQ', names.index(b'fixture'), 0x12, 0, 2, 0x400700, 1)
    b[0xb00:0xb00+len(b'/canary/source/main.void\0')] = b'/canary/source/main.void\0'
    return bytes(b)


def pe():
    b = bytearray(0x600)
    b[:2] = b'MZ'
    put(b, 0x3c, 'I', 0x80)
    b[0x80:0x84] = b'PE\0\0'
    put(b, 0x84, 'HHIIIHH', 0x8664, 1, 0, 0, 0, 240, 0x22)
    optional = 0x98
    put(b, optional, 'H', 0x20b)
    put(b, optional+24, 'Q', 0x140000000)
    put(b, optional+32, 'II', 0x1000, 0x200)
    put(b, optional+56, 'II', 0x2000, 0x200)
    put(b, optional+68, 'H', 3)
    put(b, optional+108, 'I', 16)
    put(b, optional+120, 'II', 0x1000, 40)
    put(b, optional+112+6*8, 'II', 0x10a0, 28)
    b[0x188:0x190] = b'.rdata\0\0'
    put(b, 0x190, 'IIIIIIHHI', 0x400, 0x1000, 0x400, 0x200, 0, 0, 0, 0, 0x40000040)
    put(b, 0x200, 'IIIII', 0x1060, 0, 0, 0x1050, 0x1070)
    b[0x250:0x25c] = b'fixture.dll\0'
    put(b, 0x260, 'QQ', 0x1080, 0)
    put(b, 0x270, 'QQ', 0x1080, 0)
    b[0x280:0x28a] = b'\0\0fixture\0'
    codeview = b'RSDS' + bytes(16) + struct.pack('<I', 1) + b'C:\\canary\\source\\main.void\0'
    put(b, 0x2a0, 'IIHHIIII', 0, 0, 0, 0, 2, len(codeview), 0x10c0, 0x2c0)
    b[0x2c0:0x2c0+len(codeview)] = codeview
    return bytes(b)


def macho():
    def path_command(tag, value, minimum):
        raw = value.encode() + b'\0'
        size = (minimum+len(raw)+7)//8*8
        result = bytearray(size)
        put(result, 0, 'III', tag, size, minimum)
        result[minimum:minimum+len(raw)] = raw
        return result
    commands = [path_command(0xc, '@rpath/libfixture.dylib', 24),
                path_command(0x8000001c, '/producer/absolute/lib', 12)]
    segment = bytearray(152)
    put(segment, 0, 'II16sQQQQIIII', 0x19, 152, b'__DWARF', 0, 0x1000, 0x300, 0x100, 7, 1, 1, 0)
    put(segment, 72, '16s16sQQIIIIIIII', b'__debug_info', b'__DWARF', 0, 1, 0x300, 0, 0, 0, 0, 0, 0, 0)
    commands.append(segment)
    b = bytearray(0x400)
    put(b, 0, 'IIIIIIII', 0xfeedfacf, 0x100000c, 0, 2, len(commands), sum(map(len, commands)), 0, 0)
    offset = 32
    for c in commands:
        b[offset:offset+len(c)] = c
        offset += len(c)
    canary = '/canary/source/main.void'.encode('utf-16le') + b'\0\0'
    b[0x320:0x320+len(canary)] = canary
    return bytes(b)


try:
    with tempfile.TemporaryDirectory(prefix='void publish fixtures ') as directory:
        root = Path(directory)
        def audit(data, target='linux-x64', name='image'):
            path = root/name
            path.write_bytes(data)
            result = command([auditor, 'audit', target, path, '/canary/source/main.void', 'C:\\canary\\source\\main.void'])
            check(result.returncode == 0, rows[-1])
            return json.loads(result.stdout)

        for name, data, target, dependency in [('ELF', elf(), 'linux-x64', 'libc.so.6'),
                                              ('PE', pe(), 'windows-x64', 'fixture.dll'),
                                              ('Mach-O', macho(), 'macos-arm64', '@rpath/libfixture.dylib')]:
            report = audit(data, target, name)
            check(report['inspected'] and report['format'] == name, report)
            check(dependency in report['dependencies'], report)
            check(report['debug'], report)
            check(report['sha256'] == hashlib.sha256(data).hexdigest(), report)
            codes = [f['code'] for f in report['findings']]
            check('forbidden-string' in codes, report)
            if name != 'PE':
                check('loader-path' in codes and 'forbidden-string' in codes, report)
            if name == 'ELF':
                check(report['symbols'] and report['interpreter'].startswith('/lib64/'), report)
                check('libc.so.6:GLIBC_2.42' in report['versions'], report)
            oracle = command([readobj, '--file-headers', '--sections', root/name])
            check(oracle.returncode == 0 and name.encode() in oracle.stdout, rows[-1])
            wrong = audit(data, 'macos-arm64' if target != 'macos-arm64' else 'linux-x64')
            check(any(f['code'] == 'target-mismatch' for f in wrong['findings']), wrong)
            # Every header truncation is invalid; no inspected input is run.
            for length in range(1, 64):
                truncated = audit(data[:length], target)
                check(not truncated['inspected'], (name, length, truncated))
        damaged = bytearray(elf())
        put(damaged, 64+32, 'Q', 2**64-1)
        check(not audit(damaged)['inspected'], 'ELF overflow')
        damaged = bytearray(elf())
        put(damaged, 0x510+12, 'I', 16)
        check(not audit(damaged)['inspected'], 'ELF malformed version chain')
        for offset, fmt, value, label in [(0x200, 'B', 0, 'empty interpreter'),
                                           (0x300+5*16+8, 'Q', 0, 'zero version count'),
                                           (56, 'H', 0, 'missing program headers')]:
            damaged = bytearray(elf())
            put(damaged, offset, fmt, value)
            report = audit(damaged)
            check(not report['inspected'] and any(f['code'] == 'malformed-image' for f in report['findings']), label)
        # Sectionless dynamic requirements still inspect through program headers.
        stripped = bytearray(elf())
        put(stripped, 40, 'Q', 0)
        put(stripped, 60, 'HH', 0, 0)
        report = audit(stripped)
        check(report['inspected'] and report['versions'] == ['libc.so.6:GLIBC_2.42'], report)
        damaged = bytearray(pe())
        put(damaged, 0x200+12, 'I', 0xffffffff)
        check(not audit(damaged, 'windows-x64')['inspected'], 'PE invalid import name')
        damaged = bytearray(pe())
        damaged[0x250:] = b'A'*(len(damaged)-0x250)
        check(not audit(damaged, 'windows-x64')['inspected'], 'PE name must terminate inside the file-backed image')
        damaged = bytearray(pe())
        put(damaged, 0x80+16, 'I', 0xffffffff)
        check(not audit(damaged, 'windows-x64')['inspected'], 'PE inconsistent symbol declaration')
        crowded_symbols = bytearray(pe()) + bytearray(4097*18+4)
        put(crowded_symbols, 0x80+12, 'II', 0x600, 4097)
        put(crowded_symbols, 0x600+4097*18, 'I', 4)
        report = audit(crowded_symbols, 'windows-x64')
        check(not report['inspected'] and [f['code'] for f in report['findings']] == ['limit'],
              'PE symbol policy budget is incomplete inspection, not a malformed-image claim')
        for image, target, offset, fmt, value in [(elf(), 'linux-x64', 60, 'H', 4097),
                                                  (macho(), 'macos-arm64', 16, 'I', 4097)]:
            excessive = bytearray(image)
            put(excessive, offset, fmt, value)
            report = audit(excessive, target)
            check(not report['inspected'] and [f['code'] for f in report['findings']] == ['limit'],
                  'declared table budget must not claim malformed bytes')
        damaged = bytearray(pe())
        put(damaged, 0x80+12, 'I', 0x500)
        put(damaged, 0x500, 'I', 4)
        check(audit(damaged, 'windows-x64')['inspected'], 'PE string table can remain without COFF symbols')
        damaged = bytearray(pe())
        put(damaged, 0x98+120+4, 'I', 20)
        check(not audit(damaged, 'windows-x64')['inspected'], 'PE directory needs a bounded terminator')
        damaged = bytearray(pe())
        put(damaged, 0x98+112+13*8, 'II', 0x1100, 64)
        put(damaged, 0x300, 'IIIIIIII', 1, 0x1050, 0, 0, 0, 0, 0, 0)
        report = audit(damaged, 'windows-x64')
        check(report['inspected'] and report['dependencies'] == ['fixture.dll', 'fixture.dll'], 'bounded PE delay declarations')
        put(damaged, 0x300, 'I', 0)
        check(not audit(damaged, 'windows-x64')['inspected'], 'unsupported PE delay VA addressing fails closed')
        damaged = bytearray(macho())
        put(damaged, 36, 'I', 0xfffffff8)
        check(not audit(damaged, 'macos-arm64')['inspected'], 'Mach-O invalid command')
        damaged = bytearray(macho())
        commands, extent = struct.unpack_from('<II', damaged, 16)
        put(damaged, 32+extent, 'IIIIII', 2, 24, 0x380, 1, 0x390, 0x30)
        put(damaged, 16, 'II', commands+1, extent+24)
        put(damaged, 0x380, 'IBBHQ', 1, 0xe, 1, 0, 0)
        damaged[0x390:0x3c0] = b'\0'+b'A'*47
        check(not audit(damaged, 'macos-arm64')['inspected'], 'Mach-O symbol names cannot escape the declared string table')
        damaged[0x3bf] = 0
        check(audit(damaged, 'macos-arm64')['inspected'], 'valid bounded Mach-O symbol table')
        # Exceed the finding budget using bounded repeated loader declarations.
        crowded = bytearray(elf()) + bytearray(0x20000-0x1000)
        put(crowded, 64+32, 'QQ', len(crowded), len(crowded))
        entries = [(5, 0x400400), (10, len(b'\0libc.so.6\0/producer/absolute/lib\0GLIBC_2.42\0'))]
        entries += [(29, len(b'\0libc.so.6\0'))]*4093 + [(0, 0)]
        for index, pair in enumerate(entries):
            put(crowded, 0x1000+index*16, 'QQ', *pair)
        put(crowded, 176+8, 'Q', 0x1000)
        put(crowded, 176+32, 'QQ', len(entries)*16, len(entries)*16)
        report = audit(crowded, 'macos-arm64')
        check(not report['inspected'] and len(report['findings']) == 4096 and report['findings'][-1]['code'] == 'limit', 'finding exhaustion is incomplete inspection')
        # Fixed deterministic mutations must return a bounded report rather than
        # crash, hang or turn malformed data into execution authority.
        for label, original, target in [('elf', elf(), 'linux-x64'), ('pe', pe(), 'windows-x64'), ('macho', macho(), 'macos-arm64')]:
            for i in range(32):
                data = bytearray(original)
                offset = (i*37+17) % len(data)
                data[offset] ^= (1 << (i % 8))
                report = audit(data, target)
                check(report['scope'] == 'image-declarations-only' and len(report['findings']) <= 4096, (label, i, report))
        project = root/'project'
        project.mkdir()
        manifest = "schema=1\n[project]\nid='org.test.sources'\nname='Fixture'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n"
        toml = project/'void.project.toml'
        toml.write_text(manifest)
        (project/'main.void').write_text('func main() -> i32:\n    return 0\n')
        p = command([compiler, 'publish', '--project', project, '--entry', '/main.void'])
        check(p.returncode != 0 and b'exact project-relative' in p.stderr,
              'drive-rooted Windows entry must reject during argument parsing')
        for args in [[], ['--project', project], ['--project', project, '--target', 'linux-x64'],
                     ['--project', project, '--profile', 'performance'], ['--target', 'linux-arm64'],
                     ['--profile', 'debug'], ['--project', project, '--project', project],
                     ['--entry', '../main.void'], ['--allow-native'], ['main.void']]:
            before = sorted(p.relative_to(project).as_posix() for p in project.rglob('*'))
            out = root/'fresh output'
            p = command([compiler, 'publish', *args, '-o', out], cwd=project)
            check(p.returncode != 0 and not out.exists(), rows[-1])
            if not args or args[:2] == ['--project', project] and len(args) in (2, 4) and '--project' not in args[2:]:
                check(b'Publish capability unavailable' in p.stderr, rows[-1])
            check(before == sorted(p.relative_to(project).as_posix() for p in project.rglob('*')), 'rejection changed project')
        p = command([compiler, 'publish', '--project', project], cwd=project)
        check(p.returncode != 0 and not (project/'dist').exists(), rows[-1])
        existing = root/'existing'
        existing.mkdir()
        p = command([compiler, 'publish', '--project', project, '-o', existing])
        check(p.returncode != 0 and b'fresh directory' in p.stderr and not list(existing.iterdir()), rows[-1])
        toml.write_text(manifest.replace('schema=1', 'schema=2') + "[publish]\ntarget='windows-x64'\ndefault_profile='protected'\n")
        p = command([compiler, 'publish', '--project', project, '-o', root/'schema2'])
        check(p.returncode != 0 and b'windows-x64/protected' in p.stderr and not (root/'schema2').exists(), rows[-1])
        library = manifest.replace("kind='application'", "kind='library'").replace("[application]\nid='org.test.app'", "[package]\nid='org.test.lib'\nname='fixture'")
        toml.write_text(library)
        p = command([compiler, 'publish', '--project', project, '-o', root/'library'])
        check(p.returncode != 0 and b'libraries are ineligible' in p.stderr.lower() and not (root/'library').exists(), rows[-1])
        for content in [None, 'schema=2\n[invalid', manifest.replace('schema=1', 'schema=3'),
                        manifest.replace('schema=1', 'schema=2')+'[publish]\nlto="thin"\n']:
            if content is None:
                toml.unlink()
            else:
                toml.write_text(content)
            p = command([compiler, 'publish', '--project', project, '-o', root/'rejected'])
            check(p.returncode != 0 and not (root/'rejected').exists(), rows[-1])
        print(f'PASS {checks} deterministic publisher and binary fixture checks')
finally:
    if evidence:
        evidence.write_text(json.dumps(dict(checks=checks, rows=rows), indent=2))
