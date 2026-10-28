"""Automatic miss/hit/bypass stores code while each process initializes fresh state."""
from pathlib import Path
import sys
import tempfile
import support as h
compiler = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID-collections-cache-') as directory:
    work = Path(directory); source = work / 'cached.void'
    source.write_text(Path(__file__).with_name('reload.void').read_text().replace('func entry(command: i32) -> i32:', 'func exercise(command: i32) -> i32:') + '\nfunc main():\n    exercise(0)\n    print(exercise(1))\n    print(exercise(4))\n')
    cache = work / '.void/cache/v1'
    for switches in (['--no-opt'], []):
        before = set(cache.iterdir()) if cache.exists() else set()
        result = h.run([compiler, 'run', source, *switches], expected=b'301\n7\n', small=True)
        try:
            artifacts = set(cache.iterdir()) if cache.exists() else set()
            assert len(artifacts - before) == 1, 'automatic cache did not publish once'
            frozen = {path: (path.stat().st_mtime_ns, path.read_bytes()) for path in artifacts}
            result = h.run([compiler, 'run', source, *switches], expected=b'301\n7\n', small=True)
            assert all((path.stat().st_mtime_ns, path.read_bytes()) == value for path, value in frozen.items()), 'cache hit republished code'
            result = h.run([compiler, 'run', source, '--no-cache', *switches], expected=b'301\n7\n', small=True)
            assert set(cache.iterdir()) == artifacts, 'cache bypass published code'
        except AssertionError as error:
            h.crash.preserve(result.args, h.ROOT, result, source_inputs=[work], provenance_inputs=[Path(__file__)],
                             failure_kind='test_assertion', evidence_context={'cache_assertion': str(error)})
            raise
print('Collections automatic cache miss/hit/bypass and fresh captured-callback/equality state passed')
