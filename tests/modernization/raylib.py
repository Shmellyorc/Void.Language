"""G88 typed raylib values, exact native conversions and compiler-owned tooling.

raylib.py voidc voidls approved-catalog receipt.json
Uses existing admitted provider artifacts; no provider installation or approval.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/raylib'))
from lsp_client import Client
from enums import ENUMS, verify_enums
from test_output import output_directory

compiler, server, catalog, report = (Path(p).resolve() for p in sys.argv[1:])
env = dict(os.environ, VOID_NATIVE_CATALOG=str(catalog))
records = []
source = '''use pkg/raylib as rl
use io as io
use log as log
func identity<t>(input: t) -> t?:
    return input
# Pinned provider AutomationEventType: INPUT_KEY_UP=1, INPUT_KEY_DOWN=2.
# Only valid provider codes are injected: automation payloads are not range checked.
func keyboard_contracts() -> i32:
    if rl.keyboard_key.escape.value != 256 or rl.keyboard_key.a.value != 65:
        return 20
    for code in [i32.min_value, -1, 0, 512, i32.max_value]:
        if rl.is_key_pressed_code(code) or rl.is_key_pressed_repeat_code(code) or rl.is_key_down_code(code) or rl.is_key_released_code(code) or rl.is_key_up_code(code):
            return 21
    # Check both named ergonomics and direct enum-to-i32 native conversion.
    rl.play_automation_event(rl.automation_event(0, 2, [rl.keyboard_key.a.value, 0, 0, 0]))
    if not rl.is_key_down(rl.keyboard_key.a) or not rl.is_key_pressed(rl.keyboard_key.a) or rl.is_key_up(rl.keyboard_key.a):
        return 22
    if rl.is_key_pressed_repeat(rl.keyboard_key.a) != rl.is_key_pressed_repeat_code(65) or rl.is_key_released(rl.keyboard_key.a) != rl.is_key_released_code(65):
        return 23
    if rl.get_key_pressed() != 65 or rl.get_key_pressed() != 0:
        return 24
    rl.play_automation_event(rl.automation_event(0, 1, [65, 0, 0, 0]))
    if rl.is_key_down(rl.keyboard_key.a) or not rl.is_key_up(rl.keyboard_key.a):
        return 25
    for code in [302, 314]:
        if code as? rl.keyboard_key is Some(_):
            return 26
        rl.play_automation_event(rl.automation_event(0, 2, [code, 0, 0, 0]))
        var queued = rl.get_key_pressed()
        if queued != code or not rl.is_key_down_code(queued) or not rl.is_key_pressed_code(queued) or rl.is_key_up_code(queued):
            return 27
        if rl.is_key_pressed_repeat_code(queued) or rl.is_key_released_code(queued) or rl.get_key_pressed() != 0:
            return 28
        rl.set_exit_key_code(queued)
        rl.play_automation_event(rl.automation_event(0, 1, [code, 0, 0, 0]))
        if rl.is_key_down_code(code) or not rl.is_key_up_code(code):
            return 29
    rl.set_exit_key_code(0)
    rl.set_exit_key(rl.keyboard_key.escape)
    # Headless backend may supply no name. Preserve nullable owner/close behavior.
    var named = rl.get_key_name(rl.keyboard_key.a)
    if named != null:
        rl.buffer_close(named as rl.byte_buffer)
    var unnamed = rl.get_key_name_code(302)
    if unnamed != null:
        rl.buffer_close(unnamed as rl.byte_buffer)
    return 0

func main() -> i32:
    rl.set_trace_log_level(rl.trace_log_level.warning)
    var keyboard_result = keyboard_contracts()
    if keyboard_result != 0:
        return keyboard_result
    var mask = rl.config_flags.window_hidden | rl.config_flags.window_resizable
    if not mask.has(rl.config_flags.window_hidden) or mask.value != 132:
        return 1
    rl.set_config_flags(mask)
    var extended = 2147483648 as rl.config_flags
    if extended.value != 2147483648:
        return 2
    if 999 as? rl.keyboard_key is Some(_):
        return 3
    var optional = identity(rl.keyboard_key.escape)
    if optional is Ok(var key):
        if key.value != 256:
            return 4
    else:
        return 5
    var camera = rl.camera_3d(projection = rl.camera_projection.orthographic)
    if camera.projection != rl.camera_projection.orthographic.value:
        return 6
    var patch = rl.n_patch_info(rl.rectangle(), layout = rl.n_patch_layout.three_patch_vertical)
    if patch.layout != 1:
        return 7
    if rl.get_pixel_data_size(2, 3, rl.pixel_format.uncompressed_r8g8b8a8) != 24:
        return 8
    var owned = rl.gen_image_color(2, 3, rl.color(r = 7)) as rl.image
    defer rl.unload_image(owned)
    rl.image_format(owned, rl.pixel_format.uncompressed_r8g8b8a8)
    if owned.pixel_format != rl.pixel_format.uncompressed_r8g8b8a8.value or owned.width != 2:
        return 9
    var selection = rl.gesture_flags.tap | rl.gesture_flags.swipe_left
    if not selection.has_any(rl.gesture_flags.tap) or selection.without(rl.gesture_flags.tap).value != (32 as u32):
        return 10
    var lookup: {rl.keyboard_key: string} = {rl.keyboard_key.escape: "escape"}
    if lookup[rl.keyboard_key.escape] != rl.keyboard_key.escape.name:
        return 11
    if io.open_mode.read.value != 0 or log.level.info.value != 2:
        return 12
    print("G88 raylib typed/native contracts passed")
    return 0
'''


def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def save():
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576, source=source,
        compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
        server_sha256=hashlib.sha256(server.read_bytes()).hexdigest(),
        catalog={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in catalog.iterdir() if p.is_file()},
        records=records), indent=2) + '\n')


def run(args, expected=None, status=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env,
                            timeout=240, preexec_fn=limits if os.name == 'posix' else None)
    row = dict(command=list(map(str, args)), exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
    records.append(row); save()
    assert result.returncode == status, row
    if expected:
        assert expected in result.stdout, row
    assert not any(word in result.stderr for word in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')), row


inventory = json.loads((ROOT / 'tools/raylib/inventory.json').read_text())
verify_enums((ROOT / 'verified-packages/raylib/src/raylib.void').read_text(), inventory['enumerations'])
# Every provider discriminant is checked in actual compiled code, independently
# of the source declaration parser. The Clang/header parity gate checks the oracle.
checks = []
for enumeration in inventory['enumerations']:
    for value in enumeration['values']:
        checks.append(f'    if rl.{value["void_name"]}.value != {value["value"]}:\n        return 90\n')
source = source.replace('    print("G88 raylib', ''.join(checks) + '    print("G88 raylib')

with tempfile.TemporaryDirectory(prefix='g88-raylib-', dir=output_directory()) as directory:
    work = Path(directory); project = work / 'consumer'
    run([compiler, 'new', project])
    run([compiler, 'add', 'raylib', '--project', project, '--dir', ROOT / 'verified-packages/raylib'])
    with (project / 'void.project.toml').open('a') as file:
        file.write('\n[native-requirements.raylib]\nid="org.voidlang.raylib"\nversion="=6.0.0"\nabi=3\n')
    entry = next((project / 'src').glob('*.void')); entry.write_text(source)
    run([compiler, 'tidy', '--project', project])
    run([compiler, 'check', '--project', project])
    for profile in ('debug', 'release'):
        run([compiler, 'run', '--project', project, '--allow-native', '--profile', profile, '--no-cache'],
            'G88 raylib typed/native contracts passed')
        exe = work / (profile + ('.exe' if os.name == 'nt' else '.app'))
        run([compiler, 'build', '--project', project, '--allow-native', '--profile', profile, '-o', exe])
        run([exe], 'G88 raylib typed/native contracts passed')
    # Named and raw operations retain separate exact, nominal argument contracts.
    for operation, expression in [('set_exit_key', '7'), ('set_exit_key', 'rl.texture_filter.point'),
                                  ('set_exit_key_code', 'rl.keyboard_key.escape'),
                                  ('is_key_down_code', 'rl.texture_filter.point'),
                                  ('is_key_down_code', '302 as i64'),
                                  ('get_key_name_code', '302.0')]:
        entry.write_text('use pkg/raylib as rl\nfunc main():\n    rl.' + operation + '(' + expression + ')\n')
        run([compiler, 'check', '--project', project], status=1)
    entry.write_text(source)
    os.environ['VOID_NATIVE_CATALOG'] = str(catalog)
    with Client(server, project, project=True, preexec_fn=limits if os.name == 'posix' else None) as client:
        initialized, _ = client.request('initialize', {'capabilities': {'textDocument': {
            'publishDiagnostics': {'versionSupport': True},
            'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                               'tokenTypes': ['enum', 'enumMember', 'function'], 'tokenModifiers': []}}}})
        client.notify('initialized', {}); client.sync()
        uri = entry.as_uri()
        client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'version': 1,
                      'languageId': 'void', 'text': source}})
        notes = client.sync(); records.append(dict(diagnostics=notes)); save()
        assert not [d for note in notes for d in note['diagnostics'] if d.get('severity') == 1], notes

        def query(method, needle=None, delta=0, **extra):
            params = {'textDocument': {'uri': uri}, **extra}
            if needle:
                before = source[:source.index(needle) + delta]
                params['position'] = dict(line=before.count('\n'), character=len(before.rsplit('\n', 1)[-1]))
            response, _ = client.request('textDocument/' + method, params)
            records.append(dict(method=method, response=response)); save()
            assert 'error' not in response, response
            return response.get('result')

        hover = query('hover', 'rl.config_flags.window_hidden', len('rl.config_flags.') + 2)
        assert 'hide window' in str(hover).lower(), hover
        defined = query('definition', 'rl.config_flags.window_hidden', len('rl.config_flags.') + 2)
        assert 'raylib.void' in str(defined), defined
        references = query('references', 'rl.keyboard_key.escape', len('rl.keyboard_key.') + 2,
                           context={'includeDeclaration': True})
        assert references and len(references) >= 3, references
        items = query('completion', 'rl.config_flags.window_hidden', len('rl.config_flags.'))
        assert 'window_resizable' in str(items) and 'window_hidden' in str(items), items
        signature = query('signatureHelp', 'rl.get_pixel_data_size(2, 3, ', len('rl.get_pixel_data_size(2, 3, '))
        assert 'format: pixel_format' in str(signature) and 'exact integer value' in str(signature), signature
        raw_hover = query('hover', 'rl.is_key_down_code(queued)', len('rl.') + 4)
        assert 'raw provider key code' in str(raw_hover).lower() and '512' in str(raw_hover), raw_hover
        raw_signature = query('signatureHelp', 'rl.is_key_down_code(queued)', len('rl.is_key_down_code('))
        assert 'key: i32' in str(raw_signature) and '302' in str(raw_signature), raw_signature
        raw_items = query('completion', 'rl.is_key_down_code(queued)', len('rl.'))
        assert 'is_key_down_code' in str(raw_items) and 'is_key_down' in str(raw_items), raw_items
        tokens = query('semanticTokens/full')
        legend = initialized['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        kinds = {legend[tokens['data'][i + 3]] for i in range(0, len(tokens['data']), 5)}
        assert {'enum', 'enumMember'} <= kinds, kinds
        records.append(dict(server_stderr=client.logs.decode())); save()
print(f'G88 raylib: {sum(len(e["values"]) for e in inventory["enumerations"])} discriminants, JIT/AOT O0/O2 keyboard codes and 9 editor queries passed')
