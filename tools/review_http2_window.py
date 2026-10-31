#!/usr/bin/env python3
"""Isolate one HTTP/2 provider correction without changing authoritative curl.

Recompile exactly http2.c with the matching production command, replace only
that member in a disposable archive, and independently link the actual native
HTTP/Runtime test against it. This is review tooling, never a production build.
"""
import argparse
import datetime
import difflib
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=['release', 'debug', 'strict'], required=True)
    parser.add_argument('--native-only', action='store_true', help='Focused native retest; does not claim source acceptance')
    args = parser.parse_args()
    build = ROOT / ('build/llvm23-' + args.profile)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    evidence = ROOT / f'docs/net/http/evidence/http2-window-review-{args.profile}-{stamp}'
    work = ROOT / f'.toolchains/http2-window-review/{args.profile}-{stamp}'
    evidence.mkdir(parents=True); work.mkdir(parents=True)
    shutil.copy2(build / 'CMakeCache.txt', work / 'CMakeCache.txt')
    records = {'profile': args.profile, 'scope': 'isolated one-file comparison; authoritative source/archive unchanged by this run', 'commands': []}
    environment = os.environ.copy()
    if args.profile == 'strict':
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def save(): (evidence / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
    def run(argv, label, cwd=ROOT):
        argv = list(map(str, argv)); started = time.monotonic()
        with (evidence / (label + '.stdout')).open('wb') as out, (evidence / (label + '.stderr')).open('wb') as err:
            result = subprocess.run(argv, cwd=cwd, env=environment, stdout=out, stderr=err, timeout=300)
        records['commands'].append({'argv': argv, 'cwd': str(cwd), 'exit_code': result.returncode, 'seconds': time.monotonic() - started}); save()
        assert result.returncode == 0, (label, str(evidence))
    run(['python3', ROOT / 'tools/verify_curl_source.py'], 'authoritative-source')
    original = ROOT / 'third_party/curl/lib/http2.c'
    authoritative_sha256 = digest(original)
    provenance = json.loads((ROOT / 'third_party/curl/VOID-PROVENANCE.json').read_text())
    archive = ROOT / '.toolchains/http/downloads/curl-8.23.0.tar.xz'
    assert digest(archive) == provenance['archive_sha256']
    # The correction may already be integrated. Reconstruct the old member
    # from the verified signed release without changing authoritative source.
    with tarfile.open(archive) as release:
        before = release.extractfile('curl-8.23.0/lib/http2.c').read().decode()
    assert hashlib.sha256(before.encode()).hexdigest() == provenance['pristine_sha256']['lib/http2.c']
    first = before.index("      rv = nghttp2_submit_window_update", before.index("static CURLcode cf_h2_update_local_win"))
    last = before.index("      stream->local_window_size = dwsize;", first)
    redundant = before[first:last]
    assert redundant.count("return CURLE_HTTP2;") == 1 and before.count(redundant) == 1
    after = before.replace(redundant, '')
    source = work / 'http2.c'; source.write_text(after)
    patch = ''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True), fromfile='a/lib/http2.c', tofile='b/lib/http2.c'))
    (evidence / 'candidate.patch').write_text(patch)
    records['source'] = {'authoritative_sha256': authoritative_sha256,
        'upstream_sha256': hashlib.sha256(before.encode()).hexdigest(),
        'candidate_sha256': digest(source), 'patch_sha256': digest(evidence / 'candidate.patch')}
    commands = json.loads((build / 'compile_commands.json').read_text())
    def compile_one(original_file, candidate_file, output, label):
        command = next(row for row in commands if Path(row['file']).resolve() == original_file)
        compile_args = command.get('arguments') or shlex.split(command['command'])
        rewritten = []; index = 0
        while index < len(compile_args):
            arg = compile_args[index]
            if arg in ['-MF', '-MT', '-MQ']:
                index += 2; continue
            if arg in ['-MD', '-MMD', '-MP']:
                index += 1; continue
            if arg == '-o':
                rewritten += ['-o', str(output)]; index += 2; continue
            rewritten.append(str(candidate_file) if arg == str(original_file) else arg); index += 1
        run(rewritten, label, Path(command['directory']))
    object_file = work / 'http2.c.o'
    compile_one(original, source, object_file, 'compile-one-member')
    ar = ROOT / '.toolchains/llvm-23.1.4/bin/llvm-ar'
    production_archive = build / 'curl/lib/libcurl.a'
    production_sha256 = digest(production_archive)
    control = work / 'control'; control.mkdir()
    control_source = control / 'http2.c'; control_source.write_text(before)
    control_object = control / 'http2.c.o'
    compile_one(original, control_source, control_object, 'compile-original-member')
    original_archive = control / 'libcurl.a'; candidate_archive = work / 'libcurl.a'
    shutil.copy2(production_archive, original_archive)
    run([ar, 'r', original_archive, control_object], 'restore-original-member-in-control')
    shutil.copy2(original_archive, candidate_archive)
    run([ar, 'r', candidate_archive, object_file], 'replace-one-member')
    names = subprocess.check_output([ar, 't', original_archive], text=True).splitlines()
    assert names == subprocess.check_output([ar, 't', candidate_archive], text=True).splitlines()
    changed = []
    for name in names:
        if subprocess.check_output([ar, 'p', original_archive, name]) != subprocess.check_output([ar, 'p', candidate_archive, name]): changed.append(name)
    assert changed == ['http2.c.o'], changed
    records['changed_archive_members'] = changed
    records['archive_sha256'] = {'original': digest(original_archive), 'candidate': digest(candidate_archive)}
    replacements = {}
    for name in ['native', 'runtime']:
        path = ROOT / f'tests/net/http/{name}.cpp'; output = work / (name + '.cpp.o')
        compile_one(path, path, output, 'compile-test-' + name)
        replacements[f'CMakeFiles/void_http_native.dir/tests/net/http/{name}.cpp.o'] = str(output)
    def link_one(target, output, replacements=None):
        replacements = replacements or {}
        original_args = shlex.split((build / ('CMakeFiles/' + target + '.dir/link.txt')).read_text())
        link = []; index = 0
        while index < len(original_args):
            arg = original_args[index]
            if arg == '-Xlinker' and original_args[index + 1].startswith('--dependency-file='):
                index += 2; continue
            link.append(replacements.get(arg, arg)); index += 1
        link[link.index('-o') + 1] = str(output)
        archive_argument = next(i for i, arg in enumerate(link) if arg == 'curl/lib/libcurl.a' or arg == str(production_archive))
        link[archive_argument] = str(candidate_archive)
        run(link, 'link-' + target, build)
    binary = work / 'candidate_native'
    link_one('void_http_native', binary, replacements)
    records['binary_sha256'] = digest(binary); save()
    run(['python3', ROOT / 'tests/net/http/fixture.py', binary, '--http2', '--evidence', evidence / 'native'], 'native-candidate')
    if not args.native_only:
        compiler = work / 'voidc'; runner = work / 'void_http_source_runner'
        link_one('voidc', compiler); link_one('void_http_source_runner', runner)
        manifest = (build / 'void-native.txt').read_text()
        assert str(production_archive) in manifest
        (work / 'void-native.txt').write_text(manifest.replace(str(production_archive), str(candidate_archive)))
        inventory = (build / 'void-native-resources.txt').read_text()
        inventory = inventory.replace(digest(production_archive) + '\t' + str(production_archive),
                                      digest(candidate_archive) + '\t' + str(candidate_archive))
        (work / 'void-native-resources.txt').write_text(inventory)
        run(['python3', ROOT / 'tests/net/http/source.py', compiler, '--runner', runner,
             '--evidence', evidence / 'source'], 'source-candidate')
    assert digest(original) == authoritative_sha256 and digest(production_archive) == production_sha256
    records['result'] = 'PASS (isolated native only)' if args.native_only else 'PASS (isolated native and source candidate only)'
    save(); print(evidence)

if __name__ == '__main__': main()
