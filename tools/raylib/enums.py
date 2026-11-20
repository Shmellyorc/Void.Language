"""Public enum spelling and parity checks against the pinned Clang inventory.

The native descriptors still use exact C primitive types. This table only names
21 source types; discriminants come from the provider, never a second value map.
"""
import re

# C typedef -> (public type, enumerator prefix, backing, flags)
ENUMS = {
    'ConfigFlags': ('config_flags', 'FLAG_', 'u32', True),
    'TraceLogLevel': ('trace_log_level', 'LOG_', 'i32', False),
    'KeyboardKey': ('keyboard_key', 'KEY_', 'i32', False),
    'MouseButton': ('mouse_button', 'MOUSE_BUTTON_', 'i32', False),
    'MouseCursor': ('mouse_cursor', 'MOUSE_CURSOR_', 'i32', False),
    'GamepadButton': ('gamepad_button', 'GAMEPAD_BUTTON_', 'i32', False),
    'GamepadAxis': ('gamepad_axis', 'GAMEPAD_AXIS_', 'i32', False),
    'MaterialMapIndex': ('material_map_index', 'MATERIAL_MAP_', 'i32', False),
    'ShaderLocationIndex': ('shader_location_index', 'SHADER_LOC_', 'i32', False),
    'ShaderUniformDataType': ('shader_uniform_type', 'SHADER_UNIFORM_', 'i32', False),
    'ShaderAttributeDataType': ('shader_attribute_type', 'SHADER_ATTRIB_', 'i32', False),
    'PixelFormat': ('pixel_format', 'PIXELFORMAT_', 'i32', False),
    'TextureFilter': ('texture_filter', 'TEXTURE_FILTER_', 'i32', False),
    'TextureWrap': ('texture_wrap', 'TEXTURE_WRAP_', 'i32', False),
    'CubemapLayout': ('cubemap_layout', 'CUBEMAP_LAYOUT_', 'i32', False),
    'FontType': ('font_type', 'FONT_', 'i32', False),
    'BlendMode': ('blend_mode', 'BLEND_', 'i32', False),
    'Gesture': ('gesture_flags', 'GESTURE_', 'u32', True),
    'CameraMode': ('camera_mode', 'CAMERA_', 'i32', False),
    'CameraProjection': ('camera_projection', 'CAMERA_', 'i32', False),
    'NPatchLayout': ('n_patch_layout', 'NPATCH_', 'i32', False),
}


def member_name(name, prefix):
    member = name.removeprefix(prefix).lower()
    return 'none' if member == 'null' else member


def verify_enums(source, enumerations):
    if {entry['name'] for entry in enumerations} != set(ENUMS):
        raise ValueError('Reaudit new or removed provider enums')
    for entry in enumerations:
        typ, prefix, backing, flags = ENUMS[entry['name']]
        pattern = rf'(?m)^{"flags " if flags else ""}enum {typ}\({backing}\):\n((?:    .*\n|\n)+)'
        found = re.search(pattern, source)
        if not found:
            raise ValueError('Missing typed public enum: ' + typ)
        actual = dict(re.findall(r'^    (\w+) = (-?\d+)$', found[1], re.M))
        expected = {member_name(value['name'], prefix): str(value['value']) for value in entry['values']}
        if actual != expected:
            raise ValueError('Provider discriminant mismatch: ' + typ)
        # Every member must expose the provider description in compiler docs.
        if len(re.findall(r'^    ## \S', found[1], re.M)) < len(expected):
            raise ValueError('Undocumented enum members: ' + typ)
        for value in entry['values']:
            if re.search(r'^const ' + value['name'].lower() + r'\b', source, re.M):
                raise ValueError('Retired parallel numeric alias: ' + value['name'])
