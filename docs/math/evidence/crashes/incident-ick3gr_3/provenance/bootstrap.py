"""Reuse the established Random process boundary, with Math evidence and provenance.

Import this before constructing or executing any source fixture. Direct Popen/LSP
clients are covered too; crash preservation completes before temporary cleanup.
"""
from pathlib import Path
import os
import hashlib
import json
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
os.environ['VOID_CRASH_EVIDENCE'] = str(ROOT / 'docs/math/evidence/crashes')
sys.path.insert(0, str(ROOT / 'tests/random'))
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
import harness as base
import crash
from crash import install, run as preserving_run
sys.path.insert(0, str(HERE))
install()

# Keep the compiler identity beside test evidence before any fixture executes.
# Linked apps and LSP subprocesses have their own executable hash in the inherited
# incident receipt; this immutable context correlates them with the compiler.
IDENTITY = None
if len(sys.argv) > 1:
    invoked = Path(sys.argv[1]).resolve()
    compiler = invoked if invoked.name in ('voidc', 'voidc.exe') else invoked.with_name('voidc.exe' if os.name == 'nt' else 'voidc')
    if compiler.is_file():
        digest = hashlib.sha256(compiler.read_bytes()).hexdigest()
        manifest = compiler.parent / 'void-native.txt'
        context = {'compiler': str(compiler), 'compiler_sha256': digest,
                   'native_manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest() if manifest.is_file() else None,
                   'native_manifest': manifest.read_text() if manifest.is_file() else None}
        IDENTITY = ROOT / 'docs/math/evidence' / ('compiler-identity-' + digest + '.json')
        IDENTITY.parent.mkdir(parents=True, exist_ok=True)
        raw = (json.dumps(context, indent=2) + '\n').encode()
        descriptor, temporary = tempfile.mkstemp(prefix='.compiler-identity-', suffix='.tmp', dir=IDENTITY.parent)
        try:
            with os.fdopen(descriptor, 'wb') as stream:stream.write(raw)
            try:
                # Publishing only a completed inode prevents a simultaneous
                # first reader from observing a partially written JSON file.
                os.link(temporary, IDENTITY)
            except FileExistsError:
                assert IDENTITY.read_bytes() == raw, IDENTITY
        finally:
            Path(temporary).unlink()
        # Remove the writable temporary name before Windows read-only marking.
        IDENTITY.chmod(0o444)


def retain(command, *args, **kwargs):
    kwargs['provenance_inputs'] = (*kwargs.get('provenance_inputs', ()), *HERE.glob('*.*'), *((IDENTITY,) if IDENTITY else ()))
    return preserving_run(command, *args, **kwargs)

_original_preserve = crash.preserve

def preserve(command, cwd, result, source_inputs=(), provenance_inputs=()):
    working = Path(cwd or Path.cwd()).resolve()
    # Controlled Math fixtures may contain an edited relocated stdlib. Preserve
    # those actual bytes too, without recursively scanning the repository root.
    controlled = (working != ROOT and 'math' in str(working).lower() and
                  (working.is_relative_to(Path(tempfile.gettempdir()).resolve()) or working.is_relative_to(ROOT / 'build')))
    descendants = tuple(working.rglob('*.void')) if controlled else ()
    return _original_preserve(command, cwd, result,
                              (*source_inputs, *descendants),
                              (*provenance_inputs, *HERE.glob('*.*'), *((IDENTITY,) if IDENTITY else ())))

crash.preserve = preserve
base.preserve = preserve
base.preserving_run = retain
run = base.run

def __getattr__(name):
    return getattr(base, name)
