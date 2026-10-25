"""Compiler inference and explicit global types on an actual 1 MiB POSIX stack."""
from pathlib import Path
import resource
import subprocess
import sys
import tempfile

def small_stack():
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))

with tempfile.TemporaryDirectory(prefix="VOID global stack ") as directory:
    source = Path(directory) / "main.void"
    source.write_text(''.join(f'var item_{n} = item_{n+1}\n' for n in range(300)) + 'var item_300 = 1\nfunc main():\n    pass\n')
    result = subprocess.run([sys.argv[1], "check", str(source)], preexec_fn=small_stack, capture_output=True, timeout=60)
    assert result.returncode == 1 and b"limit" in result.stderr.lower(), (result.returncode, result.stderr)
    source.write_text(''.join(f'var item_{n}: i32 = item_{n+1}\n' for n in range(300)) + 'var item_300: i32 = 1\nfunc main():\n    pass\n')
    result = subprocess.run([sys.argv[1], "check", str(source)], preexec_fn=small_stack, capture_output=True, timeout=60)
    assert result.returncode == 0, (result.returncode, result.stderr)
print("1 MiB inferred-global guard and explicit-type chain passed")
