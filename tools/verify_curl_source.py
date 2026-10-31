#!/usr/bin/env python3
"""Verify the retained release inventory and four directly approved corrections."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = root / "third_party/curl"
provenance = json.loads((source / "VOID-PROVENANCE.json").read_text())
expected = dict(provenance["pristine_sha256"])
corrections = provenance["local_corrections"]
assert set(corrections) == {"lib/cookie.c", "lib/cookie.h", "lib/getinfo.c", "lib/http2.c"}
for name, correction in corrections.items():
    assert expected[name] == correction["upstream_sha256"]
    expected[name] = correction["corrected_sha256"]
errors = []
for name, digest in expected.items():
    path = source / name
    if not path.is_file() or path.is_symlink():
        errors.append(f"missing/nonregular source: {name}")
    elif hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        errors.append(f"source digest mismatch: {name}")
allowed = set(expected) | {"VOID-PROVENANCE.json", "NOTICE"}
for path in source.rglob("*"):
    if path.is_file() and path.relative_to(source).as_posix() not in allowed:
        errors.append(f"unrecorded source: {path.relative_to(source)}")
reviews = provenance["approved_corrections"]
assert set(reviews) == {"cookie_enumeration", "http2_window"}
reviewed_files = []
for name, review in reviews.items():
    patch = root / review["review_diff"]
    if hashlib.sha256(patch.read_bytes()).hexdigest() != review["sha256"]:
        errors.append(f"review diff digest mismatch: {name}")
    reviewed_files.extend(review["files"])
assert len(reviewed_files) == len(set(reviewed_files)) and set(reviewed_files) == set(corrections)
if errors:
    raise SystemExit("\n".join(errors))
print(f"curl {provenance['version']}: {len(expected)} files verified; four private corrections")
