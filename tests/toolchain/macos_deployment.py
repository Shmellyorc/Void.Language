"""Inspect actual AOT/JIT Mach-O deployment commands, then link and execute."""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile


def minimum_versions(data):
    assert struct.unpack_from('<I', data)[0] == 0xfeedfacf, 'expected native Mach-O 64'
    count = struct.unpack_from('<I', data, 16)[0]
    offset, versions = 32, []
    for _ in range(count):
        command, size = struct.unpack_from('<II', data, offset)
        assert size >= 8 and offset + size <= len(data)
        if command == 0x32:  # LC_BUILD_VERSION
            platform, minimum = struct.unpack_from('<II', data, offset + 8)
            assert platform == 1, 'expected macOS platform'
            versions.append(minimum)
        elif command == 0x24:  # LC_VERSION_MIN_MACOSX
            versions.append(struct.unpack_from('<I', data, offset + 8)[0])
        offset += size
    assert versions, 'missing deployment load command'
    return versions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('minimum')
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    parts = [int(part) for part in args.minimum.split('.')]
    parts += [0] * (3 - len(parts))
    expected = (parts[0] << 16) | (parts[1] << 8) | parts[2]
    inspected = 0
    def inspect(data):
        nonlocal inspected
        assert minimum_versions(data) == [expected], minimum_versions(data)
        inspected += 1
    def run(argv, diagnostic=''):
        result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, (argv, result.stdout, result.stderr)
        assert result.stderr == diagnostic, (argv, result.stderr)
    with tempfile.TemporaryDirectory(prefix='VOID macOS deployment ') as directory:
        root = Path(directory)
        source = root / 'main.void'
        source.write_text('func main() -> i32:\n    return 0\n')
        for flags in ([], ['--no-opt']):
            obj = root / 'main.o'
            run([compiler, 'emit', source, '-o', obj, *flags]); inspect(obj.read_bytes())
            binary = root / 'main'
            run([compiler, 'build', source, '-o', binary, *flags]); inspect(binary.read_bytes())
            run([binary])
            run([compiler, 'run', source, '--no-cache', *flags])
        proof = root / 'proof.o'
        run([compiler, '--emit-proof', proof]); inspect(proof.read_bytes())
        run([compiler, 'run', source, '--cache'], 'Debug cache: miss\n')
        run([compiler, 'run', source, '--cache'], 'Debug cache: hit\n')
        artifacts = list((root / '.void/cache/v1').iterdir())
        assert artifacts, 'JIT did not persist its native object'
        for artifact in artifacts:
            data = artifact.read_bytes()
            assert data[:64] == hashlib.sha256(data[64:]).hexdigest().encode()
            inspect(data[129:])
    print(f'macOS {args.minimum}: {inspected} AOT/executable/JIT load commands verified; JIT/cache/AOT execution passed')


if __name__ == '__main__': main()
