"""Sprint 2 process acceptance: one binder and canonical numeric calls, JIT/AOT."""
import json, os, pathlib, subprocess, sys, tempfile
compiler=pathlib.Path(sys.argv[1]).resolve()
clang=sys.argv[2]
linker=sys.argv[3] if len(sys.argv)>3 else ''
checks=0

def run(args, ok=True):
    global checks
    result=subprocess.run([str(x) for x in args],capture_output=True,text=True)
    checks+=1
    assert result.returncode==(0 if ok else 1),(args,result.returncode,result.stdout,result.stderr)
    return result

binding='''value struct counter:
    value: i32
func (c counter) next() -> i32:
    c.value += 1
    return c.value
func pair(a: i32, b: i32 = 9) -> i32:
    return a * 10 + b
func inferred(step = 0.5) -> f32:
    return step
func generic<t>(left: t, right: t = 0) -> t:
    return left + right
func maximum<t>(value: t = t.max_value) -> t:
    return value
func magnitude<t>(value: t) -> t:
    return t.abs(value).clamp(0, t.max_value)
func (c counter) choose<t>(left: t, right: t = 0) -> t:
    return left + right
func (c counter) combine(a: i32 = 3, b: i32 = 4) -> i32:
    return c.value + a * 10 + b
func entry(input: i32) -> i32:
    var c: counter
    var p = pair(b = c.next(), a = c.next())
    if p != 21 or c.value != 2:
        return 10
    if pair(2) != 29 or pair(2, b = 4) != 24 or inferred() != 0.5:
        return 11
    if generic(right = 2, left = 3) != 5 or generic<i32>(left = 3) != 3:
        return 12
    if c.combine(b = c.next(), a = c.next()) != 47 or c.value != 4:
        return 13
    if maximum<i8>() != 127 or maximum<i16>() != 32767:
        return 17
    if magnitude(-7) != 7:
        return 16
    if c.choose(right = 2, left = 4) != 6 or c.choose<i32>(left = 4) != 4:
        return 15
    if c.next().abs().clamp(0, 3) != 3 or c.value != 5:
        return 14
    return 1
'''

def numeric_source():
    declarations=[];body=[]
    for t in ['i8','u8','i16','u16','i32','u32','i64','u64','f32','f64']:
        declarations += [f'const {t}_max: {t} = {t}.max_value',f'const {t}_min: {t} = {t}.min_value',f'const {t}_abs: {t} = {t}.abs(7)']
        v=-7 if t.startswith(('i','f')) else 7
        body += [f'    var {t}_x: {t} = {v}',f'    if {t}_x.abs() != {t}_abs or {t}.abs(value = {t}_x) != 7:', '        return 20',f'    if {t}.clamp(7, upper = 5, lower = 1) != 5 or {t}_x.abs().clamp(1, 5) != 5:', '        return 21',f'    if {t}_max <= {t}_min:', '        return 22']
        if not t.startswith('f'):
            width=int(t[1:]);signed=t.startswith('i')
            maximum=(1<<(width-int(signed)))-1;minimum=-(1<<(width-1)) if signed else 0
            body += [f'    if {t}_max != {maximum} or {t}_min != {minimum}:', '        return 28']
        if t.startswith('u'):body += [f'    if {t}_min != 0:', '        return 23']
        if t.startswith('f'):
            maximum=((1<<24)-1)<<104 if t=='f32' else ((1<<53)-1)<<971
            body += [f'    if {t}_max != {maximum} or {t}_min != -{maximum}:', '        return 29']
            body += [f'    var {t}_inf: {t} = 1 / (0 as {t})',f'    var {t}_nan: {t} = 0 / (0 as {t})',f'    var {t}_zero: {t} = -0',f'    if {t}_max / {t}_max != 1 or {t}_min != -{t}_max:', '        return 24',f'    if 1 / {t}_zero.abs() != {t}_inf or 1 / {t}_zero.clamp(-1, 1) != -{t}_inf:', '        return 25',f'    if {t}_inf.clamp(-{t}_inf, {t}_inf) != {t}_inf:', '        return 26',f'    var {t}_propagated = {t}.clamp({t}_nan, -{t}_inf, {t}_inf)',f'    if {t}_propagated == {t}_propagated:', '        return 27']
    return '\n'.join(declarations)+'\nfunc entry(input: i32) -> i32:\n'+'\n'.join(body)+'\n    return 1\n'

with tempfile.TemporaryDirectory(prefix='VOID sprint2 ') as temp:
    root=pathlib.Path(temp)
    for name,source in [('binding',binding),('numeric',numeric_source())]:
        path=root/(name+'.void');path.write_text(source)
        run([compiler,'check',path])
        for optimized in [False,True]:
            flags=[] if optimized else ['--no-opt']
            assert run([compiler,'run',path,'0',*flags]).stdout.strip()=='1'
            obj=root/(name+str(optimized)+('.obj' if os.name=='nt' else '.o'))
            exe=root/(name+str(optimized)+('.exe' if os.name=='nt' else '.bin'))
            run([compiler,'emit',path,'-o',obj,*flags])
            link_flags=['-fuse-ld='+linker] if linker else []
            run([clang,*link_flags,obj,'-o',exe])
            assert run([exe,'0']).stdout.strip()=='1'
    # Defaults resolve in the declaration's module, not the caller's scope.
    (root/'helper.void').write_text('const base = 3\nfunc amount(value: i32 = base) -> i32:\n    return value\n')
    imported=root/'imported.void';imported.write_text('use "helper.void" as helper\nfunc entry(value: i32) -> i32:\n    var base = 9\n    return helper.amount() + helper.amount(value = 2)\n')
    for flags in [[],['--no-opt']]: assert run([compiler,'run',imported,'0',*flags]).stdout.strip()=='5'
    readonly=root/'readonly.void';readonly.write_text('func read(const items: [i32], fallback: i32 = 2) -> i32:\n    return items[0] + fallback\nfunc entry(x: i32) -> i32:\n    const items = [1]\n    return read(fallback = 3, items = items)\n')
    run([compiler,'check',readonly])
    staged=root/'staged.void';staged.write_text('struct leaf:\n    value: i32\nstruct owner:\n    child: leaf\n    value: i32\nfunc helper(dummy: i32, target: owner) -> i32:\n    target.value += dummy\n    return target.child.value + target.value\nfunc (target owner) init(child: leaf):\n    target.child = child\n    target.value = helper(target = target, dummy = 3)\nfunc entry(value: i32) -> i32:\n    return owner(leaf(value = value)).value\n')
    for flags in [[],['--no-opt']]: assert run([compiler,'run',staged,'7',*flags]).stdout.strip()=='10'

    invalid=[
        ('func f(x: i32, y: i32 = 2):\n    pass\nfunc entry(x: i32):\n    f(x = 1, 2)\n','Positional'),
        ('func f(x: i32):\n    pass\nfunc entry(x: i32):\n    f(1, x = 2)\n','more than once'),
        ('func f(x: i32):\n    pass\nfunc entry(x: i32):\n    f(y = 1)\n','Unknown parameter'),
        ('func f(x: i32 = 1, y: i32):\n    pass\n','Required parameters'),
        ('func f(x: i32, y: i32 = x):\n    pass\n','Unknown identifier'),
        ('func f(x = null):\n    pass\n','concrete parameter type'),
        ('func f<t>(x = 1):\n    pass\n','explicit'),
        ('func read() -> i32:\n    return 1\nfunc f(x: i32 = read()):\n    pass\n','compile-time constant'),
        ('const bad = i8.abs(i8.min_value)\n','Signed minimum abs'),
        ('func invalid<t>(value: t = t.min_value.abs()) -> t:\n    return value\nfunc entry(x: i32) -> i32:\n    return invalid<i8>(value = 1) as i32\n','Signed minimum abs'),
        ('const bad = f64.clamp(0.0d / 0.0d, 2, 1)\n','Clamp bounds'),
        ('const bad = f32.clamp(1, 0.0f / 0.0f, 2)\n','Clamp bounds'),
        ('func entry(x: i32) -> i32:\n    return i32.abs(1.0)\n','Type mismatch'),
        ('func entry(x: i32):\n    i32.max_value = x\n','Expected a lowercase identifier'),
        ('func (x i32) extension() -> i32:\n    return x\n','Receiver declarations'),
        ('func mutate(items: [i32], amount: i32):\n    items.push(amount)\nfunc entry(x: i32):\n    const items = [1]\n    mutate(amount = 2, items = items)\n','readonly'),
    ]
    for index,(source,diagnostic) in enumerate(invalid):
        path=root/f'invalid{index}.void';path.write_text(source)
        result=run([compiler,'check',path],False)
        assert diagnostic in result.stderr,(source,result.stderr)
    failures=[('i8.abs(i8.min_value)','Signed minimum abs'),('i16.min_value.abs()','Signed minimum abs'),('i32.min_value.abs()','Signed minimum abs'),('i64.min_value.abs()','Signed minimum abs'),('f64.clamp(0.0d / 0.0d, 2, 1)','Clamp bounds'),('f32.clamp(1, 0.0f / 0.0f, 2)','Clamp bounds'),('f64.clamp(1, 0, 0.0d / 0.0d)','Clamp bounds')]
    for index,(expression,diagnostic) in enumerate(failures):
        path=root/f'failure{index}.void';path.write_text(f'func entry(x: i32) -> i32:\n    var result = {expression}\n    return result as i32\n')
        for flags in [[],['--no-opt']]:
            result=run([compiler,'run',path,'0',*flags],False)
            assert diagnostic in result.stderr,(expression,result.stderr)
            if index in [0,4,5]:
                obj=root/f'failure{index}{bool(flags)}.o';exe=root/(f'failure{index}{bool(flags)}'+('.exe' if os.name=='nt' else '.bin'))
                run([compiler,'emit',path,'-o',obj,*flags])
                run([clang,*(['-fuse-ld='+linker] if linker else []),obj,'-o',exe])
                native=run([exe,'0'],False)
                assert diagnostic in native.stderr+native.stdout,(expression,native.stderr,native.stdout)
print(f'Sprint 2: {checks} argument/numeric/diagnostic normal/O2 JIT and linked AOT checks passed')
