"""Independent frontend acceptance/rejection and exact canonical arity binding."""
from pathlib import Path
import sys
import tempfile
import harness as h

compiler=Path(sys.argv[1]).resolve()
small='--small-stack' in sys.argv
with tempfile.TemporaryDirectory(prefix='VOID random frontend ') as directory:
    work=Path(directory)
    serial=0
    def check(body, valid=True, diagnostic=None, prelude='use random\n'):
        global serial
        serial+=1
        source=work/f'case{serial}.void'
        source.write_text(prelude+'func main():\n'+''.join('    '+line+'\n' for line in body.splitlines()))
        result=h.run([compiler,'check',source],work,status=0 if valid else 1,small=small)
        if diagnostic:
            assert diagnostic.encode() in result.stderr,(diagnostic,result.stderr)
        return source
    for target in ('i8','u8','i16','u16','i32','u32','i64','u64','f32','f64'):
        number='2.0' if target.startswith('f') else '2'
        low='1.0' if target.startswith('f') else '1'
        for receiver in ('random','engine'):
            prefix='var engine = random.generator(seed = 42)\n' if receiver=='engine' else ''
            check(prefix+f'var a = {receiver}.rand_{target}()\nvar b = {receiver}.rand_{target}(maximum = {number})\nvar c = {receiver}.rand_{target}(upper = {number}, lower = {low})\nvar d = {receiver}.try_rand_{target}({number})\nvar e = {receiver}.try_rand_{target}(lower = {low}, upper = {number})')
            check(prefix+f'var a = {receiver}.rand_{target}({number}, {number}, {number})',False,'argument')
            check(prefix+f'var a = {receiver}.rand_{target}(true)',False,'Type mismatch')
            wrong='i64' if target!='i64' else 'u64'
            check(prefix+f'var typed: {wrong} = 2\nvar a = {receiver}.rand_{target}(typed)',False,'Type mismatch')
    for call in ('random.rand_i32(lower = 1)', 'random.rand_i32(upper = 2)', 'random.rand_i32(maximum = 2, upper = 3)', 'random.rand_i32(lower = 1, lower = 2)', 'random.try_rand_f32(0.0, 1.0, 2.0)', 'random.rand_i32<i32>(2)'):
        check('var a = '+call,False)
    check('var engine = random.generator(seed = 42)\nvar a = engine.try_rand_i32()',False,'one or two bounds')
    check('const engine = random.generator(seed = 42)\nvar a = engine.rand_u64()',False)
    check('var a = random.generator(seed = 42).rand_u64()',False)
    check('var a = random.generator()',False,'seed')
    check('var a = random._arity_rand_i32_1(2)',False)
    check('var a = _random_xor(1, 2)',False)
    check('var a = _random_seed()',False)
    check('var a = _random_domain_max<i32>()',False)
    for arguments in ('true, false','1.0, 2.0','1 as i8, 2 as i16'):
        check('var engine = random.generator(seed = 42)\nvar a = engine.rand_range('+arguments+')',False)
    check('var engine = random.generator(seed = 42)\nvar a = engine.rand_range<i64>(i64.min_value, i64.max_value)')
    check('var callable = random.rand_i32\nvar a = callable()')
    check('var callable = random.rand_i32\nvar a = callable(2)',False,'function-value')
    check('var callable: func(i32) -> i32 = random.rand_i32',False)
    # Alias resolution selects the same canonical declaration.
    check('var engine = rng.generator(seed = 42)\nvar a = rng.rand_i32(2)\nvar b = engine.rand_i32(1, 2)',prelude='use random as rng\n')
    # A project module with the same basename retains ordinary arity rules.
    libs=work/'libs'; libs.mkdir()
    (libs/'random.void').write_text('func rand_i32(value: i32) -> i32:\n    return value\n')
    check('var a = random.rand_i32(1)',prelude='use "./libs/random.void"\n')
    check('var a = random.rand_i32()',False,prelude='use "./libs/random.void"\n')
    check('var a = random.rand_i32(1, 2)',False,prelude='use "./libs/random.void"\n')
    check('var a = random.rand_i32(maximum = 2)',False,prelude='use "./libs/random.void"\n')
    # Ordinary duplicates and name shadowing are preserved.
    fixture=work/'ordinary.void'
    fixture.write_text('func f():\n    pass\nfunc f(a: i32):\n    pass\n')
    h.run([compiler,'check',fixture],work,status=1,small=small)
    fixture.write_text('func seed():\n    pass\nvalue struct thing:\n    x: i32\nfunc (self thing) init(seed: i32):\n    self.x = seed\n')
    h.run([compiler,'check',fixture],work,status=1,small=small)
    # Deep readonly projections must not be laundered by collection APIs.
    prelude='use random\nstruct box:\n    n: i32\n'
    check('var items = [box(n = 1)]\nvar selected = random.choose(items)\nselected.n = 2',False,prelude=prelude)
    check('var items = [box(n = 1)]\nvar copied = random.shuffled(items)\ncopied[0].n = 2',False,prelude=prelude)
    check('var items = [box(n = 1)]\nvar copied = random.shuffled(items)\nrandom.shuffle(copied)',False,prelude=prelude)
    # Direct deterministic worker helper is allowed; default and entropy are effects.
    for operation,valid in [('var engine = random.generator(seed = value)\nreturn engine.rand_u64()',True), ('return random.rand_u64()',False), ('var created = random.from_entropy()\nreturn value',False)]:
        fixture=work/f'worker{serial}.void';serial+=1
        fixture.write_text('use random\nfunc worker(value: u64) -> u64:\n'+''.join('    '+x+'\n' for x in operation.splitlines())+'async func main():\n    var task = spawn worker(42)\n    var answer = await task\n')
        h.run([compiler,'check',fixture],work,status=0 if valid else 1,small=small)
    # Named expressions retain source evaluation order even when reordered by binding.
    fixture=work/'evaluation.void'
    fixture.write_text('''use random
var counter = 0
func tick(value: i32) -> i32:
    counter = counter * 10 + value
    return value
func main():
    var engine = random.generator(seed = 42)
    var value = engine.rand_i32(upper = tick(4), lower = tick(1))
    print(counter)
    print(value)
    var a = random.rand_i32(lower = 7, upper = 8)
    print(a)
''')
    for flags in (['--no-opt'],[]):
        h.run([compiler,'run',fixture,*flags],work,b'41\n1\n7\n',small=small)
print(f'Random frontend: {h.checks} processes passed; 1 MiB={small}')
