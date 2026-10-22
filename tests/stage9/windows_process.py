"""Explicit Windows application spelling; argv and physical files stay unchanged."""
import ntpath
import os


def windows_application_path(path):
    text = os.fspath(path).replace('/', '\\')
    if not ntpath.isabs(text) or not ntpath.splitdrive(text)[0]:
        raise ValueError('An absolute Windows application path is required')
    if text.startswith('\\\\?\\'):
        return text
    if text.startswith('\\\\'):
        return '\\\\?\\UNC\\' + text[2:]
    return '\\\\?\\' + text
