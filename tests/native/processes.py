"""Separate processes relocate integrated native/task code; no live state is cached."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

host, repository = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="void-stage6-") as temporary:
    root = Path(temporary)
    source = root / "main.void"
    (root / "wrappers.void").write_bytes((repository / "examples/native/wrappers.void").read_bytes())
    text = (repository / "examples/native/integrated.void").read_text(encoding="utf-8")
    # A worker's real synchronous frame needs the Windows ABI stack-probe helper.
    text = text.replace("return answer + 1", "return stack(answer) + 1")
    text += "\nfunc stack(value: i32) -> i32:\n" + "".join(
        f'    var root{i} = "native root {i}"\n' for i in range(160)) + "    return value\n"
    source.write_text(text, encoding="utf-8")
    cache = root / "cache"
    cache.mkdir()

    def run(expected=None):
        result = subprocess.run([str(host), "--cached-integrated", str(source), str(cache)],
                                text=True, capture_output=True, timeout=90)
        assert result.returncode == 0, result
        status, value = result.stdout.strip().split(":")
        assert value == "32" and (expected is None or status == expected), result
        return status

    first = run()
    if os.name == "nt":
        assert first == "unavailable", "Windows private cache admission must retain its safe bypass"
    if first == "unavailable":
        run("unavailable")
    else:
        assert first == "miss", first
        run("hit")
        entries = list((cache / ".void/cache/v1").iterdir())
        assert len(entries) == 1, entries
        entries[0].write_bytes(b"incomplete integrated native artifact")
        run("miss")
        # Publication never overwrites a rejected entry; preserve Stage 5 policy.
        run("miss")
        assert entries[0].read_bytes() == b"incomplete integrated native artifact"
        entries[0].unlink()
        run("miss")
        run("hit")
    print("Integrated fresh-process cold/hit/corrupt fallback (or safe bypass): passed")
