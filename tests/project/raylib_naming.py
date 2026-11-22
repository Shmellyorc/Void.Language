"""Canonical numeric-unit names and partial values, using an explicit native catalog.

raylib_naming.py voidc catalog fresh-output [--graphics]
No downloads, native preparation, user installation or user-project edits.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = '''use pkg/raylib as rl
use os

var global_2d = rl.camera_2d(target = rl.vector2(3, 4))
var global_3d = rl.camera_3d(fovy = 60)
var partial_global = rl.camera_2d(zoom = 3)
var global_vector = rl.vector3(y = 2)
var global_transform = rl.transform()

func vector_identity(value: rl.vector2) -> rl.vector2:
    var local: rl.vector2 = value
    return local
func rectangle_identity(value: rl.rectangle) -> rl.rectangle:
    var local: rl.rectangle = value
    return local
func color_identity(value: rl.color) -> rl.color:
    var local: rl.color = value
    return local

func headless() -> i32:
    var flat = rl.camera_2d()
    var spatial = rl.camera_3d()
    if flat.zoom != 1 or flat.rotation != 0 or flat.offset.x != 0 or flat.target.y != 0:
        return 1
    if spatial.position.z != 1 or spatial.target.z != 0 or spatial.up.y != 1 or spatial.fovy != 45 or spatial.projection != rl.camera_projection.perspective.value:
        return 2
    if global_2d.target.x != 3 or global_2d.target.y != 4 or global_2d.zoom != 1 or global_3d.fovy != 60 or partial_global.zoom != 3:
        return 3
    var selected = rl.camera_3d(projection = rl.camera_projection.orthographic, fovy = 10)
    if selected.fovy != 10 or selected.projection != rl.camera_projection.orthographic.value or selected.up.y != 1:
        return 4
    var local = rl.camera_2d(zoom = 2)
    var point: rl.vector2 = vector_identity(rl.vector2(10, 20))
    var screen = rl.get_world_to_screen_2d(point, local)
    var world = rl.get_screen_to_world_2d(screen, local)
    if screen.x != 20 or screen.y != 40:
        return 5
    if world.x != point.x or world.y != point.y:
        return 6
    var matrix = rl.get_camera_matrix_2d(local)
    if matrix.m0 != 2 or matrix.m15 != 1:
        return 7
    var equivalent = rl.camera_3d(rl.vector3(4, 5, 6), rl.vector3(1, 2, 3), rl.vector3(0, 1, 0), 70, rl.camera_projection.perspective)
    if equivalent.position.x != 4 or equivalent.target.y != 2 or equivalent.fovy != 70:
        return 8
    var identity = rl.matrix()
    var opaque: rl.color = color_identity(rl.color(r = 255))
    var empty: rl.rectangle = rectangle_identity(rl.rectangle(x = 2))
    if rl.color_to_int(opaque) != -16776961 or not rl.check_collision_point_rec(point, rl.rectangle(width = 40, height = 40)):
        return 15
    var forward = rl.ray()
    var miss = rl.ray_collision()
    var bounds = rl.bounding_box()
    var patch = rl.n_patch_info(source = rl.rectangle(width = 10, height = 10))
    if global_vector.x != 0 or global_vector.y != 2 or global_vector.z != 0:
        return 9
    if global_transform.rotation.w != 1 or global_transform.scale.x != 1 or global_transform.translation.x != 0:
        return 10
    if identity.m0 != 1 or identity.m5 != 1 or identity.m10 != 1 or identity.m15 != 1 or identity.m1 != 0:
        return 11
    if opaque.r != 255 as u8 or opaque.a != 255 as u8 or empty.width != 0 or forward.direction.z != -1:
        return 12
    if miss.hit or bounds.min.x != 0 or bounds.max.x != 0 or patch.layout != rl.n_patch_layout.nine_patch.value:
        return 13
    print("raylib numeric names, ABI values, local/global partial values and canonical spellings passed")
    return 0

func graphics() -> i32:
    rl.set_trace_log_level(rl.trace_log_level.warning)
    rl.set_config_flags(rl.config_flags.window_hidden)
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
    rl.begin_mode_3d(rl.camera_3d())
    rl.draw_line_3d(first, second, color_value)
    rl.draw_point_3d(first, color_value)
    rl.draw_circle_3d(first, 1, third, 0, color_value)
    rl.draw_triangle_3d(first, second, third, color_value)
    rl.draw_triangle_strip_3d(points, 3, color_value)
    rl.end_mode_3d()
    rl.end_drawing()
    print("raylib canonical graphics spellings passed")
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
        constructors = re.findall(r'^func ([a-z0-9_]+)\([^\n]*\) -> native\.\1:',
            (ROOT / 'verified-packages/raylib/src/raylib.void').read_text(), re.MULTILINE)
        assert {'vector2', 'rectangle', 'color'} <= set(constructors)
        results['qualified_constructor_types'] = constructors
        source = SOURCE + ''.join(f'\nfunc qualified_{name}(value: rl.{name}) -> rl.{name}:\n'
            f'    var local: rl.{name} = value\n    return local\n' for name in constructors)
        entry.write_text(source)
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
        entry.write_text(source.replace('camera_2d(zoom = 2)', 'camera_2d(unknown_zoom = 2)'))
        rejected = run(compiler, 'check', '--project', project, expected=1)
        assert 'unknown_zoom' in rejected.stderr and 'VOID2004' in rejected.stderr, rejected.stderr
        entry.write_text(source)
        results['status'] = 'PASS'
    except BaseException as error:
        results.update(status='FAIL', error=repr(error))
        raise
    finally:
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('Raylib numeric names:', len(results['commands']), 'commands PASS; JIT/AOT O0/O2:', ', '.join(modes))


if __name__ == '__main__':
    main()
