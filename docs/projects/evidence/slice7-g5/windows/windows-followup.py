from pathlib import Path
import json
import subprocess
import time

share = Path('/home/shmellyorc/VM/slice7-g5')
vm = 'Windows 11'

def ready(path, state):
    try:
        return json.loads(path.read_text()).get('status') == state
    except (OSError, ValueError):
        return False

def wait_for(predicate, timeout=2700):
    deadline = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() >= deadline:
            raise TimeoutError('Windows evidence did not arrive')
        time.sleep(5)

def launch(cmd):
    def keyboard(*args):
        subprocess.run(['VBoxManage', 'controlvm', vm, *args], check=True)
    keyboard('keyboardputscancode', 'e0', '5b', '13', '93', 'e0', 'db')
    time.sleep(.7)
    keyboard('keyboardputstring', 'cmd.exe /d /c "' + chr(92)*2 + chr(92).join(('vboxsvr','VM','slice7-g5',cmd)) + '"')
    keyboard('keyboardputscancode', '1c', '9c')
    print('Started', cmd, flush=True)

wait_for(lambda: ready(share/'compiler/overlay.json','restored'))
launch(r'compiler-final\build.cmd')
wait_for(lambda: ready(share/'compiler-final/overlay.json','restored'))
launch('graphics.cmd')
wait_for(lambda: (share/'graphics-Debug-qualified/aot-optimized/result.json').exists())
time.sleep(3)
launch('audit-graphics.cmd')
