#!/usr/bin/env python3
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests' / 'SourceAvailabilityPublishNativeBoundaryFallbacks'


def check(condition, detail='check failed'):
    if not condition:
        raise AssertionError(detail)
    print('True')


def invoke(*args, cwd=ROOT, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd,
                          capture_output=True, text=True, encoding='utf-8', env=env)


def project(root: Path, name: str, source: str, unsafe=False, compiler_extra=None) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    compiler = {'unsafe': unsafe}
    if compiler_extra:
        compiler.update(compiler_extra)
    (root / f'{name}.voidproj').write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.348',
        'compiler': compiler}), encoding='utf-8')
    path = root / 'Program.void'
    path.write_text(source, encoding='utf-8')
    return path


def exe(root: Path, name: str, publish=False) -> Path:
    folder = 'publish' if publish else 'bin'
    return root / folder / (name + ('.exe' if os.name == 'nt' else ''))


def run_binary(path: Path, cwd: Path):
    return subprocess.run([str(path)], cwd=cwd, capture_output=True,
                          text=True, encoding='utf-8')


def stack_lines(stderr: str):
    marker = 'Stack trace:\n'
    if marker not in stderr:
        return []
    return [line for line in stderr.split(marker, 1)[1].splitlines()
            if line.startswith('   at ')]


def line_number(source: str, token: str):
    return source[:source.index(token)].count('\n') + 1


def main():
    # Development run keeps the locked #347 rich source-first presentation.
    result = invoke('run', FIXTURE)
    source_path = FIXTURE / 'Program.void'
    source = source_path.read_text(encoding='utf-8')
    crash_line = line_number(source, 'monster.Health')
    check(result.returncode == 1, result)
    check(result.stderr.startswith('VOID runtime error: object reference is null\n'), result.stderr)
    check('V.GameLoop.Crash()' in result.stderr, result.stderr)
    check(f'{crash_line:6d} | ' in result.stderr and 'monster.Health' in result.stderr, result.stderr)
    check('^~~~~~~' in result.stderr, result.stderr)
    check(result.stderr.index(f'{crash_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)
    frames = stack_lines(result.stderr)
    check(len(frames) == 3, frames)
    check('V.GameLoop.Crash()' in frames[0] and 'V.GameLoop.Run()' in frames[1] and 'V.Program.Main()' in frames[2], frames)

    with tempfile.TemporaryDirectory(prefix='void-source-fallback-348-', dir=ROOT) as temp_name:
        temp = Path(temp_name)
        base_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }\n    public static void Main() { Crash(); }\n}\n'''
        app = temp / 'App'
        app_source = project(app, 'App', base_source)
        built = invoke('build', '.', cwd=app)
        check(built.returncode == 0, built.stderr)
        debug_exe = exe(app, 'App')
        check(debug_exe.is_file(), debug_exe)
        generated = app / '.void' / 'App.c'
        generated_text = generated.read_text(encoding='utf-8')
        check('vc_runtime_source_path_is_absolute' in generated_text, 'absolute source helper missing')
        check(str(app).replace('\\', '/') in generated_text.replace('\\', '/'), 'debug source root missing')

        # Changed cwd must never select a same-named decoy source from runtime cwd.
        decoy = temp / 'Elsewhere'
        decoy.mkdir()
        (decoy / 'Program.void').write_text('DECOY-SOURCE-348-MUST-NEVER-APPEAR\n', encoding='utf-8')
        result = run_binary(debug_exe, decoy)
        check(result.returncode == 1, result)
        check('item.Value' in result.stderr, result.stderr)
        check('DECOY-SOURCE-348' not in result.stderr, result.stderr)
        check('Stack trace:' in result.stderr and len(stack_lines(result.stderr)) == 2, result.stderr)
        check('Program.void:' in result.stderr.replace('\\', '/'), result.stderr)

        # A moved debug executable still uses the authoritative compilation root.
        moved = temp / 'Moved'
        moved.mkdir()
        moved_exe = moved / debug_exe.name
        shutil.copy2(debug_exe, moved_exe)
        result = run_binary(moved_exe, moved)
        check(result.returncode == 1, result)
        check('item.Value' in result.stderr and 'Program.Crash()' in result.stderr, result.stderr)
        check('DECOY-SOURCE-348' not in result.stderr, result.stderr)

        # Deleted source degrades to location + stack; runtime cwd decoys remain ignored.
        saved = app_source.with_suffix('.void.saved')
        app_source.rename(saved)
        result = run_binary(moved_exe, decoy)
        check(result.returncode == 1, result)
        check('VOID runtime error: object reference is null' in result.stderr, result.stderr)
        check('Program.Crash() in ./Program.void:' in result.stderr.replace('\\', '/'), result.stderr)
        check('Stack trace:' in result.stderr and len(stack_lines(result.stderr)) == 2, result.stderr)
        check('item.Value' not in result.stderr, result.stderr)
        check('DECOY-SOURCE-348' not in result.stderr, result.stderr)
        check('| ' not in result.stderr, result.stderr)
        saved.rename(app_source)

        # Publish uses existing build-mode policy: no developer source root is embedded.
        published = invoke('publish', '.', cwd=app)
        check(published.returncode == 0, published.stderr)
        publish_exe = exe(app, 'App', publish=True)
        check(publish_exe.is_file(), publish_exe)
        publish_generated = generated.read_text(encoding='utf-8')
        check('static VC_MAYBE_UNUSED const char *vc_runtime_source_root = "";' in publish_generated and
              'static VC_MAYBE_UNUSED const bool vc_runtime_source_text_enabled = false;' in publish_generated,
              'publish source lookup should be disabled')
        check(str(app).encode('utf-8') not in publish_exe.read_bytes(), 'publish binary leaked compilation root')
        result = run_binary(publish_exe, app)
        check(result.returncode == 1, result)
        check('VOID runtime error: object reference is null' in result.stderr, result.stderr)
        check('Program.Crash() in ./Program.void:' in result.stderr.replace('\\', '/'), result.stderr)
        check('Stack trace:' in result.stderr and len(stack_lines(result.stderr)) == 2, result.stderr)
        check('item.Value' not in result.stderr and '| ' not in result.stderr, result.stderr)

        publish_moved = temp / 'PublishMoved'
        publish_moved.mkdir()
        copied_publish = publish_moved / publish_exe.name
        shutil.copy2(publish_exe, copied_publish)
        result = run_binary(copied_publish, publish_moved)
        check(result.returncode == 1, result)
        check('Program.Crash() in ./Program.void:' in result.stderr.replace('\\', '/'), result.stderr)
        check('Stack trace:' in result.stderr and 'item.Value' not in result.stderr, result.stderr)

        # Unhandled user exceptions use the same availability fallback without changing semantics.
        throw_source = '''public static class Program\n{\n    public static void Crash() { throw new Exception("missing-source-348"); }\n    public static void Main() { Crash(); }\n}\n'''
        throwing = temp / 'Throwing'
        throw_path = project(throwing, 'Throwing', throw_source)
        check(invoke('build', '.', cwd=throwing).returncode == 0)
        throw_exe = exe(throwing, 'Throwing')
        throw_saved = throw_path.with_suffix('.void.saved')
        throw_path.rename(throw_saved)
        result = run_binary(throw_exe, decoy)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: missing-source-348\n'), result.stderr)
        check('Program.Crash() in ./Program.void:' in result.stderr.replace('\\', '/'), result.stderr)
        check('Stack trace:' in result.stderr and len(stack_lines(result.stderr)) == 2, result.stderr)
        check('throw new Exception' not in result.stderr and '| ' not in result.stderr, result.stderr)
        throw_saved.rename(throw_path)

        # Runtime.Fail remains a structured fatal VOID path, not a native crash.
        fatal_source = '''public static class Program\n{\n    public static void Fatal() { Runtime.Fail("fatal-348"); }\n    public static void Main() { Fatal(); }\n}\n'''
        fatal = temp / 'Fatal'
        project(fatal, 'Fatal', fatal_source)
        result = invoke('run', '.', cwd=fatal)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: fatal-348\n'), result.stderr)
        check('Program.Fatal()' in result.stderr and 'Stack trace:' in result.stderr, result.stderr)
        check('Native process crash:' not in result.stderr and 'VOID4000' not in result.stderr, result.stderr)

        # VOID -> native -> VOID callback keeps only real VOID re-entry/outer frames.
        if os.name != 'nt':
            callback = temp / 'Callback'
            native_dir = callback / 'native'
            native_dir.mkdir(parents=True)
            (native_dir / 'fixture.c').write_text(
                'void void348_invoke(void (*callback)(void)) { callback(); }\n', encoding='utf-8')
            cc = os.environ.get('CC') or 'cc'
            obj = native_dir / 'fixture.o'
            lib = native_dir / 'libvoid348fixture.a'
            c_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                                       '-c', str(native_dir / 'fixture.c'), '-o', str(obj)],
                                      capture_output=True, text=True, encoding='utf-8')
            check(c_result.returncode == 0, c_result.stderr)
            ar_result = subprocess.run([os.environ.get('AR') or 'ar', 'rcs', str(lib), str(obj)],
                                       capture_output=True, text=True, encoding='utf-8')
            check(ar_result.returncode == 0, ar_result.stderr)
            callback_source = '''public delegate void Callback();\npublic class Item { public int Value; }\npublic static class NativeFixture { [Native("void348_invoke")] public static unsafe extern void Invoke(Callback callback); }\npublic static class Program\n{\n    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }\n    public static unsafe void Outer() { NativeFixture.Invoke(Crash); }\n    public static unsafe void Main() { Outer(); }\n}\n'''
            project(callback, 'Callback', callback_source, unsafe=True,
                    compiler_extra={'libraryPaths': ['native'], 'libraries': ['void348fixture']})
            result = invoke('run', '.', cwd=callback)
            check(result.returncode == 1, result)
            frames = stack_lines(result.stderr)
            check(len(frames) == 3, frames)
            check('Program.Crash()' in frames[0] and 'Program.Outer()' in frames[1] and 'Program.Main()' in frames[2], frames)
            check('void348_invoke' not in result.stderr and 'native' not in '\n'.join(frames).lower(), frames)

            # Controlled POSIX native crash is classified natively with no stale VOID fault site.
            native_source = '''public static class Native { [Native("abort")] public static unsafe extern void Abort(); }\npublic static class Program { public static unsafe void Main() { Native.Abort(); } }\n'''
            crashing = temp / 'NativeCrash'
            project(crashing, 'NativeCrash', native_source, unsafe=True)
            result = invoke('run', '.', cwd=crashing)
            check(result.returncode == 1, result)
            check(result.stderr.startswith('Native process crash: process terminated by signal '), result.stderr)
            check('SIGABRT' in result.stderr, result.stderr)
            check('VOID runtime error:' not in result.stderr and 'Unhandled' not in result.stderr, result.stderr)
            check('Stack trace:' not in result.stderr and '| ' not in result.stderr and 'VOID4000' not in result.stderr, result.stderr)

        # Ordinary nonzero exit remains silent and preserves its code.
        exit_source = '''public static class Native { [Native("exit")] public static unsafe extern void Exit(int code); }\npublic static class Program { public static unsafe void Main() { Native.Exit(7); } }\n'''
        exiting = temp / 'NativeExit'
        project(exiting, 'NativeExit', exit_source, unsafe=True)
        result = invoke('run', '.', cwd=exiting)
        check(result.returncode == 7, result)
        check(result.stderr == '', result.stderr)

        # A successful compiler stub that emits no executable exercises launch failure distinctly.
        stub_source = temp / 'tool.c'
        stub_source.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        stub = temp / ('tool.exe' if os.name == 'nt' else 'tool')
        cc = os.environ.get('CC') or 'cc'
        stub_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                                      str(stub_source), '-o', str(stub)], capture_output=True, text=True, encoding='utf-8')
        check(stub_result.returncode == 0, stub_result.stderr)
        launch = temp / 'Launch'
        project(launch, 'Launch', 'public static class Program { public static void Main() {} }\n')
        env = dict(os.environ, CC=str(stub))
        result = invoke('run', '.', cwd=launch, env=env)
        check(result.returncode == 1, result)
        check('could not execute' in result.stderr and 'VOID4000' in result.stderr, result.stderr)
        check('Native process crash:' not in result.stderr and 'VOID runtime error:' not in result.stderr, result.stderr)

        # Unicode/path-rich development source rendering still works when source is available.
        unicode_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Main()\n    {\n        Console.WriteLine("雪🙂"); Item item = null; Console.WriteLine(item.Value);\n    }\n}\n'''
        unicode_root = temp / 'nested path 雪' / 'Unicode'
        project(unicode_root, 'Unicode', unicode_source)
        result = invoke('run', '.', cwd=unicode_root)
        check(result.returncode == 1, result)
        check('雪🙂' in result.stderr and 'item.Value' in result.stderr, result.stderr)
        check('Stack trace:' in result.stderr and 'Program.Main()' in result.stderr, result.stderr)

        # Generated C remains strict; debug and publish metadata policies are explicit.
        strict = temp / 'Strict'
        project(strict, 'Strict', base_source)
        check(invoke('build', '.', cwd=strict).returncode == 0)
        strict_c = strict / '.void' / 'Strict.c'
        strict_text = strict_c.read_text(encoding='utf-8')
        check('vc_runtime_source_path_is_absolute' in strict_text, 'source identity resolver missing')
        strict_obj = temp / 'strict.o'
        compile_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                                         '-IRuntime/include', '-c', str(strict_c), '-o', str(strict_obj)],
                                        cwd=ROOT, capture_output=True, text=True, encoding='utf-8')
        check(compile_result.returncode == 0, compile_result.stderr)
        check(invoke('publish', '.', cwd=strict).returncode == 0)
        publish_text = strict_c.read_text(encoding='utf-8')
        check('static VC_MAYBE_UNUSED const char *vc_runtime_source_root = "";' in publish_text and
              'static VC_MAYBE_UNUSED const bool vc_runtime_source_text_enabled = false;' in publish_text,
              'publish source-text policy missing')

    version = invoke('version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381', version.stdout)
    compiler = (ROOT / 'Compiler' / 'src' / 'compiler.c').read_text(encoding='utf-8')
    host_header = (ROOT / 'Compiler' / 'include' / 'host_process.h').read_text(encoding='utf-8')
    host_source = (ROOT / 'Compiler' / 'src' / 'host_process.c').read_text(encoding='utf-8')
    check('vc_runtime_source_path_is_absolute' in compiler and 'mode != VC_BUILD_PUBLISH' in compiler,
          'source availability architecture missing')
    check('vc_host_process_run_status' in host_header and 'VC_HOST_PROCESS_SIGNALED' in host_header,
          'detailed host process status missing')
    check('VC_HOST_PROCESS_SIGNALED' in host_source and 'WIFSIGNALED' in host_source,
          'POSIX signal classification missing')
    check('0xc0000005ul' in compiler and 'Windows native exception status' in compiler,
          'Windows native status path missing')
    check('last_source' not in compiler.lower() and 'find any' not in compiler.lower(),
          'forbidden source guessing state introduced')


if __name__ == '__main__':
    main()
