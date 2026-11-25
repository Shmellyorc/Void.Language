"""Shared retention policy for local engineering harnesses."""
import os
from pathlib import Path
from tempfile import TemporaryDirectory

_scratch = []


def evidence_directory(investigation):
    root = Path(os.environ.get('VOID_EVIDENCE_ROOT',
        Path.home() / 'Documents/Void.Language-archives')).expanduser().resolve()
    temporary = Path(os.environ.get('TMPDIR', '/tmp')).resolve()
    if root == Path('/tmp') or root.is_relative_to('/tmp') or root == temporary or root.is_relative_to(temporary):
        raise ValueError('Retained engineering evidence must be outside temporary storage')
    checkout = Path(__file__).resolve().parents[1]
    if root == checkout or root.is_relative_to(checkout):
        raise ValueError("Engineering evidence must be outside the maintained checkout")
    relative = Path(investigation)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("Evidence namespace must be a relative path without parent traversal")
    target = root / relative
    target.mkdir(mode=0o700, parents=True, exist_ok=True)
    return target


def scratch_directory(prefix):
    """Own disposable work until process exit, including exception unwinding."""
    from test_output import output_directory
    directory = TemporaryDirectory(prefix=prefix, dir=output_directory())
    _scratch.append(directory)
    return Path(directory.name)


def documentation_evidence(area):
    """External home shared by legacy qualification writers and CI collectors."""
    return evidence_directory('qualification/' + area)
