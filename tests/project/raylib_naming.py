"""Numeric-unit names and partial cameras, using an explicit native catalog.

raylib_naming.py voidc catalog fresh-output [--graphics]
No downloads, native preparation, user installation or user-project edits.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = '''use pkg/raylib as rl
use os

var global_2d = rl.camera_2d(target = rl.vector2(3, 4))
var global_3d = rl.camera_3d(fovy = 60)
var legacy_global = rl.camera2_d(zoom = 3)

func headless() -> i32:
    var flat = rl.camera_2d()
    var spatial = rl.camera_3d()
    if flat.zoom != 1 or flat.rotation != 0 or flat.offset.x != 0 or flat.target.y != 0:
        return 1
    if spatial.position.z != 1 or spatial.target.z != 0 or spatial.up.y != 1 or spatial.fovy != 45 or spatial.projection != rl.camera_perspective:
        return 2
    if global_2d.target.x != 3 or global_2d.target.y != 4 or global_2d.zoom != 1 or global_3d.fovy != 60 or legacy_global.zoom != 3:
        return 3
    var selected = rl.camera_3d(projection = rl.camera_orthographic, fovy = 10)
    if selected.fovy != 10 or selected.projection != rl.camera_orthographic or selected.up.y != 1:
        return 4
    var local = rl.camera_2d(zoom = 2)
    var point = rl.vector2(10, 20)
    var screen = rl.get_world_to_screen_2d(point, local)
    var old_screen = rl.get_world_to_screen2_d(point, local)
    var world = rl.get_screen_to_world_2d(screen, local)
    var old_world = rl.get_screen_to_world2_d(screen, local)
    if screen.x != 20 or screen.y != 40 or old_screen.x != screen.x or old_screen.y != screen.y:
        return 5
    if world.x != point.x or world.y != point.y or old_world.x != world.x or old_world.y != world.y:
        return 6
    var matrix = rl.get_camera_matrix_2d(local)
    var old_matrix = rl.get_camera_matrix2_d(local)
    if matrix.m0 != 2 or matrix.m15 != 1 or matrix.m0 != old_matrix.m0 or matrix.m12 != old_matrix.m12:
        return 7
    var original = rl.camera3_d(rl.vector3(4, 5, 6), rl.vector3(1, 2, 3), rl.vector3(0, 1, 0), 70, rl.camera_perspective)
    var equivalent = rl.camera_3d(rl.vector3(4, 5, 6), rl.vector3(1, 2, 3), rl.vector3(0, 1, 0), 70, rl.camera_perspective)
    if original.position.x != equivalent.position.x or original.target.y != equivalent.target.y or original.fovy != equivalent.fovy:
        return 8
    print("raylib numeric names, ABI values, local/global partial cameras and legacy spellings passed")
    return 0

func graphics() -> i32:
    rl.set_trace_log_level(rl.log_warning)
    rl.set_config_flags(rl.flag_window_hidden as u32)
    var owner = rl.init_window(64, 64, "raylib numeric names") as rl.window
    defer rl.close_window(owner)
    var color_value = rl.white()
    var first = rl.vector3(0, 0, 0)
    var second = rl.vector3(1, 0, 0)
    var third = rl.vector3(0, 1, 0)
    var points = rl.vector3_array_from([first, second, third])
    defer rl.vector3_array_close(points)
    rl.begin_drawing()
    rl.begin_mode_2d(rl.camera_2d())
    rl.end_mode_2d()
    rl.begin_mode2_d(rl.camera2_d())
    rl.end_mode2_d()
    rl.begin_mode_3d(rl.camera_3d())
    rl.draw_line_3d(first, second, color_value)
    rl.draw_point_3d(first, color_value)
    rl.draw_circle_3d(first, 1, third, 0, color_value)
    rl.draw_triangle_3d(first, second, third, color_value)
    rl.draw_triangle_strip_3d(points, 3, color_value)
    rl.end_mode_3d()
    rl.begin_mode3_d(rl.camera3_d())
    rl.draw_line3_d(first, second, color_value)
    rl.draw_point3_d(first, color_value)
    rl.draw_circle3_d(first, 1, third, 0, color_value)
    rl.draw_triangle3_d(first, second, third, color_value)
    rl.draw_triangle_strip3_d(points, 3, color_value)
    rl.end_mode3_d()
    rl.end_drawing()
    print("raylib preferred and compatibility graphics spellings passed")
    return 0

func main() -> i32:
    if os.args() is Ok(var arguments):
        for argument in arguments:
            if argument == "graphics":
                return graphics()
    return headless()
'''


def main():
    compiler, catalog, output = (Path(a).resolve() for a in sys.argv[1:4])
    output.mkdir()
    env = dict(os.environ, VOID_NATIVE_CATALOG=str(catalog))
    results = {'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
               'commands': [], 'status': 'RUNNING'}

    def run(*args, expected=0):
        result = subprocess.run(list(map(str, args)), cwd=output, env=env,
                                capture_output=True, text=True, timeout=240)
        results['commands'].append({'argv': list(map(str, args)), 'expected': expected,
                                    'actual': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert result.returncode == expected, results['commands'][-1]
        assert not any(s in result.stderr for s in ('ERROR: AddressSanitizer', 'ERROR: LeakSanitizer', 'runtime error:')), result.stderr
        return result

    try:
        project = output / 'project'
        run(compiler, 'new', project)
        run(compiler, 'add', 'raylib', '--dir', ROOT / 'verified-packages/raylib', '--project', project)
        manifest = project / 'void.project.toml'
        manifest.write_text(manifest.read_text() + '\n[native-requirements.raylib]\nid="org.voidlang.raylib"\nversion="=6.0.0"\nabi=3\n')
        entry = project / 'src/app.void'
        entry.write_text(SOURCE)
        run(compiler, 'tidy', '--project', project)
        run(compiler, 'check', '--project', project)
        modes = ['headless', 'graphics'] if '--graphics' in sys.argv else ['headless']
        for profile in ['debug', 'release']:
            for mode in modes:
                result = run(compiler, 'run', '--no-cache', '--project', project, '--allow-native', '--profile', profile, '--', mode)
                assert 'spellings passed' in result.stdout, result.stdout
            binary = output / profile / ('app.exe' if os.name == 'nt' else 'app')
            run(compiler, 'build', '--project', project, '--allow-native', '--profile', profile, '-o', binary)
            for mode in modes:
                result = run(binary, mode)
                assert 'spellings passed' in result.stdout, result.stdout
        entry.write_text(SOURCE.replace('camera_2d(zoom = 2)', 'camera_2d(unknown_zoom = 2)'))
        rejected = run(compiler, 'check', '--project', project, expected=1)
        assert 'unknown_zoom' in rejected.stderr and 'VOID2004' in rejected.stderr, rejected.stderr
        entry.write_text(SOURCE)
        results['status'] = 'PASS'
    except BaseException as error:
        results.update(status='FAIL', error=repr(error))
        raise
    finally:
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('Raylib numeric names:', len(results['commands']), 'commands PASS; JIT/AOT O0/O2:', ', '.join(modes))


if __name__ == '__main__':
    main()
