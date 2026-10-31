"""Expected checked invalidation is distinct from compiler crashes and assertions."""
from pathlib import Path
import sys
import tempfile
import support as h
compiler = Path(sys.argv[1]).resolve()
source = Path(__file__).with_name('mutation.void')
with tempfile.TemporaryDirectory(prefix='VOID-collections-failures-') as directory:
    work = Path(directory)
    for mode in ([], ['--no-opt']):
        obj = work / 'failure.o'; executable = work / 'failure.app'
        h.run([compiler, 'emit', source, '-o', obj, *mode], small=True)
        h.link(compiler, obj, executable, sources=[source])
        for native in (False, True):
            for command in [1, 2, 3, 4, 11, 12, 21, 22, 23, *range(30, 38)]:
                arguments = [executable, str(command)] if native else [compiler, 'run', source, str(command), '--no-cache', *mode]
                result = h.run(arguments, status=1, sources=[source], small=True,
                               context={'mode': 'expected_iterator_diagnostic', 'native': native, 'command': command})
                if b'modified' not in result.stderr.lower():
                    h.crash.preserve(result.args, h.ROOT, result, source_inputs=[source],
                                     failure_kind='test_assertion', evidence_context={'expected': 'list modification diagnostic'})
                    raise AssertionError(result.stderr)
            for command in (7, 10, 20):
                arguments = [executable, str(command)] if native else [compiler, 'run', source, str(command), '--no-cache', *mode]
                h.run(arguments, expected=h.native_text('7\n'), sources=[source], small=True)
print('Collections expected invalidation and early-return execution passed in JIT/O2/linked AOT/O2')
