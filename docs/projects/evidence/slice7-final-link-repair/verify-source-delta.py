"""Preserve the exact patch and verify the empty-list behavior on other platforms."""
from difflib import unified_diff
from pathlib import Path
import json, re, shlex

root=Path.cwd()
work=root/'build/slice7-final-link-repair'
evidence=root/'docs/projects/evidence/slice7-final-link-repair'
opening=json.loads((work/'opening.json').read_text())
paths=set(opening['repair_paths'])-{'docs/projects/SLICE7_FINAL_CLOSEOUT.md'}
tests={
    'ergonomics_sprint6':'tests/stage8/sprint6.py',
    'stage10_fields_aot':'tests/stage10/fields.py',
    'stage10_temporal_aot':'tests/stage10/temporal.py',
    'stage11_globals_aot':'tests/stage11/globals.py',
    'stage10_console_aot':'tests/stage10/console.py',
    'stage10_operators_aot':'tests/stage10/operators.py',
    'stage10_conversion_aot':'tests/stage10/conversion.py',
    'application_native_sprint4':'tests/stage8/sprint4.py',
    'text_collections_sprint3':'tests/stage8/sprint3.py',
}
assert paths=={'CMakeLists.txt',*tests.values()}
before=(work/'before/CMakeLists.txt').read_text()
after=(root/'CMakeLists.txt').read_text()
setting='''  set(legacy_aot_host_libraries "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(legacy_aot_host_libraries ${VOID_HOST_LINK_FILES})
  endif()
'''
assert setting in after
for test,path in tests.items():
    pattern=r'^  add_test\(NAME '+test+r'\s+COMMAND([^)]*)\)'
    old=shlex.split(re.search(pattern,before,re.M)[1])
    new=shlex.split(re.search(pattern,after,re.M)[1])
    assert new==old+['${legacy_aot_host_libraries}'],test
    original=(work/'before'/path).read_text()
    revised=(root/path).read_text()
    empty=re.sub(r'^host_libraries=sys.argv\[\d+:\]\n','',revised,flags=re.M)
    empty=empty.replace(', *host_libraries = ', ' = ')
    empty=empty.replace(' *host_libraries,','').replace('*host_libraries,','')
    assert empty==original,path

with (evidence/'source.patch').open('w') as patch:
    for path in sorted(paths):
        patch.write(f'diff --git a/{path} b/{path}\n')
        patch.writelines(unified_diff((work/'before'/path).read_text().splitlines(keepends=True),
            (root/path).read_text().splitlines(keepends=True),f'a/{path}',f'b/{path}'))
print(json.dumps(dict(registrations=tests, linux_only_archives='VOID_HOST_LINK_FILES',
    non_linux_argv_and_recipes='equivalent with an empty archive list'),indent=2))
