#!/usr/bin/env python3
"""Copy the explicit public directory into the website-only deployment output."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'dist'
if OUTPUT.is_symlink():
    raise SystemExit('Refusing to replace a symlink at website/dist')
if OUTPUT.exists():
    shutil.rmtree(OUTPUT)
shutil.copytree(ROOT / 'public', OUTPUT)
print(f'Built {len(list(OUTPUT.rglob("*")))} entries in {OUTPUT}')
