"""Relocated HTTP/URL/JSON modules, native link closure and corruption rejection."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import examples
import fixture

ROOT = Path(__file__).resolve().parents[3]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    server = fixture.Server(('127.0.0.1', 0), examples.Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    commands = []
    with tempfile.TemporaryDirectory(prefix='VOID HTTP installation ') as directory:
        work = Path(directory); prefix = work / 'original'; relocated = work / 'relocated 雪 # % installation'
        env = os.environ.copy(); env.pop('VOID_STDLIB_ROOT', None)
        env['VOID_HTTP_URL'] = f'http://127.0.0.1:{server.server_port}/json'
        def run(argv, expected=None, code=0, diagnostic=''):
            start = time.monotonic()
            result = subprocess.run(list(map(str, argv)), cwd=work, env=env, capture_output=True, timeout=180)
            number = len(commands)
            (evidence / f'{number}.stdout').write_bytes(result.stdout)
            (evidence / f'{number}.stderr').write_bytes(result.stderr)
            commands.append(dict(argv=list(map(str, argv)), exit_code=result.returncode,
                                 seconds=time.monotonic() - start))
            (evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
            assert result.returncode == code, (argv, result.stderr.decode(errors='replace'))
            if expected is not None: assert result.stdout.decode() == expected
            assert diagnostic in result.stderr.decode(), result.stderr
        try:
            for component in ('compiler', 'tooling'):
                run(['cmake', '--install', compiler.parent, '--prefix', prefix, '--component', component])
            prefix.rename(relocated)
            installed = relocated / 'bin' / compiler.name
            for module in ('net/http.void', 'url.void', 'json.void', 'io.void', 'error.void'):
                assert (relocated / 'share/void/stdlib' / module).read_bytes() == (ROOT / 'stdlib' / module).read_bytes()
            provenance = json.loads((relocated / 'share/void/licenses/http/VOID-PROVENANCE.json').read_text())
            assert len(provenance['local_corrections']) == 4
            for archive in (('libidn2-2.3.8.tar.gz', 'libunistring-1.5.tar.xz') if __import__('sys').platform == 'linux' else ()):
                assert (relocated / 'share/void/licenses/http/corresponding-source' / archive).is_file()
            source = work / 'project 雪 # %' / 'main.void'
            source.parent.mkdir()
            source.write_bytes((ROOT / 'examples/net_http/api_json.void').read_bytes())
            lines = (installed.parent / 'void-native.txt').read_text().splitlines()
            assert lines[0] == 'VOID native 23.1.4 v2'
            resources = [(installed.parent / name).resolve() for name in lines[3:5]]
            flags = [line.replace('@VOID_NATIVE_DIR@', str(installed.parent)) for line in lines[5:]]
            if lines[2]: flags += ['--ld-path=' + lines[2]]
            if __import__('sys').platform == 'linux': flags += ['-ldl']
            for options in ([], ['--no-opt']):
                run([installed, 'run', source, *options], 'Example JSON complete\n')
                binary = work / ('built-' + str(bool(options)) + ('.exe' if os.name == 'nt' else ''))
                run([installed, 'build', source, '-o', binary, *options])
                run([binary], 'Example JSON complete\n')
                obj = work / 'program.o'; direct = work / ('direct.exe' if os.name == 'nt' else 'direct')
                run([installed, 'emit', source, '-o', obj, *options])
                run([lines[1], '--driver-mode=g++', obj, *resources, *flags, '-o', direct])
                run([direct], 'Example JSON complete\n')
            # A present, still-linkable archive must not silently replace a pinned artifact.
            archive = Path(flags[0])
            saved = archive.read_bytes()
            archive.write_bytes(saved + b'\0')
            protected = work / 'protected-output'; protected.write_bytes(b'existing output')
            run([installed, 'build', source, '-o', protected], code=1,
                diagnostic='Native link resource checksum mismatch')
            assert protected.read_bytes() == b'existing output'
            archive.write_bytes(saved)
            run([installed, 'build', source, '-o', protected])
            run([protected], 'Example JSON complete\n')
        finally:
            server.shutdown(); server.server_close()
    print(f'Installed HTTP/URL/JSON: {len(commands)} relocated native/JIT/link checks passed')

if __name__ == '__main__': main()
