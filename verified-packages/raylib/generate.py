"""Audit pinned raylib declarations with Clang and emit ordinary native-bind inputs.

This is a header-to-descriptor generator, not a compiler or dependency installer.
Unsupported signatures remain explicit in inventory.json. --check is read-only.
"""
import argparse
from collections import Counter
from hashlib import sha256
import json
from pathlib import Path
import re
import subprocess
from owned_bindings import OWNERS, extend
from callback_bindings import extend as callback_extensions

ROOT = Path(__file__).resolve().parent
HEADER_HASH = '047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b'
SCALARS = {'void': 'void', 'int': 'c_int', 'unsigned int': 'u32',
           'unsigned char': 'u8', 'char': 'c_char', 'float': 'f32', 'double': 'f64', 'bool': 'c_bool', '_Bool': 'c_bool'}
VOID = dict(SCALARS, **{'int': 'i32', 'char': 'i8', 'bool': 'bool', '_Bool': 'bool'})
POD = {'Vector2', 'Vector3', 'Vector4', 'Matrix', 'Color', 'Rectangle',
       'NPatchInfo', 'Camera3D', 'Camera2D', 'Transform', 'Ray', 'RayCollision', 'BoundingBox',
       'BoneInfo', 'VrDeviceInfo', 'VrStereoConfig', 'AutomationEvent'}
# These return borrowed OS addresses or affect ownership in the existing window shim.
FORBIDDEN = {'CloseWindow': 'Window release must consume the application-thread resource ledger entry.',
             'CloseAudioDevice': 'Audio device shutdown must follow sound/music/stream resource cleanup.',
             'InitAudioDevice': 'Audio device needs an owning acquisition and ordered terminal cleanup.'}
RECORD_GAPS = {
    'BoneInfo': 'Inline char array requires an exact C char adapter; char is not a signed/unsigned-byte alias in the descriptor.',
    'VrDeviceInfo': 'Scalar-array record generation is unfinished; the existing descriptor supports these arrays.',
    'VrStereoConfig': 'Inline arrays of Matrix records are not representable by the current scalar-array descriptor shape.',
}
RUNTIME_FUNCTIONS = {'CheckCollisionCircles', 'Fade', 'GetCameraMatrix', 'GetRayCollisionBox'}


def snake(name):
    name = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', name)
    return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name).lower()


def clang_run(clang, *args):
    result = subprocess.run([str(clang), '-x', 'c', '-std=c11', '-fsyntax-only', *args,
                             str(ROOT / 'upstream/raylib.h')], capture_output=True, text=True, check=True)
    return result.stdout


def declaration(text, node):
    span = node['range']
    begin = span['begin'].get('expansionLoc', span['begin'])
    end = span['end'].get('expansionLoc', span['end'])
    return text[begin['offset']:end['offset'] + end['tokLen']] + ';'


def resolve_type(name, aliases):
    while name in aliases:
        other = aliases[name]
        if other == name or other.startswith(('struct ', 'enum ')):
            break
        name = other
    return name


def blocker(node, aliases):
    signature = node['type']['qualType']
    if node['name'] in FORBIDDEN:
        return FORBIDDEN[node['name']]
    if node.get('variadic'):
        return 'Technically unsupported: native-bind admits fixed prototypes only. Exact arbitrary C varargs/va_list calls are not representable; a formatted-message substitute is not this declaration.'
    if any('Callback' in p['type']['qualType'] for p in node.get('inner', []) if p['kind']=='ParmVarDecl'):
        return 'Technically unsupported: G5 callbacks require context-first scalar signatures and a provider unregister-and-drain barrier. This upstream context-free pointer/buffer/va_list callback is outside that ABI; no source callback is registered.'
    types = [signature.split('(', 1)[0].strip(), *[p['type']['qualType'] for p in node.get('inner', []) if p['kind'] == 'ParmVarDecl']]
    failures = []
    for typ in types:
        canonical = resolve_type(typ, aliases)
        if canonical in SCALARS or canonical in POD:
            continue
        if 'Callback' in typ:
            failures.append('Retained context-free callbacks lack a provider unregister-and-drain barrier; G11 hold applies. Pointer/buffer callback parameters are not admitted.')
        elif '*' in canonical or canonical in {'ModelAnimPose', 'va_list', '__builtin_va_list'}:
            failures.append('Pointer input/output needs audited extent, allocation and retention adapters; resource calls cannot combine byte spans or records (src/ir/c_primitive.cpp).')
        else:
            failures.append(RECORD_GAPS.get(canonical,
                f'{typ} requires native resource/borrow adapters; descriptor resources allow one first-position owner, one acquisition name, no record/span combination (src/ir/c_primitive.cpp).'))
    return ' '.join(dict.fromkeys(failures)) or None


def generate(clang):
    header = ROOT / 'upstream/raylib.h'
    raw = header.read_bytes()
    if sha256(raw).hexdigest() != HEADER_HASH:
        raise ValueError('Pinned raylib.h identity mismatch')
    version = subprocess.check_output([str(clang), '--version'], text=True)
    if not re.search(r'\b23\.1\.4\b', version):
        raise ValueError('Use pinned Clang 23.1.4')
    text = raw.decode()
    nodes = json.loads(clang_run(clang, '-Xclang', '-ast-dump=json'))['inner']
    nodes = [n for n in nodes if n.get('loc', {}).get('offset') is not None and not n.get('loc', {}).get('includedFrom')]
    aliases = {n['name']: n['type']['qualType'] for n in nodes if n['kind'] == 'TypedefDecl'}
    layouts = {}
    for block in clang_run(clang, '-Xclang', '-fdump-record-layouts-complete').split('*** Dumping AST Record Layout'):
        match = re.search(r'^\s*0 \| struct (\w+)\n', block, re.M)
        size = re.search(r'\[sizeof=(\d+), align=(\d+)\]', block)
        if not match or not size:
            continue
        fields = [(int(m[1]), m[2].split()[-1].lstrip('*'))
                  for m in re.finditer(r'^\s*(\d+) \|   (\S.*)$', block, re.M)]
        layouts[match[1]] = {'size': int(size[1]), 'alignment': int(size[2]), 'offsets': dict((name, offset) for offset, name in fields)}
    functions = [n for n in nodes if n['kind'] == 'FunctionDecl']
    expected = re.findall(r'^RLAPI\s+.*?\b(\w+)\s*\(', text, re.M)
    if Counter(n['name'] for n in functions) != Counter(expected) or len(expected) != 600:
        raise ValueError('AST/public RLAPI parity failed')
    records = [n for n in nodes if n['kind'] == 'RecordDecl']
    inventory = {'schema': 1, 'raylib_version': '6.0.0', 'header_sha256': HEADER_HASH,
                 'complete_binding': False, 'functions': [], 'records': [], 'enumerations': [], 'typedefs': [], 'macros': []}
    checks = ['/* Generated by generate.py; do not edit. */', '#include <stddef.h>', '#include <stdint.h>', '#include <raylib.h>']
    declarations = ['/* Generated by generate.py; do not edit. */', '#pragma once', '#include "raylib_provider.h"']
    c = ['/* Generated by generate.py; do not edit. */', '#include "raylib_api.h"', '#include "raylib_parity.h"']
    descriptor = []
    def mapped(t):
        array = re.fullmatch(r'(.+?)\s*\[(\d+)\]', t)
        if array:
            base = resolve_type(array[1], aliases)
            if base in SCALARS:
                stride = {'char':1,'unsigned char':1,'int':4,'unsigned int':4,'float':4,'double':8}[base]
                return f'array:{SCALARS[base]}:{array[2]}:{stride}'
            return f'array_record:{snake(base)}:{array[2]}:{layouts[base]["size"]}'
        canonical = resolve_type(t, aliases)
        return SCALARS[canonical] if canonical in SCALARS else 'record:' + snake(canonical)
    for n in records:
        name = n['name']
        complete = n.get('completeDefinition', False)
        layout = layouts.get(name)
        entry = {'name': name, 'c_declaration': declaration(text, n), 'void_name': snake(name) if name in POD else None,
                 'abi': layout, 'implementation': 'record' if name in POD else 'blocked',
                 'ownership': 'plain value' if name in POD or name in RECORD_GAPS else 'native pointer/resource fields require audited owners' if complete else 'opaque upstream type',
                 'blocker': RECORD_GAPS.get(name, 'native resource/borrow adapter is unfinished') if name not in POD else None,
                 'compile_verification': 'native/raylib_parity.h' if complete else 'opaque; sizeof is intentionally unavailable',
                 'test_coverage': 'Clang AST inventory and native layout assertions' if complete else 'inventory parity'}
        inventory['records'].append(entry)
        if not complete:
            continue
        checks.append(f'_Static_assert(sizeof({name}) == {layout["size"]} && _Alignof({name}) == {layout["alignment"]}, "{name} layout");')
        fields = [f for f in n.get('inner', []) if f['kind'] == 'FieldDecl']
        for f in fields:
            offset = layout['offsets'][f['name']]
            checks.append(f'_Static_assert(offsetof({name}, {f["name"]}) == {offset}, "{name}.{f["name"]} offset");')
        if name not in POD:
            continue
        # Color is already in the historical descriptor. Every other POD record
        # keeps its real nested fields and native layout, not opaque handles.
        if name != 'Color':
            descriptor.append(f'record {snake(name)} {name} {layout["size"]} {layout["alignment"]} {len(fields)}')
            for f in fields:
                descriptor.append(f'field {snake(f["name"])} {f["name"]} {mapped(f["type"]["qualType"])} {layout["offsets"][f["name"]]}')
    for n in functions:
        name = n['name']
        typ = n['type']['qualType']
        result = typ.split('(', 1)[0].strip()
        parameters = [p for p in n.get('inner', []) if p['kind'] == 'ParmVarDecl']
        param_types = [p['type']['qualType'] for p in parameters]
        check_params = ', '.join(param_types + (['...'] if n.get('variadic') else [])) or 'void'
        checks.append(f'_Static_assert(_Generic(&{name}, {result} (*)({check_params}): 1, default: 0), "{name} signature");')
        failure = blocker(n, aliases)
        exposed = snake(name) if failure is None else None
        inventory['functions'].append({'name': name, 'c_declaration': declaration(text, n), 'void_name': exposed,
            'native_abi': {'result': mapped(result), 'parameters': list(map(mapped, param_types))} if exposed else None,
            'ownership': 'no transferred ownership; arguments and results are scalar/POD values' if exposed else 'requires per-signature ownership adapter; see blocker and original C declaration',
            'implementation': 'generated thin C forwarding wrapper + voidc native-bind' if exposed else 'blocked',
            'blocker': failure, 'compile_verification': 'native/raylib_parity.h' + ('; native-bind signature/layout assertions' if exposed else ''),
            'test_coverage': 'tests/project/verified_raylib.py; examples/raylib-consumer/src/main.void (real C JIT/AOT computation)' if name in RUNTIME_FUNCTIONS else 'declaration inventory and C signature assertion; not individually runtime-exercised'})
        if failure:
            continue
        symbol = 'vd_raylib_api_' + name
        descriptor.append('bind ' + exposed + ' ' + symbol + ' ' + mapped(result) + (' ' + ' '.join(map(mapped, param_types)) if parameters else ''))
        cparams = ', '.join(p['type']['qualType'] + ' p' + str(i) for i, p in enumerate(parameters)) or 'void'
        # bool is a C keyword supplied by raylib.h on both admitted targets.
        declarations.append(f'VOID_RAYLIB_EXPORT {result} {symbol}({cparams});')
        args = ', '.join('p' + str(i) for i in range(len(parameters)))
        c.append(f'{result} {symbol}({cparams}) {{ VdRaylibCallScope vd_scope __attribute__((cleanup(vd_raylib_call_end)))=vd_raylib_call_begin(); if(!vd_scope.entered) '+('return;' if result=='void' else f'return ({result}){{0}};')+' '+('return ' if result!='void' else '')+f'{name}({args}); }}')
    additional = extend(nodes, inventory, aliases, POD, SCALARS, VOID, snake, resolve_type)
    callbacks=callback_extensions()
    for key in ('declarations','resources','records','bindings','body'):additional[key]+=callbacks[key]
    additional['outcomes'].update(callbacks['outcomes'])
    declarations += additional['declarations']
    c += additional['body']
    first_binding = next((i for i, line in enumerate(descriptor) if line.startswith('bind ')), len(descriptor))
    descriptor = additional['resources'] + descriptor[:first_binding] + additional['records'] + descriptor[first_binding:] + additional['bindings']
    for entry in inventory['functions']:
        if entry['name'] in additional['outcomes']:
            entry.update(additional['outcomes'][entry['name']])
    for entry in inventory['records']:
        if entry['name'] in OWNERS and entry['name'] not in POD:
            entry.update(void_name=snake(entry['name']), implementation='sealed native owner or parent-retained view',
                         ownership='Native struct remains exact C storage. Nested owner/pointer fields are parent-retained, generation-checked readonly views. Internal addresses stay opaque.',
                         blocker=None)
        elif entry['name'] in ('rAudioBuffer','rAudioProcessor'):
            entry.update(void_name='opaque_handle', implementation='parent-retained opaque native borrow',
                         ownership='Incomplete upstream type; no invented layout or numeric address.', blocker=None)
    enum_by_id = {n['id']: n for n in nodes if n['kind'] == 'EnumDecl'}
    def enum_id(n):
        for child in n.get('inner', []):
            if child.get('kind') == 'EnumType':
                return child['decl']['id']
    for n in nodes:
        if n['kind'] != 'TypedefDecl':
            continue
        if '(' in n['type']['qualType']:
            callback_type = declaration(text, n).removeprefix('typedef ').removesuffix(';')
            callback_type = callback_type.replace('(*' + n['name'] + ')', '(*)')
            checks.append(f'_Static_assert(_Generic(({n["name"]})0, {callback_type}: 1, default: 0), "{n["name"]} callback signature");')
        canonical=resolve_type(n['name'], aliases)
        known_record=canonical in POD or canonical in OWNERS
        inventory['typedefs'].append({'name': n['name'], 'c_declaration': declaration(text, n), 'native_abi': n['type']['qualType'],
            'void_name': snake(canonical) if known_record else 'transform_array' if n['name']=='ModelAnimPose' else None,
            'implementation': 'alias to exact native record/owner' if known_record else 'extent-checked transform-array view' if n['name']=='ModelAnimPose' else 'enumerator constants' if enum_id(n) in enum_by_id else 'opaque internal audio pointer' if n['name'] in ('rAudioBuffer','rAudioProcessor') else 'technically unsupported context-free callback; exact C signature asserted' if 'Callback' in n['name'] else 'C compatibility typedef',
            'compile_verification': 'Clang AST; native/raylib_parity.h', 'test_coverage': 'inventory parity'})
        if enum_id(n) not in enum_by_id:
            continue
        en = enum_by_id[enum_id(n)]
        values, value = [], -1
        for f in en.get('inner', []):
            def find_value(x):
                if 'value' in x:
                    return int(x['value'])
                for kid in x.get('inner', []):
                    found = find_value(kid)
                    if found is not None:
                        return found
            explicit = find_value(f)
            value = value + 1 if explicit is None else explicit
            values.append({'name': f['name'], 'value': value, 'void_name': f['name'].lower()})
            checks.append(f'_Static_assert({f["name"]} == {value}, "{f["name"]} value");')
        inventory['enumerations'].append({'name': n['name'], 'c_declaration': declaration(text, en), 'values': values,
            'native_abi': 'C int constants; bitmask values preserved', 'ownership': 'value',
            'implementation': 'generated i32 constants', 'compile_verification': 'native/raylib_parity.h', 'test_coverage': 'all enumerator value assertions'})
    for m in re.finditer(r'^#define\s+(\w+)([^\n]*)', text, re.M):
        name, definition = m[1], m[2].split('//')[0].strip()
        color = re.fullmatch(r'CLITERAL\(Color\)\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}', definition)
        numeric = re.fullmatch(r'\(?([-+]?\d+(?:\.\d*)?)[fF]?\)?', definition)
        implemented = bool(color or numeric or name == 'PI' or name in {'DEG2RAD', 'RAD2DEG', 'RAYLIB_VERSION'})
        inventory['macros'].append({'name': name, 'c_declaration': m[0], 'void_name': name.lower() if implemented else None,
            'native_abi': 'source constant' if implemented else 'C preprocessor configuration/compatibility macro',
            'ownership': 'value', 'implementation': 'generated constant' if implemented else 'C build configuration only',
            'compile_verification': 'pinned header identity; generated values', 'test_coverage': 'inventory parity'})
    # Existing owner, byte-span and scoped callback contracts are deliberately
    # named as gallery helpers. They are not counted as exact raylib API entries.
    inventory['counts'] = {'functions': len(functions), 'exposed_functions': sum(n['void_name'] is not None for n in inventory['functions']),
                           'records': len(records), 'enumerations': len(inventory['enumerations']), 'enumerators': sum(len(n['values']) for n in inventory['enumerations'])}
    for entry in inventory['functions']:
        entry.setdefault('exposure','direct_c_abi' if entry['implementation']=='generated thin C forwarding wrapper + voidc native-bind' else 'ownership_adapter')
    inventory['counts']['exposure_categories']=dict(Counter(entry['exposure'] for entry in inventory['functions']))
    inventory['unsupported_declarations']=[n['name'] for n in inventory['functions'] if n['void_name'] is None]
    inventory['api_accounting_complete']=len(inventory['functions'])==600 and all(n['void_name'] or n['blocker'] for n in inventory['functions'])
    inventory['complete_usable_api']=not inventory['unsupported_declarations']
    inventory['complete_binding']=False
    inventory['complete_binding_semantics']='Legacy exact-C-signature completeness; arbitrary C variadics are not exposed. complete_usable_api includes explicitly identified typed equivalents and lifecycle adapters.'
    inventory['complete_direct_c_abi']=False
    inventory['arbitrary_c_varargs_supported']=False
    return {'inventory.json': json.dumps(inventory, indent=2) + '\n',
            'native/raylib_api.h': '\n'.join(declarations) + '\n', 'native/raylib_api.c': '\n'.join(c) + '\n',
            'native/raylib_parity.h': '\n'.join(checks) + '\n', 'native/raylib_api.descriptor.inc': '\n'.join(descriptor) + '\n'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang', required=True, type=Path)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--require-complete', action='store_true', help='require a usable direct, owned or explicitly adapted interface for every declaration; does not require arbitrary C varargs ABI')
    args = parser.parse_args()
    for name, content in generate(args.clang).items():
        path = ROOT / name
        if args.check:
            if not path.is_file() or path.read_bytes() != content.encode():
                raise SystemExit('Stale generated file: ' + name)
        else:
            path.write_text(content, encoding='utf-8', newline='\n')
    inventory = json.loads((ROOT / 'inventory.json').read_text())
    source = (ROOT / 'src/raylib.void').read_text()
    names = set(re.findall(r'^func (\w+)', source, re.M))
    missing = [n['name'] for n in inventory['functions'] if n['void_name'] not in names]
    if missing: raise SystemExit('Missing physical public API: ' + ', '.join(missing))
    print('Pinned raylib header inventory and generated outputs verified' if args.check else 'Generated pinned raylib inventory and supported bindings')
    if args.require_complete:
        inventory = json.loads((ROOT / 'inventory.json').read_text())
        if not inventory['complete_usable_api']:
            raise SystemExit(f'Incomplete raylib API: {inventory["counts"]["exposed_functions"]}/600 functions exposed; see inventory.json')


if __name__ == '__main__':
    main()
