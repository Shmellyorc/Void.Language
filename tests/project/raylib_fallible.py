"""G77 real resource results and cleanup; uses an explicitly supplied native catalog.

raylib_fallible.py voidc catalog fresh-output [--graphics] [--audio]
No downloads, user installation or approval changes. Failures/fixtures are retained.
"""
import hashlib
import base64
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import wave
import zlib

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''use pkg/raylib as rl
use "./fallible.void" as demo
use os
use convert as conversion

func headless() -> i32:
    rl.set_trace_log_level(rl.log_warning)
    if rl.try_load_image("missing-image.png") is Err(var failed):
        if failed.operation != "load_image" or not (failed.kind is rl.load_error_kind.acquisition_failed):
            return 1
    else:
        return 2
    if rl.load_image("missing-image.png") != null:
        return 3
    if not (demo.transcode_image("missing-image.png", "unused.png") is Err(_)):
        return 4
    if not (demo.transcode_image("sample.png", "missing-directory/out.png") is Err(_)):
        return 5
    if not (demo.transcode_image("sample.png", "copy.png") is Ok(_)):
        return 6
    if not demo.describe_image("copy.png") or demo.describe_image("missing-image.png"):
        return 7
    for at in 0..32:
        var decoded = conversion.unwrap(rl.try_load_image("sample.png"))
        if decoded.width != 4 or not rl.is_image_valid(decoded):
            return 8
        var bytes = conversion.unwrap(rl.try_export_image_to_memory(decoded, ".png"))
        var reloaded = conversion.unwrap(rl.try_load_image_from_memory(".png", bytes, rl.buffer_bytes(bytes).count as i32))
        rl.unload_image(decoded)
        rl.buffer_close(bytes)
        if reloaded.height != 4:
            return 9
        rl.unload_image(reloaded)
    var raw = conversion.unwrap(rl.try_load_image_raw("sample.rgba", 4, 4, rl.pixelformat_uncompressed_r8g8b8a8, 0))
    rl.unload_image(raw)
    var animation = conversion.unwrap(rl.try_load_image_anim("sample.gif"))
    rl.unload_image(animation)
    var animation_bytes = conversion.unwrap(rl.try_load_file_data("sample.gif"))
    var memory_animation = conversion.unwrap(rl.try_load_image_anim_from_memory(".gif", animation_bytes, rl.buffer_bytes(animation_bytes).count as i32))
    rl.buffer_close(animation_bytes)
    rl.unload_image(memory_animation)
    var invalid = rl.buffer_from_bytes([1, 2, 3, 4])
    defer rl.buffer_close(invalid)
    if not (rl.try_load_image_from_memory(".unsupported", invalid, 4) is Err(_)):
        return 10
    if not (rl.try_load_wave_from_memory(".unsupported", invalid, 4) is Err(_)):
        return 11
    if not (rl.try_load_file_data("missing-file") is Err(_)) or not (rl.try_load_file_text("missing-file") is Err(_)):
        return 12
    if not (rl.try_save_file_text("missing-directory/out.txt", "data") is Err(_)):
        return 13
    var payload = rl.buffer_from_bytes("hello G77".to_utf8())
    defer rl.buffer_close(payload)
    conversion.unwrap(rl.try_save_file_data("data.bin", payload, 9))
    var file_data = conversion.unwrap(rl.try_load_file_data("data.bin"))
    defer rl.buffer_close(file_data)
    if rl.buffer_bytes(file_data).count != 9:
        return 14
    conversion.unwrap(rl.try_save_file_text("data.txt", "hello G77"))
    var file_text = conversion.unwrap(rl.try_load_file_text("data.txt"))
    defer rl.buffer_close(file_text)
    if rl.buffer_text(file_text) != "hello G77":
        return 15
    var compressed = conversion.unwrap(rl.try_compress_data(payload, 9))
    defer rl.buffer_close(compressed)
    var restored = conversion.unwrap(rl.try_decompress_data(compressed, rl.buffer_bytes(compressed).count as i32))
    defer rl.buffer_close(restored)
    if rl.buffer_text(restored) != "hello G77":
        return 16
    if not (rl.try_load_wave("missing.wav") is Err(_)):
        return 17
    var samples = conversion.unwrap(rl.try_load_wave("sample.wav"))
    defer rl.unload_wave(samples)
    if not rl.is_wave_valid(samples):
        return 18
    conversion.unwrap(rl.try_export_wave(samples, "copy.wav"))
    var encoded_wave = conversion.unwrap(rl.try_load_file_data("copy.wav"))
    var memory_wave = conversion.unwrap(rl.try_load_wave_from_memory(".wav", encoded_wave, rl.buffer_bytes(encoded_wave).count as i32))
    rl.buffer_close(encoded_wave)
    if not rl.is_wave_valid(memory_wave):
        return 19
    rl.unload_wave(memory_wave)
    print("G77 headless result, propagation, bytes, repeat and cleanup passed")
    return 0

func graphics() -> i32:
    rl.set_trace_log_level(rl.log_warning)
    rl.set_config_flags(rl.flag_window_hidden as u32)
    var window_owner = rl.init_window(64, 64, "VOID G77 resource loaders") as rl.window
    defer rl.close_window(window_owner)
    for at in 0..8:
        if not (rl.try_load_texture("missing.png") is Err(_)) or not (rl.try_load_model("missing.obj") is Err(_)):
            return 30
        if not (rl.try_load_font("missing-font.png") is Err(_)):
            return 31
        var codepoints = rl.load_codepoints("ABC") as rl.int_array
        if not (rl.try_load_font_ex("missing.ttf", 20, codepoints, 3) is Err(_)):
            return 35
        var invalid_font_bytes = rl.buffer_from_bytes([1, 2, 3, 4])
        var invalid_font = rl.try_load_font_from_memory(".unsupported", invalid_font_bytes, 4, 20, codepoints, 3)
        rl.buffer_close(invalid_font_bytes)
        if invalid_font is Err(var fallback_error):
            if not (fallback_error.kind is rl.load_error_kind.default_fallback):
                return 36
        else:
            return 37
        var loaded_font = conversion.unwrap(rl.try_load_font_ex("font.ttf", 20, codepoints, 3))
        rl.unload_font(loaded_font)
        var font_bytes = conversion.unwrap(rl.try_load_file_data("font.ttf"))
        var memory_font = conversion.unwrap(rl.try_load_font_from_memory(".ttf", font_bytes, rl.buffer_bytes(font_bytes).count as i32, 20, codepoints, 3))
        rl.buffer_close(font_bytes)
        rl.unload_codepoints(codepoints)
        rl.unload_font(memory_font)
        var regular_font = conversion.unwrap(rl.try_load_font("font.ttf"))
        rl.unload_font(regular_font)
        var picture = conversion.unwrap(rl.try_load_image("sample.png"))
        var gpu = conversion.unwrap(rl.try_load_texture_from_image(picture))
        var snapshot = conversion.unwrap(rl.try_load_image_from_texture(gpu))
        rl.unload_image(snapshot)
        rl.unload_texture(gpu)
        rl.unload_image(picture)
        var texture_file = conversion.unwrap(rl.try_load_texture("sample.png"))
        rl.unload_texture(texture_file)
        var cube_image = rl.gen_image_color(4, 24, rl.white()) as rl.image
        var cube = conversion.unwrap(rl.try_load_texture_cubemap(cube_image, rl.cubemap_layout_line_vertical))
        rl.unload_texture(cube)
        rl.unload_image(cube_image)
        var screen = conversion.unwrap(rl.try_load_image_from_screen())
        rl.unload_image(screen)
        var target = conversion.unwrap(rl.try_load_render_texture(16, 16))
        if not rl.is_render_texture_valid(target):
            return 32
        rl.unload_render_texture(target)
        var intentional_default = conversion.unwrap(rl.try_load_shader_from_memory(null, null))
        rl.unload_shader(intentional_default)
        if not (rl.try_load_shader(null, rl.c_string(value = "missing.frag")) is Err(_)):
            return 33
        if not (rl.try_load_shader_from_memory(null, rl.c_string(value = "invalid shader source")) is Err(_)):
            return 34
        var custom = conversion.unwrap(rl.try_load_shader(null, rl.c_string(value = "sample.frag")))
        rl.unload_shader(custom)
        var fragment_bytes = conversion.unwrap(rl.try_load_file_text("sample.frag"))
        var memory_shader = conversion.unwrap(rl.try_load_shader_from_memory(null, rl.c_string(value = rl.buffer_text(fragment_bytes))))
        rl.buffer_close(fragment_bytes)
        rl.unload_shader(memory_shader)
        var loaded_model = conversion.unwrap(rl.try_load_model("sample.obj"))
        rl.unload_model(loaded_model)
    print("G77 graphics font/shader fallback, owner and repeat passed")
    return 0

func audio() -> i32:
    rl.set_trace_log_level(rl.log_warning)
    var device = rl.init_audio_device() as rl.audio_device
    defer rl.close_audio_device(device)
    for at in 0..8:
        if not (rl.try_load_sound("missing.wav") is Err(_)) or not (rl.try_load_music_stream("missing.wav") is Err(_)):
            return 40
        var decoded = conversion.unwrap(rl.try_load_wave("sample.wav"))
        var from_wave = conversion.unwrap(rl.try_load_sound_from_wave(decoded))
        var alias = conversion.unwrap(rl.try_load_sound_alias(from_wave))
        rl.unload_sound_alias(alias)
        rl.unload_sound(from_wave)
        rl.unload_wave(decoded)
        var file_sound = conversion.unwrap(rl.try_load_sound("sample.wav"))
        rl.unload_sound(file_sound)
        var file_music = conversion.unwrap(rl.try_load_music_stream("sample.wav"))
        rl.unload_music_stream(file_music)
        var bytes = conversion.unwrap(rl.try_load_file_data("sample.wav"))
        var memory_music = conversion.unwrap(rl.try_load_music_stream_from_memory(".wav", bytes, rl.buffer_bytes(bytes).count as i32))
        rl.buffer_close(bytes)
        if not rl.is_music_valid(memory_music):
            return 41
        rl.unload_music_stream(memory_music)
        var stream = conversion.unwrap(rl.try_load_audio_stream(22050, 16, 1))
        rl.unload_audio_stream(stream)
    print("G77 audio aliases, private music bytes and cleanup passed")
    return 0

func main() -> i32:
    var mode = "headless"
    if os.args() is Ok(var arguments):
        for argument in arguments:
            mode = argument
    if mode == "graphics":
        return graphics()
    if mode == "audio":
        return audio()
    if mode == "stale" or mode == "old-stale":
        var old = conversion.unwrap(rl.try_load_image("sample.png"))
        rl.unload_image(old)
        if mode == "old-stale":
            rl.export_image(old, "invalid.png")
        else:
            rl.try_export_image(old, "invalid.png")
        return 99
    if mode == "extent" or mode == "old-extent":
        var bytes = rl.buffer_from_bytes([1, 2, 3, 4])
        defer rl.buffer_close(bytes)
        if mode == "old-extent":
            rl.load_image_from_memory(".png", bytes, 5)
        else:
            rl.try_load_image_from_memory(".png", bytes, 5)
        return 99
    return headless()
'''


def main():
    compiler, catalog, output = map(lambda x: Path(x).resolve(), sys.argv[1:4])
    modes = ['headless'] + (['graphics'] if '--graphics' in sys.argv else []) + (['audio'] if '--audio' in sys.argv else [])
    output.mkdir()
    env = dict(os.environ, VOID_NATIVE_CATALOG=str(catalog))
    results = {'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
               'catalog': str(catalog), 'modes': modes, 'commands': [], 'status': 'RUNNING'}

    def save():
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

    def run(*args, expected=0):
        r = subprocess.run(list(map(str, args)), cwd=output, env=env, capture_output=True, text=True, timeout=240)
        results['commands'].append({'argv': list(map(str, args)), 'expected': expected, 'actual': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr})
        save()
        assert r.returncode == expected, results['commands'][-1]
        assert not any(x in r.stderr for x in ('ERROR: AddressSanitizer', 'ERROR: LeakSanitizer', 'runtime error: UndefinedBehaviorSanitizer')), r.stderr
        return r

    try:
        # Tiny real native inputs, isolated from personal data and package installs.
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0))
        png += chunk(b'IDAT', zlib.compress((b'\0' + bytes([10, 20, 30, 255]) * 4) * 4)) + chunk(b'IEND', b'')
        (output / 'sample.png').write_bytes(png)
        (output / 'sample.rgba').write_bytes(bytes([10, 20, 30, 255]) * 16)
        (output / 'sample.gif').write_bytes(base64.b64decode('R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7'))
        with wave.open(str(output / 'sample.wav'), 'wb') as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(22050); w.writeframes(b'\0\0' * 2205)
        (output / 'sample.obj').write_text('v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n')
        (output / 'sample.frag').write_text('#version 330\nout vec4 finalColor;\nvoid main() { finalColor=vec4(1.0); }\n')
        if 'graphics' in modes:
            font = Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
            assert font.is_file(), 'Supply a native host with the documented test font; no download is performed'
            shutil.copyfile(font, output / 'font.ttf')
        project = output / 'project'
        run(compiler, 'new', project)
        run(compiler, 'add', 'raylib', '--dir', ROOT / 'verified-packages/raylib', '--project', project)
        manifest = project / 'void.project.toml'
        manifest.write_text(manifest.read_text() + '\n[native-requirements.raylib]\nid="org.voidlang.raylib"\nversion="=6.0.0"\nabi=3\n')
        (project / 'src/app.void').write_text(SOURCE)
        shutil.copyfile(ROOT / 'examples/raylib-consumer/src/fallible.void', project / 'src/fallible.void')
        run(compiler, 'tidy', '--project', project)
        run(compiler, 'check', '--project', project)
        for profile in ['debug', 'release']:
            for mode in modes:
                run(compiler, 'run', '--no-cache', '--project', project, '--allow-native', '--profile', profile, '--', mode)
            failures = {}
            for mode in ['stale', 'extent', 'old-stale', 'old-extent']:
                r = run(compiler, 'run', '--no-cache', '--project', project, '--allow-native', '--profile', profile, '--', mode, expected=1)
                assert 'runtime error:' in r.stderr
                failures[mode] = r.stderr.split('runtime error:', 1)[1].splitlines()[0]
            assert failures['stale'] == failures['old-stale'] and failures['extent'] == failures['old-extent'], failures
            binary = output / profile / ('app.exe' if os.name == 'nt' else 'app')
            run(compiler, 'build', '--project', project, '--allow-native', '--profile', profile, '-o', binary)
            for mode in modes:
                run(binary, mode)
        results['status'] = 'PASS'
    except BaseException as error:
        results.update(status='FAIL', error=repr(error))
        raise
    finally:
        save()
    print('G77:', len(results['commands']), 'commands passed; JIT/AOT O0/O2:', ', '.join(modes))


if __name__ == '__main__':
    main()
