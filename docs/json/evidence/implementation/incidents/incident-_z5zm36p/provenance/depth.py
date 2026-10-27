"""Exercise existing accepted/rejected call depth on an actual 1 MiB stack."""
from pathlib import Path
import sys
import tempfile
from crash import run


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))


compiler = Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory(prefix='VOID text depth ') as directory:
    source = Path(directory) / 'nested_text.void'
    for depth, success in [(32, True), (64, False), (124, False), (400, False)]:
        expression = 'take(' * depth + 'text.to_lower("ΟΣ")' + ')' * depth
        source.write_text('use text\nprivate func take(value: string) -> string:\n    return value\n'
                          'func main():\n    var answer = ' + expression + '\n')
        result = run([compiler, 'check', source], cwd=directory, capture_output=True,
                     timeout=120, preexec_fn=stack)
        assert result.returncode == (0 if success else 1), (depth, result.returncode, result.stderr)
        if not success:
            assert b'nesting' in result.stderr.lower(), result.stderr
        print(f'1 MiB call depth {depth}: ' + ('accepted' if success else 'checked rejection'))
