"""Physical raylib names/docs, named calls and compiler-owned editor results.

Usage: raylib_documentation.py voidc voidls approved-native-catalog
       raylib_documentation.py --docs-only
The native catalog is prepared separately. All project edits are isolated.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from lsp_client import Client

sys.path.insert(0, str(ROOT / 'tools/raylib'))
from public_docs import public_functions, verify_documentation_meanings, verify_fallible_api, verify_numeric_names
from names import snake, abi_snake


def documentation_checks():
    functions = {function['name']: function for function in public_functions(
        (ROOT / 'verified-packages/raylib/src/raylib.void').read_text())}
    source = (ROOT / 'verified-packages/raylib/src/raylib.void').read_text()
    assert verify_fallible_api(source) == 34
    numeric_names = verify_numeric_names(source)
    for original, preferred in {
        'Camera2D': 'camera_2d', 'Camera3D': 'camera_3d',
        'BeginMode2D': 'begin_mode_2d', 'DrawTriangleStrip3D': 'draw_triangle_strip_3d',
        'Vector3': 'vector3', 'ComputeSHA256': 'compute_sha256',
        'LoadUTF8': 'load_utf8', 'NPatchInfo': 'n_patch_info',
    }.items():
        assert snake(original) == preferred, (original, snake(original))
    assert abi_snake('Camera3D') == 'camera3_d'
    assert abi_snake('BeginMode2D') == 'begin_mode2_d'
    assert len(numeric_names) == 14
    for original, changed in [
        ('func camera_2d(', 'func missing_camera_2d('),
        ('func end_mode_3d():', 'func end_mode3_d():'),
        ('func camera_3d(', 'func camera3_d('),
    ]:
        assert original in source
        try:
            verify_numeric_names(source.replace(original, changed, 1))
        except ValueError:
            pass
        else:
            raise AssertionError(('accepted missing or retired public name', original))
    for function in functions.values():
        verify_documentation_meanings(function)
    # Reintroduce independent historical template errors in memory. These
    # guards must reject the mistakes rather than merely count present tags.
    mistakes = [
        ('window_should_close', '@return ', 'The resource-ledger close status. This operation consumes the supplied owner.'),
        ('gallery_should_close', '@return ', 'The resource-ledger close status.'),
        ('set_texture_filter', '@param filter ', 'File-extension filter.'),
        ('load_font_from_image', '@param key ', 'Keyboard key code.'),
        ('load_font_from_image', '@param first_char ', 'Starting array index.'),
        ('draw_grid', '@param spacing ', 'Character spacing in pixels.'),
        ('set_text_line_spacing', '@param spacing ', 'Character spacing.'),
        ('gen_image_gradient_linear', '@param direction ', 'Direction vector.'),
        ('seek_music_stream', '@param position ', 'Position in pixels.'),
        ('draw_cube', '@param length ', 'Number of elements to read.'),
        ('get_monitor_count', '@return ', 'Number of elements in the array.'),
        ('get_random_value', '@return ', 'The a random integer.'),
        ('get_time', '@return ', 'The numeric result described above.'),
        ('get_directory_path', 'Copy the directory', 'Get a path (uses static string).'),
        ('compute_md5', 'Compute an MD5', 'Compute MD5, returns static int[4].'),
        ('load_model_from_mesh', 'Create a model', 'This mutation invalidates earlier views.'),
        ('image_draw_triangle_ex', '@param c2 ', 'Second control point.'),
        ('image_color_replace', '@param replace ', 'Text to replace.'),
        ('load_font_from_memory', '@param file_type ', 'File format such as ".png".'),
        ('get_file_extension', '@param file_name ', 'Relative paths use the process working directory.'),
    ]
    for name, prefix, replacement in mistakes:
        function = dict(functions[name])
        docs = list(function['docs'])
        indices = [i for i, line in enumerate(docs) if line.startswith(prefix)]
        assert len(indices) == 1, (name, prefix, indices)
        # Preserve tag/label spelling so each case exercises meaning checks.
        docs[indices[0]] = (prefix if prefix.startswith('@') else '') + replacement
        function['docs'] = docs
        try:
            verify_documentation_meanings(function)
        except ValueError:
            pass
        else:
            raise AssertionError((name, 'accepted inaccurate documentation', replacement))
    for original, replacement in [
        ('func try_load_image(file_name: string)', 'func try_load_image(path: string)'),
        ('func try_load_image(file_name: string) -> result<image, load_error>:', 'func try_load_image(file_name: string) -> result<image, string>:'),
        ('## @return Ok with one image owner', '## @return A resource'),
        ('## A requested font or custom shader resolved', '# A requested font or custom shader resolved'),
    ]:
        # Target the public companion section, preserving original coverage.
        start = source.index('# G77 fallible companions.')
        tail = source[start:]
        assert original in tail, original
        changed = source[:start] + tail.replace(original, replacement, 1)
        try:
            verify_fallible_api(changed)
        except ValueError:
            pass
        else:
            raise AssertionError(('accepted broken fallible documentation', original))
    print(f'Raylib documentation checks: {len(functions)} public functions, 34 companions, {len(mistakes)} historical and 4 fallible documentation errors; 14 canonical numeric names, 8 naming cases and 3 retirement mutations passed')


documentation_checks()
if sys.argv[1:] == ['--docs-only']:
    raise SystemExit(0)

compiler, server, catalog = (Path(arg).resolve() for arg in sys.argv[1:4])
SOURCE = '''use pkg/raylib as rl
func main() -> i32:
    var ink = rl.color(10, 20, 30, 255)
    var faded = rl.fade(color_value = ink, alpha = 0.5)
    if faded.a != 127:
        return 1
    var codepoint = rl.get_codepoint(text = "A")
    if codepoint.value != 65 or codepoint.size != 1:
        return 2
    var previous = rl.get_codepoint_previous(text = "AB", position = 2)
    if previous.value != 66 or previous.size != 1:
        return 3
    var image = rl.gen_image_color(width = 2, height = 2, color_value = ink) as rl.image
    if rl.unload_image(image_value = image) != 0:
        return 4
    if rl.get_random_value(min = 41, max = 41) != 41:
        return 5
    if false:
        rl.draw_circle(center_x = 10, center_y = 20, radius = 5, color_value = ink)
    print("raylib descriptive named arguments passed")
    return 0
'''


def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'),
            'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}


with tempfile.TemporaryDirectory(prefix='VOID raylib documentation ') as scratch:
    root = Path(scratch)
    env = dict(os.environ, VOID_NATIVE_CATALOG=str(catalog))

    def run(*arguments, status=0):
        result = subprocess.run(list(map(str, arguments)), cwd=root, env=env,
                                capture_output=True, timeout=240)
        output = (result.stdout + result.stderr).decode(errors='replace')
        assert result.returncode == status, (arguments, result.returncode, output)
        assert not any(word in output for word in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')), output
        return output

    project = root / 'consumer'
    run(compiler, 'new', project)
    run(compiler, 'add', 'raylib', '--project', project, '--dir', ROOT / 'verified-packages/raylib')
    manifest = project / 'void.project.toml'
    with manifest.open('a') as file:
        file.write('\n[native-requirements.raylib]\nid="org.voidlang.raylib"\nversion="=6.0.0"\nabi=3\n')
    entry = next((project / 'src').glob('*.void'))
    entry.write_text(SOURCE)
    run(compiler, 'tidy', '--project', project)
    run(compiler, 'check', '--project', project)
    for profile in ('debug', 'release'):
        assert 'descriptive named arguments passed' in run(
            compiler, 'run', '--project', project, '--allow-native', '--profile', profile)
        image = root / profile / ('app.exe' if os.name == 'nt' else 'app')
        run(compiler, 'build', '--project', project, '--allow-native', '--profile', profile, '-o', image)
        assert 'descriptive named arguments passed' in run(image)
    entry.write_text(SOURCE.replace('center_x = 10', 'p0 = 10'))
    assert 'p0' in run(compiler, 'check', '--project', project, status=1)
    entry.write_text(SOURCE)
    run(compiler, 'check', '--project', project)

    # Client has no environment override. Only this test process inherits the
    # isolated, explicitly supplied catalog for its server subprocess.
    os.environ['VOID_NATIVE_CATALOG'] = str(catalog)
    numeric_names = verify_numeric_names((ROOT / 'verified-packages/raylib/src/raylib.void').read_text())
    with Client(server, project, project=True) as client:
        client.request('initialize', {'capabilities': {'textDocument': {
            'publishDiagnostics': {'versionSupport': True},
            'signatureHelp': {'signatureInformation': {'documentationFormat': ['plaintext']}},
            'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                               'tokenTypes': ['namespace', 'function', 'variable'], 'tokenModifiers': []},
        }}})
        client.notify('initialized', {})
        notes = client.sync()
        assert not [d for note in notes for d in note['diagnostics']], notes
        uri, version, text = entry.as_uri(), 0, SOURCE

        def update(new):
            global text, version
            text = new
            version += 1
            client.notify('textDocument/didOpen' if version == 1 else 'textDocument/didChange',
                          {'textDocument': {'uri': uri, 'version': version, 'languageId': 'void', 'text': text}}
                          if version == 1 else
                          {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': text}]})
            return client.sync()

        def query(method, needle, delta=0):
            response, _ = client.request('textDocument/' + method,
                                         {'textDocument': {'uri': uri}, 'position': point(text, needle, delta)})
            assert 'result' in response, response
            return response['result']

        update(SOURCE)
        hover = query('hover', 'rl.draw_circle', len('rl.'))['contents']['value']
        assert 'center_x: i32' in hover and 'center_y: i32' in hover, hover
        assert 'color_value' in hover and 'Draw a color-filled circle.' in hover, hover
        returns = query('hover', 'rl.get_codepoint(', len('rl.'))['contents']['value']
        assert 'size' in returns and 'consumed UTF-8 bytes' in returns, returns
        base = 'use pkg/raylib as rl\nfunc main():\n    '
        for call, active, description in [
            ('rl.draw_circle(', 0, 'Horizontal coordinate of the circle center in pixels.'),
            ('rl.draw_circle(10, ', 1, 'Vertical coordinate of the circle center in pixels.'),
            ('rl.draw_circle(radius = ', 2, 'Circle radius in pixels.'),
            ('rl.load_image(', 0, 'Path to the file. Relative paths use the process working directory.'),
            ('rl.get_codepoint_previous(position = ', 1,
             'UTF-8 byte position immediately after the codepoint. A negative value selects the end of the text.'),
            ('rl.set_texture_filter(filter = ', 1,
             'Texture scaling filter selected from the texture_filter_* constants.'),
            ('rl.load_font_from_image(key = ', 1,
             'Image background and transparency color key used to locate glyphs.'),
            ('rl.draw_grid(spacing = ', 1, 'Distance between adjacent grid lines in world units.'),
            ('rl.set_text_line_spacing(', 0, 'Vertical spacing in pixels between successive lines of text.'),
            ('rl.image_draw_triangle_ex(c2 = ', 5, 'Color at vertex v2 for interpolation across the triangle.'),
        ]:
            update(base + call)
            help = query('signatureHelp', call, len(call))
            assert help and help['activeParameter'] == active, (call, help)
            signature = help['signatures'][0]
            assert signature['parameters'][active]['documentation']['value'] == description, help

        for call, active in [('rl.camera_2d(zoom = ', 3), ('rl.camera_3d(fovy = ', 3),
                             ('rl.begin_mode_3d(', 0), ('rl.draw_line_3d(', 0)]:
            if 'camera_' in call:
                name = call.split('(', 1)[0]
                update(base + 'var selected_camera = ' + name + '()')
                hover = query('hover', name, len('rl.') + 2)['contents']['value']
                assert 'omitted' in hover, hover
            update(base + call)
            help = query('signatureHelp', call, len(call))
            assert help and help['activeParameter'] == active, (call, help)
            signature = help['signatures'][0]
            assert call[3:call.index('(')] in signature['label'], signature
            if 'camera_' in call:
                assert '= …' in signature['label'], signature
        update(base + 'rl.')
        response = query('completion', 'rl.', len('rl.'))
        items = response['items'] if isinstance(response, dict) else response
        labels = {item['label'] for item in items}
        assert set(numeric_names.values()) <= labels, labels
        assert not set(numeric_names) & labels, labels

        for call, active, description in [
            ('rl.try_load_image(', 0, 'Path to the file. Relative paths use the process working directory.'),
            ('rl.try_load_image_raw(width = ', 1, 'Width in pixels.'),
            ('rl.try_load_shader(fs_file_name = ', 1, 'Optional fragment-shader file path. null requests the native default. Present text must be NUL-free UTF-8.'),
            ('rl.try_load_font_ex(font_size = ', 1, 'Font size in pixels.'),
        ]:
            update(base + call)
            help = query('signatureHelp', call, len(call))
            assert help and help['activeParameter'] == active, (call, help)
            signature = help['signatures'][0]
            assert 'result<' in signature['label'] and 'load_error>' in signature['label'], signature
            assert signature['parameters'][active]['documentation']['value'] == description, help
        update(base + 'rl.try_load_')
        response = query('completion', 'rl.try_load_', len('rl.try_load_'))
        candidates = response['items'] if isinstance(response, dict) else response
        image_item = next(item for item in candidates if item['label'] == 'try_load_image')
        assert 'Ok' in image_item['documentation'] and 'Err' in image_item['documentation'] and 'unload_image' in image_item['documentation'], image_item
        update(base + 'var candidate = rl.try_load_image("missing.png")')
        hover = query('hover', 'rl.try_load_image', len('rl.'))['contents']['value']
        assert 'result<image, load_error>' in hover and 'unload_image' in hover and 'Err' in hover, hover

        update(base + 'var closing = rl.window_should_close()')
        closing = query('hover', 'rl.window_should_close', len('rl.'))['contents']['value']
        assert 'True when the application should close its window.' in closing, closing
        assert not any(term in closing for term in ('resource-ledger', 'consumes')), closing

        update(base + 'rl.draw_circle(cen')
        result = query('completion', 'rl.draw_circle(cen', len('rl.draw_circle(cen'))
        items = result['items'] if isinstance(result, dict) else result
        labels = {item['label'].removesuffix(' =') for item in items}
        assert {'center_x', 'center_y'} <= labels, labels
        assert not {'p0', 'p1'} & labels, labels

        notes = update(SOURCE)
        assert notes and all(not note['diagnostics'] for note in notes), notes
        semantic, _ = client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}})
        assert semantic['result']['data']
        client.shutdown()

print('Raylib docs: debug/release named calls, JIT/AOT, hover, active parameter docs, named completion and repair passed')
