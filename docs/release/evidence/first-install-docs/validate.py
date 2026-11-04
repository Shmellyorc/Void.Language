#!/usr/bin/env python3
"""Static first-install documentation checks. No builds, downloads or VOID execution.

Run from any directory: python3 docs/release/evidence/first-install-docs/validate.py
The result is written beside this script; the protected README patch is dry-run only.
"""
from pathlib import Path
import ast
import hashlib
import json
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = Path(__file__).resolve().parent
START = json.loads((EVIDENCE / "start.json").read_text())
GUIDE = "docs/SOURCE_COMPILER.md"
HANDOFF = "docs/release/PREVIEW_QUALIFICATION_HANDOFF.md"
BLOCKERS = "docs/release/PREVIEW_BLOCKERS.md"
WINDOWS = "docs/release/WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md"
PATCH = "docs/release/proposals/README_LLVM23_CLI.patch"
CHANGED = [GUIDE, HANDOFF, BLOCKERS, WINDOWS, PATCH, *[
    "docs/release/evidence/first-install-docs/" + name for name in
    ("start.json", "windows-import-source.json", "validate.py", "validation.json", "inventory.json")]]
PREVIOUS = json.loads((EVIDENCE / "validation.json").read_text()) if (EVIDENCE / "validation.json").exists() else {}
HISTORY = PREVIOUS.get("attempts", [])
REPORT = {"scope": "static documentation/source/retained-receipt validation only",
          "compiler_build_download_windows_and_LSP_execution": "NOT RUN",
          "checks": [], "commands": []}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def passed(name, detail):
    REPORT["checks"].append({"name": name, "result": "PASS", "detail": detail})


def command(argv, input_text=None):
    result = subprocess.run(argv, cwd=ROOT, input=input_text, text=True, capture_output=True)
    REPORT["commands"].append({"argv": argv, "exit_code": result.returncode,
        "stdin_sha256": hashlib.sha256(input_text.encode()).hexdigest() if input_text else None,
        "stdout": result.stdout, "stderr": result.stderr})
    assert result.returncode == 0, (argv, result.returncode, result.stdout, result.stderr)
    return result.stdout


def apply_unified_diff(original, patch):
    source = original.splitlines(keepends=True)
    lines = patch.splitlines(keepends=True)
    assert lines[:2] == ["--- a/README.md\n", "+++ b/README.md\n"]
    output, cursor, index = [], 0, 2
    while index < len(lines):
        header = re.fullmatch(r"@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@[^\n]*\n", lines[index])
        assert header, lines[index]
        start = max(0, int(header[1]) - 1)
        assert start >= cursor
        output.extend(source[cursor:start]); cursor = start; index += 1
        while index < len(lines) and not lines[index].startswith("@@ "):
            line = lines[index]; index += 1
            if line[0] in " -":
                assert source[cursor] == line[1:], (cursor, line)
                cursor += 1
            if line[0] in " +": output.append(line[1:])
            assert line[0] in " +-", line
    output.extend(source[cursor:])
    return "".join(output)


def anchors(text):
    found = set(); counts = {}
    for heading in re.findall(r"^#{1,6} (.+?)\s*#*\s*$", text, re.M):
        heading = re.sub(r"<[^>]*>", "", heading).lower().replace("`", "")
        slug = re.sub(r"[^\w -]", "", heading).replace(" ", "-")
        count = counts.get(slug, 0); counts[slug] = count + 1
        found.add(slug + ("-" + str(count) if count else ""))
    return found


try:
    assert command(["git", "rev-parse", "HEAD"]).strip() == START["head"]
    assert command(["git", "branch", "--show-current"]).strip() == "main"
    command(["git", "diff", "--cached", "--exit-code"])
    modified = command(["git", "diff", "--name-only"]).splitlines()
    assert sorted(modified) == sorted([GUIDE, HANDOFF, BLOCKERS]), modified
    status = command(["git", "status", "--porcelain=v1", "--untracked-files=all"])
    changed = sorted(line[3:] for line in status.splitlines())
    assert changed == sorted(CHANGED), changed
    for name, before in START["input_sha256"].items():
        if name not in (GUIDE, HANDOFF, BLOCKERS): assert digest(ROOT / name) == before, name
    old = command(["git", "show", "HEAD:" + HANDOFF]).split("\n", 1)[1].lstrip("\n")
    assert (ROOT / HANDOFF).read_text().endswith(old)
    passed("scope_and_historical_preservation", "Only the 10 inventoried documentation/evidence paths changed; existing Linux receipts, acceptance/crash records, root README, staged contents and HEAD preserved. Prior handoff body retained verbatim.")

    metadata = json.loads((EVIDENCE / "windows-import-source.json").read_text())
    windows = (ROOT / WINDOWS).read_text()
    assert digest(ROOT / WINDOWS) == metadata["imported_utf8_lf_sha256"]
    assert "1afd881ecfa65c616261e7aeb76ecd2bf87770b6" in windows
    hashes = ["9e7a9d283dc9ba51bdb3bed00aeb680af687bdbfa86d1743ad63ee130d662993",
        "2b9ee648cbc67976363c93a0413e48de9065ad943fd38c2d2795688286faf2ca",
        "e004b01b0a48b4663356f949065d3da571dff12cff5b4e8b0d036b05ca6dc7b4",
        "34b5d317e5f536c3e4b43317019b4e713b4f3e7423b99ef699fc256729772055",
        "fa5c8013a56d1694ffc727b766d2b7bc0455daafeec64abbda4a5de4e77df439",
        "68423ed09a68e2436fc6253a047ae5f07ce306734e5bc1348e2530f4ca3483df"]
    assert all(h in windows for h in hashes)
    passed("exact_windows_import", "Imported text hash matches the exact Boss-supplied message body, with one terminal LF. Six author-reported hashes and tested SHA retained. Original Windows file/raw receipts were not accessed or independently verified.")

    original = (ROOT / "README.md").read_text()
    proposed = apply_unified_diff(original, (ROOT / PATCH).read_text())
    assert proposed.split("## Platforms and editor support", 1)[1] == original.split("## Platforms and editor support", 1)[1]
    assert proposed.split("After building the compiler:", 1)[0] == original.split("After building the compiler:", 1)[0]
    assert "19.1.7" not in proposed and "23.1.4" in proposed and "Python **3.12+**" in proposed
    command(["git", "apply", "--check", PATCH])
    assert digest(ROOT / "README.md") == START["input_sha256"]["README.md"]
    passed("unapplied_readme_proposal", "Dry-run git apply succeeds. Proposal changes only setup/current CLI guidance; valid legacy source example and all feature/editor/licensing sections are preserved. Root README bytes unchanged.")

    documents = {name: (ROOT / name).read_text() for name in (GUIDE, HANDOFF, BLOCKERS, WINDOWS)}
    documents["README.md (proposed)"] = proposed
    link_count = 0; shell_count = 0; python_count = 0
    for name, text in documents.items():
        assert text.endswith("\n"), name
        assert all(line == line.rstrip() for line in text.splitlines()), name
        prose = re.sub(r"^```[^\n]*\n.*?^```[ \t]*$", "", text, flags=re.M | re.S)
        prose = re.sub(r"`[^`]*`", "", prose)
        for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", prose):
            target = target.strip("<>")
            if re.match(r"[a-z]+:", target): continue
            path, _, fragment = target.partition("#")
            current = ROOT / ("README.md" if name.endswith("(proposed)") else name)
            resolved = current.parent / path if path else current
            assert resolved.exists(), (name, target)
            if fragment and resolved.suffix == ".md":
                content = proposed if not path and name.endswith("(proposed)") else resolved.read_text()
                assert fragment in anchors(content), (name, target)
            link_count += 1
        for block in re.findall(r"^```sh\n(.*?)^```", text, re.M | re.S):
            command(["bash", "-n"], block); shell_count += 1
            for code in re.findall(r"<<'PY'\n(.*?)\nPY", block, re.S):
                ast.parse(code); python_count += 1
    command(["git", "diff", "--check"])
    passed("links_and_syntax", {"local_links_and_anchors": link_count, "shell_blocks_bash_n": shell_count,
        "Python_heredocs_ast_parsed": python_count, "external_network_reachability": "NOT RUN"})

    guide = documents[GUIDE]
    blocks = re.findall(r"^```sh\n(.*?)^```", guide, re.M | re.S)
    config_block = next(b for b in blocks if b.startswith("cmake -S . -B build/preview-release"))
    lines = config_block.replace("\\\n", " ").splitlines()
    documented = shlex.split(lines[0])
    sdk = "/src/void/.toolchains/llvm-23.1.4"
    documented = [s.replace("$void_llvm23_sdk", sdk) for s in documented]
    documented[documented.index("-S") + 1] = "/src/void"
    documented[documented.index("-B") + 1] = "/work/build"
    receipts = [json.loads(line) for line in (ROOT / "docs/release/evidence/commands.jsonl").read_text().splitlines()]
    by_id = {r["id"]: r for r in receipts}
    actual = by_id["fresh-void-configure"]["argv"][3:]
    assert documented == actual, (documented, actual)
    assert shlex.split(lines[1])[3:] == by_id["fresh-void-build"]["argv"][3:][3:]
    package_block = next(b for b in blocks if "sudo apt-get install" in b)
    packages = shlex.split(package_block.replace("\\\n", " "))
    declared = packages[packages.index("--no-install-recommends") + 1:]
    actual_packages = shlex.split(by_id["fresh-host-packages"]["argv"][-1])
    actual_packages = actual_packages[actual_packages.index("--no-install-recommends") + 1:]
    assert declared == actual_packages
    http_block = next(b for b in blocks if b.startswith("python3 tools/setup_http_deps.py"))
    setup_line, verify_line = http_block.replace("\\\n", " ").strip().splitlines()
    setup_argv = by_id["fresh-http-source-build"]["argv"]
    assert shlex.split(setup_line) == setup_argv[setup_argv.index("python3"):]
    verify = shlex.split(verify_line)
    verify[1] = "/src/void/" + verify[1]; verify[2] = "/src/void/" + verify[2]
    assert verify == by_id["fresh-http-prefix-verify"]["argv"][3:]
    install_block = next(b for b in blocks if b.startswith("void_install_prefix="))
    install_lines = [line for line in install_block.replace("\\\n", " ").splitlines() if line.startswith("cmake ")]
    for component, line in zip(("compiler", "tooling"), install_lines, strict=True):
        argv = shlex.split(line)
        argv = [s.replace("build/preview-release", "/work/build").replace("$void_install_prefix", "/opt/void") for s in argv]
        assert argv == by_id["fresh-install-" + component]["argv"][3:]
    smoke_blocks = [b for b in blocks if "<<'VOID'" in b]
    phases = [("/work/minimal", ["installed-minimal-check-corrected", "installed-minimal-run"]),
              ("/work/standard", ["installed-standard-check", "installed-standard-run", "installed-native-build"])]
    for block, (directory, ids) in zip(smoke_blocks, phases, strict=True):
        lines = [line for line in block.splitlines() if line.startswith("voidc ")]
        for line, command_id in zip(lines, ids, strict=True):
            argv = shlex.split(line)
            argv[0] = "/opt/void-relocated/bin/voidc"
            argv = [x.replace("$void_smoke_dir", directory) for x in argv]
            actual = by_id[command_id]["argv"]
            assert argv == actual[actual.index("/opt/void-relocated/bin/voidc"):]
    passed("documented_command_receipt_map", "HTTP setup/verify, both CMake component installs and all five minimal/standard check/run/build commands exactly match retained argv after explicit path normalization.")
    inputs = json.loads((ROOT / "docs/release/evidence/probe-inputs.json").read_text())["artifacts"]
    heredocs = re.findall(r"<<'VOID'\n(.*?)\nVOID", guide, re.S)
    assert heredocs == [a["text"].rstrip("\n") for a in inputs if a["path"].endswith(".void")]
    assert all(hashlib.sha256(a["text"].encode()).hexdigest() == a["sha256"] for a in inputs if "text" in a)
    assert all(x in (ROOT / "src/main.cpp").read_text() for x in ['command!="build"', '"--no-cache"', '"--app"'])
    assert (ROOT / "examples/architecture.void").is_file()
    sdk_source = ast.parse((ROOT / "tools/ci/llvm_sdk.py").read_text())
    assert any(isinstance(x, ast.FunctionDef) and x.name == "install" for x in sdk_source.body)
    passed("linux_recipe_and_examples", "Package set, complete Release configure argv (only paths normalized), build target/parallel arguments, HTTP profile, component installs and both smoke source texts match retained accepted evidence. SDK helper invocation is AST/source inspected only; no fresh download/runtime/example execution is claimed.")
    REPORT["result"] = "PASS"
except Exception as error:
    REPORT["result"] = "FAIL"
    REPORT["error"] = repr(error)
    raise
finally:
    REPORT["checker_sha256"] = digest(Path(__file__))
    REPORT["attempts"] = HISTORY + [{"result": REPORT.get("result"), "error": REPORT.get("error"), "passed_checks": [x["name"] for x in REPORT["checks"]], "checker_sha256": REPORT["checker_sha256"]}]
    (EVIDENCE / "validation.json").write_text(json.dumps(REPORT, indent=2) + "\n")
    inventory = {"head": START["head"], "initial_working_tree": "clean", "uncommitted": True, "unstaged": True,
        "scope": "Documentation and review evidence only; README proposal unapplied",
        "exact_changed_files": sorted(CHANGED),
        "files_except_this_inventory": {name: {"before_sha256": START["input_sha256"].get(name),
            "after_sha256": digest(ROOT / name), "bytes": (ROOT / name).stat().st_size}
            for name in sorted(CHANGED) if not name.endswith("/inventory.json")},
        "protected_readme_sha256": digest(ROOT / "README.md"),
        "windows_record_sha256": digest(ROOT / WINDOWS),
        "validation_result": REPORT.get("result"),
        "historical_inputs_protected_by": "start.json digests plus git diff allowlist and prior handoff-body equality",
        "self_hash_policy": "Own digest omitted to avoid recursive checksum."}
    (EVIDENCE / "inventory.json").write_text(json.dumps(inventory, indent=2) + "\n")
    print(json.dumps({"result": REPORT.get("result"), "checks": REPORT["checks"], "error": REPORT.get("error")}, indent=2))
