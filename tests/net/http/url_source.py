"""Source URL API through the shared typed IR, JIT and installed native link path."""
from pathlib import Path
import os
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tests/os"))
import support as h

compiler = Path(sys.argv[1]).resolve()
helpers = "--helpers-only" in sys.argv[2:]
source = ROOT / ("tests/net/http/url_helpers.void" if helpers else "tests/net/http/url.void")
# Source print uses the console's exact UTF-8/LF contract, including Windows
# redirected output. Only CLI-owned trailers use native CRT text newlines.
expected = ("true\n" * (6 if helpers else 17)).encode('utf-8')
with tempfile.TemporaryDirectory(prefix="VOID URL source ") as directory:
    work = Path(directory)
    h.run([compiler, "check", source], work, sources=(source,))
    for mode in ([], ["--no-opt"]):
        h.run([compiler, "run", source, "--no-cache", *mode], work, expected, sources=(source,))
        binary = work / ("url-" + str(bool(mode)) + (".exe" if os.name == "nt" else ""))
        h.run([compiler, "build", source, "-o", binary, *mode], work, sources=(source,))
        h.run([binary], work, expected, sources=(source,))
print(f"URL source check/JIT/O2/AOT: {h.checks} process checks passed")
