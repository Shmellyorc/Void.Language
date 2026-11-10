"""Visible Linux raylib checks using the S7 X11, pixel and input controls."""

import ctypes as c
import fcntl
import json
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import time

from PIL import Image


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
    assert display, 'A real X11 display is required'
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
    # S7 uses compositor input because this XWayland host discards synthetic keys.
    def __enter__(self):
        self.fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
        try:
            fcntl.ioctl(self.fd, 0x40045564, 1)
            for key in range(1, 128):
                fcntl.ioctl(self.fd, 0x40045565, key)
            setup = struct.pack('HHHH80sI', 3, 0x1209, 1, 1, b'VOID raylib qualification', 0)
            fcntl.ioctl(self.fd, 0x405c5503, setup)
            fcntl.ioctl(self.fd, 0x5501)
        except BaseException:
            os.close(self.fd)
            raise
        time.sleep(1)
        return self

    def right(self, down):
        os.write(self.fd, struct.pack('llHHi', 0, 0, 1, 106, down) +
                 struct.pack('llHHi', 0, 0, 0, 0, 0))

    def __exit__(self, *exception):
        try:
            self.right(0)
        finally:
            try:
                fcntl.ioctl(self.fd, 0x5502)
            finally:
                os.close(self.fd)


def center(path, color):
    with Image.open(path) as image:
        rgb = image.convert('RGB')
    assert rgb.size == (640, 360), rgb.size
    points = [(i % rgb.width, i // rgb.width) for i, pixel in enumerate(rgb.get_flattened_data())
              if pixel == color]
    assert len(points) > 100, (color, len(points))
    return [round(sum(p[axis] for p in points) / len(points), 2) for axis in (0, 1)]


def qualify(command, native, cwd, environment, output):
    output.mkdir()
    log = output / 'process.txt'
    window = None
    with Keyboard() as keyboard, log.open('wb') as stream:
        process = subprocess.Popen(list(map(str, command)), cwd=cwd, env=environment,
                                   stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            for _ in range(300):
                candidates = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name',
                                             '^VOID Raylib Gallery$'], capture_output=True, text=True, timeout=5)
                for candidate in candidates.stdout.splitlines():
                    pid = subprocess.run(['xdotool', 'getwindowpid', candidate],
                                         capture_output=True, text=True, timeout=5)
                    if pid.returncode == 0 and os.getpgid(int(pid.stdout)) == process.pid:
                        window = candidate
                        native_pid = int(pid.stdout)
                        break
                if window:
                    break
                assert process.poll() is None, log.read_text(errors='replace')
                time.sleep(.1)
            assert window, log.read_text(errors='replace')
            mapping = Path(f'/proc/{native_pid}/maps').read_text()
            for name in ('libraylib.so.600', 'libvoid_raylib.so'):
                assert str(native / name) in mapping, (name, mapping)
            subprocess.run(['xdotool', 'windowactivate', '--sync', window], check=True, timeout=5)
            first, second = output / 'before.png', output / 'after.png'
            motion(window, 140, 125)
            time.sleep(.22)
            subprocess.run(['import', '-window', window, first], check=True, timeout=10)
            motion(window, 475, 235)
            assert subprocess.check_output(['xdotool', 'getactivewindow'], text=True).strip() == window
            keyboard.right(1)
            try:
                time.sleep(.34)
                subprocess.run(['import', '-window', window, second], check=True, timeout=10)
            finally:
                keyboard.right(0)
            subprocess.run(['wmctrl', '-ic', hex(int(window))], check=True, timeout=5)
            status = process.wait(timeout=30)
            text = log.read_text(errors='replace')
            assert status == 0, (status, text, [line for line in mapping.splitlines()
                                               if 'gallium' in line or 'dri.so' in line])
            assert 'AddressSanitizer' not in text and 'UndefinedBehaviorSanitizer' not in text, text
            assert text.count('RAYLIB trace invoked VOID') == 1, text
            assert text.count('RAYLIB owner released texture and window') == 1, text
            frames = re.search(r'RAYLIB frames (\d+) input frames (\d+)', text)
            assert frames and int(frames[1]) > 5 and int(frames[2]) > 0, text
            colors = dict(pink=(255, 109, 194), green=(0, 158, 47), orange=(255, 161, 0))
            positions = {name: [center(image, color) for image in (first, second)]
                         for name, color in colors.items()}
            for name, minimum in (('pink', 35), ('green', 4), ('orange', 4)):
                assert abs(positions[name][0][0] - positions[name][1][0]) > minimum, positions
            result = dict(exit=status, frames=int(frames[1]), input_frames=int(frames[2]), positions=positions)
            print(json.dumps(result, sort_keys=True), flush=True)
            return result
        finally:
            if process.poll() is None:
                try:
                    if window:
                        subprocess.run(['wmctrl', '-ic', hex(int(window))], timeout=5)
                finally:
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait(timeout=5)
