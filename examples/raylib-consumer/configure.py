"""Write a consumer manifest with an explicit immutable source-package pin."""
import argparse
from pathlib import Path
import re


def manifest(origin, revision, standalone=False):
    if not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('revision must be a full lowercase immutable Git SHA-1')
    if not origin.startswith('https://') or any(c in origin for c in ('"', '\n', '\r', '\\')):
        raise ValueError('origin must be a literal HTTPS Git URL')
    subdir = '' if standalone else 'subdir = "verified-packages/raylib"\n'
    return f'''schema = 1
[project]
id = "org.voidlang.examples.raylib-consumer.project"
name = "Raylib package consumer"
version = "0.1.0"
kind = "application"
[application]
id = "org.voidlang.examples.raylib-consumer"
[sources]
roots = ["src"]
[build.debug]
optimization = "none"
[build.release]
optimization = "O2"
[dependencies.raylib]
id = "org.voidlang.raylib"
git = "{origin}"
rev = "{revision}"
{subdir.rstrip()}
version = "=0.1.0"
[native-requirements.raylib]
id = "org.voidlang.raylib"
version = "=6.0.0"
abi = 3
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rev', required=True)
    parser.add_argument('--origin', default='https://github.com/Shmellyorc/Void.Language.git')
    parser.add_argument('--standalone', action='store_true', help='consume a package repository whose manifest is at its root')
    args = parser.parse_args()
    destination = Path(__file__).resolve().parent / 'void.project.toml'
    if destination.exists():
        parser.error('void.project.toml already exists; preserve or remove it explicitly')
    destination.write_text(manifest(args.origin, args.rev, args.standalone), encoding='utf-8')
    print(destination)


if __name__ == '__main__':
    main()
