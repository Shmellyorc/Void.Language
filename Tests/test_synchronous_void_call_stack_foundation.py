#!/usr/bin/env python3
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests' / 'SynchronousVoidCallStackFoundation'


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
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.345',
        'compiler': {'unsafe': unsafe}}), encoding='utf-8')
    path = root / 'Program.void'
    path.write_text(source, encoding='utf-8')
    return path


def stack_lines(stderr: str):
    marker = 'Stack trace:\n'
    check(marker in stderr, stderr)
    tail = stderr.split(marker, 1)[1]
    return [line for line in tail.splitlines() if line.startswith('   at ')]


def no_internal_names(stderr: str):
    lowered = stderr.lower()
    check('vc_fn_' not in lowered, stderr)
    check('vc_m_' not in lowered, stderr)
    check('__g' not in stderr, stderr)
    check('__async_sm' not in lowered, stderr)


def main():
    # Application-shaped Main -> Run -> Loop -> PlayerTurn failure.
    result = invoke('run', FIXTURE)
    source_path = FIXTURE / 'Program.void'
    source = source_path.read_text(encoding='utf-8')
    fail_line = source[:source.index('monster.Name')].count('\n') + 1
    check(result.returncode == 1, result)
    check(result.stderr.startswith('VOID runtime error: object reference is null\n'), result.stderr)
    check(f'at V.Game.PlayerTurn() in {source_path.as_posix()}:{fail_line}:' in result.stderr.replace('\\', '/'), result.stderr)
    check('monster.Name' in result.stderr and '^~~~~~~' in result.stderr, result.stderr)
    check(result.stderr.index('VOID runtime error:') < result.stderr.index(f'{fail_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)
    frames = stack_lines(result.stderr)
    check(len(frames) == 4, frames)
    check('V.Game.PlayerTurn()' in frames[0], frames)
    check('V.Game.Loop()' in frames[1], frames)
    check('V.Game.Run()' in frames[2], frames)
    check('Program.Main()' in frames[3], frames)
    check(frames[0].endswith(f':{fail_line}:31') or f':{fail_line}:' in frames[0], frames[0])
    no_internal_names(result.stderr)

    with tempfile.TemporaryDirectory(prefix='void-call-stack-345-', dir=ROOT) as temp_name:
        temp = Path(temp_name)

        # Recursion retains every real invocation instead of deduplicating names.
        recursion_source = '''public class Item { public int Value; }
public static class Program
{
    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }
    public static void Recurse(int depth)
    {
        if (depth == 0) { Crash(); return; }
        Recurse(depth - 1);
    }
    public static void Main() { Recurse(3); }
}
'''
        recursion = temp / 'Recursion'
        project(recursion, 'Recursion', recursion_source)
        result = invoke('run', recursion)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check(sum('Program.Recurse()' in frame for frame in frames) == 4, frames)
        check('Program.Main()' in frames[-1], frames)

        # Completed and early-returned calls must not remain in a later failure stack.
        cleanup_source = '''public class Item { public int Value; }
public static class Program
{
    public static void Finished() { return; }
    public static void Early(bool stop) { if (stop) return; Finished(); }
    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }
    public static void Main()
    {
        Finished(); Finished(); Finished();
        Early(true); Early(false);
        Crash();
    }
}
'''
        cleanup = temp / 'Cleanup'
        project(cleanup, 'Cleanup', cleanup_source)
        result = invoke('run', cleanup)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check(any('Program.Crash()' in frame for frame in frames), frames)
        check(not any('Program.Finished()' in frame for frame in frames), frames)
        check(not any('Program.Early()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)

        # Constructor body is a user-visible synchronous frame; allocation helper is not.
        ctor_source = '''public class Item { public int Value; }
public class Thing
{
    public Thing() { Item item = null; Console.WriteLine(item.Value); }
}
public static class Program { public static void Main() { var thing = new Thing(); } }
'''
        ctor = temp / 'Constructor'
        project(ctor, 'Constructor', ctor_source)
        result = invoke('run', ctor)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Thing.Thing()' in frames[0], frames)
        check('Program.Main()' in frames[-1], frames)
        check('vc_ctor' not in result.stderr, result.stderr)

        # Custom property accessor gets a source-facing accessor frame.
        property_source = '''public class Item { public int Value; }
public class Thing
{
    public int Value
    {
        get { Item item = null; return item.Value; }
    }
}
public static class Program { public static void Main() { var thing = new Thing(); Console.WriteLine(thing.Value); } }
'''
        prop = temp / 'Property'
        project(prop, 'Property', property_source)
        result = invoke('run', prop)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Thing.get_Value()' in frames[0], frames)
        check('Program.Main()' in frames[-1], frames)
        check('vc_pg_' not in result.stderr and 'vc_ps_' not in result.stderr, result.stderr)

        # Closed generic type identities stay source-facing; generic methods do not expose specialization names.
        generic_source = '''public class Item { public int Value; }
public class Box<T>
{
    public void Crash() { Item item = null; Console.WriteLine(item.Value); }
}
public static class Program
{
    public static void Generic<T>(T input) { Box<T> box = new Box<T>(); box.Crash(); }
    public static void Main() { Generic<int>(1); }
}
'''
        generic = temp / 'Generic'
        project(generic, 'Generic', generic_source)
        result = invoke('run', generic)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Box<int>.Crash()' in frames[0], frames)
        check(any('Program.Generic()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)
        no_internal_names(result.stderr)

        # Method delegate dispatch records only the actually executing VOID method.
        delegate_source = '''public class Item { public int Value; }
public static class Program
{
    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }
    public static void Main() { Action action = Crash; action(); }
}
'''
        delegate = temp / 'Delegate'
        project(delegate, 'Delegate', delegate_source)
        result = invoke('run', delegate)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check('Program.Main()' in frames[-1], frames)
        check('vc_delegate' not in result.stderr.lower(), result.stderr)

        # Lambda user code gets a truthful source-facing anonymous frame, not a generated helper.
        lambda_source = '''public class Item { public int Value; }
public static class Program
{
    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }
    public static void Main() { Action action = () => Crash(); action(); }
}
'''
        lambda_project = temp / 'Lambda'
        project(lambda_project, 'Lambda', lambda_source)
        result = invoke('run', lambda_project)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check(any('Program.Main.<lambda>()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)
        no_internal_names(result.stderr)

        # Static initialization has a source-facing initialization frame.
        static_source = '''public class Item { public int Value; }
public class Config
{
    public static int Number = Init();
    public static int Init() { Item item = null; return item.Value; }
}
public static class Program { public static void Main() { Console.WriteLine(Config.Number); } }
'''
        static_init = temp / 'StaticInit'
        project(static_init, 'StaticInit', static_source)
        result = invoke('run', static_init)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Config.Init()' in frames[0], frames)
        check(any('Config.<static initializer>()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)
        check('vc_type_init_' not in result.stderr, result.stderr)

        # Each managed/native VOID execution thread owns an independent active stack.
        thread_source = '''using Void.Threading;
public class Item { public int Value; }
public static class Program
{
    public static void WorkerCrash() { Item item = null; Console.WriteLine(item.Value); }
    public static void Main()
    {
        Thread worker = new Thread(() => WorkerCrash());
        worker.Start();
        worker.Join();
    }
}
'''
        threaded = temp / 'Threaded'
        project(threaded, 'Threaded', thread_source)
        result = invoke('run', threaded)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Program.WorkerCrash()' in frames[0], frames)
        check(any('Program.Main.<lambda>()' in frame for frame in frames), frames)
        check(not any('Program.Main()' in frame for frame in frames), frames)
        check('vc_managed_thread' not in result.stderr and 'vc_native_thread' not in result.stderr, result.stderr)

        # Unhandled user exception gets the same stack beneath the #344 source diagnostic.
        throw_source = '''public static class Program
{
    public static void Crash() { throw new Exception("boom"); }
    public static void Middle() { Crash(); }
    public static void Main() { Middle(); }
}
'''
        throwing = temp / 'Throwing'
        project(throwing, 'Throwing', throw_source)
        result = invoke('run', throwing)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: boom\n'), result.stderr)
        check(result.stderr.index('throw new Exception') < result.stderr.index('Stack trace:'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check('Program.Middle()' in frames[1], frames)
        check('Program.Main()' in frames[2], frames)

        # longjmp to a catch restores the handler's frame baseline; no abandoned child survives.
        caught_source = '''public class Item { public int Value; }
public static class Program
{
    public static void Thrower() { throw new Exception("caught"); }
    public static void Caught()
    {
        try { Thrower(); }
        catch (Exception ex) { Console.WriteLine(ex.Message); }
    }
    public static void Crash() { Item item = null; Console.WriteLine(item.Value); }
    public static void Main() { Caught(); Crash(); }
}
'''
        caught = temp / 'Caught'
        project(caught, 'Caught', caught_source)
        result = invoke('run', caught)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check(not any('Program.Thrower()' in frame for frame in frames), frames)
        check(not any('Program.Caught()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)

        # Runtime.Fail remains fatal/non-exception; #347 now renders its truthful source site and active stack.
        fatal_source = 'public static class Program { public static void Main() { Runtime.Fail("foundation fatal"); } }\n'
        fatal = temp / 'Fatal'
        project(fatal, 'Fatal', fatal_source)
        result = invoke('run', fatal)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: foundation fatal\n') and 'Runtime.Fail("foundation fatal")' in result.stderr and 'Stack trace:' in result.stderr and 'Unhandled Exception:' not in result.stderr, result.stderr)

        # Generated C remains strict and frame tracking does not allocate per call.
        built = invoke('build', FIXTURE)
        generated = FIXTURE / '.void' / 'SynchronousVoidCallStackFoundation.c'
        check(built.returncode == 0 and generated.is_file(), built.stderr)
        generated_text = generated.read_text(encoding='utf-8')
        check('typedef struct VcCallFrame' in generated_text, 'call-frame representation missing')
        check('VcCallFrame *call_frame;' in generated_text, 'thread call-frame state missing')
        check('vc_call_frame_enter(&vc_call_frame' in generated_text, 'function frame registration missing')
        check('Stack trace:' in generated_text, 'stack renderer missing')
        helper = generated_text[generated_text.index('static VC_MAYBE_UNUSED void vc_call_frame_enter'):
                                generated_text.index('static VC_MAYBE_UNUSED void vc_call_frame_leave')]
        check('malloc(' not in helper and 'vc_runtime_alloc' not in helper, helper)
        cc = os.environ.get('CC') or 'cc'
        strict_obj = temp / 'strict.o'
        compile_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                                         '-Werror', '-O2', '-IRuntime/include', '-c', str(generated),
                                         '-o', str(strict_obj)], cwd=ROOT, capture_output=True,
                                        text=True, encoding='utf-8')
        check(compile_result.returncode == 0, compile_result.stderr)

    version = invoke('version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381', version.stdout)
    compiler = (ROOT / 'Compiler' / 'src' / 'compiler.c').read_text(encoding='utf-8')
    check('VcCallFrame *call_frame;' in compiler, 'per-thread call-frame state missing')
    check('vc_handler->call_frame' in compiler and 'vc_call_frame_current = vc_handler->call_frame' in compiler,
          'longjmp frame restoration missing')
    check('vc_runtime_diagnostic_context_report(vc_site' in compiler and 'vc_call_stack_report(site)' in compiler,
          'failure-time stack reporting missing')
    check('logical async' not in compiler.lower(), 'async logical ancestry implemented unexpectedly')
    check('backtrace(' not in compiler and 'CaptureStackBackTrace' not in compiler,
          'host/native stack reconstruction introduced')


if __name__ == '__main__':
    main()
