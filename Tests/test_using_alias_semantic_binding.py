#!/usr/bin/env python3
"""#353 using-alias semantic binding completion regressions."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests/UsingAliasSemanticBinding'
COUNT = 0


def expect(condition, detail='check failed'):
    global COUNT
    if not condition:
        raise AssertionError(detail)
    COUNT += 1
    print('True')


def invoke(*args, cwd=ROOT, input_text=None):
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=cwd, env=env,
                          input=input_text, capture_output=True, text=True,
                          encoding='utf-8', timeout=180)


def strict_generated(generated: Path):
    with tempfile.TemporaryDirectory(prefix='void353-strict-') as temporary:
        command = shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
            '-I' + str(ROOT / 'Runtime/include')]
        if os.name != 'nt':
            command.append('-pthread')
        command += ['-c', str(generated), '-o', str(Path(temporary) / 'alias.o')]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        expect(result.returncode == 0 and not result.stderr, result.stderr)


def check_source(source: str):
    with tempfile.TemporaryDirectory(prefix='void353-check-') as temporary:
        path = Path(temporary) / 'Program.void'
        path.write_text(source, encoding='utf-8')
        return invoke('check', path)


def expect_invalid(source: str, message: str, code: str | None = None):
    result = check_source(source)
    valid = result.returncode != 0 and message in result.stderr
    if code is not None:
        valid = valid and f'code: {code}' in result.stderr
    expect(valid, result.stderr)


def make_project(directory: Path, name: str, files: dict[str, str]):
    (directory / f'{name}.voidproj').write_text(json.dumps({
        'format': 1, 'name': name, 'output': 'exe', 'version': '0.0.353'
    }), encoding='utf-8')
    for filename, text in files.items():
        (directory / filename).write_text(text, encoding='utf-8')


def lsp_messages(path: Path, requests: list[dict]):
    uri = path.resolve().as_uri()
    text = path.read_text(encoding='utf-8')
    messages = [
        {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
        {'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
            'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': text}}},
    ]
    messages.extend(requests)
    messages.extend([
        {'jsonrpc': '2.0', 'id': 90, 'method': 'shutdown', 'params': None},
        {'jsonrpc': '2.0', 'method': 'exit', 'params': None},
    ])
    wire = ''
    for message in messages:
        encoded = json.dumps(message, separators=(',', ':'))
        wire += f'Content-Length: {len(encoded.encode("utf-8"))}\r\n\r\n{encoded}'
    result = invoke('lsp', input_text=wire)
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    return result.stdout


def main():
    # Positive integration covers: concrete type aliases, constructors, namespace
    # aliases, aliases to generic definitions, generic arguments/method arguments,
    # generic constraints, inheritance/interfaces, static members, alias chains,
    # and canonical target identity.
    result = invoke('run', FIXTURE)
    runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    expect(len(runtime) == 11 and all(line == 'True' for line in runtime), result.stdout)

    generated = FIXTURE / '.void/UsingAliasSemanticBinding.c'
    first = generated.read_bytes()
    result = invoke('build', FIXTURE)
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    expect(generated.read_bytes() == first, 'generated C changed across identical builds')
    strict_generated(generated)

    generated_text = generated.read_text(encoding='utf-8')
    # Alias spelling and the canonical spelling must share one specialization.
    expect(generated_text.count('{"Box__g1_AliasTargets353__dBase",') == 1,
           'alias/full spelling emitted duplicate Box<Base> specialization metadata')
    expect(generated_text.count('"Program.Identity()"') == 1,
           'alias/full spelling emitted duplicate generic method specialization')

    # Alias-to-alias chains are supported; cycles terminate deterministically.
    expect_invalid(
        'using A = B; using B = A; public static class Program { public static void Main(){} }',
        "using alias 'A' participates in a cyclic alias declaration", 'VOID3001')

    # Long valid chains are bounded by the actual alias declarations, not by an
    # arbitrary resolver recursion limit. Using the final alias as a generic
    # argument also exercises the pre-semantic specialization lookup.
    long_chain = ['using A0 = Void.Index;']
    long_chain.extend(f'using A{i} = A{i - 1};' for i in range(1, 71))
    long_chain.append(
        'using Void.Collections; public static class Program { public static void Main(){ '
        'List<A70> values=new List<A70>(); A70 x=new A70(7); values.Add(x); '
        'Console.WriteLine(values[0].Value); } }')
    result = check_source(' '.join(long_chain))
    expect(result.returncode == 0 and not result.stderr, result.stderr)

    # Duplicate aliases and same-scope declaration collisions are explicit rather
    # than discovery-order dependent.
    expect_invalid(
        'using A = Void.Index; using A = Void.Range; public static class Program { public static void Main(){} }',
        "using alias 'A' is already declared in this source scope", 'VOID3008')
    expect_invalid(
        'using A = Void.Index; public class A {} public static class Program { public static void Main(){} }',
        "using alias 'A' conflicts with a type declared in the same scope", 'VOID3008')
    expect_invalid(
        'using N = Void; namespace N { public class C {} } public static class Program { public static void Main(){} }',
        "using alias 'N' conflicts with a namespace declared in the same scope", 'VOID3008')

    # Missing and ambiguous targets are diagnosed at the alias declaration.
    expect_invalid(
        'using MissingAlias = Missing.Target; public static class Program { public static void Main(){} }',
        "using alias 'MissingAlias' target 'Missing.Target' could not be resolved", 'VOID3001')
    expect_invalid(
        'namespace A { public class T {} } namespace B { public class T {} } '
        'using Ambiguous = T; public static class Program { public static void Main(){} }',
        "using alias target 'T' is ambiguous", 'VOID3001')

    # The existing parser stores an alias target as a qualified name, not a type
    # expression. Constructed generic alias declarations therefore remain a scoped
    # parser-level negative rather than expanding #353's grammar.
    expect_invalid(
        'using IntList = Void.Collections.List<int>; public static class Program { public static void Main(){} }',
        "expected ';', found '<'", 'VOID2001')

    # Alias visibility follows the existing source-file using model; it does not
    # leak globally into sibling source files.
    with tempfile.TemporaryDirectory(prefix='void353-scope-') as temporary:
        project = Path(temporary)
        make_project(project, 'Scope353', {
            'AliasOwner.void': (
                'using I = Void.Index; public static class AliasOwner { '
                'public static int Good(){ I x=new I(1); return x.Value; } }'),
            'Program.void': (
                'public static class Program { public static void Main(){ '
                'I x=new I(2); Console.WriteLine(x.Value); } }'),
        })
        result = invoke('check', project)
        expect(result.returncode != 0 and "type 'I' is not supported as a local type" in result.stderr,
               result.stderr)

    # Namespace-local aliases remain visible within their namespace scope.
    result = check_source(
        'namespace N353 { using I = Void.Index; public static class C { '
        'public static int Get(){ I x=new I(4); return x.Value; } } } '
        'public static class Program { public static void Main(){ Console.WriteLine(N353.C.Get()); } }')
    expect(result.returncode == 0 and not result.stderr, result.stderr)

    # A nearer value declaration keeps ordinary expression-name precedence; aliases
    # are not textual substitution.
    result = check_source(
        'using X = Void.Index; public static class Program { public static void Main(){ '
        'int X=4; Console.WriteLine(X); } }')
    expect(result.returncode == 0 and not result.stderr, result.stderr)

    # Tooling consumes the same semantic binding. General completion exposes the
    # alias, member completion sees target statics, hover/definition resolve to the
    # canonical type, and references unify alias and fully-qualified spellings.
    path = FIXTURE / 'Program.void'
    lines = path.read_text(encoding='utf-8').splitlines()
    general_line = 82  # zero-based: source line 83, in Main after declarations
    static_line = 95   # zero-based: source line 96, StaticAlias.Get()
    general_character = len(lines[general_line])
    static_alias_character = lines[static_line].index('StaticAlias') + 1
    static_member_character = lines[static_line].index('Get')
    uri = path.resolve().as_uri()
    output = lsp_messages(path, [
        {'jsonrpc': '2.0', 'id': 2, 'method': 'textDocument/completion', 'params': {
            'textDocument': {'uri': uri},
            'position': {'line': general_line, 'character': general_character}}},
        {'jsonrpc': '2.0', 'id': 3, 'method': 'textDocument/completion', 'params': {
            'textDocument': {'uri': uri},
            'position': {'line': static_line, 'character': static_member_character}}},
        {'jsonrpc': '2.0', 'id': 4, 'method': 'textDocument/hover', 'params': {
            'textDocument': {'uri': uri},
            'position': {'line': static_line, 'character': static_alias_character}}},
        {'jsonrpc': '2.0', 'id': 5, 'method': 'textDocument/definition', 'params': {
            'textDocument': {'uri': uri},
            'position': {'line': static_line, 'character': static_alias_character}}},
        {'jsonrpc': '2.0', 'id': 6, 'method': 'textDocument/references', 'params': {
            'textDocument': {'uri': uri},
            'position': {'line': static_line, 'character': static_alias_character},
            'context': {'includeDeclaration': True}}},
    ])
    expect('"label":"StaticAlias","kind":7,"detail":"AliasTargets353.StaticData"' in output,
           output)
    expect('"label":"TypesAlias","kind":9,"detail":"namespace"' in output, output)
    expect('"id":3,"result":{"isIncomplete":false,"items":[{"label":"Number"' in output and
           '"label":"Get","kind":2,"detail":"int"' in output, output)
    expect('"id":4,"result":{"contents":{"kind":"plaintext","value":"type StaticData: StaticData"' in output,
           output)
    expect('"id":5,"result":{"uri":' in output and
           '"start":{"line":25,"character":18}' in output, output)
    expect('"id":6,"result":[' in output and
           output.count('"start":{"line":94,"character":26}') >= 1 and
           output.count('"start":{"line":95,"character":26}') >= 1 and
           output.count('"start":{"line":96,"character":37}') >= 1, output)

    # The Make target adds the milestone/version/cumulative-count guard.


if __name__ == '__main__':
    main()
