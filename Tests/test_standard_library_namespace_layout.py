#!/usr/bin/env python3
"""Conservative StandardLibrary namespace-to-path audit for #311."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / "StandardLibrary" / "Void"
mismatches = []
for path in sorted(root.rglob("*.void")):
    text = path.read_text(errors="replace")
    match = re.search(r"^\s*namespace\s+([A-Za-z_][\w.]*)\s*;", text, re.MULTILINE)
    namespace = match.group(1) if match else "Void"
    if namespace == "Void":
        expected = ()
    elif namespace.startswith("Void."):
        expected = tuple(namespace.split(".")[1:])
    else:
        mismatches.append((path, namespace, "outside Void namespace root"))
        continue
    actual = path.relative_to(root).parent.parts
    if actual != expected:
        mismatches.append((path, namespace, "/".join(expected) or "."))

if mismatches:
    for path, namespace, expected in mismatches:
        print(f"namespace/path mismatch: {path}: {namespace} -> {expected}")
    raise SystemExit(1)
print("True")
