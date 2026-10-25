"""Manual Stage 10 native stack evidence; no production or test policy changes."""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile
import time
import traceback

MIB = 1048576
EVIDENCE = Path('build/native-stack-evidence')
CONFIGURATION = os.environ.get('NATIVE_STACK_CONFIGURATION', '')
BUILD = Path('build') / ('native-stack-' + CONFIGURATION)
SYSTEM = platform.system()
SUFFIX = '.exe' if SYSTEM == 'Windows' else ''


def save(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources():
    paths = subprocess.check_output(['git', 'ls-files', '-z']).decode('utf-8').split('\0')
    return {name: digest(Path(name)) for name in paths if name}


def run(label, argv):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    record = {'argv': argv, 'exit_code': None}
    started = time.monotonic()
    print(json.dumps(argv), flush=True)
    try:
        with (EVIDENCE / (label + '.log')).open('wb') as log:
            process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            for line in process.stdout:
                log.write(line)
                log.flush()
                sys.stdout.buffer.write(line)
                sys.stdout.buffer.flush()
            record['exit_code'] = process.wait()
    finally:
        record['wall_seconds'] = time.monotonic() - started
        save(label + '.json', record)
    if record['exit_code'] != 0:
        raise subprocess.CalledProcessError(record['exit_code'], argv)


def configure_command():
    command = ['cmake', '-S', '.', '-B', str(BUILD), '-G', 'Ninja',
               '-DCMAKE_BUILD_TYPE=' + CONFIGURATION, '-DBUILD_TESTING=ON',
               '-DVOID_SANITIZERS=OFF', '-DVOID_LLVM_RESEARCH=OFF',
               '-DVOID_RETENTION_BASELINE=OFF', '-DLLVM_DIR=' + os.environ['LLVM_DIR'],
               '-DVOID_CLANG=' + os.environ['VOID_CLANG'],
               '-DVOID_LINKER=' + os.environ.get('VOID_LINKER', '')]
    if SYSTEM == 'Darwin':
        cache = Path(os.environ['THINLTO_CACHE_DIR']).resolve()
        if any(cache.iterdir()):
            raise RuntimeError('ThinLTO cache must start empty before configure')
        command.append('-DCMAKE_EXE_LINKER_FLAGS:STRING=-Wl,-cache_path_lto,"' + cache.as_posix() + '"')
    return command


def verify_pe():
    values = {}
    for target in ('voidc', 'void_tests'):
        binary = BUILD / (target + '.exe')
        run('pe-' + target, [str(Path('.toolchains/llvm-19.1.7/bin/llvm-readobj.exe').resolve()),
                             '--file-headers', str(binary)])
        text = (EVIDENCE / ('pe-' + target + '.log')).read_text(encoding='utf-8', errors='replace')
        reserves = re.findall(r'^\s*SizeOfStackReserve:\s+(0x[0-9a-fA-F]+|[0-9]+)\s*$', text, re.M)
        values[target] = {'sha256': digest(binary), 'bytes': binary.stat().st_size,
                          'stack_reserves': [int(v, 16) if v.startswith('0x') else int(v) for v in reserves]}
    passed = all(v['stack_reserves'] == [MIB] for v in values.values())
    save('pe-stack.json', {'expected_bytes': MIB, 'binaries': values, 'passed': passed})
    if not passed:
        raise RuntimeError('Both actual PE StackReserve values must equal 1048576')


def stack_boundary(label):
    if SYSTEM == 'Windows':
        if not json.loads((EVIDENCE / 'pe-stack.json').read_text())['passed']:
            raise RuntimeError('Windows PE stack gate must pass before execution')
        return
    import resource
    record = {'before': resource.getrlimit(resource.RLIMIT_STACK), 'expected_bytes': MIB}
    try:
        resource.setrlimit(resource.RLIMIT_STACK, (MIB, MIB))
    finally:
        record['observed'] = resource.getrlimit(resource.RLIMIT_STACK)
        save(label + '-stack.json', record)
    if record['observed'] != (MIB, MIB):
        raise RuntimeError('Both inherited macOS stack limits must equal 1048576')


def finish():
    after = sources()
    save('source-after.json', after)
    before_path = EVIDENCE / 'source-before.json'
    unchanged = before_path.exists() and json.loads(before_path.read_text()) == after
    binaries = {p.name: {'sha256': digest(p), 'bytes': p.stat().st_size}
                for p in (BUILD / ('voidc' + SUFFIX), BUILD / ('void_tests' + SUFFIX)) if p.is_file()}
    save('binaries.json', binaries)
    outcomes = {}
    for name in ('sdk', 'configure', 'build', 'frontend', 'semantic-depth'):
        path = EVIDENCE / (name + '.json')
        outcomes[name] = json.loads(path.read_text())['exit_code'] if path.exists() else None
    if SYSTEM == 'Windows':
        path = EVIDENCE / 'pe-stack.json'
        boundary = path.exists() and json.loads(path.read_text())['passed']
    else:
        boundary = all((EVIDENCE / (label + '-stack.json')).exists() and
                       json.loads((EVIDENCE / (label + '-stack.json')).read_text())['observed'] == [MIB, MIB]
                       for label in ('frontend', 'semantic-depth'))
    complete = unchanged and boundary and all(value == 0 for value in outcomes.values())
    save('summary.json', {'configuration': CONFIGURATION, 'command_exit_codes': outcomes,
                          'source_unchanged': unchanged, 'stack_gates_complete': complete,
                          'Windows_console_ConPTY': 'UNVERIFIED; not exercised by this workflow',
                          'Stage10_release': 'Not locked; Golden Wrench/Boss decision required'})
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as stream:
            stream.write(f'Native stack gates complete: **{complete}** ({CONFIGURATION}).\n\n'
                         'Windows console/ConPTY is unverified. This is not a Stage 10 release lock.\n')
    if not unchanged:
        raise RuntimeError('Source receipt missing or tracked bytes changed during native checks')
    if not complete:
        raise RuntimeError('Native stack gates incomplete; inspect preserved command and boundary records')


def main(phase):
    if CONFIGURATION not in ('Debug', 'Release'):
        raise RuntimeError('Only native Debug and Release are authorized here')
    if SYSTEM not in ('Windows', 'Darwin'):
        raise RuntimeError('This workflow requires a real Windows or macOS host')
    if phase == 'identity':
        architecture = platform.machine().lower()
        expected = ('amd64', 'x86_64') if SYSTEM == 'Windows' else ('arm64', 'aarch64')
        if architecture not in expected:
            raise RuntimeError('Requires Windows x64 or native macOS ARM64')
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
        save('identity.json', {'commit': commit, 'dispatch_sha': os.environ.get('GITHUB_SHA'),
                              'configuration': CONFIGURATION, 'system': platform.platform(),
                              'machine': architecture, 'runner_arch': os.environ.get('RUNNER_ARCH'),
                              'run_id': os.environ.get('GITHUB_RUN_ID'),
                              'run_attempt': os.environ.get('GITHUB_RUN_ATTEMPT')})
        if commit != os.environ['GITHUB_SHA']:
            raise RuntimeError('Checked-out source must match the exact dispatch SHA')
        save('source-before.json', sources())
        if SYSTEM == 'Darwin':
            cache = Path(tempfile.mkdtemp(prefix='void-stage10-thinlto-', dir=os.environ['RUNNER_TEMP'])).resolve()
            save('thinlto-cache.json', {'directory': str(cache), 'persistent': False})
            with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as env:
                env.write('THINLTO_CACHE_DIR=' + cache.as_posix() + '\n')
    elif phase == 'sdk':
        run('sdk', [sys.executable, 'tools/ci/llvm_sdk.py'])
    elif phase == 'configure':
        save('toolchain-environment.json', {key: os.environ.get(key) for key in
             ('CC', 'CXX', 'LLVM_DIR', 'VOID_CLANG', 'VOID_LINKER', 'SDKROOT',
              'VCToolsVersion', 'WindowsSDKVersion', 'ASAN_OPTIONS', 'UBSAN_OPTIONS')})
        tool_paths = {key: Path(os.environ[key]) for key in ('CC', 'CXX', 'VOID_CLANG')}
        tool_paths['LLVMConfig.cmake'] = Path(os.environ['LLVM_DIR']) / 'LLVMConfig.cmake'
        save('toolchain-binaries.json', {key: {'path': str(path), 'sha256': digest(path)}
                                         for key, path in tool_paths.items()})
        for label, command in [('cmake-version', ['cmake', '--version']),
                               ('ninja-version', ['ninja', '--version']),
                               ('compiler-version', [os.environ['CXX'], '--version'])]:
            run(label, command)
        if SYSTEM == 'Darwin':
            run('apple-sdk', ['xcrun', '--show-sdk-version'])
            run('apple-linker', ['xcrun', '--find', 'ld'])
        if (BUILD / 'CMakeCache.txt').exists():
            raise RuntimeError('Targeted native acceptance requires a fresh build directory')
        run('configure', configure_command())
    elif phase == 'build':
        run('build', ['cmake', '--build', str(BUILD), '--parallel', '3', '--target', 'voidc', 'void_tests'])
    elif phase == 'pe':
        if SYSTEM != 'Windows':
            raise RuntimeError('PE readback requires the actual Windows build')
        verify_pe()
    elif phase in ('frontend', 'depth'):
        label = 'frontend' if phase == 'frontend' else 'semantic-depth'
        stack_boundary(label)
        command = (['ctest', '--test-dir', str(BUILD), '-R', '^frontend$', '--no-tests=error',
                    '--output-on-failure', '--output-junit', str((EVIDENCE / 'frontend-junit.xml').resolve())]
                   if phase == 'frontend' else
                   [sys.executable, 'tests/integration/semantic_depth.py', str(BUILD / ('voidc' + SUFFIX))])
        run(label, command)
    elif phase == 'finish':
        finish()
    else:
        raise ValueError(phase)


if __name__ == '__main__':
    phase = sys.argv[1]
    try:
        main(phase)
    except Exception as error:
        exit_code = error.returncode if isinstance(error, subprocess.CalledProcessError) else 1
        save(phase + '-error.json', {'exit_code': exit_code, 'error': str(error),
                                    'traceback': traceback.format_exc()})
        traceback.print_exc()
        sys.exit(exit_code)
