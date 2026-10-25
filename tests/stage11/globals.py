"""Exercise real modules through normal/O2 JIT and independently linked AOT."""
import os
import hashlib
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile

compiler, clang, linker, runtime, fixture, sanitizer = sys.argv[1:]
flags = ["--driver-mode=g++", "-pthread"]
if sys.platform.startswith("linux"):
    flags.append("-ldl")
if linker:
    flags.append("--ld-path=" + linker)
if sanitizer == "ON":
    flags.append("-fsanitize=address,undefined")
if os.name == "nt":
    flags += ["-fms-runtime-lib=dll", "-llegacy_stdio_definitions"]
checks = 0

def run(args, expected=0):
    global checks
    result = subprocess.run(list(map(str, args)), capture_output=True, timeout=120)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    checks += 1
    return result

with tempfile.TemporaryDirectory(prefix="VOID globals ") as directory:
    root = Path(directory)
    def write(name, text):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def execute(name, files, stdout, status=0, error=b""):
        for filename, source in files.items():
            write(filename, source)
        path = root / name
        run([compiler, "check", path])
        for optimize in (False, True):
            switches = [] if optimize else ["--no-opt"]
            result = run([compiler, "run", path, *switches], status)
            assert result.stdout == stdout and error in result.stderr, (name, "JIT", result)
            obj = root / "program.o"
            exe = root / ("program.exe" if os.name == "nt" else "program")
            run([compiler, "emit", path, "-o", obj, *switches])
            run([clang, obj, runtime, fixture, *flags, "-o", exe])
            result = run([exe], status)
            assert result.stdout == stdout and error in result.stderr, (name, "AOT", result)

    execute("selected.void", {
        "selected.void": 'use "./game.void" as game\nvar score = 0\nfunc main():\n    game.reward_player()\n    print(score)\n    print(game.enemies_defeated)\n',
        "game.void": 'var enemies_defeated = 0\nfunc reward_player():\n    score += 10\n    enemies_defeated += 1\n',
    }, b"10\n1\n")
    execute("priority.void", {
        "priority.void": 'use "./priority_helper.void" as helper\nfunc main():\n    print(helper.read())\n',
        "priority_helper.void": 'use "./priority_lib.void"\nconst number = 7\nvalue struct box:\n    item: i32\nfunc value() -> i32:\n    return 8\nfunc make<t>() -> t:\n    return t()\nfunc read() -> i32:\n    return value() + number + box(item = 1).item + priority_lib.value + make<box>().item\n',
        "priority_lib.void": 'var value = 3\nvar number = 4\nvar box = 5\nvar t = 6\n',
    }, b"19\n")
    execute("order.void", {
        "order.void": 'use "./right.void" as right\nuse "./left.void" as left\nprint("script-before")\nvar outer = initialize()\nprint("script-after")\nfunc initialize() -> i32:\n    print("root")\n    return left.value + right.value\n',
        "left.void": 'use "./common.void" as common\nvar value = left_build()\nfunc left_build() -> i32:\n    print("left")\n    return common.count\n',
        "right.void": 'use "./common.void" as other\nvar value = right_build()\nfunc right_build() -> i32:\n    print("right")\n    return other.count\n',
        "common.void": 'var count = initialize()\nfunc initialize() -> i32:\n    print("common")\n    return 4\n',
    }, b"common\nleft\nright\nroot\nscript-before\nscript-after\n")
    execute("cycle.void", {
        "cycle.void": 'use "./a.void" as a\nfunc main():\n    print(a.value)\n',
        "a.void": 'use "./b.void" as b\nvar value = a_build()\nfunc a_build() -> i32:\n    print("a")\n    return 1\nfunc pure() -> i32:\n    return b.pure()\n',
        "b.void": 'use "./a.void" as a\nvar value = b_build()\nfunc b_build() -> i32:\n    print("b")\n    return 2\nfunc pure() -> i32:\n    return 7\n',
    }, b"a\nb\n1\n")
    execute("failure.void", {
        "failure.void": 'var first = read_later()\nvar later = 7\nfunc read_later() -> i32:\n    return later\nfunc main():\n    print("entry-ran")\n',
    }, b"", 1, b"not initialized")
    execute("values.void", {
        "values.void": 'struct box:\n    value: i32\nvalue struct pair:\n    item: box\nvar shared = box(value = 5)\nvar values = [shared]\nvar lookup = {"first": shared}\nvar greeting = "hi é🎮"\nvar record = pair(item = shared)\nvar callback = increment\nfunc increment() -> i32:\n    shared.value += 1\n    return shared.value\nfunc main():\n    var copy = record\n    copy.item.value = 9\n    values[0].value += 1\n    print(callback(), lookup["first"].value, greeting)\n',
    }, "11 11 hi é🎮\n".encode())
    execute("only.void", {"only.void": 'var value = initialize()\nfunc initialize() -> i32:\n    print("only")\n    return 1\n'}, b"only\n")
    execute("defaults.void", {
        "defaults.void": 'struct box:\n    value: i32\nvar number: i64\nvar flag: bool\nvar text: string\nvar optional: box?\nvar list: [i32]\nvar fixed: [i32, 2]\nvar mapping: {string: i32}\nvar outcome: result<i32, string> = Err("stored")\nfunc main():\n    print(number, flag, text.length, optional, list.count, fixed[0], mapping.count)\n    if outcome is Err(var reason):\n        print(reason)\n',
    }, b"0 false 0 null 0 0 0\nstored\n")
    execute("interface.void", {
        "interface.void": 'interface reader:\n    func read() -> i32\nstruct box:\n    value: i32\nfunc (self box) read() -> i32:\n    return self.value\nvar object = box(value = 7)\nvar view: reader = object\nfunc main():\n    object.value += 1\n    print(view.read())\n',
    }, b"8\n")
    execute("value_places.void", {
        "value_places.void": 'value struct pair:\n    first: i32\n    second: i32\nvar state = pair()\nfunc alias_change():\n    state.second = 7\nfunc (self pair) update():\n    self.first = 1\n    alias_change()\n    self.first += 1\nfunc main():\n    state.update()\n    print(state.first, state.second)\n',
    }, b"2 7\n")
    execute("property_places.void", {
        "property_places.void": 'value struct pair:\n    stored: i32\n    other: i32\nvar state = pair()\nproperty (self pair) amount: i32:\n    get:\n        return self.stored\n    set(value):\n        self.stored = value\n        state.other = 9\nfunc main():\n    state.amount = 3\n    state.amount += 1\n    print(state.stored, state.other)\n',
    }, b"4 9\n")
    execute("early_entry.void", {
        "early_entry.void": 'var state = helper()\nfunc helper() -> i32:\n    return main()\nfunc main() -> i32:\n    print("entry-ran")\n    return 0\n',
    }, b"", 1, b"before global initialization")
    execute("cycle_failure.void", {
        "cycle_failure.void": 'use "./a.void" as a\nfunc main():\n    print("entry-ran")\n',
        "a.void": 'use "./b.void" as b\nvar value: i32 = b.read()\nfunc read() -> i32:\n    return value\n',
        "b.void": 'use "./a.void" as a\nvar value: i32 = a.read()\nfunc read() -> i32:\n    return value\n',
    }, b"", 1, b"not initialized")
    execute("aliases.void", {
        "aliases.void": 'use "./counter.void"\nuse "./counter.void" as other\nfunc main():\n    count += 2\n    other.count += 3\n    print(count, counter.count, other.count)\n',
        "counter.void": 'var count = 1\n',
    }, b"6 6 6\n")
    standard = root / "standard"
    shutil.copytree(Path(__file__).resolve().parents[2] / "stdlib", standard)
    (standard / "state.void").write_text('var count = 1\nvar box = 2\ninternal var hidden = 2\nprivate var secret = 3\nfunc bump():\n    count += 1\n', encoding="utf-8")
    prior_standard = os.environ.get("VOID_STDLIB_ROOT")
    os.environ["VOID_STDLIB_ROOT"] = str(standard)
    try:
        execute("standard.void", {"standard.void": 'use state as shared\nfunc main():\n    shared.bump()\n    print(shared.count)\n'}, b"2\n")
        execute("standard_root_type.void", {
            "standard_root_type.void": 'use "./standard_type_helper.void" as helper\nvalue struct box:\n    item: i32\nfunc main():\n    print(helper.read())\n',
            "standard_type_helper.void": 'use state\nfunc read() -> i32:\n    return box(item = 5).item\n',
        }, b"5\n")
        denied = write("standard_root_constant.void", 'use "./standard_value_helper.void" as helper\nconst count = 9\nfunc main():\n    print(helper.read())\n')
        write("standard_value_helper.void", 'use state\nfunc read() -> i32:\n    return count\n')
        ambiguous = run([compiler, "check", denied], 1)
        assert b"Ambiguous" in ambiguous.stderr
        denied = write("standard_denied.void", 'use state as shared\nfunc main():\n    print(shared.hidden)\n')
        run([compiler, "check", denied], 1)
        (standard / "state.void").write_text('func read() -> i32:\n    return project_score\n', encoding="utf-8")
        denied = write("standard_boundary.void", 'use state\nvar project_score = 9\nfunc main():\n    pass\n')
        run([compiler, "check", denied], 1)
    finally:
        if prior_standard is None:
            os.environ.pop("VOID_STDLIB_ROOT", None)
        else:
            os.environ["VOID_STDLIB_ROOT"] = prior_standard
    # Fresh cache executions must never serialize live cells or skip startup.
    for _ in range(2):
        result = run([compiler, "run", root / "only.void", "--cache"])
        assert result.stdout == b"only\n"
    cache_files = list((root / ".void/cache/v1").glob("*"))
    cache_files = [path for path in cache_files if path.is_file() and len(path.name) == 64]
    if cache_files:
        artifact = cache_files[0]
        data = artifact.read_bytes()
        assert data[128:129] == b"1"
        body = data[64:128] + b"0" + data[129:]
        artifact.write_bytes(hashlib.sha256(body).hexdigest().encode() + body)
        repaired = run([compiler, "run", root / "only.void", "--cache"])
        assert repaired.stdout == b"only\n" and b"miss" in repaired.stderr, repaired
    for text in (
        'use "./private.void" as hidden\nfunc main():\n    print(hidden.secret)\n',
        'var required: box\nstruct box:\n    value: i32\n',
        'var item = 1\nfunc main():\n    var item = 2\n',
        'var first = second\nvar second = first\n',
        'struct box:\n    value: i32\nfunc freeze(const item: box) -> box:\n    return item\nvar frozen = freeze(box(value = 1))\nfunc main():\n    frozen.value = 2\n',
        'struct box:\n    value: i32\nvar target = box(value = 1)\nfunc change(const item: box):\n    target = item\nfunc main():\n    pass\n',

        'var value = start()\nasync func start() -> i32:\n    return 1\n',
    ):
        write("private.void", 'private var secret = 1\n')
        source = write("negative.void", text)
        result = run([compiler, "check", source], 1)
        assert result.stderr
print(f"{checks} real-program global commands passed")
