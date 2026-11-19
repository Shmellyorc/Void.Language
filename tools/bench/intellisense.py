#!/usr/bin/env python3
"""Measure real stdio completion round trips against an isolated project copy.

Example: python3 tools/bench/intellisense.py --server build/llvm23-release/voidls \
  --project /path/to/resolved-raylib-project --document src/app.void \
  --namespace rl --expected clear_background --output /tmp/fresh-g76-evidence
No catalog installation or native execution is performed. Output must be new.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lsp_client import Client, frame


class InteractiveClient(Client):
    def receive(self):
        while True:
            message = super().receive()
            if message.get('method') == 'workspace/semanticTokens/refresh':
                self.write(frame({'jsonrpc': '2.0', 'id': message['id'], 'result': None}))
            else:
                return message


def summary(samples):
    ordered = sorted(samples)
    return {'median_ms': statistics.median(ordered),
            'p95_ms': ordered[math.ceil(.95 * len(ordered)) - 1],
            'worst_ms': ordered[-1], 'samples_ms': samples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--document', required=True)
    parser.add_argument('--namespace', required=True)
    parser.add_argument('--expected', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=100)
    args = parser.parse_args()
    assert args.samples >= 5
    server = args.server.resolve()
    output = args.output.resolve()
    output.mkdir()  # Preserve unsuccessful as well as successful earlier runs.
    project = output / 'project'
    shutil.copytree(args.project.resolve(), project)
    document = (project / args.document).resolve()
    assert document.is_relative_to(project), 'Document must belong to the copied project'
    good = document.read_text()
    needle = args.namespace + '.'
    assert needle in good and good.endswith('\n'), 'Supply a valid namespace-using fixture ending in newline'
    report = {'schema_version': 1, 'server': str(server),
              'server_sha256': hashlib.sha256(server.read_bytes()).hexdigest(),
              'source_sha256': hashlib.sha256(good.encode()).hexdigest(),
              'platform': platform.platform(), 'cpu_count': os.cpu_count(),
              'detailed_instrumentation': os.environ.get('VOID_LSP_PROFILE') == '1',
              'samples_per_case': args.samples, 'warmup_requests': 5, 'results': {}}
    client = None

    def save():
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    try:
        with InteractiveClient(server, project, project=True) as client:
            start = time.perf_counter()
            client.request('initialize', {'capabilities': {
                'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {})
            client.sync()
            version = 1
            client.notify('textDocument/didOpen', {'textDocument': {
                'uri': document.as_uri(), 'languageId': 'void', 'version': version, 'text': good}})
            report['initial_notifications'] = client.sync()
            report['cold_initialize_open_ms'] = (time.perf_counter() - start) * 1000

            def change(text):
                nonlocal version
                version += 1
                client.notify('textDocument/didChange', {'textDocument': {
                    'uri': document.as_uri(), 'version': version}, 'contentChanges': [{'text': text}]})

            def query(text):
                offset = text.rfind(needle) + len(needle)
                suffix = text[offset:]
                if suffix and suffix.isidentifier():
                    offset += len(suffix)  # Physical caret after the typed EOF prefix.
                before = text[:offset]
                position = {'line': before.count('\n'), 'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}
                start = time.perf_counter()
                response, _ = client.request('textDocument/completion', {
                    'textDocument': {'uri': document.as_uri()}, 'position': position})
                elapsed = (time.perf_counter() - start) * 1000
                assert 'result' in response, response
                items = response['result']
                items = items.get('items', []) if isinstance(items, dict) else items
                assert args.expected in {i['label'] for i in items}, response
                return elapsed

            for name, text in [('warm_call', good), ('warm_newline', good + '    ' + needle + '\n'),
                               ('warm_eof', good + '    ' + needle)]:
                if text != good:
                    change(text)
                    client.sync()
                first = query(text)
                for _ in range(5):
                    query(text)
                report['results'][name] = {'first_request_ms': first, **summary([query(text) for _ in range(args.samples)])}
                save()
            samples = []
            for n in range(args.samples):
                # Immediate unsaved body edits, including the physical EOF boundary.
                text = good + '    ' + needle + args.expected[:n % 4]
                start = time.perf_counter()
                change(text)
                query(text)
                samples.append((time.perf_counter() - start) * 1000)
            report['results']['edited_eof'] = summary(samples)
            client.shutdown()
            report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        if client is not None:
            (output / 'server-stderr.log').write_bytes(client.logs)
        save()
    print(json.dumps({key: {k: v for k, v in value.items() if k != 'samples_ms'}
                      for key, value in report['results'].items()}, indent=2))


if __name__ == '__main__':
    main()
