"""Public HTTP facade through source analysis, JIT and native linking, loopback only."""
import argparse
import hashlib
import json
import os
try:
    import resource
except ImportError:
    resource = None
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import fixture

ROOT = Path(__file__).resolve().parents[3]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--runner', type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--async-only', action='store_true')
    parser.add_argument('--faults-only', action='store_true')
    parser.add_argument('--soak-bytes', type=int, default=134217728)
    parser.add_argument('--measure-rss', action='store_true', help='Sample each child process on Linux; includes compiler/JIT overhead')
    args = parser.parse_args()
    assert args.soak_bytes > 0
    assert not args.measure_rss or Path('/proc/self/status').exists(), 'RSS sampling requires Linux procfs'
    args.compiler = args.compiler.resolve()
    if args.runner: args.runner = args.runner.resolve()
    args.evidence = args.evidence.resolve(); args.evidence.mkdir(parents=True, exist_ok=True)
    server = fixture.Server(('127.0.0.1', 0), fixture.Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    commands = []
    sources = ['stdlib/net/http.void', 'include/void/http_ops.hpp', 'src/runtime/http.cpp', 'src/runtime/http_abi.cpp',
        'src/runtime/http_file.cpp', 'src/runtime/http_resource.cpp', 'src/runtime/native_work.cpp',
        'src/runtime/async.cpp', 'third_party/http-versions.json']
    identity = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
        'stream_bytes': args.soak_bytes,
        'stack_limits': list(resource.getrlimit(resource.RLIMIT_STACK)) if resource else None,
        'source_sha256': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
        'ASAN_OPTIONS': os.environ.get('ASAN_OPTIONS', ''), 'UBSAN_OPTIONS': os.environ.get('UBSAN_OPTIONS', '')}
    (args.evidence / 'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    def run(argv, cwd, expected=None):
        argv = list(map(str, argv)); number = len(commands)
        environment = os.environ.copy()
        for name in ['http_proxy', 'https_proxy', 'all_proxy', 'no_proxy', 'HTTPS_PROXY', 'ALL_PROXY', 'NO_PROXY']:
            environment.pop(name, None)
        environment['HTTP_PROXY'] = 'http://127.0.0.1:1'
        environment['ALL_PROXY'] = ''
        environment['NO_PROXY'] = ''
        samples = []
        with (args.evidence / f'{number}.stdout').open('w') as out, (args.evidence / f'{number}.stderr').open('w') as err:
            with subprocess.Popen(argv, cwd=cwd, env=environment, stdout=out, stderr=err) as child:
                started = time.monotonic()
                timed_out = False
                while True:
                    if args.measure_rss:
                        try:
                            for line in Path(f'/proc/{child.pid}/status').read_text().splitlines():
                                if line.startswith('VmRSS:'):
                                    samples.append({'seconds': time.monotonic() - started, 'rss_kib': int(line.split()[1])})
                        except FileNotFoundError:
                            pass
                    try:
                        status = child.wait(timeout=0.05)
                        break
                    except subprocess.TimeoutExpired:
                        if time.monotonic() - started >= 300:
                            child.kill(); child.wait(); status, timed_out = None, True
                            break
        if args.measure_rss:
            (args.evidence / f'{number}.rss.json').write_text(json.dumps(samples, indent=2) + '\n')
        commands.append({'argv': argv, 'exit_code': status, 'timed_out': timed_out})
        (args.evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert status == 0 and not timed_out, (argv, (args.evidence / f'{number}.stderr').read_text())
        if expected is not None: assert (args.evidence / f'{number}.stdout').read_text() == expected
    try:
        with tempfile.TemporaryDirectory(prefix='VOID HTTP source ') as directory:
            work = Path(directory)
            source = (ROOT / 'tests/net/http/source_sync.void').read_text().replace('@BASE@', f'http://127.0.0.1:{server.server_port}')
            source = source.replace('@FILE@', str(work / 'file.bin').replace('\\', '\\\\'))
            path = work / 'sync.void'; path.write_text(source)
            (args.evidence / 'sync.void').write_text(source)
            if not args.faults_only: run([args.compiler, 'check', path], work)
            for options in ([] if args.async_only or args.faults_only else [[], ['--no-opt']]):
                run([args.compiler, 'run', path, *options], work, 'true\n')
                binary = work / ('native-' + str(bool(options)) + ('.exe' if os.name == 'nt' else ''))
                run([args.compiler, 'build', path, '-o', binary, *options], work)
                run([binary], work, 'true\n')
            if args.runner and not args.faults_only:
                source = (ROOT / 'tests/net/http/source_async.void').read_text().replace('@BASE@', f'http://127.0.0.1:{server.server_port}')
                source = source.replace('134217728', str(args.soak_bytes))
                source = source.replace('@FILE@', str(work / 'file.bin').replace('\\', '\\\\'))
                path = work / 'async.void'; path.write_text(source)
                (args.evidence / 'async.void').write_text(source)
                run([args.compiler, 'check', path], work)
                run([args.runner, '--async', path], work)
                manifest = (args.compiler.parent / 'void-native.txt').read_text().splitlines()
                assert manifest[0] in ('VOID native 23.1.4 v1', 'VOID native 23.1.4 v2')
                flags = ['--driver-mode=g++', *manifest[5:]]
                if manifest[2]: flags += ['--ld-path=' + manifest[2]]
                for mode in ['normal', 'optimized']:
                    obj = work / ('async-' + mode + '.o'); binary = work / ('async-' + mode + ('.exe' if os.name == 'nt' else ''))
                    run([args.runner, '--emit-async', path, obj, mode], work)
                    run([manifest[1], obj, *manifest[3:5], *flags, '-o', binary], work)
                    run([binary, '7'], work, '7\n')

            if args.faults_only:
                assert args.runner
                for asynchronous in [False, True]:
                    source = (ROOT / 'tests/net/http/source_faults.void').read_text().replace('@BASE@', f'http://127.0.0.1:{server.server_port}')
                    source = source.replace('@ASYNC@', 'async ' if asynchronous else '').replace('@AWAIT@', 'await ' if asynchronous else '').replace('@SUFFIX@', '_async' if asynchronous else '')
                    path = work / ('faults-' + str(asynchronous) + '.void'); path.write_text(source)
                    (args.evidence / path.name).write_text(source)
                    run([args.runner, '--faults-async' if asynchronous else '--faults', path], work)
    finally:
        server.shutdown(); server.server_close()
    print(f'Public HTTP source: {len(commands)} analysis/JIT/native-link checks passed')

if __name__ == '__main__': main()
