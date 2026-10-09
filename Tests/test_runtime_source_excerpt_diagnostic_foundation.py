#!/usr/bin/env python3
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests' / 'RuntimeSourceExcerptDiagnosticFoundation'


def check(condition, detail='check failed'):
    if not condition:
        raise AssertionError(detail)
    print('True')


def invoke(*args, cwd=ROOT, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd,
                          capture_output=True, text=True, encoding='utf-8', env=env)


def project(root: Path, name: str, source: str, unsafe=False, relative='Program.void') -> Path:
    root.mkdir(parents=True, exist_ok=True)
    (root / f'{name}.voidproj').write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.344',
        'compiler': {'unsafe': unsafe}}), encoding='utf-8')
    source_path = root / relative
    source_path.parent.mkdir(parents=True, exist_ok=True)
    source_path.write_text(source, encoding='utf-8')
    return source_path


def excerpt(stderr: str, line: int):
    lines = stderr.splitlines()
    label = f'{line:6d} | '
    for i, value in enumerate(lines):
        if value.startswith(label):
            check(i + 1 < len(lines), stderr)
            check(lines[i + 1].startswith('       | '), stderr)
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


def main():
    # Permanent application-shaped null receiver fixture.
    result = invoke('run', FIXTURE)
    source_path = FIXTURE / 'Program.void'
    source = source_path.read_text(encoding='utf-8')
    fail_line = source[:source.index('_monster.Health')].count('\n') + 1
    check(result.returncode == 1, result)
    check(result.stderr.startswith('VOID runtime error: object reference is null\n'), result.stderr)
    normalized = result.stderr.replace('\\', '/')
    check(f'at V.GameLoop.Loop() in {source_path.as_posix()}:{fail_line}:' in normalized, result.stderr)
    line_text, marker = excerpt(result.stderr, fail_line)
    check(line_text == source.splitlines()[fail_line - 1], line_text)
    prefix = line_text[:line_text.index('_monster')]
    check(marker == ' ' * display_columns(prefix) + '^' + '~' * (len('_monster') - 1), marker)
    check(result.stderr.index('VOID runtime error:') < result.stderr.index('   at ') < result.stderr.index(f'{fail_line:6d} | '), result.stderr)
    check(result.stderr.index(f'{fail_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)
    check('.c:' not in result.stderr and '__g' not in result.stderr, result.stderr)

    with tempfile.TemporaryDirectory(prefix='void-runtime-source-344-', dir=ROOT) as temp:
        temp = Path(temp)

        # Bounds failure gains the same source-focused block.
        bounds_source = '''public static class Program\n{\n    public static void Main()\n    {\n        int[] values = new int[2];\n        Console.WriteLine(values[4]);\n    }\n}\n'''
        bounds = temp / 'Bounds'
        bounds_path = project(bounds, 'Bounds', bounds_source)
        result = invoke('run', bounds)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: array index out of range\n'), result.stderr)
        line = bounds_source[:bounds_source.index('values[4]')].count('\n') + 1
        check(f'at Program.Main() in {bounds_path.as_posix()}:{line}:' in result.stderr.replace('\\', '/'), result.stderr)
        line_text, marker = excerpt(result.stderr, line)
        check(line_text == bounds_source.splitlines()[line - 1], line_text)
        prefix = line_text[:line_text.index('values[4]')]
        check(marker == ' ' * display_columns(prefix) + '^' + '~' * (len('values[4]') - 1), marker)

        # Unhandled user exception preserves the original throw site and message.
        throw_source = '''public static class Program\n{\n    public static void Crash()\n    {\n        throw new Exception("boom");\n    }\n    public static void Middle() { Crash(); }\n    public static void Main() { Middle(); }\n}\n'''
        throwing = temp / 'Throwing'
        throw_path = project(throwing, 'Throwing', throw_source)
        result = invoke('run', throwing)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: boom\n'), result.stderr)
        line = throw_source[:throw_source.index('throw new')].count('\n') + 1
        check(f'at Program.Crash() in {throw_path.as_posix()}:{line}:' in result.stderr.replace('\\', '/'), result.stderr)
        line_text, marker = excerpt(result.stderr, line)
        check(line_text == throw_source.splitlines()[line - 1], line_text)
        prefix = line_text[:line_text.index('throw new')]
        check(marker == ' ' * display_columns(prefix) + '^' + '~' * (len('throw new Exception("boom");') - 1), marker)
        check('Program.Middle()' in result.stderr and result.stderr.index('throw new Exception') < result.stderr.index('Stack trace:'), result.stderr)

        # Unicode and tabs before the fault use display columns, not UTF-8 byte counts.
        unicode_source = '''public class Item { public int Value; }\npublic static class Program\n{\n    public static void Main()\n    {\n\tConsole.WriteLine("雪🙂"); Item item = null; Console.WriteLine(item.Value);\n    }\n}\n'''
        unicode = temp / 'Unicode'
        unicode_path = project(unicode, 'Unicode', unicode_source)
        result = invoke('run', unicode)
        check(result.returncode == 1, result)
        line = unicode_source[:unicode_source.index('item.Value')].count('\n') + 1
        line_text, marker = excerpt(result.stderr, line)
        check('雪🙂' in line_text and 'item.Value' in line_text, line_text)
        prefix = line_text[:line_text.index('item.Value')]
        check(len(marker) - len(marker.lstrip(' ')) == display_columns(prefix), marker)
        check(marker.lstrip(' ') == '^~~~', marker)
        check(f':{line}:' in result.stderr and unicode_path.as_posix() in result.stderr.replace('\\', '/'), result.stderr)

        # Nested source directories retain their source-facing path.
        nested_source = '''public class Item { public int Value; }\npublic static class Program { public static void Main() { Item value = null; Console.WriteLine(value.Value); } }\n'''
        nested = temp / 'NestedPath'
        nested_path = project(nested, 'NestedPath', nested_source, relative='Source/Game/Program.void')
        result = invoke('run', nested)
        check(result.returncode == 1, result)
        check(nested_path.as_posix() in result.stderr.replace('\\', '/'), result.stderr)
        check('value.Value' in result.stderr and '^~~~' in result.stderr, result.stderr)

        # Build with an absolute source identity, then run from another cwd.
        cwd_source = '''public class Item { public int Value; }\npublic static class Program { public static void Main() { Item value = null; Console.WriteLine(value.Value); } }\n'''
        cwd_project = temp / 'ChangedCwd'
        cwd_path = project(cwd_project, 'ChangedCwd', cwd_source)
        built = invoke('build', '.', cwd=cwd_project)
        check(built.returncode == 0, built.stderr)
        executable = cwd_project / 'bin' / ('ChangedCwd.exe' if os.name == 'nt' else 'ChangedCwd')
        other_cwd = temp / 'Elsewhere'
        other_cwd.mkdir()
        result = subprocess.run([str(executable)], cwd=other_cwd, capture_output=True,
                                text=True, encoding='utf-8')
        check(result.returncode == 1, result)
        normalized = result.stderr.replace('\\', '/')
        check('Program.void:' in normalized and 'value.Value' in result.stderr, result.stderr)
        check('^~~~' in result.stderr, result.stderr)

        # Source unavailable: keep truthful origin and omit fabricated source text.
        missing = cwd_path.with_suffix('.void.missing')
        cwd_path.rename(missing)
        result = subprocess.run([str(executable)], cwd=other_cwd, capture_output=True,
                                text=True, encoding='utf-8')
        check(result.returncode == 1, result)
        check('Program.void:' in result.stderr.replace('\\', '/'), result.stderr)
        check('value.Value' not in result.stderr and '| ' not in result.stderr, result.stderr)
        missing.rename(cwd_path)

        # Runtime.Fail remains fatal, while #347 supplies its truthful call-site source context.
        fatal_source = 'public static class Program { public static void Main() { Runtime.Fail("invariant probe"); } }\n'
        fatal = temp / 'Fatal'
        project(fatal, 'Fatal', fatal_source)
        result = invoke('run', fatal)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: invariant probe\n') and 'Runtime.Fail("invariant probe")' in result.stderr and 'Stack trace:' in result.stderr, result.stderr)

        # Ordinary native nonzero exit is still only a process exit.
        native_source = '''public static class Native { [Native("exit")] public static unsafe extern void Exit(int code); }\npublic static class Program { public static unsafe void Main() { Native.Exit(7); } }\n'''
        native = temp / 'NativeExit'
        project(native, 'NativeExit', native_source, unsafe=True)
        result = invoke('run', native)
        check(result.returncode == 7, result)
        check(result.stderr == '', result.stderr)

        # Repeated execution is deterministic for the same source and site.
        deterministic = temp / 'Deterministic'
        project(deterministic, 'Deterministic', cwd_source)
        first = invoke('run', deterministic)
        second = invoke('run', deterministic)
        check(first.returncode == second.returncode == 1, (first, second))
        check(first.stderr == second.stderr, (first.stderr, second.stderr))

        # Compile-time diagnostics keep their established compiler renderer.
        bad_source = 'public static class Program { public static void Main() { int value = ; } }\n'
        bad = temp / 'CompilerDiagnostic'
        bad_path = project(bad, 'CompilerDiagnostic', bad_source)
        result = invoke('check', bad)
        check(result.returncode == 1, result)
        check('error:' in result.stderr and 'code: VOID2001' in result.stderr, result.stderr)
        check(bad_path.as_posix() in result.stderr.replace('\\', '/'), result.stderr)
        check('int value = ;' in result.stderr and '^' in result.stderr, result.stderr)
        check('VOID runtime error:' not in result.stderr, result.stderr)

        # Strict generated C for the permanent fixture remains warning-clean.
        result = invoke('build', FIXTURE)
        generated = FIXTURE / '.void' / 'RuntimeSourceExcerptDiagnosticFoundation.c'
        check(result.returncode == 0 and generated.is_file(), result.stderr)
        cc = os.environ.get('CC') or 'cc'
        native_obj = temp / 'strict.o'
        compile_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                                         '-Werror', '-O2', '-IRuntime/include', '-c', str(generated),
                                         '-o', str(native_obj)], cwd=ROOT, capture_output=True,
                                        text=True, encoding='utf-8')
        check(compile_result.returncode == 0, compile_result.stderr)

    # Version and architecture guardrails.
    version = invoke('version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380', version.stdout)
    compiler = (ROOT / 'Compiler' / 'src' / 'compiler.c').read_text(encoding='utf-8')
    check('size_t end_line; size_t end_column;' in compiler, 'fault site span missing')
    check('vc_fault_site_source_excerpt' in compiler and 'vc_fault_site_read_line' in compiler, 'source renderer missing')
    check('vc_array_at_at' in compiler and 'vc_runtime_fail_at' in compiler, 'site-aware bounds path missing')
    check('vc_call_stack_report' in compiler and 'Stack trace:' in compiler, 'synchronous stack foundation missing')
    check('last_source' not in compiler.lower(), 'mutable last-source mechanism introduced')


if __name__ == '__main__':
    main()
