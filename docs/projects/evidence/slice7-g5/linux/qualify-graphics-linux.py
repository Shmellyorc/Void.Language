import atexit
import ctypes as c
import fcntl
import struct
from hashlib import sha256
from pathlib import Path
from PIL import Image
import json
import os
import re
import shutil
import subprocess
import sys
import time

root = Path.cwd()
top = root / 'build/slice7-raylib'
profile = sys.argv[1]
work = root / 'build/slice7-g5/linux-graphics' / (profile + '-' + sys.argv[2])
work.mkdir(parents=True)
cwd = work / 'independent cwd'
cwd.mkdir()
sdk = root / '.toolchains/llvm-23.1.4/bin/clang'
vendor = top / 'upstream/raylib-6.0_linux_amd64'
build = root / ('build/llvm23-' + profile)
cfg = {'release': 'Release', 'debug': 'Debug', 'strict': 'RelWithDebInfo'}[profile]
environment = {k: v for k, v in os.environ.items() if k != 'VOID_STDLIB_ROOT'}
if profile == 'strict':
    driver_leak_excluded = '--exclude-confirmed-mesa-lsan' in sys.argv[3:]
    leaks = 0 if driver_leak_excluded else 1
    environment.update(ASAN_OPTIONS=f'halt_on_error=1:abort_on_error=1:detect_leaks={leaks}',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')


def run(*args, env=None, status=0):
    argv = list(map(str, args))
    print(argv, flush=True)
    p = subprocess.run(argv, cwd=cwd, env=env or environment, capture_output=True, timeout=180)
    output = (p.stdout + p.stderr).decode(errors='replace')
    print(output, flush=True)
    assert p.returncode == status, (p.returncode, output)
    return output


def center(image, color):
    rgb = Image.open(image).convert('RGB')
    assert rgb.size == (640, 360), rgb.size
    points = [(i % rgb.width, i // rgb.width) for i, pixel in enumerate(rgb.get_flattened_data()) if pixel == color]
    assert len(points) > 100, (color, len(points))
    return [round(sum(p[axis] for p in points) / len(points), 2) for axis in (0, 1)]


def check_log(path, expected=0):
    output = path.read_text(errors='replace')
    assert 'AddressSanitizer' not in output and 'UndefinedBehaviorSanitizer' not in output, output
    if expected == 0:
        assert output.count('RAYLIB owner released texture and window') == 1, output
        match = re.search(r'RAYLIB frames (\d+) input frames (\d+)', output)
        assert 'RAYLIB trace invoked VOID' in output, output
        assert match and int(match[1]) > 5, output
        return [int(match[1]), int(match[2])]


class XMotion(c.Structure):
    _fields_ = [('type', c.c_int), ('serial', c.c_ulong), ('send_event', c.c_int),
                ('display', c.c_void_p), ('window', c.c_ulong), ('root', c.c_ulong),
                ('subwindow', c.c_ulong), ('time', c.c_ulong), ('x', c.c_int), ('y', c.c_int),
                ('x_root', c.c_int), ('y_root', c.c_int), ('state', c.c_uint),
                ('is_hint', c.c_char), ('same_screen', c.c_int)]


def motion(window, x, y):
    lib = c.CDLL('libX11.so.6')
    lib.XOpenDisplay.restype = c.c_void_p
    lib.XSendEvent.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_long, c.c_void_p]
    lib.XDefaultRootWindow.argtypes = [c.c_void_p]
    lib.XDefaultRootWindow.restype = c.c_ulong
    lib.XSync.argtypes = [c.c_void_p, c.c_int]
    lib.XCloseDisplay.argtypes = [c.c_void_p]
    display = lib.XOpenDisplay(None)
    assert display
    event = (c.c_long * 24)()
    msg = XMotion.from_buffer(event)
    msg.type, msg.display, msg.window = 6, display, int(window)
    msg.root, msg.x, msg.y, msg.same_screen = lib.XDefaultRootWindow(display), x, y, 1
    try:
        assert lib.XSendEvent(display, int(window), 0, 1 << 6, c.byref(event))
        lib.XSync(display, 0)
    finally:
        lib.XCloseDisplay(display)


class Keyboard:
    # XWayland's synthetic key events were discarded by the current desktop.
    # Exercise the actual compositor input path with an ephemeral test keyboard.
    def __init__(self):
        self.fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
        fcntl.ioctl(self.fd, 0x40045564, 1)  # UI_SET_EVBIT, EV_KEY
        for key in range(1, 128):
            fcntl.ioctl(self.fd, 0x40045565, key)  # UI_SET_KEYBIT
        setup = struct.pack('HHHH80sI', 3, 0x1209, 1, 1, b'VOID G5 keyboard test', 0)
        fcntl.ioctl(self.fd, 0x405c5503, setup)  # UI_DEV_SETUP
        fcntl.ioctl(self.fd, 0x5501)  # UI_DEV_CREATE
        atexit.register(self.close)
        time.sleep(1)

    def close(self):
        fcntl.ioctl(self.fd, 0x5502)  # UI_DEV_DESTROY
        os.close(self.fd)

    def right(self, down):
        os.write(self.fd, struct.pack('llHHi', 0, 0, 1, 106, down) + struct.pack('llHHi', 0, 0, 0, 0, 0))


keyboard = Keyboard()

def qualify(label, command, native, env):
    out = work / label
    out.mkdir()
    log = out / 'process.txt'
    print(label, list(map(str, command)), flush=True)
    with log.open('wb') as stream:
        process = subprocess.Popen(list(map(str, command)), cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
        window = None
        try:
            for _ in range(200):
                p = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid),'--name',
                                    '^VOID Raylib Gallery$'], capture_output=True, text=True)
                if p.stdout.strip():
                    window = p.stdout.splitlines()[0]
                    break
                if process.poll() is not None:
                    raise AssertionError(log.read_text(errors='replace'))
                time.sleep(.10)
            assert window, log.read_text(errors='replace')
            mapping = Path(f'/proc/{process.pid}/maps').read_text()
            (out / 'maps.txt').write_text(''.join(line+'\n' for line in mapping.splitlines()
                if any(name in line for name in ('raylib', 'libGL', 'libX11', 'dri.so'))))
            assert str(native / 'libraylib.so.600') in mapping and str(native / 'libvoid_raylib.so') in mapping
            subprocess.run(['xwininfo', '-id', window], stdout=(out / 'window.txt').open('wb'), check=True)
            subprocess.run(['xdotool', 'windowactivate', '--sync', window], check=True, timeout=5)
            first, second = out / 'frame-before.png', out / 'frame-after.png'
            motion(window, 140, 125)
            time.sleep(.22)
            subprocess.run(['import', '-window', window, first], check=True, timeout=10)
            motion(window, 475, 235)
            subprocess.run(['xdotool', 'windowactivate', '--sync', window], check=True, timeout=5)
            focus = subprocess.check_output(['xdotool', 'getactivewindow'], text=True).strip()
            assert focus == window, (focus, window)
            keyboard.right(1)
            try:
                time.sleep(.34)
                subprocess.run(['import', '-window', window, second], check=True, timeout=10)
            finally:
                keyboard.right(0)
                time.sleep(.15)
            if label == 'jit-normal':
                motion(window, round(center(second, (255, 161, 0))[0]), 174)
                time.sleep(.1)
                click_before, click_after = out/'click-before.png', out/'click-after.png'
                subprocess.run(['import', '-window', window, click_before], check=True, timeout=10)
                subprocess.run(['xdotool', 'click', '--window', window, '1'], check=True, timeout=5)
                time.sleep(.15)
                subprocess.run(['import', '-window', window, click_after], check=True, timeout=10)
                observed = [center(f, (255, 161, 0)) for f in (click_before, click_after)]
                assert abs(observed[0][0] - observed[1][0]) < 2, observed
                (out/'click.json').write_text(json.dumps({'orange_centroids': observed})+'\n')
            subprocess.run(['wmctrl', '-ic', hex(int(window))], check=True, timeout=5)
            status = process.wait(timeout=15)
            assert status == 0, (status, log.read_text(errors='replace'))
            frames, key_frames = check_log(log)
            pink = [center(f, (255, 109, 194)) for f in (first, second)]
            green = [center(f, (0, 158, 47)) for f in (first, second)]
            orange = [center(f, (255, 161, 0)) for f in (first, second)]
            assert abs(pink[0][0] - pink[1][0]) > 35, pink
            assert abs(green[0][0] - green[1][0]) > 4, green
            assert key_frames > 0 and abs(orange[0][0] - orange[1][0]) > 4, (key_frames, orange)
            receipt = dict(exit=status, frames=frames, input_frames=key_frames, pink=pink, green=green, orange=orange,
                           native={f.name: sha256(f.read_bytes()).hexdigest() for f in native.glob('*.so*')})
            (out / 'result.json').write_text(json.dumps(receipt, indent=2)+'\n')
            print(receipt, flush=True)
        finally:
            if process.poll() is None:
                if window:
                    subprocess.run(['wmctrl', '-ic', hex(int(window))], timeout=5)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


prefix = work / 'compiler install'
run('cmake','--install',build,'--prefix',prefix,'--config',cfg,'--component','compiler')
compiler = prefix / 'bin/voidc'
project = work / 'gallery project'
shutil.copytree(root / 'examples/raylib', project)
native = project / 'native'
run(sys.executable,project/'prepare.py','--voidc',compiler,'--clang',sdk,
    '--header',vendor/'include/raylib.h','--library',vendor/'lib/libraylib.so.6.0.0','--license',vendor/'LICENSE',
    *(['--sanitize'] if profile == 'strict' else []))
run(compiler, 'check', '--project', project)
run(compiler, 'run', '--project', project, status=2)
assert 'Raylib title rejected' in run(compiler, 'run', '--project', project, '--allow-native','--','--reject-title',status=2)
packages={}
for mode, flags in (('normal',['--no-opt']),('optimized',[])):
    headless_env = environment | ({'ASAN_OPTIONS':'halt_on_error=1:abort_on_error=1:detect_leaks=1'} if profile == 'strict' else {})
    assert 'RAYLIB trace invoked VOID' in run(compiler, 'run', '--project', project, '--allow-native',
        '--no-cache', *flags, '--', '--trace-only', env=headless_env)
    implicit = ['--implicit-close'] if mode == 'normal' else []
    qualify('jit-'+mode,[compiler,'run','--project',project,'--allow-native','--no-cache',*flags,'--',*implicit],native,environment)
    output=work/('build-'+mode)/'gallery'
    run(compiler,'build','--project',project,'--allow-native',*flags,'-o',output)
    assert not re.search(r'Shared library: \[(?:libvoid_raylib|libraylib)\.', run('readelf','-d',output))
    package=work/('relocated '+mode+' 雪')
    output.parent.rename(package)
    (package/'native').mkdir()
    for name in ('libvoid_raylib.so','libraylib.so.600','RAYLIB-LICENSE.txt'):
        shutil.copyfile(native/name,package/'native'/name)
    packages[mode]=package
project.rename(work/'archived project')
prefix.rename(work/'archived compiler')
app_env=environment | {'PATH':'/usr/bin:/bin','VOID_STDLIB_ROOT':str(work/'absent stdlib')}
for mode, package in packages.items():
    assert 'Raylib title rejected' in run(package/'gallery','--reject-title',env=app_env,status=2)
    assert 'RAYLIB trace invoked VOID' in run(package/'gallery', '--trace-only',
        env=app_env | ({'ASAN_OPTIONS':'halt_on_error=1:abort_on_error=1:detect_leaks=1'} if profile == 'strict' else {}))
    implicit = ['--implicit-close'] if mode == 'optimized' else []
    qualify('aot-'+mode,[package/'gallery',*implicit],package/'native',app_env)
print(profile,'visible raylib qualification passed',flush=True)
