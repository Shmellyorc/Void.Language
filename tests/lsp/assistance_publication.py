"""Deterministic native-Linux publication regression using an existing GDB.

Own child processes only; no production patches, timing sleeps or native code
execution. GDB changes a disposable closed input before the publication guard.
Example: assistance_publication.py --server build/llvm23-debug/voidls
  --compiler build/llvm23-debug/voidc --output /tmp/fresh-publication-evidence
The portable owned-input checks also run in the registered tooling test.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

from intellisense import InteractiveClient
from lsp_client import frame


def traceable():
    # Permit inspection of this task-owned child even with Linux ptrace_scope=1.
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(0x59616d61, ctypes.c_ulong(-1), 0, 0, 0):
        raise OSError(ctypes.get_errno(), 'Cannot authorize debugger for child')


def run(args):
    assert sys.platform == 'linux', 'This debugger qualification requires native Linux'
    gdb = shutil.which(args.gdb)
    assert gdb, 'An existing GDB is required; this test never installs it'
    output = args.output.resolve()
    output.mkdir()  # Never overwrite previous favorable or unfavorable evidence.
    source = Path(__file__).resolve().parents[2] / 'src/lsp/main.cpp'
    line = args.publication_line
    if line is None:
        line = next(n for n, text in enumerate(source.read_text().splitlines(), 1)
                    if 'if(!vd::editor_assistance_inputs_current(result))' in text)
    results = []
    for kind in ['completion', 'signatureHelp']:
        for state in ['unchanged', 'edited', 'no-history'] + (['closed-target', 'hard-link'] if kind == 'completion' else []):
            if args.case and state != args.case:
                continue
            case = output / (kind + '-' + state)
            case.mkdir()
            root = case / 'app'
            subprocess.run([str(args.compiler), 'new', str(root)], check=True, capture_output=True, timeout=30)
            model = root / 'src/model.void'
            definitions = ('public value struct point:\n    public x: i32\n'
                           'public value struct other:\n    public y: i32\n'
                           '## @param left Input value.\npublic func blend(left: i32) -> i32:\n    return left\n')
            model.write_text(definitions)
            text = ('use "./model.void" as model\nfunc main() -> i32:\n'
                    '    var p = model.point()\n    var input = 1\n    var value = model.blend(input)\n    return p.x\n')
            initial = text if state != 'no-history' else text.replace('return p.x', 'return missing')
            app = root / 'src/app.void'
            twin = root / 'src/twin.void'
            if state == 'hard-link':
                twin.write_text(definitions)
                stamp = model.stat(); os.utime(twin, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
                text = text.replace('func main()', 'use "./twin.void" as twin\nfunc main()')
                initial = text
            app.write_text(initial)
            altered = definitions.replace('public x:', 'public y:') if kind == 'completion' else definitions.replace('left', 'item')
            changed_file = model
            if state == 'closed-target':
                changed_file = app
                altered = text.replace('model.point()', 'model.other()').replace('return p.x', 'return p.y')
            mutation = case / 'mutate.py'
            mutation.write_text('from pathlib import Path\nimport os\np=Path(' + repr(str(changed_file)) + ')\n'
                's=p.stat()\np.write_text(' + repr(altered) + ')\nos.utime(p,ns=(s.st_atime_ns,s.st_mtime_ns))\n'
                'Path(' + repr(str(case / 'mutated')) + ').write_text("changed before publication guard")\n')
            if state == 'hard-link':
                mutation.write_text('from pathlib import Path\nimport os\np=Path(' + repr(str(twin)) + ')\n'
                    'p.unlink()\nos.link(' + repr(str(model)) + ',p)\n'
                    'Path(' + repr(str(case / 'mutated')) + ').write_text("same bytes/time, conflicting physical identity")\n')
            with InteractiveClient(args.server, root, project=True, preexec_fn=traceable) as client:
                client.request('initialize', {'capabilities': {'textDocument': {'semanticTokens': {
                    'requests': {'full': True}, 'formats': ['relative']}},
                    'workspace': {'semanticTokens': {'refreshSupport': True}}},
                    'initializationOptions': {'voidInteractivePresentation': True}})
                client.notify('initialized', {}); client.sync()
                if state != 'closed-target':
                    client.notify('textDocument/didOpen', {'textDocument': {'uri': app.as_uri(),
                        'languageId': 'void', 'version': 1, 'text': initial}}); client.sync()
                commands = case / 'gdb.commands'
                attached = case / 'attached'
                commands.write_text('set pagination off\nset confirm off\nbreak ' + str(source) + ':' + str(line) +
                    '\ncommands\nsilent\ndisable 1\nbt 3\nshell python3 ' + shlex.quote(str(mutation)) +
                    '\ndetach\nquit\nend\nshell touch ' + shlex.quote(str(attached)) + '\ncontinue\n')
                with (case / 'gdb.log').open('w') as log:
                    debugger = subprocess.Popen([gdb, '--batch', '-p', str(client.process.pid), '-x', str(commands)],
                                                stdout=log, stderr=subprocess.STDOUT)
                    try:
                        deadline = time.monotonic() + 15
                        while not attached.exists():
                            assert debugger.poll() is None, (case / 'gdb.log').read_text()
                            assert time.monotonic() < deadline, 'Debugger attach timed out'
                            time.sleep(.01)  # Readiness polling, not a race-triggering delay.
                        position = {'line': 5, 'character': len('    return p.')} if kind == 'completion' else {
                            'line': 4, 'character': len('    var value = model.blend(')}
                        if state == 'hard-link': position['line'] += 1
                        params = {'textDocument': {'uri': app.as_uri()}, 'position': position}
                        change = b''
                        if state in ['edited', 'no-history']:
                            change = frame({'jsonrpc': '2.0', 'method': 'textDocument/didChange', 'params': {
                                'textDocument': {'uri': app.as_uri(), 'version': 2},
                                'contentChanges': [{'text': text + '# unsaved revision\n'}]}})
                        client.write(change + frame({'jsonrpc': '2.0', 'id': 'publication',
                            'method': 'textDocument/' + kind, 'params': params}))
                        response, _ = client.response('publication')
                        assert (case / 'mutated').exists(), 'Publication breakpoint was not reached'
                        results.append({'kind': kind, 'state': state, 'response': response,
                                        'closed_input_path': str(changed_file), 'closed_input': changed_file.read_text()})
                        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                        assert response.get('error', {}).get('code') == -32801, results[-1]
                        debugger.wait(timeout=15)  # Detached before sanitizer/leak shutdown checks.
                        client.sync()
                        if args.skip_retry:
                            client.shutdown()
                            continue
                        if state == 'hard-link':
                            still_invalid, _ = client.request('textDocument/' + kind, params)
                            assert still_invalid.get('error', {}).get('code') == -32801, still_invalid
                            assert model.read_text() == twin.read_text() == definitions
                            assert os.path.samefile(model, twin)
                            twin.unlink(); twin.write_text(definitions)
                        fresh, _ = client.request('textDocument/' + kind, params)
                        assert 'result' in fresh, fresh
                        if kind == 'completion':
                            items = fresh['result']
                            if isinstance(items, dict): items = items['items']
                            assert {item['label'] for item in items} == ({'x'} if state == 'hard-link' else {'y'}), fresh
                        else:
                            assert 'item: i32' in fresh['result']['signatures'][0]['label'], fresh
                        results[-1]['fresh_retry'] = fresh
                        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                        client.sync(); client.shutdown()
                    finally:
                        if debugger.poll() is None:
                            debugger.terminate(); debugger.wait(timeout=10)
            (case / 'server-stderr.log').write_bytes(client.logs)
            print(kind, state, 'rejected changed inputs and fresh retry succeeded', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gdb', default='gdb')
    parser.add_argument('--publication-line', type=int, help='Explicit original line for a preserved pre-repair binary')
    parser.add_argument('--skip-retry', action='store_true', help='Inspect original rejection separately from retry recovery')
    parser.add_argument('--case', choices=['unchanged', 'edited', 'no-history', 'closed-target', 'hard-link'])
    args = parser.parse_args()
    args.server = args.server.resolve(); args.compiler = args.compiler.resolve()
    run(args)
