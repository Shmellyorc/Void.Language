"""Portable typed field and bounded configuration controls through real native code."""

import os
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory

compiler, resources = (Path(argument).resolve() for argument in sys.argv[1:])
here = Path(__file__).resolve().parent
examples = here.parents[1] / "examples/json"
windows = os.name == "nt"
version, clang, linker, *libraries = resources.read_text(encoding="utf-8").splitlines()
assert version == "VOID native 23.1.4 v2"
flags = ["--driver-mode=g++"]
if windows:
    sdk_linker = Path(clang).with_name("lld-link.exe").resolve()
    flags += ["-B" + str(sdk_linker.parent), "-fuse-ld=lld"]
else:
    sdk_linker = Path(linker)
    flags += ["--ld-path=" + linker]


def limit_stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    assert resource.getrlimit(resource.RLIMIT_STACK) == (1048576, 1048576)


def invoke(args, work, status=0, expected=None):
    result = subprocess.run(list(map(str, args)), cwd=work, capture_output=True, timeout=180,
                            preexec_fn=None if windows else limit_stack)
    assert result.returncode == status, (args, result)
    assert not any(marker in result.stderr for marker in
                   (b"AddressSanitizer", b"LeakSanitizer", b"runtime error:", b"VOID IR:")), result
    if expected is not None:
        actual = result.stdout.replace(b"\r\n", b"\n") if windows else result.stdout
        assert actual == expected, (args, actual, expected, result.stderr)
        assert not result.stderr, (args, result.stderr)
    return result


def independent_image(source, work, options):
    obj = work / (source.stem + (".obj" if windows else ".o"))
    image = work / (source.stem + (".exe" if windows else ".bin"))
    invoke([compiler, "emit", source, "-o", obj, *options], work)
    argv = [clang, *flags, obj, *libraries, "-o", image]
    if windows:
        plan = invoke([*argv, "-###"], work).stderr.decode("utf-8")
        launch = re.search(r'^\s*"([^"]*lld-link(?:\.exe)?)"', plan, re.M | re.I)
        assert launch and Path(launch[1].replace("\\\\", "\\")).with_suffix(".exe").resolve() == sdk_linker, plan
    assert not invoke(argv, work).stderr
    return [image]


def negative_schemas(work):
    cases = {
        "wrong_fallback": ('', 'json.get_or<u8>(tree, "absent", "wrong")', b"Type mismatch"),
        "readonly_array": (
            '    var owned: [i32] = [3]\n    const frozen = owned\n',
            'json.get_or<[i32]>(tree, "absent", frozen)', b"readonly"),
        "readonly_struct": (
            '    var owned = data(amount = 3)\n    const frozen = owned\n',
            'json.get_or<data>(tree, "absent", frozen)', b"readonly"),
        "private_schema": ('', 'json.get_or<private_data>(tree, "absent", seed)', b"JSON does not support"),
        "initializer": ('', 'json.get_or<constructed>(tree, "absent", prepared)', b"custom initializers are excluded"),
    }
    header = ("use json\nstruct data:\n    amount: i32\n"
              "struct private_data:\n    private hidden: i32\n"
              "struct constructed:\n    stored: i32\n"
              "func (self constructed) init():\n    self.stored = 1\n"
              "func probe(tree: json.value, seed: private_data, prepared: constructed):\n")
    for name, (setup, call, message) in cases.items():
        source = work / (name + ".void")
        source.write_bytes((header + setup + "    var result_value = " + call + "\n").encode())
        error = invoke([compiler, "check", source], work, status=1).stderr
        assert message.lower() in error.lower(), (name, error)
        if name == "private_schema":
            assert b"inaccessible" in error and b"hidden" in error, error
    print(f"{len(cases)} static fallback/schema rejection controls passed", flush=True)


def json_error(code, raw, token, pointer):
    return f"json {code}@{raw.index(token)}:{pointer}\n".encode()


def configuration_cases(work):
    default = '{"player":"初雪","audio":{}}'.encode()
    explicit = '{"player":"初雪","audio":{"music":false,"volume":25,"future":null},"future":[1]}'.encode()
    missing = b'{"audio":{}}'
    no_audio = b'{"player":"A"}'
    null = '{"player":"初雪","audio":{"music":null}}'.encode()
    wrong = b'{"player":"A","audio":{"music":"false"}}'
    range_error = '{"player":"初雪","audio":{"volume":256}}'.encode()
    invalid_unicode = b'{"player":"\\uD800","audio":{}}'
    duplicate = b'{"player":"A","audio":{"music":true,"\\u006dusic":false}}'
    excess_nodes = b'{"player":"A","audio":{},"extra":[' + b','.join([b'0'] * 61) + b']}'
    io_error = b"input /@stdlib/io.void::error_kind."
    loaded = "初雪 music=true volume=60\n".encode()
    cases = [
        ("defaults", default, 0, loaded),
        ("present", explicit, 0, "初雪 music=false volume=25\n".encode()),
        ("required_name", missing, 2, b"json 4@0:/player\n"),
        ("required_audio", no_audio, 2, b"json 4@0:/audio\n"),
        ("null", null, 2, json_error(6, null, b"null", "/audio/music")),
        ("wrong", wrong, 2, json_error(6, wrong, b'"false"', "/audio/music")),
        ("range", range_error, 2, json_error(7, range_error, b"256", "/audio/volume")),
        ("name_validation", b'{"player":"","audio":{}}', 3, b"validation Player name must not be empty\n"),
        ("volume_validation", b'{"player":"A","audio":{"volume":101}}', 3,
         b"validation Volume must be between 0 and 100\n"),
        ("unicode", invalid_unicode, 2, json_error(2, invalid_unicode, b"\\uD800", "/player")),
        ("duplicate", duplicate, 2, json_error(3, duplicate, b'"\\u006dusic"', "/audio/music")),
        ("utf8", b'{"player":"\xc0\xaf","audio":{}}', 1, io_error + b"invalid_utf8\n"),
        ("maximum", default.ljust(4096), 0, loaded),
        ("oversized", default.ljust(4097), 1, io_error + b"limit_exceeded\n"),
        ("nodes", excess_nodes, 2, f"json 12@{excess_nodes.rindex(b'0')}:/extra/60\n".encode()),
    ]
    for name, content, status, expected in cases:
        path = work / (name + ".json")
        path.write_bytes(content)
        yield path, status, expected


with TemporaryDirectory(prefix="VOID configuration ") as directory:
    work = Path(directory).resolve()
    assert b"LLVM 23.1.4" in invoke([compiler, "--version"], work).stdout
    assert invoke([clang, "--version"], work).stdout.startswith(b"clang version 23.1.4")
    assert invoke([sdk_linker, "--version"], work).stdout.startswith(b"LLD 23.1.4")
    negative_schemas(work)
    fields = here / "configuration_fields.void"
    example = examples / "configuration.void"
    inputs = list(configuration_cases(work))
    # Both AOT lanes run the full file matrix. Keep the repeated JIT launches
    # focused on shared boundary controls. The field fixture runs in all modes.
    jit_inputs = {"defaults", "present", "required_name", "null", "range", "utf8", "maximum", "oversized"}
    for mode, options in (("normal", ["--no-opt"]), ("optimized", [])):
        lane = work / mode
        lane.mkdir(mode=0o700)
        fields_image = independent_image(fields, lane, options)
        example_image = independent_image(example, lane, options)
        for engine, field_command, example_command in (
            ("JIT", [compiler, "run", fields, "--no-cache", *options],
             [compiler, "run", example, "--no-cache", *options, "--"]),
            ("AOT", fields_image, example_image),
        ):
            invoke(field_command, lane, expected=b"Typed defaults, ownership, located errors and budgets verified\n")
            selected = inputs if engine == "AOT" else [case for case in inputs if case[0].stem in jit_inputs]
            for path, status, expected in selected:
                invoke([*example_command, "check", path], lane, status=status, expected=expected)
            save = lane / (engine + " save")
            loaded = "初雪 music=true volume=60\n".encode()
            invoke([*example_command, "save", save], lane, expected=loaded)
            saved = save / "game.json"
            assert saved.read_bytes() == '{"player":"初雪","audio":{"music":true,"volume":60}}'.encode()
            assert {child.name for child in save.iterdir()} == {"game.json", ".void-save-lock"}
            invoke([*example_command, "check", saved], lane, expected=loaded)
            print(f"{mode} {engine}: field controls, {len(selected)} configurations and atomic reload passed", flush=True)
        invoke([compiler, "run", examples / "validated.void", "--no-cache", *options], lane,
               expected=b'{"score":7}\nScore must be nonnegative\n')
