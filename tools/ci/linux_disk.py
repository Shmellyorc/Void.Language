"""Bounded Ubuntu hosted-runner cleanup and measured LLVM/build disk preflight.

Measurements and estimation limits: docs/net/http/CI_LLVM_DISK_REPAIR.md.
Never run cleanup on a developer machine or accept caller-supplied deletion paths.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess

GIB = 1024 ** 3
# Allocated bytes measured locally with the pinned SDK and complete builds.
SDK_BYTES = 12432510976
BUILD_BYTES = {'Release': 5822873600, 'Debug': 13522292736, 'Sanitizers': 19656425472}
# Largest current provider build + installed prefix + downloaded archives;
# includes source trees within the provider build directory.
DEPS_BYTES = 898060288 + 173666304 + 229257216
THINLTO_BYTES = 132422080  # largest retained hosted build cache receipt
EDITOR_BYTES = 156200960
ARCHIVE_BYTES = 3 * GIB  # ceil(2.71 GiB reported cache); official xz is 2017061664
# Only unrelated, known runner-image SDK trees; never general /usr or toolcache.
CLEANUP = ('/usr/local/lib/android', '/usr/share/dotnet', '/opt/ghc',
           '/usr/local/.ghcup', '/usr/share/swift')


def requirements(configuration, phase):
    build = BUILD_BYTES[configuration] + THINLTO_BYTES + EDITOR_BYTES
    # Explicit 25% estimation margin for hosted link scratch, fixtures/artifact
    # compression and differences from local build sizes. Not a measured peak.
    margin = (SDK_BYTES + DEPS_BYTES + build + 3) // 4
    if phase == 'before-restore':
        return SDK_BYTES + ARCHIVE_BYTES + DEPS_BYTES + build + margin
    if phase == 'before-build':
        return build + margin
    raise ValueError(phase)


def check_space(free, required):
    if free < required:
        raise RuntimeError(f'Insufficient Linux disk capacity: available={free} bytes, '
                           f'required={required} bytes, deficit={required - free} bytes. '
                           'See disk receipts and CI_LLVM_DISK_REPAIR.md; refusing to start the next large allocation.')


def cleanup_targets():
    targets = []
    for name in CLEANUP:
        path = Path(name)
        # Reject redirects in *any* component, including dangling symlinks.
        if any(p.is_symlink() for p in (path, *path.parents)):
            raise RuntimeError(f'Refusing redirected cleanup target: {path}')
        if path.exists():
            if not path.is_dir() or path.is_mount():
                raise RuntimeError(f'Refusing non-directory or mounted cleanup target: {path}')
            targets.append(path)
    return targets


def snapshot(label, evidence):
    workspace = Path.cwd()
    scratch = Path(os.environ['RUNNER_TEMP'])
    paths = [workspace, scratch, Path('/'), *map(Path, CLEANUP),
             workspace / '.toolchains/llvm-23.1.4', workspace / '.toolchains/http',
             workspace / 'build', Path(os.environ['THINLTO_CACHE_DIR'])]
    paths = list(dict.fromkeys(p for p in paths if p.exists()))
    df = subprocess.check_output(['df', '-B1', '--', *map(str, paths)], text=True)
    # One summary per explicit directory, stay on its filesystem. No root scan.
    sized = [p for p in paths if p not in (workspace, scratch, Path('/'))]
    du = subprocess.check_output(['du', '-sx', '-B1', '--', *map(str, sized)], text=True) if sized else ''
    record = {'phase': label, 'df': df, 'du': du,
              'workspace_free': shutil.disk_usage(workspace).free,
              'temp_free': shutil.disk_usage(scratch).free,
              'same_filesystem': workspace.stat().st_dev == scratch.stat().st_dev}
    (evidence / f'disk-{label}.json').write_text(json.dumps(record, indent=2) + '\n')
    print(df + du, flush=True)
    return record


def preflight(phase, evidence):
    record = snapshot(phase, evidence)
    required = requirements(os.environ['EXPERIMENT_CONFIGURATION'], phase)
    record['required_workspace_bytes'] = required
    (evidence / f'disk-{phase}.json').write_text(json.dumps(record, indent=2) + '\n')
    print(f'{phase}: required={required} available={record["workspace_free"]} bytes', flush=True)
    check_space(record['workspace_free'], required)
    # Conservatively reserve archive in workspace even if temp is separate;
    # independently require temp capacity for download/save there as well.
    if not record['same_filesystem']:
        check_space(record['temp_free'], ARCHIVE_BYTES)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=['prepare', 'after-sdk', 'before-build'])
    args = parser.parse_args()
    release = platform.freedesktop_os_release()
    if (platform.system() != 'Linux' or os.environ.get('GITHUB_ACTIONS') != 'true'
            or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted'
            or release.get('ID') != 'ubuntu' or release.get('VERSION_ID') != '24.04'):
        raise RuntimeError('This helper is restricted to GitHub-hosted Ubuntu 24.04')
    evidence = Path(os.environ['EXPERIMENT_DIR']); evidence.mkdir(parents=True, exist_ok=True)
    if args.phase == 'prepare':
        snapshot('before-cleanup', evidence)
        targets = cleanup_targets()  # validate the entire list before deleting anything
        for path in targets:
            print(f'Removing unused preinstalled SDK: {path}', flush=True)
            subprocess.run(['sudo', 'rm', '-rf', '--one-file-system', '--', str(path)], check=True)
        subprocess.run(['sudo', 'apt-get', 'clean'], check=True)  # downloaded package archives only
        snapshot('after-cleanup', evidence)
        preflight('before-restore', evidence)
    elif args.phase == 'before-build':
        preflight('before-build', evidence)
    else:
        snapshot('after-sdk', evidence)


if __name__ == '__main__':
    main()
