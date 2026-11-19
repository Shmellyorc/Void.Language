#!/usr/bin/env python3
"""Measure warm and newly edited compiler assistance in a resolved raylib copy.

Use --server, --project (the preserved resolved raylib fixture), and a fresh
--output directory. VOID_LSP_PROFILE=1 records inclusive compiler stage timings
on stderr; ordinary measurements leave it unset. No native code is executed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import time

from intellisense import InteractiveClient, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=20)
    args = parser.parse_args()
    assert args.samples >= 5
    server, output = args.server.resolve(), args.output.resolve()
    output.mkdir()
    root = output / 'project'
    shutil.copytree(args.project.resolve(), root)
    source = ('use pkg/raylib as rl\nfunc main():\n'
              '    var shade = rl.color(10, 20, 30, 255)\n'
              '    var channel = shade.r\n    var opacity: f32 = 0.5\n'
              '    var faded = rl.fade(shade, opacity)\n')
    document = root / 'src/app.void'
    invalid = source.replace('shade.r', 'missing')
    document.write_text(invalid)  # No successful analysis can precede the first probe.
    report = {'schema_version': 1, 'server': str(server),
              'server_sha256': hashlib.sha256(server.read_bytes()).hexdigest(),
              'detailed_instrumentation': os.environ.get('VOID_LSP_PROFILE') == '1',
              'samples_per_case': args.samples, 'warmup_requests': 3, 'results': {}}
    client = None

    def save():
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    try:
        with InteractiveClient(server, root, project=True) as client:
            client.request('initialize', {'capabilities': {
                'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {})
            version = 1
            client.notify('textDocument/didOpen', {'textDocument': {'uri': document.as_uri(),
                          'languageId': 'void', 'version': version, 'text': invalid}})
            diagnostics = client.sync()
            assert any(note['diagnostics'] for note in diagnostics), diagnostics
            version += 1
            start = time.perf_counter()
            client.notify('textDocument/didChange', {'textDocument': {
                'uri': document.as_uri(), 'version': version}, 'contentChanges': [{'text': source}]})
            response, _ = client.request('textDocument/completion', {
                'textDocument': {'uri': document.as_uri()},
                'position': {'line': 3, 'character': len('    var channel = shade.')}})
            report['no_history_first_ms'] = (time.perf_counter() - start) * 1000
            assert 'result' in response, response
            items = response['result']
            items = items.get('items', []) if isinstance(items, dict) else items
            assert 'r' in {item['label'] for item in items}, response
            save()
            for method, line, character in [('completion', 3, len('    var channel = shade.')),
                                            ('signatureHelp', 5, len('    var faded = rl.fade(shade, '))]:
                for edited in (False, True):
                    samples = []
                    for n in range(args.samples + 3):
                        client.sync()  # Settling is outside the measured interval.
                        start = time.perf_counter()
                        if edited:
                            version += 1
                            client.notify('textDocument/didChange', {'textDocument': {
                                'uri': document.as_uri(), 'version': version},
                                'contentChanges': [{'text': source + '# unsaved ' + str(version) + '\n'}]})
                        response, _ = client.request('textDocument/' + method, {
                            'textDocument': {'uri': document.as_uri()},
                            'position': {'line': line, 'character': character}})
                        elapsed = (time.perf_counter() - start) * 1000
                        assert 'result' in response, response
                        value = response['result']
                        if method == 'completion':
                            items = value.get('items', []) if isinstance(value, dict) else value
                            assert 'r' in {item['label'] for item in items}, response
                        else:
                            assert value['activeParameter'] == 1 and 'alpha' in value['signatures'][0]['label'], response
                        if n >= 3:
                            samples.append(elapsed)
                    report['results'][('edited_' if edited else 'warm_') + method] = summary(samples)
                    save()
            client.sync()
            client.shutdown()
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', error=repr(error))
        raise
    finally:
        if client is not None:
            (output / 'server-stderr.log').write_bytes(client.logs)
        save()
    print(json.dumps({name: {k: v for k, v in result.items() if k != 'samples_ms'}
                      for name, result in report['results'].items()}, indent=2))


if __name__ == '__main__':
    main()
