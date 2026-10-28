"""Preserve exact source evidence before a crashing subprocess returns to its caller."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
REAL_RUN = subprocess.run
_RUN_CONTEXT = threading.local()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def crashed(status):
    # Local strict profiles reserve 87 for fatal ASan and 86 for fatal UBSan.
    return status < 0 or status in (86, 87) or status >= 0x80000000


def _signal_name(status):
    if os.name == 'nt' or status >= 0:
        return None
    try:
        return signal.Signals(-status).name
    except ValueError:
        # Signed Windows exception statuses are not POSIX signal numbers.
        return None


def preserve(command, cwd, result, source_inputs=(), provenance_inputs=(), *,
             failure_kind=None, evidence_context=None, environment=None):
    target = Path(os.environ.get('VOID_CRASH_EVIDENCE', ROOT / 'docs/text-encoding/evidence/crashes'))
    target.mkdir(parents=True, exist_ok=True)
    incident = Path(tempfile.mkdtemp(prefix='incident-', dir=target))
    working = Path(cwd or Path.cwd()).resolve()
    arguments = list(map(str, command)) if not isinstance(command, str) else [command]
    sources = {p for p in working.glob('*.void') if p.is_file()}
    for item in source_inputs:
        candidate = (working / item).resolve()
        sources.update(candidate.rglob('*.void') if candidate.is_dir() else (candidate,))
    for argument in arguments:
        candidate = Path(argument)
        if not candidate.is_absolute():
            candidate = working / candidate
        if candidate.suffix == '.void' and candidate.is_file():
            sources.add(candidate.resolve())
    # Import resolution can involve several modules; retain the canonical input set.
    sources.update(p for p in (ROOT / 'stdlib').rglob('*.void') if p.is_file())
    records = []
    for original in sorted(sources):
        if original.is_dir():
            continue
        destination = incident / ('stdlib' if original.is_relative_to(ROOT / 'stdlib') else 'inputs')
        if original.is_relative_to(ROOT / 'stdlib'):
            destination /= original.relative_to(ROOT / 'stdlib').parent
        destination.mkdir(parents=True, exist_ok=True)
        stem = original.stem + '_crash'
        copy = destination / (stem + original.suffix)
        serial = 2
        while copy.exists():
            copy = destination / f'{stem}_{serial}{original.suffix}'
            serial += 1
        raw = original.read_bytes()
        with copy.open('xb') as stream:
            stream.write(raw)
        original_hash = hashlib.sha256(raw).hexdigest()
        preserved_hash = digest(copy)
        assert preserved_hash == original_hash
        copy.chmod(0o444)
        records.append({'original': str(original), 'copy': str(copy), 'sha256': original_hash,
                        'original_sha256': original_hash, 'preserved_sha256': preserved_hash})
    provenance = incident / 'provenance'
    provenance.mkdir()
    generators = set((ROOT / 'tests/text_encoding').glob('*.py'))
    generators.update((working / item).resolve() for item in provenance_inputs)
    for argument in arguments:
        original = Path(argument)
        if not original.is_absolute():
            original = working / original
        if original.suffix == '.py' and original.is_file():
            generators.add(original.resolve())
    provenance_records = []
    for original in sorted(generators):
        if original.is_dir():
            continue
        destination = provenance / original.name
        serial = 2
        while destination.exists():
            destination = provenance / f'{original.stem}_{serial}{original.suffix}'
            serial += 1
        raw = original.read_bytes()
        with destination.open('xb') as stream:
            stream.write(raw)
        original_hash = hashlib.sha256(raw).hexdigest()
        preserved_hash = digest(destination)
        assert preserved_hash == original_hash
        destination.chmod(0o444)
        provenance_records.append({'original': str(original), 'copy': str(destination),
                                   'sha256': original_hash, 'original_sha256': original_hash,
                                   'preserved_sha256': preserved_hash})
    if os.path.dirname(arguments[0]):
        candidate = (working / arguments[0]).resolve()
        executable = str(candidate) if candidate.is_file() else None
    else:
        executable = shutil.which(arguments[0])
    build_inputs = []
    for cache in (ROOT / 'build').glob('ci-official-*/CMakeCache.txt'):
        build_inputs.append({'path': str(cache), 'sha256': digest(cache),
                             'flags': [line for line in cache.read_text().splitlines()
                                       if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:',
                                                           'CMAKE_CXX_FLAGS', 'VOID_SANITIZERS:', 'LLVM_DIR:'))]})
    record = {'failure_kind': failure_kind or ('timeout' if getattr(result, 'timed_out', False)
                                              else 'abnormal_exit' if crashed(result.returncode)
                                              else 'test_assertion'),
              'context': evidence_context or {},
              'timed_out': getattr(result, 'timed_out', False), 'command': arguments, 'cwd': str(working), 'returncode': result.returncode,
              'signal': _signal_name(result.returncode),
              'executable': executable,
              'executable_sha256': digest(Path(executable)) if executable else None,
              'sources': records, 'build_inputs': build_inputs,
              'provenance_inputs': provenance_records,
              'trigger': 'Sources come from argv and explicit source_inputs; working-directory '
                         'companions and canonical stdlib are retained conservatively. '
                         'The exact causal subset is not inferred.',
              'build': str(ROOT / 'build'),
              'environment': {key: (environment or os.environ)[key] for key in
                              ('ASAN_OPTIONS', 'UBSAN_OPTIONS', 'LSAN_OPTIONS', 'VOID_STDLIB_ROOT')
                              if key in (environment or os.environ)}}
    for name in ('stdout', 'stderr'):
        output = getattr(result, name, None)
        if isinstance(output, str):
            output = output.encode()
        (incident / (name + '.bin')).write_bytes(output or b'')
    transport = getattr(result, 'stdin', b'')
    if transport:
        (incident / 'stdin.bin').write_bytes(transport)
        editor = incident / 'editor_snapshots'
        editor.mkdir()
        pending = transport
        documents = {}
        while b'\r\n\r\n' in pending:
            header, body = pending.split(b'\r\n\r\n', 1)
            try:
                count = int(header.split(b'Content-Length: ', 1)[1].split(b'\r\n', 1)[0])
                if len(body) < count:
                    break
                message = json.loads(body[:count])
                pending = body[count:]
                parameters = message.get('params', {})
                document = parameters.get('textDocument', {})
                if message.get('method') == 'textDocument/didOpen':
                    documents[document['uri']] = (document['text'], document.get('version'))
                if message.get('method') == 'textDocument/didChange':
                    for change in parameters.get('contentChanges', []):
                        if 'range' not in change:
                            documents[document['uri']] = (change['text'], document.get('version'))
            except (ValueError, KeyError, IndexError):
                break
        from urllib.parse import unquote, urlparse
        record['editor_snapshots'] = []
        for serial, (uri, (content, version)) in enumerate(documents.items(), 1):
            original = Path(unquote(urlparse(uri).path))
            copy = editor / f'{serial}_{original.stem}_crash.void'
            raw = content.encode('utf-8')
            with copy.open('xb') as stream:
                stream.write(raw)
            assert copy.read_bytes() == raw
            copy.chmod(0o444)
            record['editor_snapshots'].append({'uri': uri, 'version': version, 'copy': str(copy),
                'sha256': hashlib.sha256(raw).hexdigest()})
    (incident / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    print('CRASH EVIDENCE:', incident, flush=True)


def run(command, *args, **kwargs):
    source_inputs = kwargs.pop('source_inputs', ())
    provenance_inputs = kwargs.pop('provenance_inputs', ())
    evidence_context = kwargs.pop('evidence_context', None)
    expected_returncode = kwargs.pop('expected_returncode', None)
    expected_stdout = kwargs.pop('expected_stdout', None)
    checked = kwargs.pop('check', False)
    previous = getattr(_RUN_CONTEXT, 'active', False)
    _RUN_CONTEXT.active = True
    try:
        # Follow subprocess.run's communication contract while retaining the
        # actual native exit status after timeout termination (also on Windows).
        timeout = kwargs.pop('timeout', None)
        supplied = kwargs.pop('input', None)
        capture = kwargs.pop('capture_output', False)
        if supplied is not None:
            if kwargs.get('stdin') is not None:
                raise ValueError('stdin and input arguments may not both be used')
            kwargs['stdin'] = subprocess.PIPE
        if capture:
            if kwargs.get('stdout') is not None or kwargs.get('stderr') is not None:
                raise ValueError('stdout and stderr may not be used with capture_output')
            kwargs['stdout'] = kwargs['stderr'] = subprocess.PIPE
        with subprocess.Popen(command, *args, **kwargs) as process:
            try:
                stdout, stderr = process.communicate(supplied, timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate()
                result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
                result.timed_out = True
                preserve(command, kwargs.get('cwd'), result, source_inputs, provenance_inputs,
                         failure_kind='timeout', evidence_context=evidence_context,
                         environment=kwargs.get('env'))
                raise subprocess.TimeoutExpired(command, timeout, output=stdout, stderr=stderr)
            except BaseException:
                process.kill()
                process.wait()
                raise
            result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
    finally:
        _RUN_CONTEXT.active = previous
    stderr_bytes = result.stderr or b''
    if isinstance(stderr_bytes, str):
        stderr_bytes = stderr_bytes.encode()
    sanitizer = re.search(rb'(?:ERROR: (?:AddressSanitizer|LeakSanitizer)|'
                          rb'UndefinedBehaviorSanitizer:|'
                          rb'\.(?:cpp|cc|c|h|hpp):\d+(?::\d+)?: runtime error:)', stderr_bytes)
    mismatch = (expected_returncode is not None and result.returncode != expected_returncode or
                expected_stdout is not None and result.stdout != expected_stdout)
    if crashed(result.returncode) or sanitizer or mismatch:
        kind = ('sanitizer_finding' if sanitizer or result.returncode in (86, 87) else
                'abnormal_exit' if crashed(result.returncode) else
                'unexpected_exit' if expected_returncode is not None and result.returncode != expected_returncode
                else 'test_assertion')
        preserve(command, kwargs.get('cwd'), result, source_inputs, provenance_inputs,
                 failure_kind=kind, evidence_context=evidence_context, environment=kwargs.get('env'))
    result.expectation_failed = bool(mismatch)
    if checked:
        result.check_returncode()
    return result


class _CapturedPipe:
    def __init__(self, stream):
        self.stream = stream
        self.chunks = []

    def read(self, *args):
        value = self.stream.read(*args)
        self.chunks.append(value)
        return value

    def readline(self, *args):
        value = self.stream.readline(*args)
        self.chunks.append(value)
        return value

    def __getattr__(self, name):
        return getattr(self.stream, name)

    def captured(self):
        if not self.chunks:
            return b''
        if isinstance(self.chunks[0], str):
            return ''.join(self.chunks).encode()
        return b''.join(self.chunks)


_REAL_POPEN = subprocess.Popen


class _CapturedInput(_CapturedPipe):
    recording = True

    def write(self, data):
        if self.recording:
            self.chunks.append(data)
        return self.stream.write(data)


class _PreservingPopen(_REAL_POPEN):
    def __init__(self, command, *args, **kwargs):
        self._source_command = command
        self._source_cwd = kwargs.get('cwd')
        self._incident_preserved = False
        self._communicating = False
        self._managed_by_run = getattr(_RUN_CONTEXT, 'active', False)
        self._intentional_stop = False
        self._communicated_output = None
        super().__init__(command, *args, **kwargs)
        if self.stdin is not None:
            self.stdin = _CapturedInput(self.stdin)
        for name in ('stdout', 'stderr'):
            pipe = getattr(self, name)
            if pipe is not None:
                setattr(self, name, _CapturedPipe(pipe))

    def wait(self, *args, **kwargs):
        status = super().wait(*args, **kwargs)
        if (crashed(status) and not self._incident_preserved and not self._communicating and
                not self._managed_by_run and not self._intentional_stop):
            from types import SimpleNamespace
            self._incident_preserved = True
            preserve(self._source_command, self._source_cwd, SimpleNamespace(
                returncode=status,
                stdout=self._communicated_output[0] if self._communicated_output else (self.stdout.captured() if self.stdout else b''),
                stderr=self._communicated_output[1] if self._communicated_output else (self.stderr.captured() if self.stderr else b''),
                stdin=self.stdin.captured() if self.stdin else b''))
        return status

    def communicate(self, *args, **kwargs):
        self._communicating = True
        supplied = args[0] if args else kwargs.get('input')
        if supplied is not None and self.stdin is not None:
            # communicate may write via os.write rather than the pipe wrapper.
            self.stdin.chunks.append(supplied)
            self.stdin.recording = False
        try:
            result = super().communicate(*args, **kwargs)
        finally:
            self._communicating = False
            if self.stdin is not None:
                self.stdin.recording = True
        self._communicated_output = result
        if not self._managed_by_run and crashed(self.returncode):
            self.wait()
        return result

    def kill(self):
        self._intentional_stop = True
        return super().kill()

    def terminate(self):
        self._intentional_stop = True
        return super().terminate()


# subprocess.run is handled after communicate, when exact captured output exists.
# Direct Popen clients are handled at wait, before their temporary scope can exit.
def install():
    subprocess.run = run
    subprocess.Popen = _PreservingPopen
