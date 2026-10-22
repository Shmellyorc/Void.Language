"""Native-only, observational controls; never assume an error number or repair."""
import argparse
import json
import os
import pathlib
import platform
import subprocess
import tempfile


def main():
    assert os.name == 'nt', 'This diagnostic requires native Windows'
    import winreg
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=pathlib.Path)
    parser.add_argument('aware', type=pathlib.Path)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    evidence = {'platform': platform.platform(), 'runs': []}
    failure = False
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                           r'SYSTEM\CurrentControlSet\Control\FileSystem') as key:
            try:
                evidence['LongPathsEnabled'] = winreg.QueryValueEx(key, 'LongPathsEnabled')[0]
            except FileNotFoundError:
                evidence['LongPathsEnabled'] = 0
        with tempfile.TemporaryDirectory(prefix='VOID Stage9 é 🎮 # % ') as temporary:
            project = pathlib.Path(temporary) / 'game # % é'
            deep = project
            for n in range(6):
                deep /= 'nested-' + str(n) + '-' + 'x' * 36
            deep.mkdir(parents=True)
            (deep / 'main.void').write_text('return 7\n', encoding='utf-8')
            assert len(str(deep)) > 260
            evidence['deep_directory'] = str(deep)
            evidence['deep_utf16_length'] = len(str(deep).encode('utf-16-le')) // 2
            for label, executable in [('baseline', args.baseline), ('longPathAware', args.aware)]:
                for fixture, path in [('short_control', project), ('original_six_level', deep)]:
                    result = subprocess.run([str(executable.resolve()), str(path)],
                                            capture_output=True, text=True,
                                            encoding='utf-8', timeout=30)
                    record = dict(process=label, fixture=fixture, returncode=result.returncode,
                                  stdout=result.stdout, stderr=result.stderr, operations={})
                    for line in result.stdout.splitlines():
                        parts = line.split('\t', 3)
                        if len(parts) == 4:
                            record['operations'][parts[0]] = dict(code=int(parts[1]),
                                                                category=parts[2], message=parts[3])
                    evidence['runs'].append(record)
                    # A failing operation is evidence, not a failed diagnostic.
                    # Unexpected process/probe failures still fail the job, after saving all results.
                    if result.returncode != 0 or 'extended_canonical' not in record['operations']:
                        failure = True
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        # ASCII escapes keep Unicode evidence legible in every runner console.
        print(json.dumps(evidence, ensure_ascii=True, indent=2), flush=True)
    assert not failure, 'Probe process/control failed; inspect the preserved JSON before drawing conclusions'


if __name__ == '__main__':
    main()
