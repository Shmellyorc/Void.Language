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
