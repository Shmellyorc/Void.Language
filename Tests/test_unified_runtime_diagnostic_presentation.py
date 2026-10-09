#!/usr/bin/env python3
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests' / 'UnifiedRuntimeDiagnosticPresentation'


def check(condition, detail='check failed'):
    if not condition:
        raise AssertionError(detail)
    print('True')


def invoke(*args, cwd=ROOT, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd,
                          capture_output=True, text=True, encoding='utf-8', env=env)


def project(root: Path, name: str, source: str, unsafe=False) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    (root / f'{name}.voidproj').write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.347',
        'compiler': {'unsafe': unsafe}}), encoding='utf-8')
    path = root / 'Program.void'
    path.write_text(source, encoding='utf-8')
    return path


def stack_lines(stderr: str):
    marker = 'Stack trace:\n'
    if marker not in stderr:
        return []
    return [line for line in stderr.split(marker, 1)[1].splitlines()
            if line.startswith('   at ')]


def excerpt(stderr: str, line: int):
    label = f'{line:6d} | '
    lines = stderr.splitlines()
    for i, value in enumerate(lines):
        if value.startswith(label) and i + 1 < len(lines) and lines[i + 1].startswith('       | '):
            return value[len(label):], lines[i + 1][len('       | '):]
    raise AssertionError(stderr)


def scalar_width(value: str, column: int) -> int:
    scalar = ord(value)
    if value == '\t':
        return 4 - column % 4
    if scalar < 32 or scalar == 127:
        return 1
    if 0x300 <= scalar <= 0x36f or 0xfe00 <= scalar <= 0xfe0f:
        return 0
    if ((0x1100 <= scalar <= 0x115f) or (0x2e80 <= scalar <= 0xa4cf) or
        (0xac00 <= scalar <= 0xd7a3) or (0xf900 <= scalar <= 0xfaff) or
        (0xff01 <= scalar <= 0xff60) or (0x1f300 <= scalar <= 0x1faff) or
        (0x20000 <= scalar <= 0x3fffd)):
        return 2
    return 1


def display_columns(text: str) -> int:
    column = 0
    for value in text:
        column += scalar_width(value, column)
    return column


def no_internal_names(stderr: str):
    lowered = stderr.lower()
    check('vc_fn_' not in lowered and 'vc_m_' not in lowered, stderr)
    check('__g' not in stderr and '__lambda_' not in lowered and '__async_sm' not in lowered, stderr)


def main():
    # Application-shaped final layout: source-focused diagnostic always precedes full stack.
    result = invoke('run', FIXTURE)
    source_path = FIXTURE / 'Program.void'
    source = source_path.read_text(encoding='utf-8')
    fail_line = source[:source.index('monster.Name')].count('\n') + 1
    check(result.returncode == 1, result)
    check(result.stderr.startswith('VOID runtime error: object reference is null\n'), result.stderr)
    normalized = result.stderr.replace('\\', '/')
    check(f'at V.GameLoop.Attack() in {source_path.as_posix()}:{fail_line}:' in normalized, result.stderr)
    line_text, marker = excerpt(result.stderr, fail_line)
    check(line_text == source.splitlines()[fail_line - 1], line_text)
    check(marker.lstrip(' ') == '^~~~~~~', marker)
    reason = result.stderr.index('VOID runtime error:')
    origin = result.stderr.index('   at V.GameLoop.Attack()')
    source_pos = result.stderr.index(f'{fail_line:6d} | ')
    stack_pos = result.stderr.index('Stack trace:')
    check(reason < origin < source_pos < stack_pos, result.stderr)
    check(result.stderr.count('Stack trace:') == 1, result.stderr)
    frames = stack_lines(result.stderr)
    check(len(frames) == 5, frames)
    check(['V.GameLoop.Attack()', 'V.GameLoop.PlayerTurn()', 'V.GameLoop.Loop()', 'V.Game.Run()', 'Program.Main()'] ==
          [next(name for name in ['V.GameLoop.Attack()', 'V.GameLoop.PlayerTurn()', 'V.GameLoop.Loop()', 'V.Game.Run()', 'Program.Main()'] if name in frame) for frame in frames], frames)
    no_internal_names(result.stderr)

    with tempfile.TemporaryDirectory(prefix='void-runtime-presentation-347-', dir=ROOT) as temp_name:
        temp = Path(temp_name)

        # Bounds failures use the same layout and now surface exact structured values as truthful help.
        bounds_source = '''public static class Program\n{\n    public static void Main()\n    {\n        int[] values = new int[2];\n        Console.WriteLine(values[4]);\n    }\n}\n'''
        bounds = temp / 'Bounds'
        bounds_path = project(bounds, 'Bounds', bounds_source)
        result = invoke('run', bounds)
        bounds_line = bounds_source[:bounds_source.index('values[4]')].count('\n') + 1
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: array index out of range\n'), result.stderr)
        check(f'at Program.Main() in {bounds_path.as_posix()}:{bounds_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        line_text, marker = excerpt(result.stderr, bounds_line)
        check('values[4]' in line_text and marker.lstrip(' ') == '^~~~~~~~~', (line_text, marker))
        help_text = 'help: index 4 is outside the valid range for an array dimension of length 2.'
        check(help_text in result.stderr, result.stderr)
        check(result.stderr.index(f'{bounds_line:6d} | ') < result.stderr.index(help_text) < result.stderr.index('Stack trace:'), result.stderr)
        check(len(stack_lines(result.stderr)) == 1 and 'Program.Main()' in stack_lines(result.stderr)[0], result.stderr)
        check(result.stderr.count('VOID runtime error:') == 1 and result.stderr.count('Stack trace:') == 1, result.stderr)

        # Explicit unhandled exception keeps established exception vocabulary and unified tail.
        throw_source = '''public static class Program\n{\n    public static void Crash() { throw new Exception("boom-347"); }\n    public static void Middle() { Crash(); }\n    public static void Main() { Middle(); }\n}\n'''
        throwing = temp / 'Throwing'
        throw_path = project(throwing, 'Throwing', throw_source)
        result = invoke('run', throwing)
        throw_line = throw_source[:throw_source.index('throw new')].count('\n') + 1
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: boom-347\n'), result.stderr)
        check(f'Program.Crash() in {throw_path.as_posix()}:{throw_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        check('throw new Exception("boom-347")' in result.stderr, result.stderr)
        check(result.stderr.index(f'{throw_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 3 and 'Program.Crash()' in frames[0] and 'Program.Middle()' in frames[1] and 'Program.Main()' in frames[2], frames)
        check(result.stderr.count('Unhandled Exception:') == 1 and result.stderr.count('Stack trace:') == 1, result.stderr)

        # Bare rethrow presentation uses #346's original reason/origin/excerpt/captured stack unchanged.
        result = invoke('run', ROOT / 'Tests' / 'ExceptionRethrowRuntimeFailureStackPreservation')
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled AppFailure: original-346\n'), result.stderr)
        check('throw new AppFailure("original-346")' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 4, frames)
        check('Program.Crash()' in frames[0] and 'Program.Work()' in frames[1] and 'Program.Rethrow()' in frames[2] and 'Program.Main()' in frames[3], frames)
        check(result.stderr.index('throw new AppFailure') < result.stderr.index('Stack trace:'), result.stderr)

        # Replacement throw is rendered from the replacement origin, never the old exception.
        replace_source = '''public static class Program\n{\n    public static void Old() { throw new Exception("old-347"); }\n    public static void Replace()\n    {\n        try { Old(); }\n        catch (Exception error) { throw new Exception("replacement-347"); }\n    }\n    public static void Main() { Replace(); }\n}\n'''
        replace = temp / 'Replace'
        replace_path = project(replace, 'Replace', replace_source)
        result = invoke('run', replace)
        replace_line = replace_source[:replace_source.index('throw new Exception("replacement-347")')].count('\n') + 1
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: replacement-347\n'), result.stderr)
        check(f'Program.Replace() in {replace_path.as_posix()}:{replace_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        check('old-347' not in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Program.Replace()' in frames[0] and 'Program.Main()' in frames[1], frames)

        # Runtime.Fail stays fatal/non-catchable but now participates in source-first presentation.
        fatal_source = '''public static class Program\n{\n    public static void Fatal() { Runtime.Fail("fatal-347"); }\n    public static void Main() { Fatal(); }\n}\n'''
        fatal = temp / 'Fatal'
        fatal_path = project(fatal, 'Fatal', fatal_source)
        result = invoke('run', fatal)
        fatal_line = fatal_source[:fatal_source.index('Runtime.Fail')].count('\n') + 1
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: fatal-347\n'), result.stderr)
        check('Unhandled Exception:' not in result.stderr, result.stderr)
        check(f'Program.Fatal() in {fatal_path.as_posix()}:{fatal_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        check('Runtime.Fail("fatal-347")' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Program.Fatal()' in frames[0] and 'Program.Main()' in frames[1], frames)
        check(result.stderr.index(f'{fatal_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)

        # Deliberately long recursive stack remains complete; no collapsing/truncation for readability.
        deep_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Recurse(int depth)\n    {\n        if (depth == 0) { Item item = null; Console.WriteLine(item.Value); return; }\n        Recurse(depth - 1);\n    }\n    public static void Main() { Recurse(32); }\n}\n'''
        deep = temp / 'Deep'
        project(deep, 'Deep', deep_source)
        result = invoke('run', deep)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: object reference is null\n'), result.stderr)
        frames = stack_lines(result.stderr)
        recurse_frames = [frame for frame in frames if 'Program.Recurse()' in frame]
        check(len(recurse_frames) == 33, (len(recurse_frames), frames))
        check(len(frames) == 34 and 'Program.Main()' in frames[-1], frames[-3:])
        check('repeated' not in result.stderr.lower() and 'truncated' not in result.stderr.lower(), result.stderr)
        check(result.stderr.index('item.Value') < result.stderr.index('Stack trace:'), result.stderr)
        check(result.stderr.count('Stack trace:') == 1, result.stderr)

        # Constructor and accessor frames keep #345's source-facing identities.
        ctor_source = '''public class Item { public int Value; }\npublic class Thing { public Thing() { Item item = null; Console.WriteLine(item.Value); } }\npublic static class Program { public static void Main() { var thing = new Thing(); } }\n'''
        ctor = temp / 'Constructor'
        project(ctor, 'Constructor', ctor_source)
        result = invoke('run', ctor)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1 and 'Thing.Thing()' in frames[0], result.stderr)
        check('Program.Main()' in frames[-1] and 'vc_ctor' not in result.stderr, result.stderr)

        prop_source = '''public class Item { public int Value; }\npublic class Thing { public int Value { get { Item item = null; return item.Value; } } }\npublic static class Program { public static void Main() { var thing = new Thing(); Console.WriteLine(thing.Value); } }\n'''
        prop = temp / 'Property'
        project(prop, 'Property', prop_source)
        result = invoke('run', prop)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1 and 'Thing.get_Value()' in frames[0], result.stderr)
        check('vc_pg_' not in result.stderr and 'Program.Main()' in frames[-1], result.stderr)

        # Generic type/method names are source-facing rather than specialization names.
        generic_source = '''public class Item { public int Value; }\npublic class Box<T> { public void Crash() { Item item = null; Console.WriteLine(item.Value); } }\npublic static class Program\n{\n    public static void Generic<T>(T input) { Box<T> box = new Box<T>(); box.Crash(); }\n    public static void Main() { Generic<int>(1); }\n}\n'''
        generic = temp / 'Generic'
        project(generic, 'Generic', generic_source)
        result = invoke('run', generic)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1 and 'Box<int>.Crash()' in frames[0], result.stderr)
        check(any('Program.Generic()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)
        no_internal_names(result.stderr)

        # Delegate and lambda frames remain readable and helper-free.
        delegate_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }\n    public static void Main() { Action action = Crash; action(); }\n}\n'''
        delegate = temp / 'Delegate'
        project(delegate, 'Delegate', delegate_source)
        result = invoke('run', delegate)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1 and 'Program.Crash()' in frames[0] and 'Program.Main()' in frames[-1], result.stderr)
        check('vc_delegate' not in result.stderr.lower(), result.stderr)

        lambda_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }\n    public static void Main() { Action action = () => Crash(); action(); }\n}\n'''
        lambda_project = temp / 'Lambda'
        project(lambda_project, 'Lambda', lambda_source)
        result = invoke('run', lambda_project)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1 and 'Program.Crash()' in frames[0], result.stderr)
        check(any('Program.Main.<lambda>()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)
        no_internal_names(result.stderr)

        # Worker-thread stack remains independent from Main.
        thread_source = '''using Void.Threading;\npublic class Item { public int Value; }\npublic static class Program\n{\n    public static void WorkerCrash() { Item item = null; Console.WriteLine(item.Value); }\n    public static void Main()\n    {\n        Thread worker = new Thread(() => WorkerCrash());\n        worker.Start();\n        worker.Join();\n    }\n}\n'''
        threaded = temp / 'Threaded'
        project(threaded, 'Threaded', thread_source)
        result = invoke('run', threaded)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1 and 'Program.WorkerCrash()' in frames[0], result.stderr)
        check(any('Program.Main.<lambda>()' in frame for frame in frames), frames)
        check(not any('Program.Main()' in frame for frame in frames), frames)
        check('vc_managed_thread' not in result.stderr and 'vc_native_thread' not in result.stderr, result.stderr)

        # Missing source remains truthful: location + stack survive while excerpt/help are not fabricated.
        missing_source = '''public class Item { public int Value; }\npublic static class Program { public static void Crash() { Item item = null; Console.WriteLine(item.Value); } public static void Main() { Crash(); } }\n'''
        missing = temp / 'Missing'
        missing_path = project(missing, 'Missing', missing_source)
        built = invoke('build', '.', cwd=missing)
        check(built.returncode == 0, built.stderr)
        executable = missing / 'bin' / ('Missing.exe' if os.name == 'nt' else 'Missing')
        saved = missing_path.with_suffix('.void.saved')
        missing_path.rename(saved)
        elsewhere = temp / 'Elsewhere'
        elsewhere.mkdir()
        result = subprocess.run([str(executable)], cwd=elsewhere, capture_output=True, text=True, encoding='utf-8')
        check(result.returncode == 1, result)
        check('Program.void:' in result.stderr.replace('\\', '/'), result.stderr)
        check('Stack trace:' in result.stderr and len(stack_lines(result.stderr)) == 2, result.stderr)
        check('item.Value' not in result.stderr and '| ' not in result.stderr, result.stderr)
        saved.rename(missing_path)

        # UTF-8/tabs use the same display-column model as the locked source renderer.
        unicode_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Main()\n    {\n\tConsole.WriteLine("雪🙂"); Item item = null; Console.WriteLine(item.Value);\n    }\n}\n'''
        unicode = temp / 'Unicode'
        project(unicode, 'Unicode', unicode_source)
        result = invoke('run', unicode)
        unicode_line = unicode_source[:unicode_source.index('item.Value')].count('\n') + 1
        line_text, marker = excerpt(result.stderr, unicode_line)
        check(result.returncode == 1 and '雪🙂' in line_text, result.stderr)
        prefix = line_text[:line_text.index('item.Value')]
        check(len(marker) - len(marker.lstrip(' ')) == display_columns(prefix), marker)
        check(marker.lstrip(' ') == '^~~~', marker)
        check(result.stderr.index(f'{unicode_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)

        # Ordinary native nonzero exit remains separate; voidc run adds no fake VOID4000 runtime diagnostic.
        native_source = '''public static class Native { [Native("exit")] public static unsafe extern void Exit(int code); }\npublic static class Program { public static unsafe void Main() { Native.Exit(7); } }\n'''
        native = temp / 'NativeExit'
        project(native, 'NativeExit', native_source, unsafe=True)
        result = invoke('run', native)
        check(result.returncode == 7, result)
        check(result.stderr == '', result.stderr)
        check('VOID4000' not in result.stderr and 'VOID runtime error:' not in result.stderr, result.stderr)

        # Compiler diagnostics remain the locked compiler renderer, not the runtime renderer.
        bad_source = 'public static class Program { public static void Main() { int value = ; } }\n'
        bad = temp / 'CompilerDiagnostic'
        bad_path = project(bad, 'CompilerDiagnostic', bad_source)
        result = invoke('check', bad)
        check(result.returncode == 1, result)
        check('error:' in result.stderr and 'code: VOID2001' in result.stderr, result.stderr)
        check(bad_path.as_posix() in result.stderr.replace('\\', '/'), result.stderr)
        check('VOID runtime error:' not in result.stderr and 'Stack trace:' not in result.stderr, result.stderr)

        # Determinism for an identical runtime failure.
        first = invoke('run', FIXTURE)
        second = invoke('run', FIXTURE)
        check(first.returncode == second.returncode == 1, (first, second))
        check(first.stderr == second.stderr, (first.stderr, second.stderr))

        # Generated C remains strict under the established flags.
        built = invoke('build', FIXTURE)
        generated = FIXTURE / '.void' / 'UnifiedRuntimeDiagnosticPresentation.c'
        check(built.returncode == 0 and generated.is_file(), built.stderr)
        generated_text = generated.read_text(encoding='utf-8')
        check('vc_runtime_frame_report' in generated_text and 'vc_runtime_diagnostic_context_report' in generated_text, 'shared renderer missing')
        check('vc_runtime_fail_bounds_at' in generated_text and 'help: index %d' in generated_text, 'bounds help missing')
        check('vc_runtime_fail_string_at' in generated_text, 'Runtime.Fail source-aware renderer missing')
        check(generated_text.count('static VC_MAYBE_UNUSED void vc_runtime_frame_report') == 1, 'duplicate frame renderer')
        cc = os.environ.get('CC') or 'cc'
        strict_obj = temp / 'strict.o'
        compile_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                                         '-IRuntime/include', '-c', str(generated), '-o', str(strict_obj)],
                                        cwd=ROOT, capture_output=True, text=True, encoding='utf-8')
        check(compile_result.returncode == 0, compile_result.stderr)

    version = invoke('version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381', version.stdout)
    compiler = (ROOT / 'Compiler' / 'src' / 'compiler.c').read_text(encoding='utf-8')
    check('vc_runtime_frame_report' in compiler and 'vc_runtime_diagnostic_context_report' in compiler, 'unified renderer missing')
    check('vc_exception_trace_report' in compiler and 'vc_call_stack_report' in compiler, 'locked stack systems missing')
    check('vc_exception_trace_capture' in compiler and 'vc_rethrow_handler' in compiler, 'locked #346 preservation missing')
    check('last_source' not in compiler.lower(), 'mutable last-source state introduced')


if __name__ == '__main__':
    main()
