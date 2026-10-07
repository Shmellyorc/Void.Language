#!/usr/bin/env python3
"""Source provenance and child termination contracts; also runs natively on Windows."""
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')


def invoke(*args, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, encoding='utf-8', env=env)


def assert_source_frames(stderr, source_path, source, expected):
    """Validate the exact synchronous VOID stack when a stack section is present.

    Each expected entry names a real method and its fault/call expression. #346
    preserves the complete captured failure chain across exception unwind/rethrow;
    unknown, missing, reordered or invented frames are rejected. The fault column
    has its own exact check below.
    """
    normalized = stderr.replace('\\', '/')
    stack_present = 'Stack trace:' in normalized
    frame_text = normalized.split('Stack trace:', 1)[1] if stack_present else normalized
    frames = [line.strip() for line in frame_text.splitlines()
              if line.lstrip().startswith('at ')]
    assert frames, stderr
    if stack_present:
        assert len(frames) == len(expected), stderr
    else:
        assert len(frames) <= len(expected), stderr
    parsed = []
    for frame in frames:
        match = re.fullmatch(r'at (.+) in (.+):(\d+):(\d+)', frame)
        assert match is not None, frame
        parsed.append((frame, match))

    if stack_present:
        assert [match[1] for _, match in parsed] == [method for method, _ in expected], stderr

    for index, ((frame, match), (method, marker)) in enumerate(
            zip(parsed, expected[:len(parsed)])):
        line = source[:source.index(marker)].count('\n') + 1
        actual_line = int(match[3])
        assert match[1] == method and match[2] == source_path.as_posix(), frame
        # The faulting top frame keeps the exact #344 site. Caller frames use
        # truthful method-declaration locations until richer call-site frame
        # metadata is introduced; never label those declarations as call sites.
        if not stack_present or index == 0:
            assert actual_line == line, frame
        assert 1 <= actual_line <= len(source.splitlines()), frame
        assert 1 <= int(match[4]) <= len(source.splitlines()[actual_line - 1]) + 1, frame


def check_frame_contract():
    # Exercise the validator independently of today's one-site renderer.
    source = 'throw origin;\nCrash();\nMiddle();\n'
    path = Path('/fixture/Program.void')
    expected = [('Program.Crash()', 'throw origin'),
                ('Program.Middle()', 'Crash();'), ('Program.Main()', 'Middle();')]
    frames = [f'   at {method} in {path.as_posix()}:{line}:1\n'
              for line, (method, _) in enumerate(expected, 1)]
    for count in (1, 2, 3):
        assert_source_frames(''.join(frames[:count]), path, source, expected)
    for invalid in (frames[0] + frames[2],
                    frames[0] + frames[1].replace('Middle()', 'Invented()'),
                    frames[0] + frames[1].replace(':2:1', ':3:1'),
                    ''.join(frames) + frames[2]):
        try:
            assert_source_frames(invalid, path, source, expected)
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted fabricated or misordered caller frames')


def main():
    check_frame_contract()
    with tempfile.TemporaryDirectory(prefix='void-runtime-sites-', dir=ROOT) as tmp:
        directory = Path(tmp)

        def project(name, source, unsafe=False):
            path = directory / name
            path.mkdir()
            (path / (name + '.voidproj')).write_text(json.dumps({
                'format': 1, 'name': name, 'output': 'exe', 'version': '0.1.0',
                'compiler': {'unsafe': unsafe}}), encoding='utf-8')
            (path / 'Program.void').write_text(source, encoding='utf-8')
            return path

        def run(name, source, unsafe=False):
            path = project(name, source, unsafe)
            result = invoke('run', path)
            return path, result

        def site(result, path, method, source, marker, callers=()):
            line = source[:source.index(marker)].count('\n') + 1
            assert result.returncode == 1, result
            assert f'at {method} in {path.as_posix()}/Program.void:{line}:' in result.stderr.replace('\\', '/'), result.stderr
            assert 'VOID4000' not in result.stderr and 'program exited' not in result.stderr, result.stderr
            assert '.c:' not in result.stderr, result.stderr
            assert_source_frames(result.stderr, path / 'Program.void', source,
                                 [(method, marker), *callers])

        null = '''public class Item { public int Value; }
public static class Program
{
    public static void Crash()
    {
        Item item = null;
        Console.WriteLine(item.Value);
    }
    public static void Main() { Crash(); }
}
'''
        path, result = run('Null', null)
        assert 'VOID runtime error: object reference is null' in result.stderr, result.stderr
        site(result, path, 'Program.Crash()', null, 'item.Value',
             [('Program.Main()', 'Crash();')])
        column = null.splitlines()[6].index('item.Value') + 1
        assert f':7:{column}\n' in result.stderr, result.stderr

        exception = '''public sealed class GameException : Exception
{
    public GameException(string message) : base(message) {}
}
public static class Program
{
    public static void Crash()
    {
        throw new GameException("boom 雪");
    }
    public static void Middle() { Crash(); }
    public static void Main() { Middle(); }
}
'''
        path, result = run('Nested', exception)
        assert 'Unhandled GameException: boom 雪' in result.stderr, result.stderr
        site(result, path, 'Program.Crash()', exception, 'throw new',
             [('Program.Middle()', 'Crash();'), ('Program.Main()', 'Middle();')])
        direct = 'public static class Program { public static void Main() { throw new Exception("main boom"); } }\n'
        path, result = run('MainThrow', direct)
        site(result, path, 'Program.Main()', direct, 'throw new')
        assert 'Exception: main boom' in result.stderr

        generic = """using V;
namespace V
{
    public static class Game
    {
        public static void Crash<T>()
        {
            throw new Exception("generic site");
        }
    }
}
public static class Program
{
    public static void Main() { Game.Crash<int>(); }
}
"""
        path, result = run('Generic', generic)
        site(result, path, 'V.Game.Crash()', generic, 'throw new',
             [('Program.Main()', 'Game.Crash<int>();')])
        assert '__g' not in result.stderr, result.stderr

        caught = '''public static class Program
{
    public static void Main()
    {
        try { throw new Exception("caught"); }
        catch (Exception error) { Console.WriteLine("handled"); }
    }
}
'''
        _, result = run('Caught', caught)
        assert result.returncode == 0 and result.stderr == '', result.stderr
        assert 'handled' in result.stdout

        cleanup = '''public sealed class Resource : IDisposable
{
    public void Dispose() { Console.WriteLine("disposed"); }
}
public static class Program
{
    public static void Crash()
    {
        throw new Exception("original");
    }
    public static void Main()
    {
        using (Resource resource = new Resource())
        {
            try { Crash(); }
            catch (Exception error)
            {
                try { throw new Exception("nested handled"); }
                catch (Exception inner) { Console.WriteLine("nested caught"); }
                throw;
            }
            finally
            {
                try { throw new Exception("cleanup handled"); }
                catch (Exception inner) { Console.WriteLine("finally"); }
            }
        }
    }
}
'''
        path, result = run('Cleanup', cleanup)
        site(result, path, 'Program.Crash()', cleanup, 'throw new Exception("original")',
             [('Program.Main()', 'Crash();')])
        assert 'Exception: original' in result.stderr and 'nested handled' not in result.stderr
        assert result.stdout.index('nested caught') < result.stdout.index('finally') < result.stdout.index('disposed')

        _, result = run('Fatal', 'public static class Program { public static void Main() { Runtime.Fail("invariant probe"); } }\n')
        assert result.returncode == 1 and 'VOID runtime error: invariant probe' in result.stderr
        assert '   at Program.Main()' in result.stderr and 'Stack trace:' in result.stderr and 'VOID4000' not in result.stderr

        native = '''public static class Native
{
    [Native("exit")] public static unsafe extern void Exit(int code);
    [Native("abort")] public static unsafe extern void Abort();
}
'''
        for code in (1, 7, 127):
            _, result = run('Exit' + str(code), native + f'public static class Program {{ public static unsafe void Main() {{ Native.Exit({code}); }} }}\n', True)
            assert result.returncode == code and result.stderr == '', result.stderr
        if os.name == 'nt':
            # Windows only exposes the status, even when exit deliberately uses an NT value.
            _, result = run('NativeStatus', native + 'public static class Program { public static unsafe void Main() { Native.Exit(-1073741819); } }\n', True)
            assert result.returncode & 0xffffffff == 0xc0000005, result.returncode
            assert 'Windows native exception status 0xc0000005 (access violation)' in result.stderr, result.stderr
            assert 'Unhandled' not in result.stderr and 'VOID4000' not in result.stderr
        _, result = run('NativeFault', native + 'public static class Program { public static unsafe void Main() { Native.Abort(); } }\n', True)
        assert result.returncode != 0 and 'Unhandled' not in result.stderr
        if os.name != 'nt':
            assert 'SIGABRT' in result.stderr and 'signal' in result.stderr, result.stderr

        # A successful tool which emits no executable exercises generated-program launch failure.
        stub_source = directory / 'tool.c'
        stub_source.write_text('int main(void) { return 0; }\n')
        stub = directory / ('tool.exe' if os.name == 'nt' else 'tool')
        subprocess.run([os.environ.get('CC') or 'cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                        '-Werror', '-O2', str(stub_source), '-o', str(stub)], check=True)
        path = project('Launch', 'public static class Program { public static void Main() {} }\n')
        result = invoke('run', path, env=dict(os.environ, CC=str(stub)))
        assert result.returncode == 1 and 'could not execute' in result.stderr and 'VOID4000' in result.stderr, result.stderr
        assert '/bin/Launch' in result.stderr.replace('\\', '/'), result.stderr
        result = invoke('build', path, env=dict(os.environ, CC=str(directory / 'missing-tool')))
        assert result.returncode == 1 and 'missing-tool' in result.stderr and 'could not execute' in result.stderr
    print('True')


if __name__ == '__main__':
    main()
