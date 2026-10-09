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
FIXTURE = ROOT / 'Tests' / 'RuntimeDiagnosticsCrossFeatureIntegration'


def check(condition, detail='check failed'):
    if not condition:
        raise AssertionError(detail)
    print('True')


def invoke(*args, cwd=ROOT, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd,
                          capture_output=True, text=True, encoding='utf-8', env=env)


def project(root: Path, name: str, source: str, *, unsafe=False, compiler_extra=None) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    compiler = {'unsafe': unsafe}
    if compiler_extra:
        compiler.update(compiler_extra)
    (root / f'{name}.voidproj').write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.349',
        'compiler': compiler}), encoding='utf-8')
    source_path = root / 'Program.void'
    source_path.write_text(source, encoding='utf-8')
    return source_path


def exe(root: Path, name: str) -> Path:
    return root / 'bin' / (name + ('.exe' if os.name == 'nt' else ''))


def run_binary(path: Path, cwd: Path):
    return subprocess.run([str(path)], cwd=cwd, capture_output=True,
                          text=True, encoding='utf-8')


def stack_lines(stderr: str):
    marker = 'Stack trace:\n'
    if marker not in stderr:
        return []
    return [line for line in stderr.split(marker, 1)[1].splitlines()
            if line.startswith('   at ')]


def assert_no_internal_names(text: str):
    for token in ('__gm', '__g1', '__lambda', '__sm', '__voidc$', 'vc_fn_'):
        check(token not in text, f'internal runtime name leaked: {token}\n{text}')


def build_native_callback(root: Path, symbol: str):
    native = root / 'native'
    native.mkdir(parents=True, exist_ok=True)
    source = native / 'fixture.c'
    source.write_text(f'void {symbol}(void (*callback)(void)) {{ callback(); }}\n', encoding='utf-8')
    cc = os.environ.get('CC') or 'cc'
    obj = native / 'fixture.o'
    lib = native / f'lib{symbol}.a'
    compile_result = subprocess.run([
        cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
        '-c', str(source), '-o', str(obj)
    ], capture_output=True, text=True, encoding='utf-8')
    check(compile_result.returncode == 0, compile_result.stderr)
    archive_result = subprocess.run([
        os.environ.get('AR') or 'ar', 'rcs', str(lib), str(obj)
    ], capture_output=True, text=True, encoding='utf-8')
    check(archive_result.returncode == 0, archive_result.stderr)
    return native


def main():
    # Application-shaped integration: interface + virtual property + constructor +
    # captured lambda + GC + generic method + closed generic type + array guard.
    result = invoke('run', FIXTURE)
    check(result.returncode == 1, result)
    check(result.stderr.startswith('VOID runtime error: array index out of range\n'), result.stderr)
    check('at Box<int>.Read()' in result.stderr, result.stderr)
    check('return _values[index];' in result.stderr, result.stderr)
    marker_lines = [line for line in result.stderr.splitlines() if '| ' in line and '^' in line]
    check(len(marker_lines) == 1, marker_lines)
    check(marker_lines[0].count('~') >= 8, marker_lines[0])
    check('help: index 7 is outside the valid range for an array dimension of length 2.' in result.stderr,
          result.stderr)
    check(result.stderr.index('return _values[index];') < result.stderr.index('Stack trace:'), result.stderr)
    frames = stack_lines(result.stderr)
    expected = ['Box<int>.Read()', 'GenericHelpers.Invoke()', 'Player.TakeTurn.<lambda>()',
                'Player.TakeTurn()', 'GameLoop.Update()', 'Game.Run()', 'Program.Main()']
    check(len(frames) == len(expected), frames)
    for index, name in enumerate(expected):
        check(name in frames[index], frames)
    check('ActorBase' not in '\n'.join(frames) and 'ITurn.' not in '\n'.join(frames), frames)
    assert_no_internal_names(result.stderr)

    with tempfile.TemporaryDirectory(prefix='void-runtime-diagnostics-integration-349-', dir=ROOT) as temp_name:
        temp = Path(temp_name)

        # Primary fault-site identity must match source-facing accessor identity.
        prop_source = '''public sealed class Item { public int Value; }\npublic sealed class Thing\n{\n    private Item _item;\n    public Thing() { _item = null; }\n    public int Value { get { return _item.Value; } }\n}\npublic static class Program { public static void Main() { Thing thing = new Thing(); Console.WriteLine(thing.Value); } }\n'''
        prop = temp / 'Property'
        project(prop, 'Property', prop_source)
        result = invoke('run', '.', cwd=prop)
        check(result.returncode == 1, result)
        check('at Thing.get_Value()' in result.stderr, result.stderr)
        check('return _item.Value;' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Thing.get_Value()' in frames[0] and 'Program.Main()' in frames[1], frames)

        # Constructor source identity must match the top stack frame.
        ctor_source = '''public sealed class Item { public int Value; }\npublic sealed class Thing\n{\n    public Thing() { Item item = null; Console.WriteLine(item.Value); }\n}\npublic static class Program { public static void Main() { Thing thing = new Thing(); } }\n'''
        ctor = temp / 'Constructor'
        project(ctor, 'Constructor', ctor_source)
        result = invoke('run', '.', cwd=ctor)
        check(result.returncode == 1, result)
        check('at Thing.Thing()' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Thing.Thing()' in frames[0] and 'Program.Main()' in frames[1], frames)

        # Static initialization is source-facing and leaves no generated helper name.
        static_source = '''public sealed class Item { public int Value; }\npublic static class Config\n{\n    private static Item Item = null;\n    public static int Value = Item.Value;\n}\npublic static class Program { public static void Main() { Console.WriteLine(Config.Value); } }\n'''
        static = temp / 'StaticInit'
        project(static, 'StaticInit', static_source)
        result = invoke('run', '.', cwd=static)
        check(result.returncode == 1, result)
        check('at Config.<static initializer>()' in result.stderr, result.stderr)
        check('Item.Value' in result.stderr, result.stderr)
        assert_no_internal_names(result.stderr)

        # Interface dispatch reports the implementation that actually executes.
        dispatch_source = '''public sealed class Item { public int Value; }\npublic interface IRunner { void Run(); }\npublic class Base { public virtual void Run() { } }\npublic sealed class Derived : Base, IRunner\n{\n    public override void Run() { Item item = null; Console.WriteLine(item.Value); }\n}\npublic static class Program\n{\n    public static void ViaInterface(IRunner runner) { runner.Run(); }\n    public static void Main() { Derived value = new Derived(); ViaInterface(value); }\n}\n'''
        dispatch = temp / 'Dispatch'
        project(dispatch, 'Dispatch', dispatch_source)
        result = invoke('run', '.', cwd=dispatch)
        frames = stack_lines(result.stderr)
        check(result.returncode == 1, result)
        check('at Derived.Run()' in result.stderr, result.stderr)
        check(len(frames) == 3 and 'Derived.Run()' in frames[0] and
              'Program.ViaInterface()' in frames[1] and 'Program.Main()' in frames[2], frames)
        check('IRunner.Run()' not in result.stderr, result.stderr)

        # Generic/delegate/lambda + GC + bare rethrow + finally preserves original failure.
        rethrow_source = '''using Void;\npublic sealed class Payload { public int Value; public Payload(int value) { Value = value; } }\npublic sealed class IntegrationFailure : Exception { public IntegrationFailure(string message) : base(message) { } }\npublic static class Cleanup { public static int Count; public static void Touch() { Count++; GC.Collect(); } }\npublic static class Pipeline\n{\n    public static void ThrowThrough<T>(T value)\n    {\n        Action callback = () => { GC.Collect(); throw new IntegrationFailure("cross-349"); };\n        callback();\n    }\n    public static void Rethrow<T>(T value)\n    {\n        try { ThrowThrough<T>(value); }\n        catch (IntegrationFailure error) { GC.Collect(); throw; }\n        finally { Cleanup.Touch(); }\n    }\n}\npublic static class Program { public static void Main() { Pipeline.Rethrow<Payload>(new Payload(7)); } }\n'''
        rethrow = temp / 'Rethrow'
        project(rethrow, 'Rethrow', rethrow_source)
        result = invoke('run', '.', cwd=rethrow)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled IntegrationFailure: cross-349\n'), result.stderr)
        check('throw new IntegrationFailure("cross-349")' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 4, frames)
        check('Pipeline.ThrowThrough.<lambda>()' in frames[0], frames)
        check('Pipeline.ThrowThrough()' in frames[1] and 'Pipeline.Rethrow()' in frames[2] and 'Program.Main()' in frames[3], frames)
        check('Cleanup.Touch()' not in result.stderr, result.stderr)
        assert_no_internal_names(result.stderr)

        # Replacement exception owns its new source and captured stack, not exception A.
        replace_source = '''public sealed class FirstFailure : Exception { public FirstFailure(string message) : base(message) {} }\npublic static class Program\n{\n    public static void Original() { throw new FirstFailure("first-349"); }\n    public static void Replace()\n    {\n        try { Original(); }\n        catch (FirstFailure error) { throw new Exception("replacement-349"); }\n    }\n    public static void Main() { Replace(); }\n}\n'''
        replace = temp / 'Replace'
        project(replace, 'Replace', replace_source)
        result = invoke('run', '.', cwd=replace)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: replacement-349\n'), result.stderr)
        check('throw new Exception("replacement-349")' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Program.Replace()' in frames[0] and 'Program.Main()' in frames[1], frames)
        check('Program.Original()' not in result.stderr and 'first-349' not in result.stderr, result.stderr)

        # Handled worker exception + GC + ThreadPool shutdown cannot contaminate later main-thread diagnostics.
        pool_source = '''using Void;\nusing Void.Threading;\npublic sealed class State { public ManualResetEvent Done; public State() { Done = new ManualResetEvent(false); } }\npublic sealed class Node { public int Value; public Node(int value) { Value = value; } }\npublic static class Program\n{\n    public static Action Work(State state)\n    {\n        return () =>\n        {\n            try { throw new Exception("handled-pool-349"); }\n            catch (Exception error) { Console.WriteLine(error.Message); }\n            GC.Collect(); state.Done.Set();\n        };\n    }\n    public static void LaterCrash() { Node value = null; Console.WriteLine(value.Value); }\n    public static void Main()\n    {\n        State state = new State();\n        Console.WriteLine(ThreadPool.QueueUserWorkItem(Work(state)));\n        Console.WriteLine(state.Done.WaitOne(5000));\n        ThreadPool.Shutdown();\n        LaterCrash();\n    }\n}\n'''
        pool = temp / 'Pool'
        project(pool, 'Pool', pool_source)
        result = invoke('run', '.', cwd=pool)
        check(result.returncode == 1, result)
        check('handled-pool-349' in result.stdout, result.stdout)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Program.LaterCrash()' in frames[0] and 'Program.Main()' in frames[1], frames)
        check('WorkerLoop' not in result.stderr and 'Program.Main.<lambda>' not in result.stderr, result.stderr)

        # Span runtime guards compose with nested caller frames without diagnostic-state corruption.
        span_source = '''using Void;\npublic static class Program\n{\n    public static void Read(Span<int> span) { Console.WriteLine(span[4]); }\n    public static void Outer() { int[] values = new int[] { 1, 2 }; Span<int> span = values; Read(span); }\n    public static void Main() { Outer(); }\n}\n'''
        span = temp / 'Span'
        project(span, 'Span', span_source)
        result = invoke('run', '.', cwd=span)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled IndexOutOfRangeException: Span index out of range\n'), result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 4, frames)
        check('Void.Span<int>.get_Item()' in frames[0] and 'Program.Read()' in frames[1] and
              'Program.Outer()' in frames[2] and 'Program.Main()' in frames[3], frames)
        assert_no_internal_names(result.stderr)

        # Iterator execution after yield is source-facing but does not fabricate pre-yield ancestry.
        iterator_source = '''using Void.Collections;\npublic sealed class Item { public int Value; }\npublic static class Program\n{\n    public static IEnumerator<int> Values()\n    {\n        yield return 1;\n        Item item = null;\n        yield return item.Value;\n    }\n    public static void Main()\n    {\n        IEnumerator<int> values = Values();\n        values.MoveNext();\n        values.MoveNext();\n    }\n}\n'''
        iterator = temp / 'Iterator'
        project(iterator, 'Iterator', iterator_source)
        result = invoke('run', '.', cwd=iterator)
        check(result.returncode == 1, result)
        check('at Program.Values()' in result.stderr and 'item.Value' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) == 2 and 'Program.Values()' in frames[0] and 'Program.Main()' in frames[1], frames)
        assert_no_internal_names(result.stderr)

        # Async resumed execution remains source-facing; no pre-await logical ancestry is invented.
        async_source = '''using Void;\nusing Void.Threading.Tasks;\npublic static class Program\n{\n    public static async Task CrashAsync()\n    {\n        await Task.Delay(1);\n        Runtime.Fail("async-fatal-349");\n    }\n    public static void Main() { Task task = CrashAsync(); task.Wait(); }\n}\n'''
        async_root = temp / 'Async'
        project(async_root, 'Async', async_source)
        result = invoke('run', '.', cwd=async_root)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: async-fatal-349\n'), result.stderr)
        check('at Program.CrashAsync()' in result.stderr, result.stderr)
        frames = stack_lines(result.stderr)
        check(len(frames) >= 1 and 'Program.CrashAsync()' in frames[0], frames)
        check('Program.Main()' not in '\n'.join(frames), frames)
        assert_no_internal_names(result.stderr)

        # Missing source + moved executable + closed generic stack: preserve identity/location/stack, never use cwd decoy.
        fallback_source = '''public sealed class Box<T>\n{\n    private T[] _values;\n    public Box(T[] values) { _values = values; }\n    public T Read(int index) { return _values[index]; }\n}\npublic static class Program\n{\n    public static void Outer() { Box<int> box = new Box<int>(new int[] { 1 }); Console.WriteLine(box.Read(5)); }\n    public static void Main() { Outer(); }\n}\n'''
        fallback = temp / 'Fallback'
        source_path = project(fallback, 'Fallback', fallback_source)
        built = invoke('build', '.', cwd=fallback)
        check(built.returncode == 0, built.stderr)
        original_exe = exe(fallback, 'Fallback')
        moved = temp / 'Moved'
        moved.mkdir()
        moved_exe = moved / original_exe.name
        shutil.copy2(original_exe, moved_exe)
        decoy = temp / 'Decoy'
        decoy.mkdir()
        (decoy / 'Program.void').write_text('DECOY-349-SOURCE\n', encoding='utf-8')
        saved = source_path.with_suffix('.void.saved')
        source_path.rename(saved)
        result = run_binary(moved_exe, decoy)
        check(result.returncode == 1, result)
        check('VOID runtime error: array index out of range' in result.stderr, result.stderr)
        check('Box<int>.Read()' in result.stderr and 'Program.Outer()' in result.stderr, result.stderr)
        check('DECOY-349-SOURCE' not in result.stderr and 'return _values[index]' not in result.stderr, result.stderr)
        check('Stack trace:' in result.stderr and '| ' not in result.stderr, result.stderr)
        assert_no_internal_names(result.stderr)
        saved.rename(source_path)

        # Unicode before a generic bounds failure retains marker/source-facing identity.
        unicode_source = '''public static class Program\n{\n    public static int Read<T>(T ignored, int[] values, int index)\n    {\n        Console.WriteLine("雪🙂"); return values[index];\n    }\n    public static void Main() { Console.WriteLine(Read<int>(1, new int[] { 3 }, 4)); }\n}\n'''
        unicode_root = temp / 'nested path 雪' / 'Unicode'
        project(unicode_root, 'Unicode', unicode_source)
        result = invoke('run', '.', cwd=unicode_root)
        check(result.returncode == 1, result)
        check('雪🙂' in result.stderr and 'values[index]' in result.stderr, result.stderr)
        check('at Program.Read()' in result.stderr and 'Stack trace:' in result.stderr, result.stderr)
        marker_lines = [line for line in result.stderr.splitlines() if '| ' in line and '^' in line]
        check(len(marker_lines) == 1 and marker_lines[0].count('~') >= 6, marker_lines)
        assert_no_internal_names(result.stderr)

        # Deep generic recursion remains complete and source-first; no frame collapsing.
        recurse_source = '''public sealed class Item { public int Value; }\npublic static class Program\n{\n    public static void Recurse<T>(int depth)\n    {\n        if (depth == 0) { Item item = null; Console.WriteLine(item.Value); return; }\n        Recurse<T>(depth - 1);\n    }\n    public static void Main() { Recurse<int>(20); }\n}\n'''
        recurse = temp / 'Recurse'
        project(recurse, 'Recurse', recurse_source)
        result = invoke('run', '.', cwd=recurse)
        check(result.returncode == 1, result)
        check(result.stderr.index('item.Value') < result.stderr.index('Stack trace:'), result.stderr)
        frames = stack_lines(result.stderr)
        recurse_frames = [line for line in frames if 'Program.Recurse()' in line]
        check(len(recurse_frames) == 21, frames)
        check('Program.Main()' in frames[-1], frames)
        check('repeated' not in result.stderr.lower(), result.stderr)
        assert_no_internal_names(result.stderr)

        if os.name != 'nt':
            # VOID -> native -> VOID callback composes with generic execution; native frame is never invented.
            callback = temp / 'Callback'
            symbol = 'void349_invoke'
            build_native_callback(callback, symbol)
            callback_source = '''public delegate void Callback();\npublic sealed class Item { public int Value; }\npublic static class NativeFixture { [Native("void349_invoke")] public static unsafe extern void Invoke(Callback callback); }\npublic static class GenericNative\n{\n    public static void Crash<T>() { Item item = null; Console.WriteLine(item.Value); }\n}\npublic static class Callbacks { public static void Entry() { GenericNative.Crash<int>(); } }\npublic static class Program\n{\n    public static unsafe void Outer() { NativeFixture.Invoke(Callbacks.Entry); }\n    public static unsafe void Main() { Outer(); }\n}\n'''
            project(callback, 'Callback', callback_source, unsafe=True,
                    compiler_extra={'libraryPaths': ['native'], 'libraries': [symbol]})
            result = invoke('run', '.', cwd=callback)
            check(result.returncode == 1, result)
            frames = stack_lines(result.stderr)
            check(len(frames) == 4, frames)
            expected_callback = ['GenericNative.Crash()', 'Callbacks.Entry()', 'Program.Outer()', 'Program.Main()']
            for index, name in enumerate(expected_callback):
                check(name in frames[index], frames)
            check(symbol not in result.stderr and 'NativeFixture.Invoke()' not in '\n'.join(frames), frames)
            assert_no_internal_names(result.stderr)

            # Native crash remains natively classified even when invoked from nested VOID frames.
            native_source = '''public static class Native { [Native("abort")] public static unsafe extern void Abort(); }\npublic static class Program\n{\n    public static unsafe void Inner() { Native.Abort(); }\n    public static unsafe void Outer() { Inner(); }\n    public static unsafe void Main() { Outer(); }\n}\n'''
            native = temp / 'NativeCrash'
            project(native, 'NativeCrash', native_source, unsafe=True)
            result = invoke('run', '.', cwd=native)
            check(result.returncode == 1, result)
            check(result.stderr.startswith('Native process crash: process terminated by signal '), result.stderr)
            check('SIGABRT' in result.stderr, result.stderr)
            check('VOID runtime error:' not in result.stderr and 'Unhandled' not in result.stderr and
                  'Stack trace:' not in result.stderr and 'VOID4000' not in result.stderr, result.stderr)

        # Representative generated C remains strict and instrumentation stays stack-local/non-managed.
        strict = temp / 'Strict'
        strict_source = '''public sealed class Item { public int Value; }\npublic static class Program\n{\n    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }\n    public static void Main() { Crash(); }\n}\n'''
        project(strict, 'Strict', strict_source)
        built = invoke('build', '.', cwd=strict)
        check(built.returncode == 0, built.stderr)
        generated = strict / '.void' / 'Strict.c'
        generated_text = generated.read_text(encoding='utf-8')
        check('VcCallFrame vc_call_frame' in generated_text, 'stack-local call frame missing')
        check('vc_call_frame_enter(&vc_call_frame' in generated_text, 'frame entry missing')
        check('vc_call_frame_leave(&vc_call_frame' in generated_text, 'frame leave missing')
        cc = os.environ.get('CC') or 'cc'
        strict_obj = temp / 'strict.o'
        compile_result = subprocess.run([
            cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
            '-IRuntime/include', '-c', str(generated), '-o', str(strict_obj)
        ], cwd=ROOT, capture_output=True, text=True, encoding='utf-8')
        check(compile_result.returncode == 0, compile_result.stderr)

    version = invoke('version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380', version.stdout)

    compiler = (ROOT / 'Compiler' / 'src' / 'compiler.c').read_text(encoding='utf-8')
    monomorph = (ROOT / 'Compiler' / 'src' / 'monomorph.c').read_text(encoding='utf-8')
    async_lower = (ROOT / 'Compiler' / 'src' / 'async_lower.c').read_text(encoding='utf-8')
    iterator = (ROOT / 'Compiler' / 'src' / 'iterator.c').read_text(encoding='utf-8')
    ast_header = (ROOT / 'Compiler' / 'include' / 'ast.h').read_text(encoding='utf-8')
    check('runtime_display_name' in compiler, 'fault-site/call-frame display unification missing')
    check('copy->span = source->span;' in monomorph, 'generic clone source span preservation missing')
    check('runtime_source_method_node' in ast_header and 'runtime_hide_frame' in ast_header,
          'generated-method source display metadata missing')
    check('runtime_source_method_node' in iterator, 'iterator source-method mapping missing')
    check('runtime_source_method_node' in async_lower and 'runtime_hide_frame = true' in async_lower,
          'async source-method/trampoline mapping missing')
    check('malloc(sizeof(VcCallFrame' not in compiler and 'pthread_mutex' not in compiler,
          'normal call-frame tracking gained allocation/lock overhead')


if __name__ == '__main__':
    main()
