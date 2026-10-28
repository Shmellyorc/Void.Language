"""Adjacent existing source suites through the established preservation helper."""
from pathlib import Path
import sys
import support as h
build = Path(sys.argv[1]).resolve()
for name, folder in [('generics','generics'), ('advanced','advanced'), ('collections','collections'), ('reference_values','reference_values')]:
    executable = build / ('void_' + name + '_tests')
    result = h.run([executable, h.ROOT], sources=[h.ROOT / 'examples' / folder, h.ROOT / 'tests' / folder],
                   context={'mode':'adjacent-source-regression','suite':name}, timeout=900)
    print(result.stdout.decode(), end='', flush=True)
    print(name + ' passed', flush=True)
print('Four adjacent existing source suites passed')
