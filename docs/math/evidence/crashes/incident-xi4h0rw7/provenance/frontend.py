"""Public exact primitive typing, ordinary binding, defaults and private isolation."""
from pathlib import Path
import sys
import os
import tempfile
import bootstrap as h

compiler=Path(sys.argv[1]).resolve();small='--small-stack' in sys.argv
core={'abs':1,'min':2,'max':2,'clamp':3,'sign':1}
floating={'is_nan':1,'is_infinite':1,'is_finite':1,'floor':1,'ceil':1,'trunc':1,'round':1,'lerp':3,'inverse_lerp':3,'sin':1,'cos':1,'tan':1,'asin':1,'acos':1,'atan':1,'atan2':2,'sqrt':1,'pow':2,'exp':1,'exp2':1,'log':1,'log2':1,'radians':1,'degrees':1}
all_functions={**core,**floating}
with tempfile.TemporaryDirectory(prefix='VOID math frontend ') as directory:
    work=Path(directory);serial=0
    def check(body,valid=True,prelude='use math\n',diagnostic=None,filename=None):
        global serial
        serial+=1;source=work/(filename or f'case{serial}.void')
        source.write_text(prelude+'func main():\n'+''.join('    '+line+'\n' for line in body.splitlines()))
        result=h.run([compiler,'check',source],work,status=0 if valid else 1,small=small,sources=(source,))
        if diagnostic:
            assert diagnostic.encode() in result.stderr,(diagnostic,result.stderr)
            assert source.resolve().as_posix().encode('utf-8')+b':' in result.stderr,result.stderr
        return source
    for target in ('i8','u8','i16','u16','i32','u32','i64','u64','f32','f64'):
        declarations=[f'var input: {target} = 1']
        for fn,arity in (all_functions if target.startswith('f') else core).items():
            args=', '.join(['input']*arity)
            result='bool' if fn.startswith('is_') else target
            declarations += [f'var {fn}_inferred: {result} = math.{fn}({args})',f'var {fn}_explicit: {result} = math.{fn}<{target}>({args})']
        check('\n'.join(declarations))
        # Test every float-only function independently for every integer width.
        if not target.startswith('f'):
            for fn,arity in floating.items():check(f'var input: {target} = 1\nvar bad = math.{fn}('+', '.join(['input']*arity)+')',False,diagnostic='requires exactly f32 or f64')
    # Explicit f64 contextual literals retain f64 precision, inference stays f32.
    check('var small: f32 = math.sin(0.5)\nvar wide: f64 = math.sin<f64>(0.5)\nvar root: f32 = math.sqrt<f32>(9)\nvar grid: i32 = math.floor(3.75) as i32\nconst local = math.sqrt(9.0)\nvar callable = math.sin<f64>\nvar answer: f64 = callable(0.5)')
    check('var wrong: f64 = math.sin(0.5)',False)
    check('var wrong = math.sqrt(9)',False)
    for fn,arity in all_functions.items():
        check(f'var single: f32 = 1\nvar wide: f64 = 1\nvar result = math.{fn}('+', '.join(['single']*(arity-1)+['wide'])+')',False) if arity>1 else None
        check('var wrong: i32 = math.'+fn+'('+', '.join(['1.0']*arity)+')',False)
        check('var result = math.'+fn+'('+', '.join(['true']*arity)+')',False)
        check('var result = math.'+fn+'<bool>('+', '.join(['true']*arity)+')',False)
    for body in ('var a = math.min(1, 2, 3)','var a = math.sin()','var a = math.pow(2.0)','var a = math.sin<f32, f64>(1.0)','var a = math.clamp(value = 1, lower = 0, lower = 2)','var a = math.atan2(x = 1.0, wrong = 2.0)','var a = math.pi','var a = math._pi<f64>()','var a = _math_sin<f64>(1.0)','var a = math._math_sin<f64>(1.0)','var a = m._math_sin(1.0)'):
        check(body,False,prelude='use math\nuse math as m\n')
    # Collections, nominal types and enums are independently rejected by every core.
    nominal='''use math
value struct number:
    value: i32
    operator +(const left: number, const right: number) -> number:
        return number(value = left.value + right.value)
    operator -(const left: number, const right: number) -> number:
        return number(value = left.value - right.value)
    operator -(const item: number) -> number:
        return number(value = -item.value)
    operator <(const left: number, const right: number) -> bool:
        return left.value < right.value
    operator >(const left: number, const right: number) -> bool:
        return left.value > right.value
    operator ==(const left: number, const right: number) -> bool:
        return left.value == right.value
enum flag:
    first
'''
    # Establish that custom operators themselves are valid before rejecting Math.
    check('var a = number(value = 1)\nvar b = -a + a\nvar c = a < b\nvar d = a > b\nvar e = a == b',prelude=nominal)
    for fn,arity in core.items():
        for expression in ('number(value = 1)','flag.first','[1, 2]','"one"'):
            check('var item = '+expression+'\nvar result = math.'+fn+'('+', '.join(['item']*arity)+')',False,prelude=nominal,diagnostic='requires a numeric primitive')
    # Runtime wrappers remain unavailable to module constants and pure defaults.
    for declaration in ('const value: f64 = math.sqrt<f64>(4)','func default(value: f64 = math.floor<f64>(1.5)):\n    pass','value struct item:\n    value: f64 = math.sin<f64>(0)'):
        source=work/f'default{serial}.void';serial+=1;source.write_text('use math\n'+declaration+'\n')
        h.run([compiler,'check',source],work,status=1,small=small)
    check('var value: i32 = intrinsic',prelude='const intrinsic: i32 = i32.abs(-7)\n')
    # A user module sharing the basename receives no compiler-owned privilege.
    library=work/'libs';library.mkdir()
    (library/'math.void').write_text('func min<t>(left: t, right: t) -> t:\n    return left\nfunc sin<t>(value: t) -> t:\n    return value\n')
    check('var a = mine.min(true, false)\nvar b = mine.sin("valid")',prelude='use "./libs/math.void" as mine\n')
    check('var a = mine.sin(1.0)',False,prelude='use "./libs/math.void" as mine\nfunc stolen(value: f32) -> f32:\n    return _math_sin(value)\n')
    # Matching top-level filename is insufficient to gain private hooks.
    check('var value = _math_sin(1.0)',False,prelude='',filename='math.void')
    check('var a: f64 = m.atan2<f64>(x = -1.0, y = 1.0)\nvar b: f64 = m.clamp<f64>(upper = 1.0, value = 0.5, lower = 0.0)',prelude='use math as m\n')
    # Exercise private calls from an isolated canonical stdlib, independently of
    # the shipped facade. User modules cannot create this identity by basename.
    isolated=work/'canonical-stdlib';isolated.mkdir()
    private_module=isolated/'math.void';private_entry=work/'private-entry.void'
    env=dict(os.environ);env['VOID_STDLIB_ROOT']=str(isolated)
    private_cases=[
        ('_math_sin<t>(value)','f32',True),('_math_sin<t>(value)','f64',True),
        ('_math_sin<t>(value)','i32',False),('_math_sin(value)','f32',False),
        ('_math_sin<t, t>(value)','f32',False),('_math_sin<t>()','f32',False),
        ('_math_sin<t>(value, value)','f32',False),('_math_sin<t>(wrong = value)','f32',False),
        ('_math_pow<t>(value, 1 as f64)','f32',False),('_math_pow<t>(value)','f64',False),
    ]
    for expression,target,valid in private_cases:
        private_module.write_text('func exercise<t>(value: t) -> t:\n    return '+expression+'\n')
        private_entry.write_text('use math\nfunc main():\n    var value = math.exercise<'+target+'>(1)\n')
        h.run([compiler,'check',private_entry],work,status=0 if valid else 1,small=small,env=env,sources=(private_entry,private_module))
    private_module.write_text('func _math_sin<t>(value: t) -> t:\n    return value\nfunc exercise<t>(value: t) -> t:\n    return _math_sin<t>(value)\n')
    private_entry.write_text('use math\nfunc main():\n    var value = math.exercise(true)\n')
    h.run([compiler,'check',private_entry],work,small=small,env=env,sources=(private_entry,private_module))
    # Bounded accepted/rejected frontend nesting, including the real 1 MiB run.
    for depth,valid in ((32,True),(64,False),(124,False),(400,False)):
        expression='take('*depth+'math.sin(0.5)'+')'*depth
        check('var answer = '+expression,valid,prelude='use math\nfunc take(value: f32) -> f32:\n    return value\n',diagnostic=None if valid else 'nesting')
    # Written order is preserved when named binding rearranges operands.
    source=work/'evaluation.void';source.write_text('''use math
var counter = 0
func tick(value: f64, digit: i32) -> f64:
    counter = counter * 10 + digit
    return value
func main():
    var result = math.clamp(upper = tick(2.0, 3), value = tick(1.0, 1), lower = tick(0.0, 2))
    print(counter)
    print(result)
''')
    for flags in (['--no-opt'],[]):h.run([compiler,'run',source,*flags],work,b'312\n1\n',small=small)
print(f'Math frontend: {h.checks} processes passed; 1 MiB={small}')
