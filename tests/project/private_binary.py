"""Linux Stage B qualification of exact linked, split-debug, stripped bytes.

Uses only controlled temporary sources. Never publishes an application or
touches the installed compiler, approvals or external projects.
"""
from pathlib import Path
import hashlib
import json
import os
import statistics
import shutil
import subprocess
import sys
import tempfile
import time

emitter, auditor, compiler, llvm = map(lambda p: Path(p).resolve(), sys.argv[1:])
def run(args, expected=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=60)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result

native = (compiler.parent / 'void-native.txt').read_text().splitlines()
assert native[0] == 'VOID native 23.1.4 v2'
assert '23.1.4' in run([llvm / 'llvm-objcopy', '--version']).stdout
linker = ['--ld-path=' + native[2]]
canaries = ['PRIVATE_PATH_B_7de94', 'private_function_b_9ac53', 'source_snippet_b_1dfee']
sync = '''func private_function_b_9ac53(value: i32) -> i32:
    if value > 0:
        return private_function_b_9ac53(value - 1)
    var source_snippet_b_1dfee = value - value
    return value / source_snippet_b_1dfee

func entry(value: i32) -> i32:
    defer {
        print("cleanup executed")
    }
    return private_function_b_9ac53(value)
'''
async_source = '''async func tick() -> i32:
    return 1

func private_function_b_9ac53(value: i32) -> i32:
    if value > 0:
        return private_function_b_9ac53(value - 1)
    var source_snippet_b_1dfee = value - value
    return value / source_snippet_b_1dfee

async func worker(value: i32) -> i32:
    var ignored = await tick()
    return private_function_b_9ac53(value)

async func entry(value: i32) -> i32:
    defer {
        print("cleanup executed")
    }
    return await worker(value)
'''
def audit(image, *extra):
    result = json.loads(run([auditor, 'audit-stage-b', 'linux-x64', image, *canaries, *extra]).stdout)
    assert result['inspected'], result
    return result

with tempfile.TemporaryDirectory(prefix='void-private-b-') as temp:
    root = Path(temp)
    private = root / 'private'
    private.mkdir(mode=0o700)
    source = root / canaries[0] / 'fixture.void'
    source.parent.mkdir()
    evidence = []
    retained_source = sync.replace('    var source_snippet', '    defer {\n        print("retained cleanup")\n    }\n    var source_snippet', 1)
    for kind, text in [('sync', sync), ('async', async_source), ('retained', retained_source)]:
        source.write_text(text)
        for mode in ['O0', 'O2']:
            stem = kind + '-' + mode
            obj, linked, final = [root / (stem + ext) for ext in ['.o', '.linked', '.final']]
            mapfile, symbols = private / (stem + '.vdiag'), private / (stem + '.debug')
            semantic_digest = hashlib.sha256((text + mode + kind).encode() + hashlib.sha256(compiler.read_bytes()).digest()).hexdigest()
            process = subprocess.Popen(list(map(str, [emitter, 'emit', source, obj, mapfile, final, mode, kind, semantic_digest])),
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                assert process.stdout.readline() == 'READY\n', process.stderr.read()
                # Existing same-build native inputs and linker policy are reused.
                run([llvm / 'clang++', *linker, obj, '-o', linked, *native[3:]])
                run([llvm / 'llvm-objcopy', '--only-keep-debug', linked, symbols])
                run([llvm / 'llvm-objcopy', '--strip-all', '--remove-section=.comment', linked, final])
                process.stdin.write('FINAL\n');process.stdin.flush()
                stdout, stderr = process.communicate(timeout=60)
                assert process.returncode == 0, (stdout, stderr)
            finally:
                if process.poll() is None:
                    process.kill();process.communicate()
            result = audit(final)
            # Native dependencies may retain their existing producer RUNPATH.
            # Stage B does not grant a portability/closure capability.
            assert all(f['code'] in ({'loader-path', 'forbidden-string'} if kind == 'retained' else {'loader-path'}) for f in result['findings']), result
            assert any(f['code'] == 'forbidden-string' for f in result['findings']) == (kind == 'retained'), result
            assert not result['debug'] and not result['static_symbols'] and result['dynamic_symbols'] and result['unwind'], result
            assert mapfile.read_bytes() not in final.read_bytes()
            assert 'VOID-PRIVATE-DIAGNOSTICS'.encode() not in final.read_bytes()
            before = run([llvm / 'llvm-readobj', '--dyn-symbols', '--unwind', linked]).stdout
            after = run([llvm / 'llvm-readobj', '--dyn-symbols', '--unwind', final]).stdout
            assert before.split('Format:', 1)[1] == after.split('Format:', 1)[1], 'Dynamic symbols or unwind records changed during reduction'
            # Resolve an actual linked function address using the private split
            # symbols. No source-bearing debuglink is added to the final image.
            assert canaries[0].encode() in symbols.read_bytes()
            function = ('/fixture.void::' + canaries[1]).encode().hex()
            names = run([llvm / 'llvm-nm', '--defined-only', '--format=posix', linked]).stdout.splitlines()
            selected = next(n.split() for n in names if n.startswith('__vd_' + function + '__void '))
            symbolicated = run([llvm / 'llvm-symbolizer', '--obj=' + str(symbols), '0x' + selected[2]]).stdout
            assert str(source) in symbolicated, symbolicated
            compressed = root / 'compressed-debug'
            run([llvm / 'llvm-objcopy', '--compress-debug-sections=zlib', linked, compressed])
            assert any(f['code'] == 'compressed-content' for f in audit(compressed)['findings'])
            error = run([final, '7'], expected=1)
            assert error.stdout == ('retained cleanup\n' if kind == 'retained' else '') + 'cleanup executed\n' and 'VOID-D1 ' in error.stderr
            assert all(c not in error.stderr for c in canaries)
            deep = run([final, '40'], expected=1)
            assert 'Source call trace limited to 32 callers' in deep.stderr
            assert deep.stderr.count('VOID-D1 ') == 33, deep.stderr
            tokens = root / 'error.txt';tokens.write_text(error.stderr)
            resolved = run([emitter, 'resolve', mapfile, final, tokens]).stdout
            assert all(c in resolved for c in canaries), resolved
            assert 'Division by zero' in resolved and 'source call' in resolved, resolved
            wrong = root / 'wrong-image';wrong.write_bytes(final.read_bytes() + b'changed')
            assert 'Mismatched private diagnostic image' in run([emitter, 'resolve', mapfile, wrong, tokens], expected=1).stderr
            missing = root / 'empty-map';missing.write_bytes(b'')
            assert 'Missing private diagnostic map' in run([emitter, 'resolve', missing, final, tokens], expected=1).stderr
            # Same source, normal development lowering: full diagnostics survive.
            devobj, dev = root / 'development.o', root / 'development'
            run([emitter, 'development', source, devobj, mode, kind])
            run([llvm / 'clang++', *linker, devobj, '-o', dev, *native[3:]])
            dev_error = run([dev, '7'], expected=1)
            assert all(c in dev_error.stderr for c in canaries), dev_error.stderr
            assert 'VOID-D1' not in dev_error.stderr
            assert dev_error.stdout == error.stdout
            # Stripping alone cannot remove development diagnostic canaries.
            stripped_dev = root / 'development-stripped'
            run([llvm / 'llvm-objcopy', '--strip-all', dev, stripped_dev])
            assert any(f['code'] == 'forbidden-string' for f in audit(stripped_dev)['findings'])
            samples = {}
            for label, image in [('development', dev), ('private', final)]:
                timings = []
                for _ in range(5):
                    start = time.perf_counter_ns();run([image, '7'], expected=1);timings.append(time.perf_counter_ns() - start)
                samples[label] = statistics.median(timings)
            record = dict(case=stem, sha256=hashlib.sha256(final.read_bytes()).hexdigest(),
                          development_bytes=dev.stat().st_size, linked_private_bytes=linked.stat().st_size,
                          final_bytes=final.stat().st_size, private_map_bytes=mapfile.stat().st_size, build_id=stdout.split()[1],
                          split_debug_bytes=symbols.stat().st_size, process_error_median_ns=samples,
                          report=result)
            evidence.append(record)
            destination = os.environ.get('VOID_STAGE_B_EVIDENCE')
            if destination:
                saved = Path(destination) / stem
                saved.mkdir(parents=True, mode=0o700, exist_ok=True)
                for artifact in [source, obj, linked, final, mapfile, symbols, tokens]:
                    shutil.copy2(artifact, saved / artifact.name)
                (saved / 'evidence.json').write_text(json.dumps(record, indent=2))
    assert len({item['build_id'] for item in evidence}) == len(evidence), 'Build identities must distinguish semantic inputs'
    # Audit catches an accidentally appended map, not only source canaries.
    contaminated = root / 'contaminated';contaminated.write_bytes(final.read_bytes() + mapfile.read_bytes())
    assert any(f['code'] == 'private-diagnostic-map' for f in audit(contaminated)['findings'])
    print(json.dumps(evidence, indent=2))
print('Stage B linked-byte canaries, private maps, ABI failure status and development diagnostics passed')
