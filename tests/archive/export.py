"""Verify ZIP bytes independently and exercise real JIT/AOT publication and read-back."""

import binascii
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
import zipfile

compiler, resources = (Path(argument).resolve() for argument in sys.argv[1:])
fixtures = Path(__file__).resolve().parent
example = fixtures.parents[1] / "examples/archive/save_export.void"
controls = fixtures / "export.void"
windows = os.name == "nt"
version, clang, linker, *libraries = resources.read_text(encoding="utf-8").splitlines()
assert version == "VOID native 23.1.4 v2"
if windows:
    sdk_linker = Path(clang).with_name("lld-link.exe").resolve()
    link_flags = ["--driver-mode=g++", "-B" + str(sdk_linker.parent), "-fuse-ld=lld"]
else:
    sdk_linker = Path(linker)
    link_flags = ["--driver-mode=g++", "--ld-path=" + linker]


def invoke(args, work, status=0, quiet=True):
    command = list(map(str, args))
    result = subprocess.run(command, cwd=work, capture_output=True, timeout=180)
    assert result.returncode == status and (not quiet or not result.stderr), (command, result)
    return result


def independent_image(source, work, options):
    obj = work / (source.stem + (".obj" if windows else ".o"))
    image = work / (source.stem + (".exe" if windows else ".bin"))
    invoke([compiler, "emit", source, "-o", obj, *options], work)
    command = [clang, *link_flags, obj, *libraries, "-o", image]
    if windows:
        plan = invoke([*command, "-###"], work, quiet=False).stderr.decode("utf-8")
        launch = re.search(r'^\s*"([^"]*lld-link(?:\.exe)?)"', plan, re.M | re.I)
        assert launch and Path(launch[1].replace("\\\\", "\\")).with_suffix(".exe").resolve() == sdk_linker, plan
    invoke(command, work)
    return [image]


def verify_zip(image, expected):
    """Check every ZIP field, extent and payload without VOID or libarchive."""
    with zipfile.ZipFile(io.BytesIO(image)) as zipped:
        assert zipped.testzip() is None
        assert zipped.namelist() == list(expected)
        assert {name: zipped.read(name) for name in expected} == expected

    end = len(image) - 22
    signature, disk, central_disk, disk_count, count, central_size, central, comment = struct.unpack_from(
        "<I4H2IH", image, end)
    assert (signature, disk, central_disk, disk_count, count, comment) == (0x06054B50, 0, 0, len(expected), len(expected), 0)
    assert central + central_size == end
    local = 0
    next_central = central
    for name, payload in expected.items():
        name_bytes = name.encode("ascii")
        crc = binascii.crc32(payload)
        fields = (10, 0, 0, 0, 33, crc, len(payload), len(payload), len(name_bytes), 0)
        assert struct.unpack_from("<I5H3I2H", image, local) == (0x04034B50, *fields)
        data = local + 30 + len(name_bytes)
        assert image[local + 30:data] == name_bytes
        assert image[data:data + len(payload)] == payload
        assert struct.unpack_from("<I6H3I5H2I", image, next_central) == (
            0x02014B50, 20, *fields, 0, 0, 0, 0, local)
        assert image[next_central + 46:next_central + 46 + len(name_bytes)] == name_bytes
        local = data + len(payload)
        next_central += 46 + len(name_bytes)
    assert (local, next_central) == (central, end)
    return central


def reader_controls(command, work, original, central, empty):
    damaged = bytearray(original)
    damaged[30 + len("A.bin")] ^= 0xFF
    central_offset = bytearray(original)
    struct.pack_into("<I", central_offset, central + 42, 1)
    unsupported = bytearray(original)
    struct.pack_into("<H", unsupported, 8, 12)
    struct.pack_into("<H", unsupported, central + 10, 12)
    unicode_name = io.BytesIO()
    with zipfile.ZipFile(unicode_name, "w") as writer:
        writer.writestr("雪.txt", b"accepted independently")
    with zipfile.ZipFile(unicode_name) as reader:
        assert reader.testzip() is None and reader.getinfo("雪.txt").flag_bits == 0x800
    for label, data, error in (
        ("crc", damaged, "crc"),
        ("truncated", original[:-1], "format"),
        ("central-offset", central_offset, "format"),
        ("unsupported-method", unsupported, "format"),
        ("unicode-filename", unicode_name.getvalue(), "format"),
    ):
        path = work / (label + ".zip")
        path.write_bytes(data)
        invoke([*command, path, error], work)
    invoke([*command, empty, "empty"], work)


with TemporaryDirectory(prefix="VOID ZIP export 雪 ") as directory:
    work = Path(directory).resolve()
    assert b"LLVM 23.1.4" in invoke([compiler, "--version"], work).stdout
    assert invoke([clang, "--version"], work).stdout.startswith(b"clang version 23.1.4")
    assert invoke([sdk_linker, "--version"], work).stdout.startswith(b"LLD 23.1.4")
    entry_max = 4 * 1024 * 1024
    image_headers = 22 + 4 * (30 + 46 + 2)
    (work / "entry-max.bin").write_bytes(b"\xff" * entry_max)
    (work / "entry-over.bin").write_bytes(b"\x00" * (entry_max + 1))
    (work / "image-max-tail.bin").write_bytes(b"\x80" * (entry_max - image_headers))
    configuration = '{\n  "player": "初雪", "volume": 85\n}\n'.encode()
    checkpoint = b"\0\x80\xff\0SAVE"
    (work / "configuration.json").write_bytes(configuration)
    (work / "checkpoint.bin").write_bytes(checkpoint)
    (work / "invalid.json").write_bytes(b'{"player":"X","volume":5,"volume":6}')
    expected = {"A.bin": b"\0\x80\xff\0", "b.json": '"雪😀"'.encode(), "z.txt": b""}
    expected_export = {
        "checkpoint.bin": checkpoint,
        "game.json": configuration,
        "manifest.json": b'{"version":1,"configuration":"game.json","checkpoint":"checkpoint.bin"}',
    }
    references = None
    for mode, flags in (("O0", ["--no-opt"]), ("O2", [])):
        link_root = work / mode
        link_root.mkdir()
        example_image = independent_image(example, link_root, flags)
        controls_image = independent_image(controls, link_root, flags)
        for engine, app, probe in (
            ("JIT", [compiler, "run", example, "--no-cache", *flags, "--"],
             [compiler, "run", controls, "--no-cache", *flags, "--"]),
            ("AOT", example_image, controls_image),
        ):
            output_root = work / f"{mode} {engine} controls"
            output = invoke([*probe, output_root], work).stdout
            assert output.splitlines() == [b"archive names, quotas and independent buffers verified"], output
            assert {p.name for p in output_root.iterdir()} == {
                ".void-save-lock", "empty.zip", "first.zip", "reordered.zip", "single.zip"}
            empty = output_root / "empty.zip"
            image = (output_root / "first.zip").read_bytes()
            assert image == (output_root / "reordered.zip").read_bytes()
            verify_zip(empty.read_bytes(), {})
            verify_zip((output_root / "single.zip").read_bytes(), {"A.bin": b"\0*\xff\0"})
            central = verify_zip(image, expected)
            # The same reader and writer execute in each lane. One independent
            # corruption matrix suffices to exercise the existing rejection paths.
            if mode == "O0" and engine == "JIT":
                reader_controls(probe, work, image, central, empty)
            target = work / f"{mode} {engine} export"
            output = invoke([*app, work / "configuration.json", work / "checkpoint.bin", target], work).stdout
            assert output.splitlines() == [
                b"export published (synchronized true, code 0, cleanup 0)",
                b"verified exact configuration, checkpoint and manifest bytes",
            ], output
            assert {p.name for p in target.iterdir()} == {".void-save-lock", "save-export.zip"}
            export = (target / "save-export.zip").read_bytes()
            verify_zip(export, expected_export)
            with zipfile.ZipFile(io.BytesIO(export)) as reader:
                assert json.loads(reader.read("game.json")) == {"player": "初雪", "volume": 85}
                assert json.loads(reader.read("manifest.json")) == {
                    "version": 1, "configuration": "game.json", "checkpoint": "checkpoint.bin"}
            absent = work / f"{mode} {engine} invalid export"
            refused = invoke([*app, work / "invalid.json", work / "checkpoint.bin", absent], work, status=2)
            assert refused.stdout.splitlines() == [b"invalid game configuration"] and not absent.exists()
            if references is None:
                references = (image, export)
            assert (image, export) == references
            print(f"{mode} {engine}: exact ZIP/CRC, quotas, ownership, G06/G09 and reader round-trip passed",
                  flush=True)
    print("Deterministic SHA-256 (control, game):", *(hashlib.sha256(image).hexdigest() for image in references))
