"""Measure one frozen manifest project with the actual CLI and LSP processes."""

import argparse
from collections import defaultdict
from hashlib import file_digest, sha256
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
from statistics import median
import subprocess
import sys
from tempfile import TemporaryDirectory
from time import perf_counter_ns

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lsp_client import Client

FIXTURE = Path(__file__).with_suffix("")
VERSION = "23.1.4"
WINDOWS = os.name == "nt"
OUTPUT = "inventory 7 7 58\n"


def digest(path):
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()


def position(text, offset):
    prefix = text[:offset]
    return {"line": prefix.count("\n"),
            "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2}


class Benchmark:
    def __init__(self, build):
        self.compiler = build / ("voidc.exe" if WINDOWS else "voidc")
        self.server = build / ("voidls.exe" if WINDOWS else "voidls")
        marker, driver, linker, *self.resources = (build / "void-native.txt").read_text(encoding="utf-8").splitlines()
        assert marker == f"VOID native {VERSION} v2", marker
        self.driver = Path(driver)
        self.linker = self.driver.with_name("lld-link.exe").resolve() if WINDOWS else Path(linker)
        self.link_flags = ["--driver-mode=g++"]
        if WINDOWS:
            self.link_flags += ["-B" + str(self.linker.parent), "-fuse-ld=lld"]
        else:
            self.link_flags += ["--ld-path=" + str(self.linker)]
        self.samples = defaultdict(list)
        self.receipts = defaultdict(set)
        self.metadata = {
            "host": platform.platform(), "architecture": platform.machine(),
            "logical_cpus": os.cpu_count(), "python": platform.python_version(),
            "benchmark_sha256": digest(Path(__file__)),
            "compiler_sha256": digest(self.compiler), "server_sha256": digest(self.server),
            "native_link_manifest_sha256": digest(build / "void-native.txt"),
            "inputs": {p.relative_to(FIXTURE).as_posix(): digest(p) for p in sorted(FIXTURE.rglob("*")) if p.is_file()},
            "toolchains": {p.name: self.run([p, "--version"], build).stdout.strip()
                           for p in (self.compiler, self.driver, self.linker)},
        }
        for value in self.metadata["toolchains"].values():
            assert re.search(r"\b23\.1\.4\b", value), value

    def time(self, name, action):
        start = perf_counter_ns()
        result = action()
        self.samples[name].append((perf_counter_ns() - start) / 1e6)
        return result

    def run(self, argv, root, *, status=0, metric=None, output=None):
        argv = list(map(str, argv))
        def launch():
            return subprocess.run(argv, cwd=root, capture_output=True, encoding="utf-8", timeout=60)
        result = self.time(metric, launch) if metric else launch()
        assert result.returncode == status, (argv, result)
        assert not any(s in result.stderr for s in
                       ("AddressSanitizer", "LeakSanitizer", "runtime error:", "PLEASE submit a bug report")), result
        if output is not None:
            assert result.stdout == output, (argv, result)
        return result

    def cli(self, root, command, *args, **options):
        return self.run([self.compiler, command, "--project", root, *args], root, **options)

    def check_cache(self, root, metric, output, expected):
        cache = root / ".void/cache/v1"
        before = set(cache.glob("[0-9a-f]" * 64))
        result = self.cli(root, "run", "--cache", metric=metric, output=output)
        mode = "unavailable" if WINDOWS else expected
        assert result.stderr.strip() == f"Debug cache: {mode}", result
        after = set(cache.glob("[0-9a-f]" * 64))
        new = after - before
        assert (len(new) == 1 if mode == "miss" else not new), (metric, mode, before, after)
        if WINDOWS:
            assert not cache.exists(), cache
        else:
            for obj in new:
                data = obj.read_bytes()
                assert sha256(data[64:]).hexdigest().encode() == data[:64]
                assert data[64:128].decode() == obj.name and data[129:], obj
        self.receipts[metric].add(f"{mode}, {len(new)} new key(s)")

    def native(self, root, repetition):
        self.run([self.compiler, "--version"], root, metric="cli.startup")
        self.cli(root, "check", metric="cli.check.first", output="Source checked successfully\n")
        self.cli(root, "check", metric="cli.check.repeat", output="Source checked successfully\n")
        self.cli(root, "resolve", "--locked", metric="cli.resolve_locked", output="Verified void.lock.\n")
        for mode, flags in (("normal", ["--no-opt"]), ("o2", [])):
            self.cli(root, "run", *flags, "--no-cache", metric=f"cli.jit.{mode}", output=OUTPUT)
            obj = root / ("inventory.obj" if WINDOWS else "inventory.o")
            image = root / ("independent.exe" if WINDOWS else "independent")
            built = root / ("native.exe" if WINDOWS else "native")
            obj.unlink(missing_ok=True)
            self.cli(root, "emit", "-o", obj, *flags, metric=f"cli.emit.{mode}")
            assert obj.stat().st_size > 0
            if repetition == 0:
                original_object = digest(obj)
                obj.unlink()
                self.cli(root, "emit", "-o", obj, *flags)
                assert digest(obj) == original_object, f"{mode} repeated object emission changed bytes"
            argv = [self.driver, *self.link_flags, obj, *self.resources, "-o", image]
            if WINDOWS and repetition == 0:
                plan = self.run([*argv, "-###"], root).stderr
                launch = re.search(r'^\s*"([^"]*lld-link(?:\.exe)?)"', plan, re.M | re.I)
                assert launch and Path(launch[1].replace("\\\\", "\\")).with_suffix(".exe").resolve() == self.linker, plan
            self.run(argv, root, metric=f"cli.independent_link.{mode}")
            self.run([image], root, metric=f"cli.aot.{mode}", output=OUTPUT)
            for warmth in ("first", "repeat"):
                built.unlink(missing_ok=True)
                self.cli(root, "build", "-o", built, *flags, metric=f"cli.build.{mode}.{warmth}")
                self.run([built], root, output=OUTPUT)
        self.check_cache(root, "cache.first", OUTPUT, "miss")
        self.check_cache(root, "cache.repeat", OUTPUT, "hit")
        cache = root / ".void/cache/v1"
        debug_keys = set(cache.iterdir()) if cache.exists() else set()
        self.cli(root, "run", "--profile", "release", metric="cache.o2_first", output=OUTPUT)
        optimized = (set(cache.iterdir()) - debug_keys) if cache.exists() else set()
        assert len(optimized) == (0 if WINDOWS else 1), optimized
        entries = {p: (p.stat().st_mtime_ns, digest(p)) for p in optimized}
        self.cli(root, "run", "--profile", "release", metric="cache.o2_repeat", output=OUTPUT)
        assert {p: (p.stat().st_mtime_ns, digest(p)) for p in optimized} == entries
        if WINDOWS:
            assert not cache.exists()
            self.receipts["cache.o2"].add("no cache directory")
        else:
            assert set(cache.iterdir()) == debug_keys | optimized
            self.receipts["cache.o2"].add("one additional optimized key, warm bytes and timestamp unchanged")
        main, manifest = root / "src/main.void", root / "void.project.toml"
        update(main, "return 1", "return 2")
        self.check_cache(root, "cache.source_changed", "inventory 7 7 59\n", "miss")
        # Change application metadata, which participates in the native identity.
        update(manifest, 'name = "Inventory Report"', 'name = "Inventory Update"')
        self.check_cache(root, "cache.manifest_changed", "inventory 7 7 59\n", "miss")
        dep = root / "deps/rates/src/api.void"
        update(dep, "return 2", "return 3")
        denied = self.cli(root, "run", "--cache", status=1, output="", metric="cache.dependency_stale")
        assert "Stale or tampered" in denied.stderr and "Debug cache:" not in denied.stderr, denied
        lock = root / "void.lock"
        original = digest(lock)
        self.cli(root, "resolve", "--refresh")
        assert digest(lock) != original
        self.check_cache(root, "cache.dependency_changed", "inventory 7 7 55\n", "miss")

    def editor(self, root, query_count):
        main, stock, draft, audit = (root / f"src/{name}.void" for name in ("main", "stock", "draft", "audit"))
        main_text, stock_text, draft_text = (p.read_text(encoding="utf-8") for p in (main, stock, draft))
        client = self.time("lsp.spawn", lambda: Client(self.server, root, project=True, cwd=root))
        last_revision = -1
        with client as c:
            def request(method, params=None, metric=None):
                action = lambda: c.request(method, params or {})
                response, notes = self.time(metric, action) if metric else action()
                assert "result" in response, (method, response, notes)
                return response["result"], notes

            def state(expected, notes, path=None, diagnostic=False, advance=True):
                nonlocal last_revision
                result, _ = request("void/projectState")
                assert result["status"] == expected, (result, notes)
                if path:
                    assert result["entry"] == path.as_posix(), result
                if diagnostic:
                    assert any(n["diagnostics"] for n in notes), notes
                else:
                    assert not any(n["diagnostics"] for n in notes), notes
                revision = int(result["selectionRevision"])
                if advance:
                    assert revision > last_revision, result
                else:
                    assert revision == last_revision, result
                last_revision = revision
                return result

            def notify(method, params, metric):
                def barrier():
                    c.notify(method, params)
                    return c.sync()
                return self.time(metric, barrier) if metric else barrier()

            def document(path):
                return {"textDocument": {"uri": path.as_uri()}}

            def opened(path, text, metric=None):
                return notify("textDocument/didOpen", {"textDocument":
                              {"uri": path.as_uri(), "version": 1, "languageId": "void", "text": text}}, metric)

            def edit(path, version, text, metric=None):
                return notify("textDocument/didChange", {"textDocument": {"uri": path.as_uri(), "version": version},
                              "contentChanges": [{"text": text}]}, metric)

            def query(kind, path, pos=None, metric=None):
                params = document(path)
                if pos is not None:
                    params["position"] = pos
                response, notes = request("textDocument/" + kind, params, metric)
                assert not notes, notes
                return response

            init, _ = request("initialize", {"capabilities": {
                "general": {"positionEncodings": ["utf-16"]},
                "textDocument": {"publishDiagnostics": {"versionSupport": True},
                                 "semanticTokens": {"requests": {"full": True}, "formats": ["relative"]}}}},
                "lsp.initialize")
            caps = init["capabilities"]
            assert caps["positionEncoding"] == "utf-16", caps
            legend = caps["semanticTokensProvider"]["legend"]["tokenTypes"]
            state("ready", notify("initialized", {}, "lsp.project_open"), main)
            state("ready", opened(main, main_text, "lsp.document_open"), main)
            dirty = main_text.replace("return 1", "return 2")
            state("ready", edit(main, 2, dirty, "lsp.edit_local_body"), main)
            assert main.read_text(encoding="utf-8") == main_text
            state("ready", opened(stock, stock_text), main)
            revised_stock = stock_text.replace("return units, total", "return units, total + 1")
            state("ready", edit(stock, 2, revised_stock, "lsp.edit_referenced_module"), main)
            ready = state("ready", opened(draft, draft_text), main)
            changed = draft_text.replace("12", "15")
            independent = state("ready", edit(draft, 2, changed, "lsp.edit_unreferenced"), main)
            assert ready["snapshotKey"] == independent["snapshotKey"]
            assert "reorder" in query("hover", draft, position(changed, changed.index("reorder")))["contents"]["value"]

            symbol = position(dirty, dirty.rindex("adjustment()"))
            prefix = position(revised_stock, revised_stock.index("item.units") + len("item."))
            for index in range(query_count + 1):
                # A new accepted revision precedes the first query group only.
                cohort = "first" if index == 0 else "repeat"
                items = query("completion", stock, prefix, f"lsp.completion.{cohort}")
                assert {"units", "unit_price"} <= {item["label"] for item in items}, items
                hover = query("hover", main, symbol, f"lsp.hover.{cohort}")
                assert "adjustment()" in hover["contents"]["value"], hover
                tokens = query("semanticTokens/full", main, metric=f"lsp.semantic_tokens.{cohort}")["data"]
                assert len(tokens) % 5 == 0
                line = column = 0
                found = False
                for i in range(0, len(tokens), 5):
                    delta, start, size, token, _ = tokens[i:i + 5]
                    line += delta
                    column = start if delta else column + start
                    if (line, column) == (symbol["line"], symbol["character"]):
                        found = size == len("adjustment") and legend[token] == "function"
                assert found, (symbol, tokens)

            # The completion probe must still validate a changed dependency
            # when no document revision or watcher notification has arrived.
            dependency = root / "deps/rates/src/api.void"
            frozen_dependency = dependency.read_bytes()
            update(dependency, "return 2", "return 3")
            assert query("completion", stock, prefix) == []
            state("ready", [], main, advance=False)
            dependency.write_bytes(frozen_dependency)
            assert {"units", "unit_price"} <= {item["label"] for item in query("completion", stock, prefix)}
            self.receipts["lsp.completion"].add("same-revision changed dependency refused, restored bytes accepted")

            syntax_error = dirty.replace("return 2", "return )")
            notes = edit(main, 3, syntax_error, "lsp.introduce_syntax_error")
            state("error", notes, diagnostic=True)
            assert any(n["uri"] == main.as_uri() and n["version"] == 3 for n in notes if n["diagnostics"]), notes
            state("ready", edit(main, 4, dirty, "lsp.repair_syntax_error"), main)
            invalid = dirty.replace("return 2", 'return "invalid"')
            state("error", edit(main, 5, invalid, "lsp.invalid_dirty_buffer"), diagnostic=True)
            assert query("semanticTokens/full", main)["data"] == []
            assert query("hover", main, symbol) is None
            state("ready", edit(main, 6, dirty, "lsp.valid_dirty_buffer"), main)
            assert query("hover", main, symbol)
            state("ready", edit(main, 5, invalid), main, advance=False)
            assert query("hover", main, symbol), "Stale edit displaced the current result"
            _, notes = request("void/reselectProject", {"entry": "src/audit.void"}, "lsp.switch_entry")
            state("ready", notes, audit)
            _, notes = request("void/reselectProject", {"entry": "src/main.void"}, "lsp.switch_back")
            state("ready", notes, main)

            # Change the disk path before the notification, as an editor would.
            notify("textDocument/didClose", document(stock), None)
            moved = stock.with_name("stock_moved.void")
            stock.rename(moved)
            state("error", notify("workspace/didRenameFiles", {"files": [{"oldUri": stock.as_uri(),
                  "newUri": moved.as_uri()}]}, "lsp.rename_referenced"), diagnostic=True)
            assert query("hover", main, symbol) is None
            renamed = dirty.replace("./stock.void", "./stock_moved.void")
            state("ready", edit(main, 7, renamed, "lsp.repair_renamed_import"), main)
            moved.unlink()
            state("error", notify("workspace/didDeleteFiles", {"files": [{"uri": moved.as_uri()}]},
                                  "lsp.remove_referenced"), diagnostic=True)
            moved.write_text(stock_text, encoding="utf-8", newline="\n")
            state("ready", notify("workspace/didCreateFiles", {"files": [{"uri": moved.as_uri()}]},
                                  "lsp.restore_referenced"), main)
            if not WINDOWS:
                process = Path(f"/proc/{c.process.pid}/status")
                if process.is_file():
                    for key, value in re.findall(r"^(VmRSS|VmHWM):\s*(\d+) kB", process.read_text(), re.M):
                        self.samples[f"memory.{key}.KiB"].append(int(value))
            c.shutdown()
            self.receipts["lsp"].add("UTF-16 token after astral character, stale edit rejected, diagnostics and reselection verified")

    def report(self):
        summary = {}
        for name, values in self.samples.items():
            ordered = sorted(values)
            summary[name] = {"n": len(values), "median": median(values),
                             "p95": ordered[math.ceil(len(values) * 0.95) - 1], "worst": ordered[-1]}
        return {"metadata": self.metadata, "summary": summary, "samples": self.samples,
                "receipts": {name: sorted(values) for name, values in self.receipts.items()}}


def update(path, old, new):
    text = path.read_text(encoding="utf-8")
    assert text.count(old) == 1, (path, old)
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, help="Verified native build with voidc, voidls and void-native.txt")
    parser.add_argument("--output", required=True, type=Path, help="New report path in disposable storage")
    parser.add_argument("--repetitions", type=int, default=20)
    parser.add_argument("--queries", type=int, default=10, help="Repeated requests per method and server")
    parser.add_argument("--lane", choices=("both", "editor", "native"), default="both")
    args = parser.parse_args()
    if min(args.repetitions, args.queries) < 1:
        parser.error("repetitions and queries must be positive")
    with args.output.open("x", encoding="utf-8") as receipt, TemporaryDirectory(prefix="VOID inventory ") as temp:
        benchmark = Benchmark(args.build.resolve())
        benchmark.metadata.update(repetitions=args.repetitions, queries=args.queries, lane=args.lane)
        directory = Path(temp)
        template = shutil.copytree(FIXTURE, directory / "template")
        try:
            benchmark.cli(template, "resolve")
            benchmark.metadata["lock_sha256"] = digest(template / "void.lock")
            for repetition in range(args.repetitions):
                print(f"Inventory benchmark repetition {repetition + 1}/{args.repetitions}", flush=True)
                for lane in ("native", "editor") if args.lane == "both" else (args.lane,):
                    with TemporaryDirectory(prefix=lane + " ", dir=directory) as iteration:
                        root = shutil.copytree(template, Path(iteration) / "project")
                        if lane == "native":
                            benchmark.native(root, repetition)
                        else:
                            benchmark.editor(root, args.queries)
        finally:
            json.dump(benchmark.report(), receipt, indent=2, sort_keys=True)
            receipt.write("\n")


if __name__ == "__main__":
    main()
