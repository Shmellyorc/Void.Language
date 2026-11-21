"""Initialization failures of the existing managed-native process fixture.

No new packages, toolchains or retained test directories are created. This helper
is called by managed_native.py after its small native provider is already built.
"""
import errno
import os
from pathlib import Path
import resource
import shutil
import subprocess

from approval_console import interact


def verify(compiler, clang, project, cwd, environment):
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    env = dict(environment, XDG_DATA_HOME=str(project.parent / 'staging-user-data'))
    env.pop('CI', None)
    runs = project / '.void/run'
    library = project.parent / 'staging-fault.so'
    compile_result = subprocess.run([str(clang), '-shared', '-fPIC',
        str(Path(__file__).with_name('native_staging_fault.c')), '-ldl', '-o', str(library)],
        env=env, capture_output=True, timeout=40)
    assert compile_result.returncode == 0, compile_result.stderr
    interact(compiler, ['run', '.', '--', 'payload'], project, env, reply=b'approve',
             contains=b'VOID native application payload')
    assert not list(runs.iterdir())

    def invoke(mode=None, status=0):
        child_env = dict(env)
        # The fixture intentionally contains spaces. LD_PRELOAD splits paths on
        # whitespace, so inherit an open descriptor instead of copying the DSO
        # to a second scratch directory with an artificially simpler name.
        with library.open('rb') as preload:
            if mode:
                child_env.update(LD_PRELOAD=f'/proc/self/fd/{preload.fileno()}', VOID_STAGING_FAULT=mode,
                                 VOID_STAGING_TEST_RUN=str(runs))
            result = subprocess.run([str(compiler), 'run', '--project', str(project),
                '--reuse-native-approval', '--', 'payload'], cwd=cwd, env=child_env,
                pass_fds=(preload.fileno(),) if mode else (), capture_output=True, timeout=60)
        assert result.returncode == status, (mode, result.returncode, result.stdout, result.stderr)
        assert not any(word in result.stderr for word in
            (b'AddressSanitizer', b'LeakSanitizer', b'UndefinedBehaviorSanitizer', b'runtime error:')), result.stderr
        return result

    # This first case is the exact Golden Wrench regression: mkdir succeeds,
    # opening the leaf fails, and the constructor never receives a stage fd.
    for mode, error in [('open', errno.EMFILE), ('inspect', errno.EIO),
                        ('marker-open', errno.EACCES), ('marker-write', errno.EDQUOT),
                        ('payload-permission', errno.EACCES), ('payload-quota', errno.EDQUOT)]:
        result = invoke(mode, status=1)
        leftovers = [(p.name, sorted(x.name for x in p.iterdir())) for p in runs.iterdir()]
        assert not leftovers, (mode, 'initialization left an orphan', result.stderr, leftovers)
        assert os.strerror(error).encode() in result.stderr, (mode, result.stderr)
        invoke()
        assert not list(runs.iterdir()), mode

    if os.geteuid() != 0:
        runs.chmod(0)
        try:
            result = invoke(status=1)
        finally:
            runs.chmod(0o700)
        assert b'Permission denied' in result.stderr and b'run' in result.stderr, result.stderr
    runs.chmod(0o755)
    try:
        result = invoke(status=1)
    finally:
        runs.chmod(0o700)
    assert b'private and unaliased' in result.stderr and b'Permission denied' not in result.stderr
    for mode in ('short-marker', 'eintr-marker'):
        invoke(mode)
        assert not list(runs.iterdir()), mode

    # A replacement with a different inode never inherits our cleanup rights.
    result = invoke('replace-open', status=1)
    assert b'Too many open files' in result.stderr, result.stderr
    replaced = [p for p in runs.iterdir() if p.name.startswith('native-')]
    parked = runs / 'fault-created-parked'
    assert len(replaced) == 1 and (replaced[0] / 'KEEP').is_file() and parked.is_dir()
    invoke()
    assert (replaced[0] / 'KEEP').is_file() and parked.is_dir()
    # These are the test's simulated user entries, inside its disposable fixture.
    shutil.rmtree(replaced[0]); parked.rmdir()

    # A killed process cannot run constructor cleanup. Without a complete
    # ownership record a future compiler must preserve the ambiguous entry.
    invoke('kill-marker', status=-9)
    interrupted, = runs.iterdir()
    assert (interrupted / '.owner').read_bytes() == b'VOI'
    unmarked = runs / 'native-user-empty'; unmarked.mkdir(mode=0o700)
    invoke()
    assert (interrupted / '.owner').read_bytes() == b'VOI' and unmarked.is_dir()
    shutil.rmtree(interrupted); unmarked.rmdir()
    assert not list(runs.iterdir())
    print('Native staging initialization/diagnostic regressions passed; hard stack 1048576', flush=True)
