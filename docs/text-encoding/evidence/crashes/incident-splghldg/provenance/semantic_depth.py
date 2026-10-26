"""Recursive frontend syntax must reach its guards before exhausting the stack.

Covers run #38's receiver and run #67's recursive type-parser stack regressions.
Linux children use 1 MiB, matching the default Windows executable stack reserve.
Keep ordinary compiler optimization and the existing nesting limits intact.
"""
import pathlib
import re
import subprocess
import sys
import tempfile


def small_stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))


with tempfile.TemporaryDirectory(prefix="void-semantic-depth-") as directory:
    source = pathlib.Path(directory) / "deep-methods.void"
    prefix = ("value struct point:\n    x: i32\n"
              "func (p point) same() -> point:\n    return p\n"
              "func (p point) inspect() -> i32:\n    return p.x\n"
              "func entry(value: i32) -> i32:\n    return point()")
    # Ordinary chains still compile; malformed deep chains must recover below.
    # A legal chain below the semantic guard must also fit capability analysis.
    # Stage 7 reproduced a pre-existing Debug overflow at 96 calls (including HEAD).
    for count in (16, 64, 96, 124):
        source.write_text(prefix + ".same()" * count + ".inspect()\n", encoding="utf-8")
        accepted = subprocess.run(
            [sys.argv[1], "check", str(source)], capture_output=True,
            encoding="utf-8", errors="strict", timeout=20,
            preexec_fn=small_stack if sys.platform == "linux" else None)
        assert accepted.returncode == 0, (count, accepted.returncode, accepted.stdout, accepted.stderr)
        assert accepted.stderr == "", accepted.stderr
    for suffix in (".inspect()", ".inspect() + 1"):
        text = prefix + ".same()" * 400 + suffix + "\nfunc later():\n    missing()\n"
        source.write_text(text, encoding="utf-8")
        result = subprocess.run(
            [sys.argv[1], "check", str(source)], capture_output=True,
            encoding="utf-8", errors="strict", timeout=20,
            preexec_fn=small_stack if sys.platform == "linux" else None)
        assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
        assert result.stdout == "", result.stdout
        diagnostic = result.stderr
        assert diagnostic.count("code: VOID1107") == 1, diagnostic
        assert diagnostic.count("code: VOID2003") == 1, diagnostic
        assert diagnostic.count(": error:") == 2, diagnostic
        nesting = re.search(r":8:(\d+): error: Source nesting limit exceeded", diagnostic)
        assert nesting, diagnostic
        column = int(nesting.group(1))
        assert text.splitlines()[7][column - 1:column + 3] == "same", diagnostic
        assert ":10:5: error: Unknown function 'missing'" in diagnostic, diagnostic
    print("Deep receiver chains: bounded stack, nesting location and later error verified")

    # Sprint 2 numeric postfix chains share the same guards and stack budget.
    for count in (16, 64, 96, 124, 400):
        text = "func entry(value: i32) -> i32:\n    return value" + ".abs()" * count + "\n"
        if count == 400:
            text += "func later():\n    missing()\n"
        source.write_text(text, encoding="utf-8")
        result = subprocess.run([sys.argv[1], "check", str(source)], capture_output=True,
                                encoding="utf-8", errors="strict", timeout=20,
                                preexec_fn=small_stack if sys.platform == "linux" else None)
        if count < 400:
            assert result.returncode == 0 and result.stderr == "", (count, result.returncode, result.stderr)
        else:
            assert result.returncode == 1, (result.returncode, result.stderr)
            assert result.stderr.count("code: VOID1107") == 1 and result.stderr.count("code: VOID2003") == 1, result.stderr
    print("Numeric receiver chains: legal depth and recovering rejection fit the unchanged 1 MiB budget")

    # Sprint 3 scalar indexing must preserve the same recursive stack contract.
    for count in (16, 64, 96, 124, 400):
        text = 'func entry(value: i32) -> string:\n    return "é"' + '[0]' * count + '\n'
        if count == 400:
            text += "func later():\n    missing()\n"
        source.write_text(text, encoding="utf-8")
        result = subprocess.run([sys.argv[1], "check", str(source)], capture_output=True,
                                encoding="utf-8", errors="strict", timeout=20,
                                preexec_fn=small_stack if sys.platform == "linux" else None)
        if count < 400:
            assert result.returncode == 0 and result.stderr == "", (count, result.returncode, result.stderr)
        else:
            assert result.returncode == 1, (result.returncode, result.stderr)
            assert result.stderr.count("code: VOID1107") == 1 and result.stderr.count("code: VOID2003") == 1, result.stderr
    print("String index chains: legal depth and recovering rejection fit the unchanged 1 MiB budget")

    # New block forms must retain the same bounded, recovering parser on 1 MiB.
    for kind in ("defer", "closure"):
        lines = ["func entry(value: i32) -> i32:"]
        for depth in range(80):
            prefix = "    " * (depth + 1)
            lines.append(prefix + ("defer {" if kind == "defer" else f"var callback_{depth} = func():"))
        lines.append("    " * 81 + "pass")
        if kind == "defer":
            for depth in reversed(range(80)):
                lines.append("    " * (depth + 1) + "}")
        lines.extend(["    var later = func() -> i32:", "        return value", "    return later()", ""])
        source.write_text("\n".join(lines), encoding="utf-8")
        result = subprocess.run([sys.argv[1], "check", str(source)], capture_output=True,
                                encoding="utf-8", errors="strict", timeout=20,
                                preexec_fn=small_stack if sys.platform == "linux" else None)
        assert result.returncode == 1, (kind, result.returncode, result.stdout, result.stderr)
        assert "code: VOID1107" in result.stderr, (kind, result.stderr)

    # CI #67: unused type-grammar temporaries exhausted a Debug 1 MiB stack
    # before the shared 128-level parser guard could reject deep fixed arrays.
    # Every recursive type form must preserve that guard and later recovery.
    forms = {
        "fixed": ("[", ", 0]"), "growable": ("[", "]"),
        "dictionary": ("{i32: ", "}"), "function": ("func() -> ", ""),
        "result": ("result<i32, ", ">"), "generic": ("box<", ">"),
        "parenthesized": ("(", ")"),
    }
    for kind in (*forms, "mixed"):
        def nested_type(count):
            value = "i32"
            for level in range(count):
                form = ("fixed", "function", "dictionary", "result")[level % 4] if kind == "mixed" else kind
                before, after = forms[form]
                value = before + value + after
            return value

        prefix = "struct box<t>:\n    value: t\n" if kind == "generic" else ""
        for count in (8, 160):
            text = prefix + f"func nested(value: {nested_type(count)}):\n    pass\n"
            if count == 160:
                text += "func later():\n    missing()\n"
            source.write_text(text, encoding="utf-8")
            result = subprocess.run([sys.argv[1], "check", str(source)], capture_output=True,
                                    encoding="utf-8", errors="strict", timeout=20,
                                    preexec_fn=small_stack if sys.platform == "linux" else None)
            assert result.returncode == (0 if count == 8 else 1), (kind, count, result.returncode, result.stdout, result.stderr)
            if count == 8:
                assert result.stderr == "", (kind, result.stderr)
            else:
                assert result.stderr.count("code: VOID1107") == 1, (kind, result.stderr)
                assert "Source nesting limit exceeded" in result.stderr, (kind, result.stderr)
                assert result.stderr.count("code: VOID2003") == 1, (kind, result.stderr)
                assert "Unknown function 'missing'" in result.stderr, (kind, result.stderr)
    print("Recursive type grammar: shallow acceptance, bounded stack, nesting diagnostic and later recovery verified")
