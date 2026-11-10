"""Cross-mode tests for the eight integer backings and fallible cast boundaries."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

compiler, evidence = (Path(name).resolve() for name in sys.argv[1:])
limits = {
    f"{sign}{width}": (-(1 << (width - 1)), (1 << (width - 1)) - 1) if sign == "i" else (0, (1 << width) - 1)
    for sign in ("i", "u") for width in (8, 16, 32, 64)
}
records = []
cases = 0


def invoke(argv, directory, expected=0, stack_bytes=None):
    def limit_stack():
        import resource
        _, hard = resource.getrlimit(resource.RLIMIT_STACK)
        resource.setrlimit(resource.RLIMIT_STACK, (stack_bytes, hard))

    result = subprocess.run(list(map(str, argv)), cwd=directory, capture_output=True, timeout=150,
                            preexec_fn=limit_stack if stack_bytes else None)
    row = {"argv": list(map(str, argv)), "status": result.returncode,
           "stdout": result.stdout.decode("utf-8", "replace"), "stderr": result.stderr.decode("utf-8", "replace")}
    if stack_bytes:
        row["stack_bytes"] = stack_bytes
    records.append(row)
    assert result.returncode == expected, row
    return row


def integer_cases():
    global cases
    declarations, calls = [], []
    for src, (src_min, src_max) in limits.items():
        for dest, (dest_min, dest_max) in limits.items():
            name = f"cast_{src}_{dest}"
            declarations.append(f"func {name}(value: {src}) -> option<{dest}>:\n    return value as? {dest}\n")
            points = {src_min, src_max, -1, 0, 1, dest_min - 1, dest_min, dest_max, dest_max + 1}
            for value in sorted(point for point in points if src_min <= point <= src_max):
                pattern = f"Some({value})" if dest_min <= value <= dest_max else "None"
                calls.append(f"    if not ({name}({value}) is {pattern}):\n        return 11")
                cases += 1
    return declarations, calls


def enum_cases():
    global cases
    declarations, calls = [], []
    for dest, (minimum, maximum) in limits.items():
        # Sparse endpoints. A signed enum here excludes zero, including in inactive storage.
        first = minimum
        declarations += [f"enum enum_{dest}({dest}):\n    first = {first}\n    last = {maximum}\n"]
        for src, (src_min, src_max) in limits.items():
            name = f"decode_{src}_{dest}"
            declarations += [f"func {name}(value: {src}) -> option<enum_{dest}>:\n    return value as? enum_{dest}\n"]
            for value in sorted({first, maximum, first - 1, first + 1, maximum + 1, -1, 0}):
                if not src_min <= value <= src_max:
                    continue
                pattern = f"Some(enum_{dest}.first)" if value == first else f"Some(enum_{dest}.last)" if value == maximum else "None"
                calls.append(f"    if not ({name}({value}) is {pattern}):\n        return 12")
                cases += 1
    return declarations, calls


interaction = """use "./modes.void" as modes

private struct option<t>:
    value: t

struct record:
    mode: modes.mode = modes.mode.quiet
    previous: modes.mode

struct owner:
    state: modes.mode
    history: [modes.mode]

struct initialized:
    state: modes.mode = modes.mode.quiet

func (self initialized) init():
    if self.state == modes.mode.loud:
        self.state = modes.mode.quiet

func identity<t>(value: t) -> t:
    return value

var evaluations: i32 = 0
const one = 255 as? modes.mode
const hole = 4 as? modes.mode
const maximum = 18446744073709551615 as? u64
const from_i64 = 2147483648 as? i64
const from_i32 = 2147483647 as? i32
const lower = -9223372036854775808 as? i64
const too_narrow = -129 as? i8

func next(value: i32) -> i32:
    evaluations += 1
    return value

func main() -> i32:
    if not (one is Some(modes.mode.loud)) or not (hole is None):
        return 1
    if not (maximum is Some(18446744073709551615)) or not (lower is Some(-9223372036854775808)):
        return 2
    if not (from_i64 is Some(2147483648)) or not (from_i32 is Some(2147483647)) or not (too_narrow is None):
        return 3
    var loud = modes.mode.quiet
    if identity(next(255) as? modes.mode) is Some(var converted):
        loud = converted
    else:
        return 4
    if not (next(-1) as? modes.mode is None) or evaluations != 2 or loud != modes.mode.loud:
        return 5
    var shadow: option<i32> = option<i32>(value = 21)
    if shadow.value != 21 or modes.score(loud) != 19 or modes.score_cast(next(255) as? modes.mode) != 19:
        return 6
    var defaults = initialized()
    if defaults.state != modes.mode.quiet or not (modes.default_score() is None):
        return 10
    var data = record(previous = loud)
    var recent: [modes.mode, 2] = [data.mode, data.previous]
    var object = owner(state = recent[0], history = [recent[1]])
    var table: {i32: modes.mode} = {1: object.history[0]}
    for i in 0..<4000:
        var transient = owner(state = modes.mode.quiet, history = [modes.mode.loud])
        if transient.state != modes.mode.quiet:
            return 7
    if object.state != modes.mode.quiet or table[1] != modes.mode.loud:
        return 8
    var existing: i16 = 7
    if not (existing as i64 as? u8 is Some(7)):
        return 9
    return 0
"""
modes = """public enum mode(u8):
    quiet = 3
    loud = 255

public func score(value: mode) -> i32:
    return match value:
        mode.quiet: 7
        mode.loud: 19

struct maybe_state:
    cached: option<mode> = None

func (self maybe_state) init():
    if self.cached is Some(mode.loud):
        self.cached = None

public func default_score() -> option<mode>:
    return maybe_state().cached

public func score_cast<t>(value: option<t>) -> i32:
    if value is Some(var decoded):
        return score(decoded)
    return 0
"""

storage = """enum mode(u8):
    ready = 7
    done = 255

var initializations: i32 = 0
var cleanups: i32 = 0

value struct stamp:
    state: mode = mode.ready

func (self stamp) init():
    if self.state == mode.ready:
        initializations += 1
    self.state = mode.done

struct holder:
    rows: [[stamp, 2], 3]
    absent: option<mode>

func count_cleanup():
    cleanups += 1

func select_mode() -> mode:
    defer count_cleanup()
    return mode.done

func main() -> i32:
    var rows: [[stamp, 2], 3]
    var saved = holder(rows = rows, absent = None)
    var count: i32 = 0
    for row in saved.rows:
        for cell in row:
            if cell.state != mode.done:
                return 1
            count += 1
    if count != 6 or initializations != 6 or not (saved.absent is None):
        return 2
    if select_mode() != mode.done or cleanups != 1:
        return 3
    return 0
"""

try:
    with tempfile.TemporaryDirectory(prefix="VOID S6 numeric enum ") as temporary:
        root = Path(temporary).resolve()
        integer_functions, integer_checks = integer_cases()
        enum_functions, enum_checks = enum_cases()
        boundary = "\n".join(integer_functions + enum_functions)
        # Split checks into bounded activations for Windows Debug stack and diagnostics.
        checks = integer_checks + enum_checks
        chunks = [checks[start:start+32] for start in range(0, len(checks), 32)]
        for n, chunk in enumerate(chunks):
            boundary += f"\nfunc boundary_{n}() -> i32:\n" + "\n".join(chunk) + "\n    return 0\n"
        boundary += "\nfunc main() -> i32:\n" + "".join(
            f"    if boundary_{n}() != 0:\n        return {20+n}\n" for n in range(len(chunks))) + "    return 0\n"
        (root / "modes.void").write_text(modes, encoding="utf-8")
        for name, source in [("matrix", boundary), ("integration", interaction), ("storage", storage)]:
            file = root / (name + ".void")
            file.write_text(source, encoding="utf-8")
            for optimization in (["--no-opt"], []):
                mode = "normal" if optimization else "o2"
                row = invoke([compiler, "run", file, "--no-cache", *optimization], root)
                assert not row["stderr"], row
                output = root / (name + "-" + mode + (".exe" if os.name == "nt" else ""))
                invoke([compiler, "build", file, "-o", output, *optimization], root)
                invoke([output], root)
        file = root / "integration.void"
        if sys.platform == "linux":
            for optimization in (["--no-opt"], []):
                invoke([compiler, "run", file, "--no-cache", *optimization], root, stack_bytes=1024 * 1024)
        # Persistent native cache publication is unavailable on Windows.
        # Require that exact result and confirm both runs still execute correctly.
        windows = os.name == "nt"
        for expected_cache in (("unavailable", "unavailable") if windows else ("miss", "hit")):
            row = invoke([compiler, "run", file, "--cache"], root)
            assert f"Debug cache: {expected_cache}" in row["stderr"], row
        # A discriminant-only edit changes the compiled semantics and cache key.
        helper = root / "modes.void"
        helper.write_text(modes.replace("loud = 255", "loud = 254"), encoding="utf-8")
        row = invoke([compiler, "run", file, "--cache"], root, 1)
        expected_cache = "unavailable" if windows else "miss"
        assert f"Debug cache: {expected_cache}" in row["stderr"], row
        if windows:
            assert not (root / ".void" / "cache").exists(), "Windows published a native cache"

        prefix = "enum mode(u8):\n    idle = 1\n"
        invalid = [
            ("duplicate name", prefix + "    idle = 2\n", "Duplicate enum"),
            ("duplicate value", prefix + "    ready = +1\n", "Duplicate numeric enum"),
            ("duplicate mathematical zero", "enum mode(i8):\n    idle = -0\n    ready = 0\n", "Duplicate numeric enum"),
            ("invalid backing", "enum mode(f32):\n    idle = 1\n", "fixed-width integer"),
            ("negative unsigned", "enum mode(u64):\n    idle = -1\n", "unsigned"),
            ("payload", prefix + "    ready(x: i32)\n", "Expected '='"),
            ("implicit", prefix + "    ready\n", "Expected '='"),
            ("arithmetic member", prefix + "    ready = 2 + 3\n", "Expected end of statement"),
            ("float cast", "func main():\n    var x = 1.0 as? i32\n", "as? requires an integer"),
            ("bool cast", "func main():\n    var x = true as? i32\n", "as? requires an integer"),
            ("tagged cast", "enum tag:\n    idle\nfunc main():\n    var x = 1 as? tag\n", "as? requires an integer"),
            ("enum extraction", prefix + "func main():\n    var x = mode.idle as? u8\n", "as? requires an integer"),
            ("unrelated enums", prefix + "enum other(u8):\n    idle = 1\nfunc main():\n    var x: mode = other.idle\n", "Type mismatch"),
            ("enum arithmetic", prefix + "func main():\n    var x = mode.idle + mode.idle\n", "Type mismatch"),
            ("implicit conversion", prefix + "func main():\n    var x: mode = 1\n", "Type mismatch"),
            ("uninitialized enum", prefix + "func main():\n    var x: mode\n", "explicit initializer"),
            ("default enum array", prefix + "func main():\n    var x: [mode, 2]\n", "explicit values"),
            ("shadow is nominal", "struct option<t>:\n    value: t\nfunc main():\n    var x: option<i8> = 1 as? i8\n", "Type mismatch"),
            ("None needs context", "func main():\n    var x = None\n", "contextual option"),
            ("legacy as still checks", "func main():\n    var x = 256 as u8\n", "outside the u8 range"),
            ("literal past unsigned domain", "func main():\n    var x = 18446744073709551616 as? u64\n", "fixed-width integer domain"),
            ("literal below signed domain", "func main():\n    var x = -9223372036854775809 as? i64\n", "fixed-width integer domain"),
            ("split operator", "func main():\n    var x = 1 as ? i32\n", "without an intervening space"),
        ]
        for kind, (lo, hi) in limits.items():
            for value in (lo - 1, hi + 1):
                invalid.append((f"{kind} bound {value}", f"enum mode({kind}):\n    invalid = {value}\n", "range"))
        for n, (label, source, message) in enumerate(invalid):
            file = root / f"invalid_{n}.void"
            file.write_text(source + ("\nfunc main():\n    pass\n" if "func main" not in source else ""), encoding="utf-8")
            row = invoke([compiler, "check", file], root, 1)
            assert message in row["stderr"], (label, row)
    print(f"S6 {cases} boundary pairs, {len(invalid)} invalid sources and {len(records)} executed processes")
finally:
    evidence.write_text(json.dumps({"boundary_cases": cases, "records": records}, indent=2) + "\n", encoding="utf-8")
