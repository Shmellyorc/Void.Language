"""Application adapter/CLI and independently admitted sink JIT/AOT acceptance."""
import os, pathlib, subprocess, sys, tempfile
compiler, host, clang, linker, runtime, fixture = sys.argv[1:7]
sanitizers=sys.argv[7]=='ON'
flags=['--driver-mode=g++', *(['--ld-path='+linker] if linker else []), *(['-fsanitize=address,undefined'] if sanitizers else [])]
if os.name=='nt': flags+=['-fms-runtime-lib=dll','-llegacy_stdio_definitions']
checks=0

def run(args, status=0, output=None, diagnostic=None):
    global checks
    result=subprocess.run([str(a) for a in args],capture_output=True,text=True,timeout=60);checks+=1
    assert result.returncode==status,(args,result.returncode,result.stdout,result.stderr)
    if output is not None: assert result.stdout==output,(args,result.stdout,result.stderr)
    if diagnostic: assert diagnostic in result.stderr+result.stdout,(args,result.stdout,result.stderr)
    return result

with tempfile.TemporaryDirectory(prefix='VOID Sprint 4 ') as directory:
    root=pathlib.Path(directory);source=root/'main.void'
    def write(text): source.write_text(text,encoding='utf-8')
    def link(obj,exe): run([clang,*flags,obj,runtime,fixture,'-pthread','-o',exe])
    cases=[('var value = 10\nvalue += 5\nreturn value\n',15),
           ('var text = "A🎮\\0"\nif text.length == 3:\n    return 7\nreturn 2\n',7),
           ('var value = 10\n',0),('return\n',0),
           ('if true:\n    return\nreturn 9\n',0),
           ('func main():\n    var value = 10\n',0),
           ('func main() -> i32:\n    return 7\n',7),
           ('return 255\n',255),
           ('return -1\n',4294967295 if os.name=='nt' else 255),
           ('func helper() -> i32:\n    return 3\nvar value = helper()\nreturn value + 2\n',5),
           ('func main() -> i32:\n    var xs: [i32] = [1, 2, 7]\n    return xs[-1]\n',7)]
    for number,(text,status) in enumerate(cases):
        write(text)
        run([compiler,'check',source])
        run([compiler,'run',root],status,'')
        run([compiler,'run',source,'--no-opt'],status,'')
        for mode in ['normal','optimized']:
            obj=root/(f'app{number}-{mode}.obj');exe=root/(f'app{number}-{mode}.exe')
            run([compiler,'emit',source,'-o',obj,*(['--no-opt'] if mode=='normal' else [])])
            link(obj,exe);run([exe],status,'');run([exe,'7'],2,'')
    for text,diagnostic in [
        ('func main() -> i32:\n    if false:\n        return 1\n','Missing return'),
        ('func main() -> string:\n    return "bad"\n','entry'),
        ('func main(value: i32) -> i32:\n    return value\n','entry'),
        ('func main():\n    pass\npass\n','compete'),
        ('return "bad"\n','Type mismatch'),
        ('if true:\n    var local = 1\nfunc helper() -> i32:\n    return local\nreturn helper()\n','local')]:
        write(text);run([compiler,'run',source],1,diagnostic=diagnostic)
    # Imported executable statements reject under check as well as execution.
    (root/'library.void').write_text('var value = 7\npass\n',encoding='utf-8')
    write('use "/library.void" as library\nfunc main():\n    pass\n')
    run([compiler,'check',source],1,diagnostic='Imported modules')
    # Explicit file selection, no arbitrary public function entry scan.
    alternate=root/'alternate.void';alternate.write_text('return 11\n',encoding='utf-8')
    run([compiler,'run',alternate],11,'')
    write('func helper() -> i32:\n    return 7\n');run([compiler,'run',source],1)
    # Existing numeric adapter keeps printed result, status zero and main helper.
    legacy='func main() -> i32:\n    return 99\nfunc entry(value: i32) -> i32:\n    return value + 2\n'
    write(legacy);run([compiler,'run',source,'5'],0,'7\n');run([compiler,'run',source],0,'2\n')
    run([compiler,'run',source,'--app'],2,diagnostic='removed')
    obj=root/'legacy.obj';exe=root/'legacy.exe';run([compiler,'emit',source,'-o',obj]);link(obj,exe);run([exe,'5'],0,'7\n')
    # Invocation errors do not consume a numeric argument as application input.
    write('return 7\n');run([compiler,'run',source,'4'],2);run([compiler,'check',source,'--app'],2)
    run([compiler,'run',source],7,'')
    run([compiler,'--jit-proof','--app'],2,'')
    run([compiler,'--emit-proof',root/'invalid-proof.obj','--app'],2,'')
    # Checked errors retain failure status in both JIT modes and linked AOT.
    write('func main() -> i32:\n    var xs: [i32] = []\n    return xs[0]\n')
    for extra in [[],['--no-opt']]:run([compiler,'run',source,*extra],1,diagnostic='index')
    obj=root/'failure.obj';exe=root/'failure.exe';run([compiler,'emit',source,'-o',obj]);link(obj,exe);run([exe],1,diagnostic='index')
    # A fresh process cache hit preserves application status policy and identity.
    write('return 7\n')
    first=run([compiler,'run',source,'--cache'],7,'')
    second=run([compiler,'run',source,'--cache'],7,'')
    if 'unavailable' not in first.stderr: assert 'miss' in first.stderr and 'hit' in second.stderr,(first.stderr,second.stderr)
    write(legacy);run([compiler,'run',source,'5','--cache'],0,'7\n')
    write('return 7\n');run([compiler,'run',source,'--cache'],7,'')
    # Explicit host admission is required to emit the sink; CLI cannot grant it.
    sink=pathlib.Path(__file__).resolve().parents[2]/'examples/native_sink/main.void'
    run([compiler,'check',sink],1)
    top=root/'sink-top.void'
    text=sink.read_text(encoding='utf-8');prefix,body=text.split('func main() -> i32:\n')
    top.write_text(prefix+'if true:\n'+body,encoding='utf-8')
    for example in [sink,top]:
        for mode in ['normal','optimized']:
            obj=root/f'sink-{example.stem}-{mode}.obj';exe=root/f'sink-{example.stem}-{mode}.exe'
            run([host,'--emit',example,obj,mode]);link(obj,exe);run([exe],7,'')
print(f'Sprint 4 process checks passed: {checks}')
