#!/usr/bin/env python3
"""#350 direct-tree, cross-feature integration audit and permanent regressions."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
COUNT = 0


def check(condition, detail='check failed'):
    global COUNT
    if not condition:
        raise AssertionError(detail)
    COUNT += 1
    print('True')


def invoke(*args, cwd=ROOT, env=None):
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd, env=env,
                          capture_output=True, text=True, encoding='utf-8', timeout=180)


def project(root, name, source, unsafe=False):
    root.mkdir(parents=True, exist_ok=True)
    (root / (name + '.voidproj')).write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.350',
        'compiler': {'unsafe': unsafe}}), encoding='utf-8')
    path = root / 'Program.void'
    path.write_text(source, encoding='utf-8')
    return path


def binary(root, name, publish=False):
    return root / ('publish' if publish else 'bin') / (name + ('.exe' if os.name == 'nt' else ''))


def frames(stderr):
    check('Stack trace:\n' in stderr, stderr)
    return [line.strip().split(' in ', 1)[0][3:]
            for line in stderr.split('Stack trace:\n', 1)[1].splitlines()
            if line.startswith('   at ')]


def rich_failure(result, reason, names, expression=None):
    check(result.returncode == 1, result)
    check(result.stderr.startswith(reason + '\n'), result.stderr)
    check(frames(result.stderr) == names, result.stderr)
    origin = [line.strip() for line in result.stderr.split('Stack trace:', 1)[0].splitlines()
              if line.startswith('   at ')]
    top = [line.strip() for line in result.stderr.split('Stack trace:', 1)[1].splitlines()
           if line.startswith('   at ')][0]
    check(origin == [top], result.stderr)
    check(not any(token in result.stderr for token in
                  ('__gm', '__g1', '__lambda', '__sm', '__voidc$', 'vc_fn_')), result.stderr)
    if expression:
        check(expression in result.stderr, result.stderr)
        check(result.stderr.index(expression) < result.stderr.index('Stack trace:'), result.stderr)
        marker = [line for line in result.stderr.splitlines() if '| ' in line and '^' in line]
        check(len(marker) == 1 and marker[0].count('~') >= len(expression) - 1, result.stderr)


def strict_generated(root, name, temp):
    generated = root / '.void' / (name + '.c')
    before = generated.read_bytes()
    result = invoke('build', root)
    check(result.returncode == 0, result.stderr)
    check(generated.read_bytes() == before, 'nondeterministic generated C: ' + name)
    result = subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
        '-I' + str(ROOT / 'Runtime/include'), '-pthread', '-c', str(generated),
        '-o', str(temp / (name + '.o'))], capture_output=True, text=True, timeout=180)
    check(result.returncode == 0 and not result.stderr, result.stderr)


def lsp(text, path, positions):
    uri = path.resolve().as_uri()
    messages = [{'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
                {'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
                    'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': text}}}]
    for identifier, method, offset in positions:
        pos = {'line': text[:offset].count('\n'), 'character': offset - text.rfind('\n', 0, offset) - 1}
        params = {'textDocument': {'uri': uri}, 'position': pos}
        if method == 'references':
            params['context'] = {'includeDeclaration': True}
        messages.append({'jsonrpc': '2.0', 'id': identifier, 'method': 'textDocument/' + method, 'params': params})
    messages.extend([{'jsonrpc': '2.0', 'id': 99, 'method': 'shutdown', 'params': None},
                     {'jsonrpc': '2.0', 'method': 'exit', 'params': None}])
    payload = b''
    for message in messages:
        body = json.dumps(message).encode()
        payload += b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body
    result = subprocess.run([str(VOIDC), 'lsp'], cwd=ROOT, input=payload, capture_output=True, timeout=180)
    check(result.returncode == 0 and not result.stderr, result.stderr)
    output = result.stdout
    responses = {}
    while output:
        header, separator, content = output.partition(b'\r\n\r\n')
        assert separator, output
        length = int(header.split(b': ')[1])
        message = json.loads(content[:length])
        output = content[length:]
        if 'id' in message:
            responses[message['id']] = message
    check(all(identifier in responses and 'error' not in responses[identifier]
              for identifier, _, _ in positions), responses)
    return responses


def main():
    with tempfile.TemporaryDirectory(prefix='void350-audit-') as temporary:
        temp = Path(temporary)
        out_name = 'OutVariableIntegrationAudit'
        out = ROOT / 'Tests' / out_name
        result = invoke('run', out)
        check(result.returncode == 0 and not result.stderr, result.stderr)
        runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
        check(len(runtime) == 20 and all(line == 'True' for line in runtime), result.stdout)
        strict_generated(out, out_name, temp)

        # Completion must discover the same bound declarations as compilation,
        # even through array/object initializers, dimensions and switch scrutinees.
        path = out / 'Program.void'
        text = path.read_text(encoding='utf-8')
        queries = []
        uses = [('arrayValue', 'arrayValue == typedValue', 'int'),
                ('switchValue', 'switchValue == 7', 'int'),
                ('collectionValue', 'number > collectionValue', 'int'),
                ('scrutineeValue', 'scrutineeValue == 7', 'int'),
                ('dimensionValue', 'sized.Length == dimensionValue', 'int'),
                ('initializerValue', 'initialized.Value == initializerValue', 'int'),
                ('nestedGeneric', 'nestedGeneric.Value != null', 'Box<Box<int>>')]
        for index, (name, needle, _) in enumerate(uses):
            offset = text.index(needle) + needle.index(name)
            queries.extend([(2 + index * 3, 'hover', offset), (3 + index * 3, 'completion', offset),
                            (4 + index * 3, 'definition', offset)])
        responses = lsp(text, path, queries)
        for index, (name, _, kind) in enumerate(uses):
            hover = responses[2 + index * 3]['result']
            check(hover['contents']['value'] == f'local {name}: {kind}', hover)
            items = {item['label']: item for item in responses[3 + index * 3]['result']['items']}
            check(name in items and items[name]['detail'] == kind, (name, items.get(name)))
            check('__g' not in json.dumps(list(items.values())), items)
            check(responses[4 + index * 3]['result']['uri'] == path.as_uri(), responses)
        reference_offset = text.index('arrayValue == typedValue')
        refs = lsp(text, path, [(2, 'references', reference_offset)])[2]['result']
        check(len(refs) == 3, refs)  # declaration, direct use, captured lambda

        pre = 'public static class Program { static bool P(out int x){x=7;return true;} public static void Main(){'
        # Both inferred and target-typed conditional paths must reject reads when
        # one arm can be skipped; the other arm cannot read a skipped declaration.
        for index, body in enumerate([
            'bool yes=false; bool ok=yes?P(out var x):false; Console.WriteLine(x);',
            'bool yes=true; var ok=yes?false:P(out int x); Console.WriteLine(x);',
            'bool yes=false; bool ok=yes?P(out var x):x>0;',
            'bool yes=true; var ok=yes?x>0:P(out var x);',
            'bool ok=P(out var a)||P(out var b); Console.WriteLine(b);',
            'if(P(out var a)||P(out var b)){Console.WriteLine(b);}',
            'bool ok=true switch {true=>P(out var x),_=>false}; Console.WriteLine(x);',
            'bool ok=false&&P(out var x); for(int i=0;i<0;){x=7;break;} Console.WriteLine(x);',
            'bool flag=false;bool ok=flag&&P(out var x); for(int i=0;i<1;Console.WriteLine(x)){if(flag)continue;x=7;i++;}',
        ]):
            root = temp / ('Negative' + str(index))
            source = project(root, root.name, pre + body + '}}')
            result = invoke('check', source, '--diagnostics=json')
            records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
            check(result.returncode != 0 and len(records) == 1, result.stderr)
            check(records[0]['code'].startswith('VOID300'), records)
            check('definitely assigned' in records[0]['message'] or 'unknown identifier' in records[0]['message'], records)
            check(records[0].get('span') is not None, records)

        for name, body in {
            'ForRefOutRead': 'bool run=false; bool ok=run&&P(out var x); int other=2; ref int alias=ref x; for(int i=0;run;i++){alias=ref other;run=false;}Console.WriteLine(alias);',
            'WhileRefOutRead': 'bool run=false; bool ok=run&&P(out var x); int other=2; ref int alias=ref x; while(run){alias=ref other;run=false;}Console.WriteLine(alias);',
            'SwitchSkippedAssignment': 'bool ok=false&&P(out var x); bool r=true switch {false=>P(out x),_=>false}; Console.WriteLine(x);',
            'SwitchSiblingAssignment': 'bool ok=false&&P(out var x); int r=true switch {false=>P(out x)?1:0,_=>x}; Console.WriteLine(r);',
        }.items():
            root = temp / name
            project(root, name, pre + body + '}}')
            result = invoke('check', root, '--diagnostics=json')
            check(result.returncode == 1, result)
            records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
            check('definitely assigned' in records[0]['message'], records)

        positive = temp / 'ConditionalGuards'
        source = project(positive, positive.name, pre + '''
            var result=(P(out var a)&&P(out var b)) ? a+b : 0;
            bool guarded=true switch {true when P(out var c)&&P(out var d)=>c==d,_=>false};
            Console.WriteLine(result==14 && guarded);
        }}''')
        result = invoke('run', positive)
        check(result.returncode == 0 and result.stdout.splitlines()[-1] == 'True', result.stderr)
        strict_generated(positive, positive.name, temp)
        guard_text = source.read_text()
        responses = lsp(guard_text, source, [(2, 'completion', guard_text.index('=>c==d') + 3),
                                           (3, 'completion', guard_text.index('_=>false') + 3)])
        check({'c', 'd'} <= {item['label'] for item in responses[2]['result']['items']}, responses)
        check(not ({'c', 'd'} & {item['label'] for item in responses[3]['result']['items']}), responses)

        assigned = temp / 'SwitchAllAssigned'
        project(assigned, assigned.name, pre + 'bool ok=false&&P(out var x); Console.WriteLine(!ok); bool r=true switch {false=>P(out x),_=>P(out x)}; Console.WriteLine(r&&x==7);}}')
        result = invoke('run', assigned)
        check(result.returncode == 0 and result.stdout.splitlines()[-1] == 'True', result.stderr)
        strict_generated(assigned, assigned.name, temp)

        # Loop increments receive assignments from fallthrough/continue while
        # the zero-iteration exit cannot inherit body-only assignments.
        loop = temp / 'ForIncrement'
        project(loop, loop.name, pre + "bool ok=false&&P(out var x); Console.WriteLine(!ok); for(int i=0;i<1;Console.WriteLine(x==9)){x=9;i++;}" + '}}')
        result = invoke('run', loop)
        check(result.returncode == 0 and result.stdout.splitlines()[-1] == 'True', result.stderr)
        strict_generated(loop, loop.name, temp)

        # Inline scalar storage shares ordinary setjmp/longjmp protection and
        # stays usable through a later ordinary out call with the same ABI.
        scalar = temp / 'ScalarUnwind'
        project(scalar, scalar.name, '''public static class Program {
            static void Set(out int x){x=1;}
            public static void Main(){Set(out var x);
                try{x=9;throw new Exception("handled");}catch(Exception e){Console.WriteLine(x==9);}
                Set(out x);Console.WriteLine(x==1);}}''')
        result = invoke('run', scalar)
        check(result.returncode == 0 and result.stdout.splitlines()[-2:] == ['True', 'True'], result.stderr)
        strict_generated(scalar, scalar.name, temp)
        generated = (scalar / '.void' / (scalar.name + '.c')).read_text()
        check('int32_t volatile vc_l_' in generated, 'inline scalar missed established unwind protection')

        indirect = temp / 'IndirectScalarUnwind'
        project(indirect, indirect.name, '''public enum Flag {Initial,Changed} public static class Program {
            static void Set(int value,out int x){x=value;}
            static void Change(int value,ref int x){x=value;}
            static void SetFlag(out Flag x){x=Flag.Changed;}
            public static void Main(){Set(1,out var inferred);Set(1,out int typed);int ordinary=1;Flag flag=Flag.Initial;
                try{Set(9,out inferred);Change(8,ref typed);Set(7,out ordinary);SetFlag(out flag);throw new Exception("handled");}
                catch(Exception e){Console.WriteLine(inferred==9 && typed==8 && ordinary==7 && flag==Flag.Changed);}}}''')
        result = invoke('run', indirect)
        check(result.returncode == 0 and result.stdout.splitlines()[-1] == 'True', result.stderr)
        strict_generated(indirect, indirect.name, temp)
        generated = (indirect / '.void' / (indirect.name + '.c')).read_text()
        check(generated.count('int32_t volatile vc_l_') >= 3 and
              re.search(r'\bvc_e_\d+ volatile vc_l_\d+', generated),
              'ref/out mutation missed established scalar unwind protection')

        # Completion remains visible in trailing loop-body whitespace and ends
        # at the real closing delimiter, rather than at the last statement token.
        scope = temp / 'Scope'
        scope_text = pre + "while(P(out var scoped)){Console.WriteLine(scoped);\n    \n}\nConsole.WriteLine(1);}}"
        scope_path = project(scope, scope.name, scope_text)
        positions = [(2, 'completion', scope_text.index('    \n') + 2),
                     (3, 'completion', scope_text.index('Console.WriteLine(1)'))]
        responses = lsp(scope_text, scope_path, positions)
        check(any(item['label'] == 'scoped' for item in responses[2]['result']['items']), responses)
        check(all(item['label'] != 'scoped' for item in responses[3]['result']['items']), responses)

        shadow = temp / 'ShadowScope'
        shadow_pre = pre.replace('public static void Main()', 'static void Text(out string x){x="inner";} public static void Main()')
        shadow_text = shadow_pre + 'int value=1; {Text(out var value); Console.WriteLine(value);} {Text(out var gone);} Console.WriteLine(value);}}'
        shadow_path = project(shadow, shadow.name, shadow_text)
        inner = shadow_text.index('Console.WriteLine(value)') + len('Console.WriteLine(')
        outer = shadow_text.rindex('Console.WriteLine(value)') + len('Console.WriteLine(')
        responses = lsp(shadow_text, shadow_path, [(2, 'hover', inner), (3, 'completion', inner),
                                                   (4, 'hover', outer), (5, 'completion', outer)])
        check(responses[2]['result']['contents']['value'] == 'local value: string', responses)
        check(responses[4]['result']['contents']['value'] == 'local value: int', responses)
        inner_items = {item['label']: item for item in responses[3]['result']['items']}
        outer_items = {item['label']: item for item in responses[5]['result']['items']}
        check(inner_items['value']['detail'] == 'string', inner_items)
        check(outer_items['value']['detail'] == 'int' and 'gone' not in outer_items, outer_items)

        # Legal pointer storage uses ordinary unsafe/out rules, without inference defaults.
        pointers = temp / 'PointerOut'
        project(pointers, pointers.name, '''public static unsafe class Program {
            static unsafe void Pointer(out int* value){value=null;}
            static unsafe void Mutate(int* original){try{Pointer(out original);throw new Exception("handled");}
                catch(Exception e){Console.WriteLine(original==null);}}
            public static unsafe void Main(){Pointer(out int* typed);Pointer(out var inferred);Pointer(out _);
                Console.WriteLine(typed==inferred);int* memory=stackalloc int[1];Mutate(memory);}}''', unsafe=True)
        result = invoke('run', pointers)
        check(result.returncode == 0 and result.stdout.splitlines()[-2:] == ['True', 'True'], result.stderr)
        strict_generated(pointers, pointers.name, temp)
        check('int32_t * volatile vc_mp_' in (pointers / '.void' / (pointers.name + '.c')).read_text(),
              'pointer parameter needs volatile pointer storage, preserving its pointee type')

        app_name = 'RuntimeDiagnosticsApplicationAudit'
        app = ROOT / 'Tests' / app_name
        result = invoke('run', app)
        rich_failure(result, 'VOID runtime error: array index out of range',
                     ['Box<Box<int>>.Read()', 'Pipeline.Invoke()', 'Runner.Run.<lambda>()',
                      'Runner.Run()', 'Program.Main()'], '_values[index]')
        check('help: index 8 is outside the valid range for an array dimension of length 1.' in result.stderr, result.stderr)
        check('Pipeline.Old()' not in result.stderr and 'Pipeline.Success()' not in result.stderr, result.stderr)
        strict_generated(app, app_name, temp)

        thread_name = 'RuntimeDiagnosticsThreadSourceAudit'
        thread = ROOT / 'Tests' / thread_name
        result = invoke('run', thread)
        rich_failure(result, 'VOID runtime error: array index out of range',
                     ['Program.Crash()', 'Program.Worker()', 'Program.Main.<lambda>()'], 'values[index]')
        check('at Program.Main()' not in result.stderr and 'thread-handled-350' not in result.stderr, result.stderr)
        strict_generated(thread, thread_name, temp)

        # A separate temporary source allows missing-source and publish checks
        # without touching authoritative permanent source files.
        relocated = temp / 'Source'
        original = project(relocated, 'Source', (app / 'Program.void').read_text())
        built = invoke('build', '.', cwd=relocated)
        check(built.returncode == 0, built.stderr)
        decoy = temp / 'ChangedCwd'
        decoy.mkdir()
        (decoy / 'Program.void').write_text('DECOY-350\n')
        moved = decoy / binary(relocated, 'Source').name
        shutil.copy2(binary(relocated, 'Source'), moved)
        rich = subprocess.run([str(moved)], cwd=decoy, capture_output=True, text=True, timeout=180)
        rich_failure(rich, 'VOID runtime error: array index out of range',
                     ['Box<Box<int>>.Read()', 'Pipeline.Invoke()', 'Runner.Run.<lambda>()', 'Runner.Run()', 'Program.Main()'], '_values[index]')
        check('DECOY-350' not in rich.stderr, rich.stderr)
        original.rename(original.with_suffix('.saved'))
        result = subprocess.run([str(moved)], cwd=decoy, capture_output=True, text=True, timeout=180)
        rich_failure(result, 'VOID runtime error: array index out of range',
                     ['Box<Box<int>>.Read()', 'Pipeline.Invoke()', 'Runner.Run.<lambda>()', 'Runner.Run()', 'Program.Main()'])
        check('| ' not in result.stderr and 'DECOY-350' not in result.stderr, result.stderr)
        original.with_suffix('.saved').rename(original)
        built = invoke('publish', '.', cwd=relocated)
        check(built.returncode == 0, built.stderr)
        result = subprocess.run([str(binary(relocated, 'Source', True))], cwd=decoy,
                                capture_output=True, text=True, timeout=180)
        rich_failure(result, 'VOID runtime error: array index out of range',
                     ['Box<Box<int>>.Read()', 'Pipeline.Invoke()', 'Runner.Run.<lambda>()', 'Runner.Run()', 'Program.Main()'])
        check('| ' not in result.stderr, result.stderr)

        # Out local storage in a throw operand must be materialized before throwing.
        thrown = temp / 'ThrowOut'
        project(thrown, thrown.name, '''public sealed class E:Exception { public E(out int x){x=1;} }
            public static class Program {public static void Main(){throw new E(out var value);}}''')
        result = invoke('run', thrown)
        rich_failure(result, 'Unhandled E: Exception', ['Program.Main()'], 'throw new E(out var value);')
        strict_generated(thrown, thrown.name, temp)

        # Fatal failure after handled A owns only its current synchronous context.
        fatal = temp / 'FatalAfterHandled'
        project(fatal, fatal.name, '''using Void; public static class Program {
            static void Old(){throw new Exception("stale-350");}
            static void Fatal(){Runtime.Fail("fatal-350");}
            public static void Main(){try{Old();}catch(Exception e){GC.Collect();}
                try{Fatal();}catch(Exception e){Console.WriteLine("incorrectly-caught");}}}''')
        result = invoke('run', fatal)
        rich_failure(result, 'VOID runtime error: fatal-350', ['Program.Fatal()', 'Program.Main()'], 'Runtime.Fail("fatal-350")')
        check('stale-350' not in result.stderr and 'incorrectly-caught' not in result.stdout, result)
        strict_generated(fatal, fatal.name, temp)

        # Long captured stacks remain complete through repeated bare rethrow.
        deep = temp / 'DeepRethrow'
        project(deep, deep.name, '''public static class Program {
            static void Recurse(int depth){if(depth<0)return;if(depth>0){Recurse(depth-1);return;}throw new Exception("deep-350");}
            static void Inner(){try{Recurse(80);}catch(Exception e){throw;}}
            static void Outer(){try{Inner();}catch(Exception e){throw;}}
            public static void Main(){Outer();}}''')
        result = invoke('run', deep)
        rich_failure(result, 'Unhandled Exception: deep-350',
                     ['Program.Recurse()'] * 81 + ['Program.Inner()', 'Program.Outer()', 'Program.Main()'], 'throw new Exception("deep-350");')
        strict_generated(deep, deep.name, temp)

        if os.name != 'nt':
            import signal
            native = temp / 'NativeAfterHandled'
            project(native, native.name, '''public static class Native {
                [Native("raise")] public static unsafe extern int Raise(int signal);}
                public static class Program {static void Old(){throw new Exception("stale-native-350");}
                public static unsafe void Main(){try{Old();}catch(Exception e){}
                int.TryParse("1",out var parsed);Native.Raise(SIGNAL+parsed-1);}}'''.replace('SIGNAL', str(int(signal.SIGSEGV))), unsafe=True)
            result = invoke('run', native)
            check(result.returncode == 1 and result.stderr.startswith('Native process crash:'), result.stderr)
            check('SIGSEGV' in result.stderr, result.stderr)
            check(all(word not in result.stderr for word in ['VOID runtime error:', 'stale-native-350', 'Stack trace:', 'Program.void:', 'VOID4000']), result.stderr)

            ordinary = temp / 'OrdinaryExit'
            project(ordinary, ordinary.name, '''public static class Native {
                [Native("exit")] public static unsafe extern void Exit(int code);}
                public static class Program {public static unsafe void Main(){Native.Exit(7);}}''', unsafe=True)
            result = invoke('run', ordinary)
            check(result.returncode == 7 and not result.stderr, result)

    check(invoke('version').stdout.strip() == 'voidc 0.0.370')
    print(f'#350 focused assertions: {COUNT}')


if __name__ == '__main__':
    main()
