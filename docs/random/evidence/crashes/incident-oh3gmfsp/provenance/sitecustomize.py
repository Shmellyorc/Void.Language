"""Opt-in crash preservation for existing Python regression harnesses."""
import os

if os.environ.get('VOID_CRASH_EVIDENCE'):
    from crash import install
    install()
