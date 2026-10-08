#!/usr/bin/env python3
"""#352 ordinary-local definite-assignment completion regressions."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests/OrdinaryLocalDefiniteAssignment'
COUNT = 0


def expect(condition, detail='check failed'):
    global COUNT
    if not condition:
        raise AssertionError(detail)
    COUNT += 1
    print('True')


def invoke(*args, cwd=ROOT):
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd, env=env,
                          capture_output=True, text=True, encoding='utf-8', timeout=180)


def strict_generated(generated: Path, label: str):
    with tempfile.TemporaryDirectory(prefix='void352-strict-') as temporary:
        command = shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
            '-I' + str(ROOT / 'Runtime/include')]
        if os.name != 'nt':
            command.append('-pthread')
        command += ['-c', str(generated), '-o', str(Path(temporary) / (label + '.o'))]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        expect(result.returncode == 0 and not result.stderr, result.stderr)


def check_source(source: str):
    with tempfile.TemporaryDirectory(prefix='void352-check-') as temporary:
        path = Path(temporary) / 'Program.void'
        path.write_text(source, encoding='utf-8')
        return invoke('check', path, '--diagnostics=json')


def expect_valid(source: str):
    result = check_source(source)
    expect(result.returncode == 0 and not result.stderr, result.stderr)


def expect_unassigned(source: str, local_name: str, message_part='definitely assigned'):
    result = check_source(source)
    records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
    valid = result.returncode != 0 and len(records) == 1
    if valid:
        record = records[0]
        span = record.get('span') or {}
        start = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        valid = (record.get('code') == 'VOID3000' and message_part in record.get('message', '') and
                 0 <= start < end <= len(source) and source[start:end] == local_name)
    expect(valid, result.stderr)


def main():
    # Broad positive integration: ordinary assignment, initializer, branch merge,
    # early return, ?:, &&/!, switch statement/expression, do/for break+continue,
    # out assignment, lambda capture, generic/reference/value types, try/catch/finally,
    # using, shadowing, and iterator suspension.
    result = invoke('run', FIXTURE)
    runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    expect(len(runtime) == 23 and all(line == 'True' for line in runtime), result.stdout)

    generated = FIXTURE / '.void/OrdinaryLocalDefiniteAssignment.c'
    first = generated.read_bytes()
    result = invoke('build', FIXTURE)
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    expect(generated.read_bytes() == first, 'generated C changed across identical builds')
    strict_generated(generated, 'ordinary-local-definite-assignment')

    # Direct read: diagnostic is attached to the read token, not the declaration.
    expect_unassigned(
        'public static class Program { public static void Main(){ int value; Console.WriteLine(value); } }',
        'value')

    # Ordinary assignment and initialized declarations remain valid independently of
    # the larger runtime fixture.
    expect_valid(
        'public static class Program { public static void Main(){ int value; value=10; Console.WriteLine(value); } }')
    expect_valid(
        'public static class Program { public static void Main(){ int value=10; Console.WriteLine(value); } }')

    # Branch joins only preserve assignment present on every reachable continuation.
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;if(b)value=1;return value;} '
        'public static void Main(){Console.WriteLine(Read(false));} }', 'value')
    expect_valid(
        'public static class Program { static int Read(bool b){int value;if(b)value=1;else value=2;return value;} '
        'public static void Main(){Console.WriteLine(Read(true));} }')
    expect_valid(
        'public static class Program { static int Read(bool b){int value;if(b)value=1;else return 0;return value;} '
        'public static void Main(){Console.WriteLine(Read(true));} }')

    # Conditional and short-circuit assignments do not leak from skipped paths.
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;int x=b?(value=1):2;return value;} '
        'public static void Main(){Console.WriteLine(Read(false));} }', 'value')
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;bool ok=b&&((value=1)>0);return value;} '
        'public static void Main(){Console.WriteLine(Read(false));} }', 'value')
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;bool ok=b||((value=1)>0);return value;} '
        'public static void Main(){Console.WriteLine(Read(true));} }', 'value')
    expect_valid(
        'public static class Program { static int Read(bool b){int value;if(!(b&&((value=1)>0)))return 0;return value;} '
        'public static void Main(){Console.WriteLine(Read(true));} }')

    # Switch statement and switch expression merges use the same declaration identity
    # and require assignment on each path that reaches the following read.
    expect_unassigned(
        'public static class Program { static int Read(int k){int value;switch(k){case 0:value=1;break;}return value;} '
        'public static void Main(){Console.WriteLine(Read(1));} }', 'value')
    expect_valid(
        'public static class Program { static int Read(int k,bool b){int value;switch(k){case 0:if(b){value=1;break;}'
        'else return 0;default:value=2;break;}return value;} public static void Main(){Console.WriteLine(Read(0,true));} }')
    expect_unassigned(
        'public static class Program { static int Read(int k,bool b){int value;switch(k){case 0:if(b){value=1;break;}'
        'else break;default:value=2;break;}return value;} public static void Main(){Console.WriteLine(Read(0,false));} }',
        'value')
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;int x=b switch {true=>(value=1),_=>2};return value;} '
        'public static void Main(){Console.WriteLine(Read(false));} }', 'value')

    # while/for bodies can execute zero times; do executes its body first. Break and
    # continue paths retain only assignment that really reaches the continuation.
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;while(b){value=1;b=false;}return value;} '
        'public static void Main(){Console.WriteLine(Read(false));} }', 'value')
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;for(;b;){value=1;b=false;}return value;} '
        'public static void Main(){Console.WriteLine(Read(false));} }', 'value')
    expect_valid(
        'public static class Program { static int Read(){int value;do{value=1;}while(false);return value;} '
        'public static void Main(){Console.WriteLine(Read());} }')
    expect_valid(
        'public static class Program { static int Read(){int value;for(;;){value=1;break;}return value;} '
        'public static void Main(){Console.WriteLine(Read());} }')
    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;do{if(b)continue;value=1;}while(false);return value;} '
        'public static void Main(){Console.WriteLine(Read(true));} }', 'value')

    expect_unassigned(
        'public static class Program { static int Read(bool b){int value;do{if(b)break;try{break;}finally{value=1;}}'
        'while(false);return value;} public static void Main(){Console.WriteLine(Read(true));} }', 'value')

    # Declaration identity, not spelling, owns the assignment fact.
    expect_unassigned(
        'public static class Program { public static void Main(){int value;{int value=10;Console.WriteLine(value);}'
        'Console.WriteLine(value);} }', 'value')
    expect_unassigned(
        'public static class Program { public static void Main(){int value=10;{int value;Console.WriteLine(value);}} }',
        'value')

    # out establishes assignment; ref/in/plain reads still require prior assignment.
    expect_valid(
        'public static class Program { static void Set(out int x){x=7;} public static void Main(){int value;Set(out value);'
        'Console.WriteLine(value);} }')
    expect_unassigned(
        'public static class Program { static void Touch(ref int x){x=7;} public static void Main(){int value;Touch(ref value);} }',
        'value')
    expect_unassigned(
        'public static class Program { public static void Main(){int value;value+=1;Console.WriteLine(value);} }',
        'value', 'definitely assigned before compound assignment')

    # Capture observes the same source-flow state; a closure cannot read a local that
    # was unassigned at the capture expression.
    expect_unassigned(
        'public static class Program { public static void Main(){int value;Func<int> read=()=>value;value=7;'
        'Console.WriteLine(read());} }', 'value')

    # Exception and structured cleanup paths merge the same ordinary-local bit.
    expect_unassigned(
        'public static class Program { static int Read(bool fail){int value;try{if(fail)throw new Exception("x");value=1;}'
        'catch(Exception e){}return value;} public static void Main(){Console.WriteLine(Read(false));} }', 'value')
    expect_unassigned(
        'using Void; public sealed class R:IDisposable{public void Dispose(){}} public static class Program{'
        'static int Read(bool b){int value;using(R r=new R()){if(b)value=1;}return value;}'
        'public static void Main(){Console.WriteLine(Read(false));}}', 'value')

    # Iterator source analysis rejects a local that remains unassigned across yield.
    expect_unassigned(
        'using Void.Collections; public static class Program { static IEnumerator<int> Values(){int value;yield return 0;'
        'yield return value;} public static void Main(){IEnumerator<int> e=Values();while(e.MoveNext())Console.WriteLine(e.Current);} }',
        'value', 'before it is assigned')

    # Async lowering receives a source binding that is already definitely assigned.
    async_good = (
        'using Void.Threading.Tasks; public static class Program { static async Task<int> Read(){int value;value=7;'
        'await Task.Yield();return value;} public static void Main(){Task<int> pending=Read();} }')
    with tempfile.TemporaryDirectory(prefix='void352-async-') as temporary:
        project = Path(temporary)
        (project / 'Async352.voidproj').write_text(json.dumps({
            'format': 1, 'name': 'Async352', 'output': 'exe', 'version': '0.0.352'}), encoding='utf-8')
        (project / 'Program.void').write_text(async_good, encoding='utf-8')
        result = invoke('build', project)
        expect(result.returncode == 0 and not result.stderr, result.stderr)

    expect_unassigned(
        'using Void.Threading.Tasks; public static class Program { static async Task<int> Read(){int value;'
        'await Task.Yield();return value;} public static void Main(){Task<int> pending=Read();} }', 'value')

    # Human diagnostics retain the established source-facing presentation.
    with tempfile.TemporaryDirectory(prefix='void352-human-') as temporary:
        path = Path(temporary) / 'Program.void'
        path.write_text('public static class Program { public static void Main(){ int value; Console.WriteLine(value); } }',
                        encoding='utf-8')
        result = invoke('check', path)
        expect(result.returncode != 0 and "local 'value' cannot be read before it is definitely assigned" in result.stderr and
               'code: VOID3000' in result.stderr and '| ' in result.stderr and '^' in result.stderr, result.stderr)

    # Milestone/version/count guards are checked by the Make target after this script.


if __name__ == '__main__':
    main()
