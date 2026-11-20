"""Check the physical public API against pinned C names and Section 46 docs.

This maintenance check reads the library's one-line top-level signatures. It
does not generate native declarations or change compiler-owned import behavior.
"""
import re
from names import canonical_name, NUMERIC_PUBLIC_NAMES

# These source interfaces intentionally differ from their C prototypes.
# Entries include all source parameters, in their existing order.
ADAPTED_PARAMETERS = {
    'close_window': ['window'],
    'close_audio_device': ['device'],
    'get_codepoint_previous': ['text', 'position'],
    'gen_image_font_atlas': ['glyphs', 'glyph_count', 'font_size', 'padding', 'pack_method'],
    'text_format': ['value', 'spec'],
    'set_trace_log_callback': ['handler'],
    'set_load_file_data_callback': ['handler'],
    'set_save_file_data_callback': ['handler'],
    'set_load_file_text_callback': ['handler'],
    'set_save_file_text_callback': ['handler'],
    'set_audio_stream_callback': ['stream', 'handler'],
    'attach_audio_stream_processor': ['stream', 'handler'],
    'detach_audio_stream_processor': ['registration'],
    'attach_audio_mixed_processor': ['handler'],
    'detach_audio_mixed_processor': ['registration'],
}


def split_parameters(text):
    """Split known source signatures, retaining nested callback/array types."""
    parts, stack = [], []
    start = 0
    for index, char in enumerate(text):
        if char in '([<':
            stack.append(char)
        elif char in ')]>' and not (char == '>' and index and text[index - 1] == '-'):
            if not stack:
                raise ValueError('Unbalanced public signature: ' + text)
            opener = stack.pop()
            if (opener, char) not in [('(', ')'), ('[', ']'), ('<', '>')]:
                raise ValueError('Unbalanced public signature: ' + text)
        elif char == ',' and not stack:
            parts.append(text[start:index].strip())
            start = index + 1
    if stack:
        raise ValueError('Unbalanced public signature: ' + text)
    if text[start:].strip():
        parts.append(text[start:].strip())
    return parts


def public_functions(source):
    for match in re.finditer(r'^func (\w+)(?:<[^\n]*?>)?\(', source, re.M):
        start = match.end()
        end, depth = start, 1
        while depth:
            if source[end] == '(':
                depth += 1
            elif source[end] == ')':
                depth -= 1
            end += 1
        line_end = source.index('\n', end)
        parameters = []
        for declaration in split_parameters(source[start:end - 1]):
            label, typ = declaration.split(':', 1)
            parameters.append((label.removeprefix('const ').strip(), typ.strip()))
        result = source[end:line_end].strip().removeprefix('->').rstrip(':').strip()
        preceding = source[:match.start()].splitlines()
        docs = []
        for line in reversed(preceding):
            if not line.startswith('##'):
                break
            docs.append(line.removeprefix('##').strip())
        yield {'name': match[1], 'parameters': parameters, 'result': result,
               'begin': match.start(), 'start': start, 'end': end - 1,
               'line_end': line_end, 'docs': list(reversed(docs))}


def parameter_name(name, occupied, snake):
    label = snake(name)
    if label == 'event' or label in occupied:
        return label + '_value'
    return label


def expected_parameters(function, declaration, snake, occupied):
    name = function['name']
    if name in ADAPTED_PARAMETERS:
        return [parameter_name(label, occupied, snake) for label in ADAPTED_PARAMETERS[name]]
    parameters = [node for node in declaration.get('inner', []) if node['kind'] == 'ParmVarDecl']
    # Output pointers become result fields or counted owners. GenImageFontAtlas
    # is the sole omitted middle parameter and has an explicit mapping above.
    return [parameter_name(node['name'], occupied, snake) for node in parameters[:len(function['parameters'])]]


def verify_documentation_meanings(function):
    """Catch known cross-domain template errors without fixing prose in place."""
    name = function['name']
    docs = function['docs']
    text = '\n'.join(docs)
    returns = '\n'.join(line for line in docs if line.startswith('@return '))
    if function['result'] == 'bool' and re.search(r'resource-ledger|close status|consumes? .*owner', text, re.I):
        raise ValueError(f'{name}: a boolean query must not describe owner consumption')
    if re.search(r'The numeric result described above|Whether the condition described above holds|\bthe (?:the|a)\b', text, re.I):
        raise ValueError(f'{name}: vague or ungrammatical documentation')
    if name in {'get_monitor_count', 'get_gamepad_axis_count', 'get_touch_point_count',
                'get_directory_file_count', 'get_directory_file_count_ex', 'get_codepoint_count'}:
        if re.search(r'elements? in (?:the )?array', returns, re.I):
            raise ValueError(f'{name}: this count is not the length of an input array')
    if name in {'get_directory_path', 'get_prev_directory_path', 'get_working_directory',
                'get_application_directory', 'compute_md5', 'compute_sha1', 'compute_sha256', 'text_split'}:
        if re.search(r'(?:uses?|returns?|using).*static', text, re.I):
            raise ValueError(f'{name}: the VOID adapter copies the native static result')
    if name == 'load_model_from_mesh' and 'invalidates earlier views' in text:
        raise ValueError(f'{name}: an exclusive ownership transfer is not an in-place mutation')
    parameter_docs = dict(line.removeprefix('@param ').split(' ', 1)
                          for line in docs if line.startswith('@param '))
    wrong_domains = {
        ('set_texture_filter', 'filter'): r'file|extension',
        ('load_font_from_image', 'key'): r'keyboard|key code',
        ('load_font_from_image', 'first_char'): r'offset|index',
        ('draw_grid', 'spacing'): r'character|font|text',
        ('set_text_line_spacing', 'spacing'): r'character',
        ('gen_image_gradient_linear', 'direction'): r'vector',
        ('seek_music_stream', 'position'): r'pixels?',
        ('image_color_replace', 'replace'): r'text|string',
        ('load_font_from_memory', 'file_type'): r'\.png',
        ('is_file_name_valid', 'file_name'): r'working directory',
        ('get_file_extension', 'file_name'): r'working directory',
        ('is_file_extension', 'file_name'): r'working directory',
    }
    for label, typ in function['parameters']:
        description = parameter_docs[label]
        wrong = wrong_domains.get((name, label))
        if wrong and re.search(wrong, description, re.I):
            raise ValueError(f'{name}.{label}: documentation uses the wrong operation domain')
        if name == 'image_draw_triangle_ex' and typ == 'native.color' and 'control point' in description.lower():
            raise ValueError(f'{name}.{label}: this is a vertex color, not a Bezier control point')
        if label == 'length' and typ == 'f32' and name in {'draw_cube', 'draw_cube_wires', 'gen_mesh_plane', 'gen_mesh_cube'}:
            if re.search(r'element|character|count|array', description, re.I):
                raise ValueError(f'{name}.{label}: a world-space length is not an element count')


def verify_public_api(source, nodes, inventory, snake):
    upstream = {node['name']: node for node in nodes if node['kind'] == 'FunctionDecl'}
    entries = {entry.get('public_name', entry['void_name']): entry for entry in inventory['functions']}
    occupied = set(re.findall(r'^(?:func|struct|value struct|const) (\w+)', source, re.M))
    count = parameters = returns = 0
    for function in public_functions(source):
        name = function['name']
        labels = [label for label, _ in function['parameters']]
        if name in entries:
            expected = expected_parameters(function, upstream[entries[name]['name']], snake, occupied)
            if labels != expected:
                raise ValueError(f'{name}: public parameter names {labels} must match {expected}')
        elif any(re.fullmatch(r'p\d+', label) for label in labels):
            raise ValueError(f'{name}: anonymous helper parameter')
        docs = function['docs']
        if not docs or not any(line and not line.startswith('@') for line in docs):
            raise ValueError(f'{name}: missing public description')
        documented = [match[1] for line in docs if (match := re.fullmatch(r'@param (\w+) \S.*', line))]
        if documented != labels:
            raise ValueError(f'{name}: parameter documentation {documented} must match {labels}')
        return_docs = [line for line in docs if line.startswith('@return ')]
        if bool(function['result']) != bool(return_docs) or len(return_docs) > 1:
            raise ValueError(f'{name}: return documentation does not match signature')
        verify_documentation_meanings(function)
        count += 1
        parameters += len(labels)
        returns += bool(function['result'])
    verify_fallible_api(source)
    verify_numeric_names(source)
    print(f'Public documentation verified: {count} functions, {parameters} parameters, {returns} returns')


# Source-level companions; these do not add native declarations or ABI entries.
FALLIBLE_RESOURCES = {
    'image': ('load_image', 'load_image_raw', 'load_image_anim', 'load_image_anim_from_memory',
              'load_image_from_memory', 'load_image_from_texture', 'load_image_from_screen'),
    'texture': ('load_texture', 'load_texture_from_image', 'load_texture_cubemap'),
    'render_texture': ('load_render_texture',),
    'font': ('load_font', 'load_font_ex', 'load_font_from_memory'),
    'shader': ('load_shader', 'load_shader_from_memory'),
    'wave': ('load_wave', 'load_wave_from_memory'),
    'sound': ('load_sound', 'load_sound_from_wave', 'load_sound_alias'),
    'music': ('load_music_stream', 'load_music_stream_from_memory'),
    'audio_stream': ('load_audio_stream',),
    'model': ('load_model',),
    'byte_buffer': ('load_file_data', 'load_file_text', 'export_image_to_memory',
                    'compress_data', 'decompress_data'),
}
FALLIBLE_WRITES = ('save_file_data', 'save_file_text', 'export_image', 'export_wave')


# Raw-code companions preserve the provider domain beyond named KeyboardKey cases.
KEY_CODE_OPERATIONS = ('is_key_pressed', 'is_key_pressed_repeat', 'is_key_down',
                       'is_key_released', 'is_key_up', 'set_exit_key', 'get_key_name')


def verify_fallible_api(source):
    functions = {f['name']: f for f in public_functions(source)}
    expected = {operation: typ for typ, names in FALLIBLE_RESOURCES.items() for operation in names}
    expected.update((name, 'bool') for name in FALLIBLE_WRITES)
    companions = {'try_' + name for name in expected}
    raw_keys = {name + '_code' for name in KEY_CODE_OPERATIONS}
    if len(functions) != 798 or len(functions.keys() - companions - raw_keys) != 757:
        raise ValueError('Keep 757 base functions, 34 fallible companions and 7 raw keyboard-code operations')
    for name in KEY_CODE_OPERATIONS:
        raw = functions.get(name + '_code')
        if not raw or raw['parameters'] != [('key', 'i32')] or raw['result'] != functions[name]['result']:
            raise ValueError(name + '_code: retain the raw i32 provider contract and named result type')
    for operation, typ in expected.items():
        name = 'try_' + operation
        function = functions.get(name)
        if not function or function['parameters'] != functions[operation]['parameters']:
            raise ValueError(name + ': parameters must retain the existing operation labels/types/order')
        if function['result'] != f'result<{typ}, load_error>':
            raise ValueError(name + ': expected actual lowercase result type')
        docs = function['docs']
        labels = [label for label, _ in function['parameters']]
        tags = [m[1] for line in docs if (m := re.fullmatch(r'@param (\w+) \S.*', line))]
        if labels != tags:
            raise ValueError(name + ': missing or unordered parameter documentation')
        returns = [line for line in docs if line.startswith('@return ')]
        if len(returns) != 1 or not all(word in returns[0] for word in ('Ok', 'Err', 'load_error')):
            raise ValueError(name + ': document actual success and failure results')
        if typ != 'bool' and not all(word in returns[0] for word in ('owner', 'release')):
            raise ValueError(name + ': document resource ownership and release')
        if typ in ('font', 'shader') and 'default_fallback' not in returns[0]:
            raise ValueError(name + ': document native default fallback behavior')
        if not any(line and not line.startswith('@') for line in docs):
            raise ValueError(name + ': missing operation description')
    for declaration in ('enum load_error_kind:', 'struct load_error extends core_error.error:',
                        '    acquisition_failed', '    invalid_resource', '    default_fallback',
                        '    operation_failed', '    operation: string', '    kind: load_error_kind'):
        at = source.find('\n' + declaration + '\n')
        if at < 0 or not source[:at].splitlines()[-1].lstrip().startswith('## '):
            raise ValueError('Missing source documentation for ' + declaration.strip())
    return len(companions)


def verify_numeric_names(source):
    """Canonical numeric-unit names are the only supported public spellings."""
    functions = {f['name']: f for f in public_functions(source)}
    retired = [name for name in functions if canonical_name(name) != name]
    if retired:
        raise ValueError('Retired public numeric-unit names: ' + ', '.join(retired))
    for name in NUMERIC_PUBLIC_NAMES.values():
        if name not in functions:
            raise ValueError(name + ': missing canonical public function')
    return dict(NUMERIC_PUBLIC_NAMES)
