"""Exercise the real CLI boundary with strict UTF-8 output decoding."""
import pathlib
import subprocess
import sys
import tempfile


def main(compiler):
    cases = [
        (b"\x80", r"\x80"),
        (b"\xbf", r"\xBF"),
        (b"\xc0\xaf", r"\xC0\xAF"),
        (b"\xe0\x80\xaf", r"\xE0\x80\xAF"),
        (b"\xed\xa0\x80", r"\xED\xA0\x80"),
        (b"\xf4\x90\x80\x80", r"\xF4\x90\x80\x80"),
        (b"\xf5\x80\x80\x80", r"\xF5\x80\x80\x80"),
        (b"\xff", r"\xFF"),
        (b"\xe2(\xa1", r"\xE2(\xA1"),
        (b"\xe2", r"\xE2"),
        (b"\xe2\x82", r"\xE2\x82"),
        (b"\xf0\x9f", r"\xF0\x9F"),
    ]
    count = 0
    with tempfile.TemporaryDirectory(prefix="void utf8 diagnostics ") as folder:
        root = pathlib.Path(folder)
        source = root / "invalid.void"
        diagnostic_path = source.resolve().as_posix()
        output = root / "protected.o"
        sentinel = b"previous valid object\n"
        for newline in (b"\n", b"\r\n", b"\r"):
            for index, (bad, escaped) in enumerate(cases):
                # Alternate BOMs; truncated cases end exactly at EOF. Other cases
                # also prove valid non-ASCII bytes after damage stay unchanged.
                suffix = " é😀" if index < 9 else ""
                prefix = "# é😀 "
                raw = (b"\xef\xbb\xbf" if index % 2 else b"") + b"# header" + newline
                raw += prefix.encode("utf-8") + bad + suffix.encode("utf-8")
                source.write_bytes(raw)
                for flags in ([], ["--no-opt"]):
                    for command in ("check", "run", "emit"):
                        output.write_bytes(sentinel)
                        arguments = ["10"] if command == "run" else ["-o", str(output)] if command == "emit" else []
                        result = subprocess.run([compiler, command, str(source), *arguments, *flags],
                                                capture_output=True, timeout=10)
                        # Deliberately no replacement/backslash error handler:
                        # either stream containing malformed UTF-8 fails the test.
                        stdout = result.stdout.decode("utf-8", errors="strict")
                        stderr = result.stderr.decode("utf-8", errors="strict").replace("\r\n", "\n")
                        assert result.returncode == 1 and stdout == "", (command, result.returncode, stdout, stderr)
                        assert stderr.startswith(f"{diagnostic_path}:2:6: error: Invalid UTF-8 source.\n"), stderr
                        assert "code: VOID1001\n" in stderr, stderr
                        assert f"\n 2 | {prefix}{escaped}{suffix}\n   |      ^^^^\n" in stderr, stderr
                        assert output.read_bytes() == sentinel, "invalid source replaced output"
                        assert source.read_bytes() == raw, "diagnostics modified original source bytes"
                        count += 1
        # Same-line BOM omission and a different scalar column, including damage
        # at the first byte and an invalid continuation byte after valid Unicode.
        for prefix, bad, column in [(b"", b"\xff", 1), ("# é😀 ".encode(), b"\x80", 6)]:
            source.write_bytes(b"\xef\xbb\xbf" + prefix + bad)
            result = subprocess.run([compiler, "check", str(source)], capture_output=True, timeout=10)
            text = result.stderr.decode("utf-8", errors="strict").replace("\r\n", "\n")
            assert result.stdout.decode("utf-8", errors="strict") == "" and result.returncode == 1
            assert text.startswith(f"{diagnostic_path}:1:{column}: error:"), text
            assert "\ufeff" not in text and "^^^^\n" in text, text
            count += 1
    print(f"Strict UTF-8 CLI diagnostics passed: {count} check/run/emit probes")


if __name__ == "__main__":
    main(sys.argv[1])
