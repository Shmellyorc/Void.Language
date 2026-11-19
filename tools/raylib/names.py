"""Public raylib spelling, separate from already-published native identities."""
import re


def abi_snake(name):
    """Keep the original descriptor/record/binding identifiers stable."""
    name = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', name)
    return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name).lower()


def canonical_name(name):
    """Keep numeric-letter units together: camera3_d -> camera_3d.

    A trailing number alone (vector3, sha256) is unchanged. This only repairs
    an existing word/digits/single-letter split, including inside longer names.
    """
    return re.sub(r'([a-z])(\d+)_([a-z])(?=_|$)', r'\1_\2\3', name)


def snake(name):
    return canonical_name(abi_snake(name))


# Retired public names are listed only to prevent their reintroduction.
NUMERIC_PUBLIC_NAMES = {
    'camera3_d': 'camera_3d',
    'camera2_d': 'camera_2d',
    'begin_mode2_d': 'begin_mode_2d',
    'end_mode2_d': 'end_mode_2d',
    'begin_mode3_d': 'begin_mode_3d',
    'end_mode3_d': 'end_mode_3d',
    'get_world_to_screen2_d': 'get_world_to_screen_2d',
    'get_screen_to_world2_d': 'get_screen_to_world_2d',
    'get_camera_matrix2_d': 'get_camera_matrix_2d',
    'draw_line3_d': 'draw_line_3d',
    'draw_point3_d': 'draw_point_3d',
    'draw_circle3_d': 'draw_circle_3d',
    'draw_triangle3_d': 'draw_triangle_3d',
    'draw_triangle_strip3_d': 'draw_triangle_strip_3d',
}
