"""Expected checked invalidation is distinct from compiler crashes and assertions."""
from pathlib import Path
import sys
import support as h
compiler = Path(sys.argv[1]).resolve()
source = Path(__file__).with_name('mutation.void')
for mode in ([], ['--no-opt']):
    for command in [1, 2, 3, 4, 11, 12, 21, 22, 23, *range(30, 38)]:
        result = h.run([compiler, 'run', source, str(command), '--no-cache', *mode], status=1,
                       context={'mode': 'expected_iterator_diagnostic', 'command': command})
        if b'modified' not in result.stderr.lower():
            h.crash.preserve(result.args, h.ROOT, result, source_inputs=[source],
                             failure_kind='test_assertion', evidence_context={'expected': 'list modification diagnostic'})
            raise AssertionError(result.stderr)
    for command in (7, 10, 20):
        h.run([compiler, 'run', source, str(command), '--no-cache', *mode], expected=b'7\n')
print('Collections expected invalidation and early-return execution passed')
