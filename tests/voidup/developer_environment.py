"""Preserve developer session/consent context without admitting tool overrides."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('compiler', type=Path)
p.add_argument('--source', type=Path, default=ROOT/'apps/voidup')
a = p.parse_args()
preserved = ['CI', 'XDG_DATA_HOME', 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME',
             'XDG_STATE_HOME', 'XDG_RUNTIME_DIR', 'DISPLAY', 'WAYLAND_DISPLAY',
             'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS']
with tempfile.TemporaryDirectory(prefix='voidup-environment-') as directory:
    work = Path(directory)
    for source in a.source.glob('*.void'):
        shutil.copyfile(source, work/source.name)
    (work/'probe.void').write_text('''use "./manager.void" as manager
use "./cli.void" as cli
use "./model.void" as model
use "./presentation.void" as presentation
use json
use io
func main() -> i32:
    if io.open_directory(PROBE_DIRECTORY) is Ok(var directory):
        with directory as held:
            var context = manager.up_context(presentation = presentation.up_presentation(), options = cli.up_options(root_path = "/fixture"), root_cap = held, versions_cap = held, metadata_cap = held, transactions_cap = held, control_state = model.up_control())
            if json.stringify(manager.up_environment(context, "release")) is Ok(var rendered_environment):
                print(rendered_environment)
                return 0
    return 1
'''.replace('PROBE_DIRECTORY', json.dumps(str(work))))
    environment = dict(os.environ, **{name: '/fixture/'+name for name in preserved},
                       VOID_STDLIB_ROOT=str(ROOT/'stdlib'), LD_PRELOAD='', VOID_NATIVE_CATALOG='/untrusted')
    def limits():
        import resource
        resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    result = subprocess.run([str(a.compiler.resolve()), 'run', '--no-cache', str(work/'probe.void')],
                            env=environment, capture_output=True, text=True, timeout=120,
                            preexec_fn=limits if os.name != 'nt' else None)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    actual = dict(item.split('=', 1) for item in json.loads(result.stdout))
    missing = [name for name in preserved if actual.get(name) != environment[name]]
    assert not missing, {'lost_session_or_consent_context': missing, 'actual': actual}
    assert not {'VOID_STDLIB_ROOT', 'VOID_NATIVE_CATALOG', 'LD_PRELOAD'} & actual.keys(), actual
    assert actual['PATH'].startswith('/fixture/toolchains/release/sdk/bin'), actual
print('PASS: CI, XDG and desktop session context retained; tool/loader overrides excluded')
