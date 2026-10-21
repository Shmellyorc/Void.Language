"""Stage 8 Sprint 6: source/diagnostic, normal/O2 JIT and linked AOT parity."""
import os,pathlib,subprocess,sys,tempfile
compiler=pathlib.Path(sys.argv[1]).resolve(); clang=sys.argv[2];linker=sys.argv[3]
runtime=pathlib.Path(sys.argv[4]).resolve();fixture=pathlib.Path(sys.argv[5]).resolve();sanitizers=sys.argv[6]=='ON'
flags=['--driver-mode=g++',*(['--ld-path='+linker] if linker else []),*(['-fsanitize=address,undefined'] if sanitizers else [])]
if os.name=='nt': flags+=['-fms-runtime-lib=dll','-llegacy_stdio_definitions']
checks=0

def run(args,status=0):
    global checks
    p=subprocess.run([str(a) for a in args],capture_output=True,text=True,encoding="utf-8",timeout=60);checks+=1
    assert p.returncode==status,(args,p.returncode,p.stdout,p.stderr)
    return p

with tempfile.TemporaryDirectory(prefix='VOID sprint6 ') as directory:
    root=pathlib.Path(directory);source=root/'main.void'
    program=(pathlib.Path(__file__).parent/'sprint6/ergonomics.void').read_text(encoding='utf-8')
    source.write_text(program, encoding='utf-8')
    for opt in [False,True]:
        switches=[] if opt else ['--no-opt']
        for value,want in [(1,28),(2,18),(0,38),(-1,38)]:
            assert run([compiler,'run',source,value,*switches]).stdout.strip()==str(want)
        obj=root/('optimized.o' if opt else 'normal.o');exe=obj.with_suffix('.exe' if os.name=='nt' else '.bin')
        run([compiler,'emit',source,'-o',obj,*switches]);run([clang,*flags,obj,runtime,fixture,'-pthread','-o',exe])
        for value,want in [(1,28),(2,18),(0,38),(-1,38)]:assert run([exe,value]).stdout.strip()==str(want)
    # Preserve physical line ending handling through call and indentation tokens.
    for newline in ['\r\n','\r']:
        source.write_bytes(program.replace('\n',newline).encode('utf-8'))
        assert run([compiler,'run',source,1,'--no-opt']).stdout.strip()=='28'
    # Nested quoted string substitutions share lexical and parser resource guards.
    nested='"seed"'
    for _ in range(8):nested='"{'+nested+'}"'
    source.write_text('const text = '+nested+'\nfunc entry(input: i32) -> i32:\n    if text == "seed":\n        return 7\n    return 90\n', encoding='utf-8')
    assert run([compiler,'run',source,0]).stdout.strip()=='7'
    for _ in range(80):nested='"{'+nested+'}"'
    source.write_text('const text = '+nested+'\n', encoding='utf-8');assert 'limit' in run([compiler,'check',source],1).stderr.lower()
    # Imported qualified types and access-dependent structural relationships.
    (root/'types.void').write_text('interface api:\n    func read() -> i32\nstruct node:\n    value: i32\nfunc (self node) read() -> i32:\n    return self.value\n', encoding='utf-8')
    source.write_text('use "types.void" as lib\nconst yes = lib.node has lib.api\nfunc entry(input: i32) -> i32:\n    if yes:\n        return input\n    return 0\n', encoding='utf-8')
    assert run([compiler,'run',source,7]).stdout.strip()=='7'
    source.write_text('use "types.void" as lib\nstruct wrapper:\n    using lib.node\nfunc entry(input: i32) -> i32:\n    if wrapper has lib.api:\n        return input\n    return 0\n', encoding='utf-8')
    assert run([compiler,'run',source,7]).stdout.strip()=='7'
    (root/'api.void').write_text('interface api:\n    func read() -> i32\n', encoding='utf-8')
    (root/'wrapper.void').write_text('use "types.void" as original\nstruct wrapper:\n    using original.node\n', encoding='utf-8')
    source.write_text('use "wrapper.void" as wrappers\nuse "api.void" as interfaces\nfunc entry(input: i32) -> i32:\n    if wrappers.wrapper has interfaces.api:\n        return 90\n    return 7\n', encoding='utf-8')
    assert run([compiler,'run',source,1]).stdout.strip()=='7'
    negatives=[
      ('return i32 has string','nonnullable'),
      ('var obj = counter()\n    return obj has counter','Unsupported type'),
      ('return counter has i32','nonnullable'),
      ('var s = "{1}"\n    return input','Type mismatch'),
      ('var s = "{true}"\n    return input','Type mismatch'),
      ('var s = "{input # hidden}"\n    return input','delimiter'),
      ('var s = "{}"\n    return input','requires a string'),
      ('var s = "{input:F2}"\n    return input','Expected'),
      ('var s = "}"\n    return input','Unmatched'),
      ('var s = "{\"x\""\n    return input','control'),
      ('var x = if true:\n        1\n    return input','else'),
      ('var x = if 1:\n        1\n    else:\n        2\n    return input','bool'),
      ('var x = if true:\n        1\n    else:\n        "s"\n    return input','Type mismatch'),
      ('var x = if true:\n        1\n        2\n    else:\n        3\n    return input','Expected'),
      ('var x = if true:\n        1\n    else:\n        missing()\n    return input','Unknown'),
      ('const obj = counter()\n    var alias = if false:\n        counter()\n    else:\n        obj\n    alias.count++\n    return input','readonly'),
      ('var x = add(\n        right = 2,\n        1,\n    )\n    return input','Positional'),
      ('var x = add(\n        left = 1,\n        left = 2,\n    )\n    return input','more than once'),
      ('var x = 1 +\n        2\n    return input','expression'),
    ]
    header='struct counter:\n    count: i32\nfunc add(left: i32, right: i32) -> i32:\n    return left + right\nfunc entry(input: i32) -> i32:\n    '
    for body,diagnostic in negatives:
        source.write_text(header+body+'\n', encoding='utf-8');p=run([compiler,'check',source],1)
        assert diagnostic.lower() in p.stderr.lower(),(body,p.stderr)
    source.write_text('interface api:\n    func read() -> i32\nfunc guarded<t>(item: t) -> i32:\n    if t has api:\n        return item.read()\n    else:\n        return missing()\nstruct node:\n    value: i32\nfunc (self node) read() -> i32:\n    return self.value\nfunc entry(input: i32) -> i32:\n    return guarded(node())\n', encoding='utf-8')
    assert 'missing' in run([compiler,'check',source],1).stderr
    source.write_text('interface api:\n    func read() -> i32\nstruct node:\n    value: i32\nfunc (self node) read() -> i32:\n    return 1\nfunc choose(yes = node has api) -> bool:\n    return yes\nfunc entry(input: i32) -> i32:\n    if choose():\n        return 7\n    return 90\n', encoding='utf-8')
    assert run([compiler,'run',source,1,'--no-opt']).stdout.strip()=='7'
    # Multiline header and generic argument syntax remains outside the contract.
    for invalid in ['func f(\n    x: i32\n):\n    pass\n','func f<t,\n    u>(x: t):\n    pass\n']:
        source.write_text(invalid, encoding='utf-8');run([compiler,'check',source],1)
    source.write_text('func main() -> i32:\n    return if true:\n        7\n    else:\n        9\n', encoding='utf-8')
    for switches in [[],['--no-opt']]:assert run([compiler,'run',source,*switches],7).stdout==''
print(f'Sprint 6 process checks passed: {checks}')
