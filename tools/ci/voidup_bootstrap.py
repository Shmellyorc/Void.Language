"""Pinned host-provider preparation for the selected seven-job experiment.

No host-prefix cache or ambient-library fallback. HTTP prepares the shared zlib
first. Private catalog signing and A/B package tests are deliberately separate.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time

import http_bootstrap as http

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from verify_host_deps import verify_prefix

HOST_RESERVE = 2 * 1024**3  # Conservative estimate, not a native-platform peak.
APP_RESERVE = 512 * 1024**2
COMMON_TESTS = {'host_capabilities_dependencies', 'host_capabilities_portable',
                'host_capabilities_portable_source', 'voidup_smoke', 'voidup_ci_integration'}
LINUX_TESTS = {'host_capabilities', 'host_capabilities_stack', 'host_capabilities_frontend',
               'host_capabilities_lsp', 'host_capabilities_native', 'host_capabilities_install'}


def layout(configuration):
    plan = http.make_plan(configuration)  # Share the existing SDK/HTTP profile and native identity.
    system = plan['identity']['host']['system']
    bundle = {'Linux': '', 'Windows': 'windows-x86_64/', 'Darwin': 'macos-arm64/'}[system]
    plan['host_prefix'] = f'.toolchains/host/prefix/{bundle}{plan["profile"]}'
    zlib = ('zs.lib' if plan['profile'] == 'Release' else 'zsd.lib') if system == 'Windows' else 'libz.a'
    plan['zlib'] = str(ROOT / plan['prefix'] / 'lib' / zlib)
    return plan


def run(evidence, label, argv):
    command = list(map(str, argv)); start = time.monotonic()
    with (evidence / (label + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    http.save(evidence, label + '.json', dict(argv=command, exit_code=result.returncode,
                                           wall_seconds=time.monotonic()-start))
    if result.returncode:
        raise RuntimeError(f'VOIDUP {label} failed; see {evidence / (label + ".log")}')


def preflight(plan, evidence):
    system = plan['identity']['host']['system']
    required = ['cmake', 'ninja', 'msbuild' if system == 'Windows' else 'make']
    tools = {name: shutil.which(name) for name in required}
    if not all(tools.values()):
        raise RuntimeError(f'Host provider prerequisite missing: {tools}')
    workspace = shutil.disk_usage(ROOT).free
    scratch_path = Path(tempfile.gettempdir())
    scratch = shutil.disk_usage(scratch_path).free
    http.save(evidence, 'preflight.json', dict(system=system, tools=tools,
        workspace_free_bytes=workspace, temp_free_bytes=scratch,
        required_workspace_bytes=HOST_RESERVE+APP_RESERVE, required_temp_bytes=HOST_RESERVE,
        note='Additional host/application reservation only; existing Linux preflight covers SDK/full build. Native Windows/macOS total peaks remain unmeasured.'))
    if workspace < HOST_RESERVE+APP_RESERVE or scratch < HOST_RESERVE:
        raise RuntimeError('Insufficient host-provider/application disk headroom; see preflight.json')


def verify(plan, evidence):
    # The existing HTTP validator hashes its prefix and corresponding sources.
    http.verify_prefix(plan, evidence)
    host = plan['identity']['host']; prefix = ROOT / plan['host_prefix']
    receipt = verify_prefix(prefix, plan['profile'], host['system'], host['architecture'])
    zlib = Path(plan['zlib'])
    if Path(receipt['zlib']).resolve() != zlib.resolve() or receipt['zlib_sha256'] != http.digest(zlib):
        raise RuntimeError('Host providers do not use the selected HTTP profile zlib')
    if host['system'] == 'Darwin' and (Path(receipt['sdk']).resolve() != Path(host['sdk']).resolve()
                                     or receipt['deployment_target'] != host['deployment_target']):
        raise RuntimeError('Host provider macOS SDK/deployment mismatch')
    http.save(evidence, 'host-dependency-manifest.json', receipt)
    http.save(evidence, 'verified.json', dict(profile=plan['profile'], prefix=plan['host_prefix'],
        manifest_sha256=http.digest(prefix/'VOID-HOST-DEPENDENCIES.json'), zlib_sha256=http.digest(zlib)))


def test_plan(plan, evidence, build):
    tests = json.loads(subprocess.check_output(['ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))['tests']
    names = {test['name'] for test in tests}
    required = COMMON_TESTS | (LINUX_TESTS if plan['identity']['host']['system'] == 'Linux' else set())
    if missing := sorted(required - names):
        raise RuntimeError(f'Host/application tests not registered: {missing}')
    if any(name.startswith('voidup_') and name not in COMMON_TESTS for name in names):
        raise RuntimeError('Private signed installation tests must remain outside hosted CTest')
    selected = [test for test in tests if test['name'] in required]
    for test in selected:
        properties = {item['name']: item['value'] for item in test.get('properties', [])}
        if properties.get('DISABLED') or not test.get('command'):
            raise RuntimeError(f'Host/application test disabled or empty: {test["name"]}')
    http.save(evidence, 'test-plan.json', dict(required=sorted(required), tests=selected,
        execution='Existing unfiltered CTest invocation; no exclusions added',
        private_signing_credentials=False))
    print(f'Host/VOIDUP test registration verified: {len(required)} required tests')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=['bootstrap', 'verify', 'test-plan'])
    parser.add_argument('--configuration', choices=['Debug', 'Release', 'Sanitizers'], default=os.environ.get('EXPERIMENT_CONFIGURATION', 'Release'))
    parser.add_argument('--evidence', type=Path, default=Path(os.environ.get('EXPERIMENT_DIR', 'build/link-experiment'))/'voidup')
    parser.add_argument('--build', type=Path)
    args = parser.parse_args(); evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    plan = layout(args.configuration); http.save(evidence, 'identity.json', plan)
    if args.phase == 'test-plan':
        test_plan(plan, evidence, args.build or ROOT/'build'/args.configuration)
        return
    if args.phase == 'bootstrap':
        preflight(plan, evidence)
        http.verify_prefix(plan, evidence)
        if (ROOT/plan['host_prefix']).exists():
            raise RuntimeError('Host prefix already exists; refusing implicit reuse (use verify for a local prefix)')
        before = set((ROOT/'docs/voidup/evidence').glob('dependencies-*'))
        try:
            run(evidence, 'build-host-providers', [sys.executable, ROOT/'tools/setup_host_deps.py', '--profile', plan['profile'], '--jobs', '3'])
        finally:
            # Retain native MSBuild/CMake/autoconf diagnostics even on failure.
            base = ROOT/plan['host_prefix'].replace('/prefix/', '/build/')
            for name in ('CMakeCache.txt', 'CMakeConfigureLog.yaml', 'config.log', 'test-suite.log', '*.trs', '*.log'):
                for source in base.rglob(name):
                    target=evidence/'provider-builds'/source.relative_to(base)
                    target.parent.mkdir(parents=True, exist_ok=True);shutil.copy2(source,target)
            for source in set((ROOT/'docs/voidup/evidence').glob('dependencies-*')) - before:
                shutil.copytree(source,evidence/'provider-builds'/source.name)
    verify(plan, evidence)


if __name__ == '__main__':
    main()
