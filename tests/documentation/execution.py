"""Documentation warnings remain nonfatal through the existing CLI routes."""
from pathlib import Path
import os, subprocess, sys, tempfile
compiler=Path(sys.argv[1]).resolve()
checks=0

def run(args, output=None, warning=False):
    global checks
    result=subprocess.run(list(map(str,args)), capture_output=True, timeout=120)
    assert result.returncode==0,(args,result.returncode,result.stdout,result.stderr)
    if output is not None:assert result.stdout==output,(args,result.stdout)
    assert (b'VOID2022' in result.stderr)==warning,(args,result.stderr)
    if warning:assert b'warning' in result.stderr and b'VOID2023' in result.stderr and b'VOID2024' in result.stderr,result.stderr
    checks+=1

with tempfile.TemporaryDirectory(prefix='VOID nonfatal docs é😀 ') as temp:
    root=Path(temp);source=root/'main.void'
    body='func add(value: i32, extra: i32 = 2) -> i32:\n    return value + extra\nfunc main():\n    print(add(40))\n'
    docs='## Adds a value.\n## @param value First input.\n## @param value Duplicate input.\n## @param typo Unknown input.\n## @return Total.\n'
    for warned in (False,True):
        source.write_text((docs if warned else '')+body,encoding='utf-8')
        run([compiler,'check',source],warning=warned)
        for switches in (['--no-opt'],[]):
            run([compiler,'run',source,*switches],b'42\n',warned)
            obj=root/'program.o'
            run([compiler,'emit',source,'-o',obj,*switches],warning=warned)
            assert obj.stat().st_size>0
            exe=root/('program.exe' if os.name=='nt' else 'program')
            run([compiler,'build',source,'-o',exe,*switches],warning=warned)
            run([exe],b'42\n')
print(f'Documentation execution: {checks} processes passed (check, normal/O2 JIT, emit, linked build and native execution)')
