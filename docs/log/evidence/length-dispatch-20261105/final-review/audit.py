"""Read existing source, disassembly and receipts; run no compiler/test/benchmark."""
from pathlib import Path
import collections, hashlib, json, subprocess

ROOT = Path(__file__).resolve().parents[5]
EVIDENCE = Path(__file__).resolve().parent.parent
OUT = Path(__file__).resolve().parent
record = json.loads((EVIDENCE / 'results.json').read_text())
private = Path(record['private_evidence'])
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
read_git = lambda *args: subprocess.check_output(['git', *args], cwd=ROOT)
source = ROOT / 'src/runtime/text_case.cpp'
source_hash = sha(source)
assert source_hash == record['verification']['candidate_source_sha256'] == 'bdbad97fffce38ed2f5213b825ee5917f773610e5901f0e88c21b9b00375c2f1'
assert source.read_bytes() == (EVIDENCE/'candidate-text_case.cpp').read_bytes() == (private/'candidate-text_case.cpp').read_bytes()
assert read_git('diff', '--', 'src/runtime/text_case.cpp') == (EVIDENCE/'candidate.patch').read_bytes()
assert read_git('diff', '--cached') == b''
production_names = read_git('diff', '--name-only', '--', 'src', 'include', 'stdlib', 'tests', 'examples', 'CMakeLists.txt', 'cmake', '.github', 'README.md', 'LICENSE').decode().splitlines()
assert production_names == ['src/runtime/text_case.cpp'], production_names
assert all(path.startswith('docs/') for path in read_git('ls-files', '--others', '--exclude-standard').decode().splitlines())
original = (EVIDENCE/'original-text_case.cpp').read_text()
candidate = source.read_text()
assert read_git('show', 'HEAD:src/runtime/text_case.cpp') == original.encode()
start = 'int vd::Runtime::translate_scalars('
end = 'extern "C" int vd_text_translate('
assert candidate[:candidate.index(start)] == original[:original.index(start)]
assert candidate[candidate.index(end):] == original[original.index(end):]
a = original.index('        auto replacement_root = root_word(&replacement);')
b = original.index('\n    }\n    if (max_bytes < 0)', a)
assert ''.join('    '+line if line.strip() else line for line in original[a:b].splitlines(keepends=True)) in candidate
publication = '''        std::lock_guard lock(mutex_);
        require_buffer_output(output);
        heap_.validate_output_address(output);
        *output = reference_source(input);'''
assert publication in original and candidate.count(publication) == 2
manifest = json.loads((EVIDENCE/'SHA256SUMS.json').read_text())
for name, digest in manifest.items():
    assert sha(EVIDENCE/name) == digest, name
for name, digest in record['binary_hashes'].items():
    assert sha(name) == digest, name
expected_overlay = candidate.replace('#include "unicode_case_tables.inc"', '#include "'+str(ROOT/'src/runtime/unicode_case_tables.inc')+'"')
assert (EVIDENCE/'candidate-plain.cpp').read_text() == expected_overlay == (private/'candidate-plain.cpp').read_text()
objects = {}
compiles = []
links = []
executables = {}
executions = []
for command in record['commands']:
    argv = command['command']
    if '-c' in argv and str(private/'candidate-plain.cpp') in argv:
        output = argv[argv.index('-o')+1]
        objects[output] = command['source_hashes'][output]
        assert sha(output) == objects[output]
        compiles.append({'source': str(private/'candidate-plain.cpp'), 'output': output, 'output_sha256': objects[output], 'argv': argv, 'receipt': command['private_receipt']})
for command in record['commands']:
    argv = command['command']
    selected = [obj for obj in objects if obj in argv]
    if selected and '-o' in argv and '-c' not in argv:
        obj = selected[0]
        assert command['source_hashes'][obj] == objects[obj]
        runtime_index = next(i for i, arg in enumerate(argv) if Path(arg).name == 'libvoid_runtime.a')
        assert argv.index(obj) < runtime_index
        output = str(Path(argv[argv.index('-o')+1]))
        digest = sha(output)
        assert record['binary_hashes'][output] == digest
        executables[output] = digest
        links.append({'candidate_object': obj, 'candidate_object_sha256': objects[obj], 'output': output, 'output_sha256': digest, 'object_precedes_runtime_archive': True, 'receipt': command['private_receipt']})
for command in record['commands']:
    argv = command['command']
    if argv[0] in executables:
        assert command['returncode'] == 0 and command['executable_sha256'] == executables[argv[0]]
        executions.append({'executable': argv[0], 'sha256': executables[argv[0]], 'argv': argv, 'receipt': command['private_receipt']})
expected_executables = {'original-candidate', 'components-candidate', 'correctness', 'ownership', 'log-host', 'native-extra', 'native-extra-strict', 'fixture1-normal', 'fixture1-o2', 'fixture2-normal', 'fixture2-o2'}
assert {Path(name).name for name in executables} == expected_executables
assert len(objects) == 2 and {item['executable'] for item in executions} == set(executables)
assert len(record['commands']) == 51 and all(c['returncode'] == 0 for c in record['commands'])
assert len(record['samples']) == 120 and sum(row['sample'] >= 0 for row in record['samples']) == 96
for name in ['disassembly-candidate.stdout', 'linked-main-disassembly.txt', 'linked-helper-disassembly.txt']:
    assert sha(EVIDENCE/name) == sha(private/name)
disassembly = (EVIDENCE/'disassembly-candidate.stdout').read_text()
for fragment in ['b01:', 'movq\t%rax, %rbx', 'e1d:', 'cmpq\t$0x80, %rbx', 'e24:', 'ja\t0xfd3', 'callq\t0x22a0', 'fd3:', 'ShortAsciiNoMatch::prove']:
    assert fragment in disassembly, fragment
result = {
    'date': '2026-11-05', 'timezone': 'America/Vancouver',
    'scope': 'Final source/evidence audit only. No Runtime, test, compiler, sanitizer or benchmark execution.',
    'head': read_git('rev-parse', 'HEAD').decode().strip(),
    'candidate_source_sha256': source_hash,
    'baseline_source_sha256': sha(EVIDENCE/'original-text_case.cpp'),
    'complete_production_patch': '../candidate.patch',
    'complete_production_patch_sha256': sha(EVIDENCE/'candidate.patch'),
    'production_changed_paths': production_names,
    'candidate_function_only_changed': True,
    'original_general_loop_identical_except_indentation': True,
    'checked_alias_publication_sequence_identical': True,
    'historical_manifest_sha256': sha(EVIDENCE/'SHA256SUMS.json'),
    'historical_artifacts_verified': len(manifest),
    'retained_binaries_libraries_tools_verified': len(record['binary_hashes']),
    'overlay': {'only_change': 'Make unicode_case_tables.inc include absolute; all candidate function bytes unchanged', 'sha256': sha(EVIDENCE/'candidate-plain.cpp')},
    'candidate_compilations': compiles, 'candidate_links': links, 'candidate_successful_executions': executions,
    'source_and_binary_chain_counts': {'objects': len(objects), 'linked_candidate_executables': len(executables), 'successful_candidate_execution_records': len(executions)},
    'historical_recorded_command_count': 51, 'historical_recorded_commands_all_pass': True,
    'new_tests_or_measurements': 0,
    'long_path_verified': 'Saved mandatory extent -> e1d cmp $0x80,%rbx -> e24 ja fd3 original general path. Skips e62 call to 22a0 helper, its lock/root/string lookup and short-fallback poll. Linked disassembly copies match hashed private artifacts.',
    'commit_state': 'Uncommitted and unstaged. STOP for Golden Wrench final GO/HOLD.',
    'log_perf': 'OPEN: original file200 strictly <10 ms remains FAIL.'
}
(OUT/'verification.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({'candidate_source_sha256': source_hash, 'historical_artifacts_verified': len(manifest), 'retained_binaries_libraries_tools_verified': len(record['binary_hashes']), **result['source_and_binary_chain_counts'], 'new_tests_or_measurements': 0}, indent=2))
