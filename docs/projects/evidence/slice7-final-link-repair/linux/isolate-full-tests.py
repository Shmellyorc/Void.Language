"""Keep full-suite output in a task-owned mirror and preserve test identities."""
from pathlib import Path
import json,re,sys

root=Path.cwd()
profile=sys.argv[1]
prior=root/'build/slice7-final-integrated'/f'linux-{profile}'
work=root/'build/slice7-final-link-repair'
mirror=work/f'test-source-{profile}'
build=work/(f'native-linux-{profile}' if sys.argv[2:]==['--native'] else f'linux-{profile}')
registered=build/'CTestTestfile.cmake'
expected={'release':443,'debug':456,'strict':433}[profile]
paths=re.compile(re.escape(root.as_posix())+r'(?=/(?!build/|\.toolchains/)|")')
add=re.compile(r'^add_test\(\[=\[(.*?)\]=\]')
legacy={'ergonomics_sprint6','stage10_fields_aot','stage10_temporal_aot','stage11_globals_aot',
        'stage10_console_aot','stage10_operators_aot','stage10_conversion_aot',
        'application_native_sprint4','text_collections_sprint3'}

def commands(source):
    return {m[1]:line for line in source.splitlines() if (m:=add.match(line))}

original=(prior/'CTestTestfile-original.cmake').read_text()
if build.name.startswith('native-'):
    original=original.replace(str(prior),str(build))
corrected=registered.read_text()
old,new=commands(original),commands(corrected)
assert len(old)==len(new)==expected and old.keys()==new.keys()
cache=(build/'CMakeCache.txt').read_text()
host=Path(re.search(r'^VOID_HOST_DEPS_ROOT:[^=]+=(.*)$',cache,re.M)[1])/'lib'
archives=f' "{host}/libarchive.a" "{host}/libsodium.a"'
for name,line in old.items():
    assert new[name]==(line[:-1]+archives+')' if name in legacy else line), (name,line,new[name])

def properties(source):
    return [line.split(' _BACKTRACE_TRIPLES',1)[0] for line in source.splitlines()
            if line.startswith('set_tests_properties(')]

assert properties(original)==properties(corrected)

output=[]
for line in corrected.splitlines():
    if line.startswith('add_test([=[host_capabilities_dependencies]=]'):
        pass
    elif line.startswith('add_test([=[async_syntax_aot_'):
        line=line.replace(str(root/'tests/async/syntax_aot.py'),str(mirror/'tests/async/syntax_aot.py'))
    else:
        line=paths.sub(mirror.as_posix(),line)
    output.append(line)
output.append(f'set_tests_properties(lsp_collections_stdlib PROPERTIES ENVIRONMENT "VOID_TEST_STDLIB={root}/stdlib")')
isolated='\n'.join(output)+'\n'
assert commands(isolated).keys()==new.keys()
(build/'CTestTestfile-canonical.cmake').write_text(corrected)
registered.write_text(isolated)
print(json.dumps(dict(profile=profile, registered_tests=expected, unchanged_registration_properties=True,
    recipe_changes=sorted(legacy), host_archives=[str(host/'libarchive.a'),str(host/'libsodium.a')],
    source_mirror=str(mirror),
    attested_source_control='host_capabilities_dependencies',
    exact_aot_source_paths='async_syntax_aot_*',
    standard_library_uri_control='lsp_collections_stdlib'),indent=2))
