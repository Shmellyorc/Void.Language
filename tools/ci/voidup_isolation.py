"""Temporarily hide producer inputs during disposable hosted consumer checks."""
from contextlib import contextmanager


@contextmanager
def hidden_producer_paths(paths):
    moved = []
    try:
        for path in paths:
            held = path.with_name(path.name + '.voidup-held')
            if not path.exists() or held.exists():
                raise RuntimeError(f'Cannot isolate producer input: {path}')
            path.rename(held)
            moved.append((path, held))
        yield
    finally:
        for path, held in reversed(moved):
            held.rename(path)
