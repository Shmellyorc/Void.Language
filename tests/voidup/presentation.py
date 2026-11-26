"""Credential-free VOIDUP status policy; installation qualification is separate."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

application = str(Path(sys.argv[1]).resolve())
checks = 0

with tempfile.TemporaryDirectory(prefix='VOIDUP presentation 雪 ') as temporary:
    root = Path(temporary) / 'private'

    def run(args, status=0):
        global checks
        result = subprocess.run([application, '--root', str(root), *args], capture_output=True, timeout=60)
        checks += 1
        assert result.returncode == status, (args, result)
        return result

    default = run(['doctor'])
    assert not default.stderr
    for mode in ('auto', 'off'):
        current = run(['--status', mode, 'doctor'])
        assert current.stdout == default.stdout and not current.stderr
    plain = run(['--status', 'plain', 'doctor'])
    assert plain.stdout == default.stdout and b'Finished' in plain.stderr and b'VOIDUP in ' in plain.stderr
    assert b'\x1b' not in plain.stderr
    assert not run(['--status', 'plain', 'list']).stderr
    missing = run(['--status', 'plain', 'which', 'voidc'], 1)
    assert json.loads(missing.stderr)['operation'] == 'which'
    for args in (['--status'], ['--status', 'bad', 'doctor'],
                 ['--status', 'plain', '--status', 'off', 'doctor']):
        assert json.loads(run(args, 2).stderr)['operation'] == 'arguments'
    assert b'--status' in run(['--help']).stdout
    if os.name != 'nt':
        import pty
        import select
        import termios
        for extra, expected in (({}, True), ({'CI': '1'}, False), ({'TERM': 'dumb'}, False),
                                ({'TERM': 'unknown'}, False), ({'TERM': None}, False),
                                ({'TERMINFO': '/void-g75-missing-terminfo', 'TERMINFO_DIRS': '/void-g75-missing-terminfo'}, True)):
            master, slave = pty.openpty()
            environment = os.environ | {'TERM': 'xterm'}
            environment.pop('CI', None)
            for key, value in extra.items():
                if value is None:
                    environment.pop(key, None)
                else:
                    environment[key] = value
            data = b''
            try:
                termios.tcsetwinsize(slave, (24, 80))
                assert os.isatty(slave) and termios.tcgetwinsize(slave) == (24, 80)
                result = subprocess.run([application, '--root', str(root), 'doctor'], stdout=subprocess.PIPE,
                                        stderr=slave, env=environment, timeout=60)
                while select.select([master], [], [], 0)[0] and (chunk := os.read(master, 4096)):
                    data += chunk
            finally:
                os.close(slave)
                os.close(master)
            assert result.returncode == 0 and (b'Finished' in data) == expected, (extra, result, data)

print(f'VOIDUP presentation passed ({checks} captured processes plus native PTY controls)')
