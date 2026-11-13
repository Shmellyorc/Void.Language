"""Keep enum comparison guidance scoped to the already-typed operands."""

from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory

compiler = Path(sys.argv[1]).resolve()
adventure = Path(__file__).resolve().parent / "fixtures/adventure_instant_keys"
ENUM = "Tagged enum values do not support built-in '==' or '!='."
CASE = "Use 'is' to test this enum case, or 'match' to handle multiple cases."
NEGATED_CASE = "Use a negated 'is' test to exclude this enum case, or 'match' to handle multiple cases."
MATCH = "Use 'match' to inspect the cases and compare any payloads explicitly."
STRUCT = "Value struct equality is not implemented."


def invoke(*args):
    return subprocess.run([compiler, *args], capture_output=True, encoding="utf-8", timeout=60)


def diagnostics(output):
    pattern = r"^.+[/\\]([^/\\\n]+\.void):(\d+):(\d+): error: (.+)\n    code: (VOID\d+)$"
    return [(file, int(line), int(col), message, code)
            for file, line, col, message, code in re.findall(pattern, output, re.M)]


def position(source, target):
    prefix = source[:source.index(target)]
    return prefix.count("\n") + 1, len(prefix.rsplit("\n", 1)[-1]) + 1


def reject(path, source, target, code, message, hint=None):
    path.write_bytes(source.encode("utf-8"))
    result = invoke("check", path)
    assert result.returncode == 1 and not result.stdout, result
    line, col = position(source, target)
    errors = diagnostics(result.stderr)
    assert len(errors) == 1, result.stderr
    file, actual_line, actual_col, actual, actual_code = errors[0]
    assert (file, actual_line, actual_col, actual_code) == (path.name, line, col, code), result.stderr
    assert actual == message, result.stderr
    assert re.findall(r"^    Hint: (.+)$", result.stderr, re.M) == ([hint] if hint else []), result.stderr
    return result.stderr


result = invoke("check", adventure / "main.void")
assert result.returncode == 1 and not result.stdout, result
assert diagnostics(result.stderr) == [
    ("game.void", line, col, ENUM, "VOID2016")
    for line, col in [(27, 28), (29, 33), (31, 33), (33, 33), (35, 33), (37, 33)]
], result.stderr
assert re.findall(r"^    Hint: (.+)$", result.stderr, re.M) == [CASE] * 6, result.stderr

with TemporaryDirectory(prefix="VOID enum diagnostics ") as directory:
    root = Path(directory)
    source = root / "probe.void"
    (root / "events.void").write_text("public enum notice:\n    idle\n    done(amount: i32)\n", encoding="utf-8")
    prefix = '''use "./events.void" as api

func probe(left: api.notice, right: api.notice) -> bool:
    return '''
    for expression, target, hint in (
        ("left == api.notice.idle", "==", CASE),
        ("left != api.notice.idle", "!=", NEGATED_CASE),
        ("left == api.notice.idle()", "==", CASE),
        ("left == right", "==", MATCH),
        ("left != right", "!=", MATCH),
        ("left == api.notice.done(7)", "==", MATCH),
        ("left != api.notice.done(7)", "!=", MATCH),
        ("api.notice.idle == right", "==", MATCH),
    ):
        reject(source, prefix + expression + "\n", target, "VOID2016", ENUM, hint)
    reject(source, prefix + "left == 7\n", "==", "VOID2001",
           "Type mismatch: expected /events.void::notice, received i32.")
    reject(source, '''use "./events.void" as api
enum other:
    idle
func probe(left: api.notice) -> bool:
    return left != other.idle
''', "!=", "VOID2001", "Type mismatch: expected /events.void::notice, received /probe.void::other.")
    reject(source, '''value struct point:
    x: i32
func probe(left: point, right: point) -> bool:
    return left == right
''', "==", "VOID2016", STRUCT)
    for declaration in ("option<i32>", "result<i32, string>"):
        reject(source, f"func probe(left: {declaration}, right: {declaration}) -> bool:\n    return left != right\n",
               "!=", "VOID2016", STRUCT)
    generic = reject(source, '''use console
func compare<t>(left: t, right: t) -> bool:
    return left == right
func main() -> i32:
    if compare<console.key>(console.key.up, console.key.down):
        return 1
    return 0
''', "==", "VOID2016", ENUM, MATCH)
    assert re.search(r"Generic specialization requested here\. at .+[/\\]probe\.void:5:8$", generic, re.M), generic
    assert re.search(r"Generic declaration\. at .+[/\\]probe\.void:2:6$", generic, re.M), generic
    reject(source, '''enum bag:
    present(items: [i32])
func probe(left: bag, right: bag) -> bool:
    return left is right
''', "right\n", "VOID2001", "Use explicit list/dictionary patterns for collection contents.")

    source.write_bytes(b'''use "./events.void" as api
use console

enum signal(u8):
    calm = 0
    noisy = 3

enum flag:
    empty
    full(number: i32)
    operator ==(const left: flag, const right: flag) -> bool:
        return left is flag.empty and right is flag.empty

struct box:
    number: i32

func equal<t>(left: t, right: t) -> bool:
    return left == right

func matching(left: api.notice, right: api.notice) -> bool:
    match left:
        api.notice.idle:
            return right is api.notice.idle
        api.notice.done(var amount):
            if right is api.notice.done(var other_amount):
                return amount == other_amount
            return false

func main() -> i32:
    var value = box(number = 4)
    var same = value
    var other = box(number = 4)
    if not (value == same and value != other):
        return 1
    if not (equal<i32>(2, 2) and 2 != 3 and "up" == "up" and "up" != "down"):
        return 2
    if not (signal.calm == signal.calm and signal.calm != signal.noisy):
        return 3
    if not (flag.empty == flag.empty and flag.empty != flag.full(4)):
        return 4
    var pressed = console.key.up
    if not (pressed is key.up and not (pressed is console.key.down)):
        return 5
    var finished = api.notice.done(7)
    if not (finished is api.notice.done(7) and api.notice.idle is api.notice.idle()):
        return 6
    if not matching(finished, api.notice.done(7)) or matching(finished, api.notice.done(8)):
        return 7
    return 0
''')
    for command in (("check", source), ("run", source, "--no-cache", "--no-opt"), ("run", source, "--no-cache")):
        checked = invoke(*command)
        assert checked.returncode == 0 and not checked.stderr, checked

print("Six Adventure operators, contextual hints, exact positions, and valid equality/pattern execution passed")
