"""Offline consistency and exact upstream reconstruction, not publisher authentication."""
from pathlib import Path
import hashlib
import json
import re
import sys

installed = sys.argv[1] == "--installed"
root = Path(sys.argv[2]) if installed else Path(sys.argv[1]) / "third_party" / "tomlplusplus"
source = root / "corresponding-source" if installed else root
receipt = json.loads((root / "upstream.json").read_text())
assert receipt["commit"] == "30172438cee64926dc41fdd9c11fb3ba5b2ba9de"
assert receipt["tag"] == "v3.4.0"
for name, field in (("toml.hpp", "header_sha256"), ("LICENSE", "license_sha256"),
                    ("void-bounds-and-locations.patch", "patch_sha256")):
    base = source if name == "toml.hpp" else root
    assert hashlib.sha256((base / name).read_bytes()).hexdigest() == receipt[field], name
assert "TOML parser" in (source / "README.md").read_text()
current = (source / "toml.hpp").read_text().splitlines(keepends=True)
patch = (root / "void-bounds-and-locations.patch").read_text().splitlines(keepends=True)
original = []
cursor = 0
index = 2
while index < len(patch):
    header = re.fullmatch(r"@@ -(\d+),(\d+) \+(\d+),(\d+) @@\n", patch[index])
    assert header, patch[index]
    target_start = int(header[3]) - 1
    original.extend(current[cursor:target_start])
    cursor = target_start
    index += 1
    removed = inserted = 0
    while index < len(patch) and not patch[index].startswith("@@"):
        line = patch[index]
        assert line[0] in " +-", line
        if line[0] in " +":
            assert current[cursor] == line[1:], (index, current[cursor], line)
            cursor += 1
            inserted += 1
        if line[0] in " -":
            original.append(line[1:])
            removed += 1
        index += 1
    assert removed == int(header[2]) and inserted == int(header[4])
original.extend(current[cursor:])
assert hashlib.sha256("".join(original).encode()).hexdigest() == receipt["upstream_header_sha256"]
assert receipt["upstream_header_sha256"] == "6b5172ad4dd6519aec67b919181fa7a38a2234131e5b2afa232dfe444819783e"
print("project_manifest_provenance: 3 file hashes and exact pinned upstream reconstruction passed")
