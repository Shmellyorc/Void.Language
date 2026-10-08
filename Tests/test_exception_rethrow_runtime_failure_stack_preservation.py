#!/usr/bin/env python3
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests' / 'ExceptionRethrowRuntimeFailureStackPreservation'


def check(condition, detail='check failed'):
    if not condition:
        raise AssertionError(detail)
    print('True')


def invoke(*args, cwd=ROOT, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd,
                          capture_output=True, text=True, encoding='utf-8', env=env)


def project(root: Path, name: str, source: str) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    (root / f'{name}.voidproj').write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.346'}), encoding='utf-8')
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
    # Permanent application-shaped bare-rethrow fixture: original throw site/stack survive unwind.
    result = invoke('run', FIXTURE)
    source_path = FIXTURE / 'Program.void'
    source = source_path.read_text(encoding='utf-8')
    throw_line = source[:source.index('throw new AppFailure')].count('\n') + 1
    check(result.returncode == 1, result)
    check(result.stderr.startswith('Unhandled AppFailure: original-346\n'), result.stderr)
    normalized = result.stderr.replace('\\', '/')
    check(f'at Program.Crash() in {source_path.as_posix()}:{throw_line}:' in normalized, result.stderr)
    check('throw new AppFailure("original-346")' in result.stderr, result.stderr)
    check('^~~~~' in result.stderr, result.stderr)
    check(result.stderr.index('Unhandled AppFailure:') < result.stderr.index(f'{throw_line:6d} | ') < result.stderr.index('Stack trace:'), result.stderr)
    frames = stack_lines(result.stderr)
    check(len(frames) == 4, frames)
    check('Program.Crash()' in frames[0], frames)
    check('Program.Work()' in frames[1], frames)
    check('Program.Rethrow()' in frames[2], frames)
    check('Program.Main()' in frames[3], frames)
    no_internal_names(result.stderr)

    with tempfile.TemporaryDirectory(prefix='void-exception-stack-346-', dir=ROOT) as temp_name:
        temp = Path(temp_name)

        # Multiple bare rethrows preserve the first throw's site and complete original chain.
        repeated_source = '''public static class Program
{
    public static void Crash() { throw new Exception("deep"); }
    public static void Inner() { try { Crash(); } catch (Exception error) { throw; } }
    public static void Outer() { try { Inner(); } catch (Exception error) { throw; } }
    public static void Main() { Outer(); }
}
'''
        repeated = temp / 'Repeated'
        repeated_path = project(repeated, 'Repeated', repeated_source)
        result = invoke('run', repeated)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: deep\n'), result.stderr)
        repeated_throw_line = repeated_source[:repeated_source.index('throw new Exception')].count('\n') + 1
        check(f'Program.Crash() in {repeated_path.as_posix()}:{repeated_throw_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        frames = stack_lines(result.stderr)
        check([name for name in ('Program.Crash()', 'Program.Inner()', 'Program.Outer()', 'Program.Main()')
               if any(name in frame for frame in frames)] ==
              ['Program.Crash()', 'Program.Inner()', 'Program.Outer()', 'Program.Main()'], frames)
        check('Program.Crash()' in frames[0] and 'Program.Main()' in frames[-1], frames)

        # Replacement throw is genuinely new: new message, origin and stack begin at replacement site.
        replacement_source = '''public static class Program
{
    public static void Old() { throw new Exception("old"); }
    public static void Replace()
    {
        try { Old(); }
        catch (Exception error) { throw new Exception("replacement"); }
    }
    public static void Main() { Replace(); }
}
'''
        replacement = temp / 'Replacement'
        replacement_path = project(replacement, 'Replacement', replacement_source)
        result = invoke('run', replacement)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: replacement\n'), result.stderr)
        replacement_line = replacement_source[:replacement_source.index('throw new Exception("replacement")')].count('\n') + 1
        check(f'Program.Replace() in {replacement_path.as_posix()}:{replacement_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Replace()' in frames[0] and 'Program.Main()' in frames[-1], frames)
        check(not any('Program.Old()' in frame for frame in frames), frames)

        # VOID's existing explicit `throw error;` semantics are a fresh explicit throw site.
        throw_ex_source = '''public static class Program
{
    public static void Old() { throw new Exception("same-object"); }
    public static void Replace()
    {
        try { Old(); }
        catch (Exception error) { throw error; }
    }
    public static void Main() { Replace(); }
}
'''
        throw_ex = temp / 'ThrowEx'
        throw_ex_path = project(throw_ex, 'ThrowEx', throw_ex_source)
        result = invoke('run', throw_ex)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: same-object\n'), result.stderr)
        throw_ex_line = throw_ex_source[:throw_ex_source.index('throw error;')].count('\n') + 1
        check(f'Program.Replace() in {throw_ex_path.as_posix()}:{throw_ex_line}:' in result.stderr.replace('\\', '/'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Replace()' in frames[0] and 'Program.Main()' in frames[-1], frames)
        check(not any('Program.Old()' in frame for frame in frames), frames)

        # A fully swallowed exception emits no diagnostic and leaves no captured state behind.
        swallowed_source = '''public static class Program
{
    public static void Thrower() { throw new Exception("swallowed"); }
    public static void Main()
    {
        try { Thrower(); }
        catch (Exception error) { Console.WriteLine(error.Message); }
        Console.WriteLine("after");
    }
}
'''
        swallowed = temp / 'Swallowed'
        project(swallowed, 'Swallowed', swallowed_source)
        result = invoke('run', swallowed)
        check(result.returncode == 0, result)
        check('swallowed' in result.stdout and 'after' in result.stdout, result.stdout)
        check(result.stderr == '', result.stderr)

        # Handled exception A cannot contaminate a later unrelated runtime failure B.
        later_source = '''public class Item { public int Value; }
public static class Program
{
    public static void Thrower() { throw new Exception("old"); }
    public static void First() { try { Thrower(); } catch (Exception error) { } }
    public static void Later() { Item item = null; Console.WriteLine(item.Value); }
    public static void Main() { First(); Later(); }
}
'''
        later = temp / 'Later'
        project(later, 'Later', later_source)
        result = invoke('run', later)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: object reference is null\n'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Later()' in frames[0] and 'Program.Main()' in frames[-1], frames)
        check(not any('Program.Thrower()' in frame or 'Program.First()' in frame for frame in frames), frames)
        check('old' not in result.stderr, result.stderr)

        # Nested exception B may be handled without overwriting exception A's preserved snapshot.
        nested_source = '''public static class Program
{
    public static void A() { throw new Exception("A"); }
    public static void B() { throw new Exception("B"); }
    public static void Nest()
    {
        try { A(); }
        catch (Exception first)
        {
            try { B(); }
            catch (Exception second) { Console.WriteLine(second.Message); }
            throw;
        }
    }
    public static void Main() { Nest(); }
}
'''
        nested = temp / 'Nested'
        project(nested, 'Nested', nested_source)
        result = invoke('run', nested)
        check(result.returncode == 1, result)
        check('B' in result.stdout, result.stdout)
        check(result.stderr.startswith('Unhandled Exception: A\n'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.A()' in frames[0], frames)
        check(any('Program.Nest()' in frame for frame in frames), frames)
        check(not any('Program.B()' in frame for frame in frames), frames)

        # Successful finally cleanup must not replace the original exception snapshot.
        finally_source = '''public static class Program
{
    public static void Crash() { throw new Exception("original-finally"); }
    public static void Cleanup() { Console.WriteLine("cleanup"); }
    public static void Work() { try { Crash(); } finally { Cleanup(); } }
    public static void Main() { Work(); }
}
'''
        finally_project = temp / 'Finally'
        project(finally_project, 'Finally', finally_source)
        result = invoke('run', finally_project)
        check(result.returncode == 1, result)
        check('cleanup' in result.stdout, result.stdout)
        check(result.stderr.startswith('Unhandled Exception: original-finally\n'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check(any('Program.Work()' in frame for frame in frames), frames)
        check(not any('Program.Cleanup()' in frame for frame in frames), frames)

        # A new exception from finally follows existing replacement semantics and gets its own stack.
        final_replace_source = '''public static class Program
{
    public static void Crash() { throw new Exception("old-finally"); }
    public static void Work()
    {
        try { Crash(); }
        finally { throw new Exception("new-finally"); }
    }
    public static void Main() { Work(); }
}
'''
        final_replace = temp / 'FinallyReplace'
        project(final_replace, 'FinallyReplace', final_replace_source)
        result = invoke('run', final_replace)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: new-finally\n'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Work()' in frames[0] and 'Program.Main()' in frames[-1], frames)
        check(not any('Program.Crash()' in frame for frame in frames), frames)
        check('old-finally' not in result.stderr, result.stderr)

        # Repeated recursive frames survive unwind into a catch and a later bare rethrow.
        recursion_source = '''public static class Program
{
    public static void Crash() { throw new Exception("recursive"); }
    public static void Recurse(int depth)
    {
        if (depth == 0) { Crash(); return; }
        Recurse(depth - 1);
    }
    public static void Wrap() { try { Recurse(3); } catch (Exception error) { throw; } }
    public static void Main() { Wrap(); }
}
'''
        recursion = temp / 'Recursion'
        project(recursion, 'Recursion', recursion_source)
        result = invoke('run', recursion)
        check(result.returncode == 1, result)
        frames = stack_lines(result.stderr)
        check('Program.Crash()' in frames[0], frames)
        check(sum('Program.Recurse()' in frame for frame in frames) == 4, frames)
        check(any('Program.Wrap()' in frame for frame in frames), frames)
        check('Program.Main()' in frames[-1], frames)

        # Managed worker exception snapshot transfers safely across worker detach / Join rethrow.
        thread_source = '''using Void.Threading;
public static class Program
{
    public static void Deep() { throw new Exception("worker-346"); }
    public static void Worker() { Deep(); }
    public static void Main()
    {
        Thread worker = new Thread(() => Worker());
        worker.Start();
        worker.Join();
    }
}
'''
        thread = temp / 'Threaded'
        project(thread, 'Threaded', thread_source)
        result = invoke('run', thread)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('Unhandled Exception: worker-346\n'), result.stderr)
        frames = stack_lines(result.stderr)
        check('Program.Deep()' in frames[0], frames)
        check(any('Program.Worker()' in frame for frame in frames), frames)
        check(any('Program.Main.<lambda>()' in frame for frame in frames), frames)
        check(not any('Program.Main()' in frame for frame in frames), frames)
        check('vc_managed_thread' not in result.stderr and 'vc_native_thread' not in result.stderr, result.stderr)

        # Runtime.Fail remains a distinct fatal path, not a catchable VOID exception.
        fatal_source = '''public static class Program
{
    public static void Fatal() { Runtime.Fail("fatal-346"); }
    public static void Main() { Fatal(); }
}
'''
        fatal = temp / 'Fatal'
        project(fatal, 'Fatal', fatal_source)
        result = invoke('run', fatal)
        check(result.returncode == 1, result)
        check(result.stderr.startswith('VOID runtime error: fatal-346\n'), result.stderr)
        check('Unhandled Exception:' not in result.stderr, result.stderr)

        # Generated C locks the active-vs-captured split and remains strict C11.
        built = invoke('build', FIXTURE)
        generated = FIXTURE / '.void' / 'ExceptionRethrowRuntimeFailureStackPreservation.c'
        check(built.returncode == 0 and generated.is_file(), built.stderr)
        generated_text = generated.read_text(encoding='utf-8')
        check('typedef struct VcCallFrame' in generated_text, 'active call-frame representation missing')
        check('typedef struct VcCapturedCallFrame' in generated_text, 'captured frame representation missing')
        check('VcCapturedCallFrame *captured_call_frames;' in generated_text, 'per-thread captured arena missing')
        check('captured_start' in generated_text and 'captured_count' in generated_text, 'handler captured range missing')
        check('vc_exception_trace_capture' in generated_text and 'vc_throw_captured' in generated_text,
              'throw-time captured stack integration missing')
        check('vc_rethrow_handler' in generated_text, 'bare rethrow preservation helper missing')
        check('vc_exception_trace_rewind' in generated_text, 'handled snapshot cleanup missing')
        check('static VcCapturedCallFrame *vc_' not in generated_text, 'process-global captured stack introduced')
        helper = generated_text[generated_text.index('static VC_MAYBE_UNUSED bool vc_exception_trace_capture'):
                                generated_text.index('static VC_MAYBE_UNUSED bool vc_exception_trace_copy')]
        check('vc_runtime_alloc' not in helper and 'vc_gc_root_push' not in helper,
              'managed allocation/rooting introduced into stack capture')
        cc = os.environ.get('CC') or 'cc'
        strict_obj = temp / 'strict.o'
        compile_result = subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                                         '-Werror', '-O2', '-IRuntime/include', '-c', str(generated),
                                         '-o', str(strict_obj)], cwd=ROOT, capture_output=True,
                                        text=True, encoding='utf-8')
        check(compile_result.returncode == 0, compile_result.stderr)

    version = invoke('version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.370', version.stdout)
    compiler = (ROOT / 'Compiler' / 'src' / 'compiler.c').read_text(encoding='utf-8')
    check('VcCapturedCallFrame *captured_call_frames;' in compiler, 'captured stack is not thread-owned')
    check('vc_rethrow_handler(&' in compiler, 'bare rethrow does not preserve captured range')
    check('vc_exception_trace_copy' in compiler and 'vc_exception_trace_import' in compiler,
          'cross-thread immutable snapshot transfer missing')
    check('vc_throw_captured' in compiler, 'captured throw propagation helper missing')
    check('active_caught_exception' in compiler, 'caught snapshot lifetime tracking missing')
    check('backtrace(' not in compiler and 'CaptureStackBackTrace' not in compiler,
          'native stack reconstruction introduced')
    check('logical async' not in compiler.lower(), 'logical async ancestry implemented unexpectedly')


if __name__ == '__main__':
    main()
