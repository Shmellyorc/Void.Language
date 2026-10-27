"""Deterministic numeric and collection evidence, actual source in JIT/O2 and linked AOT."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import tempfile
import harness as h
from reference import Reference, MASK, bits, rounded, verify

verify()
compiler = Path(sys.argv[1]).resolve()
small = '--small-stack' in sys.argv
source = ['use random', 'use convert', 'use encoding/binary', '', '''func raw_bits<t>(value: t) -> u64:
    var bytes = binary.encode(value)
    var result: u64 = 0
    for index in bytes.count - 1..0:
        result = result * 256 + (bytes[index] as u64)
    return result
''', 'func main():']
expected = []
vectors = 0

def emit(statement, value):
    global vectors
    source.append('    print(' + statement + ')')
    expected.append(str(value).lower())
    vectors += 1

for serial, seed in enumerate((0, 1, 42, 1 << 63, MASK)):
    source.append(f'    var engine{serial} = random.generator(seed = {seed})')
    rng = Reference(seed)
    # Loop keeps the frontend fixture compact while exercising long transitions.
    source += ['    for index in 0..<64:', f'        print(engine{serial}.rand_u64())']
    expected.extend(str(rng.word()) for _ in range(64))
    vectors += 64

for width in (8, 16, 32, 64):
    for signed in (False, True):
        target = ('i' if signed else 'u') + str(width)
        low = -(1 << (width - 1)) if signed else 0
        high = (1 << (width - signed)) - 1
        rng = Reference(42)
        name = 'g' + target
        source.append(f'    var {name} = random.generator(seed = 42)')
        emit(f'{name}.rand_{target}()', rng.integer(width, signed))
        for maximum in (1, 2, 3, (1 << (width - 1)) + (0 if signed else 1), high):
            if maximum > high:
                continue
            emit(f'{name}.rand_{target}({maximum})', rng.integer(width, signed, 0, maximum))
            emit(f'convert.unwrap({name}.try_rand_{target}(maximum = {maximum}))', rng.integer(width, signed, 0, maximum))
        for lower, upper in [(low, high), (low, low + 1), (high - 1, high), (-3 if signed else 0, 7)]:
            emit(f'{name}.rand_{target}(upper = {upper}, lower = {lower})', rng.integer(width, signed, lower, upper))
            emit(f'convert.unwrap({name}.try_rand_{target}({lower}, {upper}))', rng.integer(width, signed, lower, upper))
        emit(f'{name}.rand_range<{target}>({low}, {high})', rng.integer(width, signed, low, high, True))
        emit(f'convert.unwrap({name}.try_rand_range<{target}>({low}, {high}))', rng.integer(width, signed, low, high, True))
        emit(f'{name}.rand_range<{target}>({high}, {high})', high)
        # Exact module forwarding and same aliased default stream.
        source.append('    random.seed(42)')
        default = Reference(42)
        emit(f'random.rand_{target}()', default.integer(width, signed))
        emit(f'convert.unwrap(random.try_rand_{target}())', default.integer(width, signed))
        emit(f'random.rand_{target}(7)', default.integer(width, signed, 0, 7))
        emit(f'convert.unwrap(random.try_rand_{target}(7))', default.integer(width, signed, 0, 7))
        emit(f'random.rand_{target}({low}, {high})', default.integer(width, signed, low, high))
        emit(f'convert.unwrap(random.try_rand_{target}(upper = {high}, lower = {low}))', default.integer(width, signed, low, high))
        emit(f'random.rand_range<{target}>({low}, {high})', default.integer(width, signed, low, high, True))
        emit(f'convert.unwrap(random.try_rand_range<{target}>({low}, {high}))', default.integer(width, signed, low, high, True))
        # All invalid bounds checked without advancing, and enum/category evidence.
        for expression in (f'{name}.try_rand_{target}(0)', f'{name}.try_rand_{target}(2, 2)', f'{name}.try_rand_{target}(3, 1)', f'{name}.try_rand_range<{target}>(3, 1)', f'random.try_rand_{target}(0)', f'random.try_rand_{target}(2, 2)'):
            source += [f'    if {expression} is Err(var reason):', '        print(reason.code)', '    else:', '        print(-99)']
            expected.append('1'); vectors += 1
        emit(f'{name}.rand_u64()', rng.word())
        emit('random.rand_u64()', default.word())

# Natural integer rejection consumes exactly three words.
source.append('    var reject_integer = random.generator(seed = 42)')
rng = Reference(42)
emit('reject_integer.rand_u8(129)', rng.integer(8, False, 0, 129))
emit('reject_integer.rand_u64()', rng.word())

for precision in (32, 64):
    target = f'f{precision}'
    name = 'g' + target
    source.append(f'    var {name} = random.generator(seed = 0)')
    rng = Reference(0)
    source += ['    for index in 0..<32:', f'        print(raw_bits({name}.rand_{target}()))']
    expected.extend(str(bits(rng.unit(precision), precision)) for _ in range(32)); vectors += 32
    pairs = [(0, 1), (1, 1 + 2 ** (-23 if precision == 32 else -52)), (-1, 1), (-10, -5), (0.5, 1), (0, 2 ** (-149 if precision == 32 else -1074)), (-2 ** (-149 if precision == 32 else -1074), 0), (-(2-2**(-23 if precision==32 else -52))*2**(127 if precision==32 else 1023),(2-2**(-23 if precision==32 else -52))*2**(127 if precision==32 else 1023))]
    def lit(value):
        wire = struct.pack('<f' if precision==32 else '<d', value)
        return f'convert.unwrap(binary.decode<{target}>([' + ','.join(map(str,wire)) + ']))'
    for lower, upper in pairs:
        emit(f'raw_bits({name}.rand_{target}({lit(lower)}, {lit(upper)}))', bits(rng.floating(precision,lower,upper),precision))
        emit(f'raw_bits(convert.unwrap({name}.try_rand_{target}({lit(lower)}, {lit(upper)})))', bits(rng.floating(precision,lower,upper),precision))
    # Reset for independently reviewed adjacent-endpoint retry vector.
    source.append(f'    {name}.seed(0)'); rng = Reference(0)
    emit(f'raw_bits({name}.rand_{target}(1.0, {lit(pairs[1][1])}))', bits(rng.floating(precision,1,pairs[1][1]),precision))
    emit(f'{name}.rand_u64()', rng.word())
    source.append('    random.seed(42)'); default = Reference(42)
    emit(f'raw_bits(random.rand_{target}())', bits(default.unit(precision),precision))
    emit(f'raw_bits(convert.unwrap(random.try_rand_{target}()))', bits(default.unit(precision),precision))
    for args,lower,upper in [('10.0',0,10), ('5.0, 10.0',5,10)]:
        emit(f'raw_bits(random.rand_{target}({args}))',bits(default.floating(precision,lower,upper),precision))
        emit(f'raw_bits(convert.unwrap(random.try_rand_{target}({args})))',bits(default.floating(precision,lower,upper),precision))
    for lower,upper,code in [(0.,0.,1), (2.,1.,1), (float('inf'),0.,2), (0.,float('nan'),2), (float('-inf'),float('inf'),2)]:
        for receiver in (name, 'random'):
            source += [f'    if {receiver}.try_rand_{target}({lit(lower)}, {lit(upper)}) is Err(var reason):','        print(reason.code)','    else:','        print(-99)']
            expected.append(str(code)); vectors += 1
    emit(f'{name}.rand_u64()',rng.word())
    emit('random.rand_u64()',default.word())

# SplitMix's second expanded state is zero for this seed, so the first output
# word is exactly zero. This pins +0 float bits and full signed minima directly.
zero_seed = (-2 * 0x9e3779b97f4a7c15) & MASK
assert Reference(zero_seed).word() == 0
for target in ('f32', 'f64'):
    source.append(f'    var zero_{target} = random.generator(seed = {zero_seed})')
    emit(f'raw_bits(zero_{target}.rand_{target}())', 0)
for target,width in [('i8',8),('i16',16),('i32',32),('i64',64)]:
    source.append(f'    var zero_{target} = random.generator(seed = {zero_seed})')
    emit(f'zero_{target}.rand_{target}()', -(1 << (width-1)))

source += ['    var stream = random.generator(seed = 42)', '    var saved_stream = stream']
rng = Reference(42)
emit('stream.boolean()', rng.word() >= 1 << 63)
emit('saved_stream.rand_u64()', Reference(42).word())
emit('stream.rand_u64()',rng.word())
source += ['    stream.seed(42)', '    var list = [0, 1, 2, 3]', '    var alias = list', '    stream.shuffle(list)']
rng = Reference(42)
for index, value in enumerate(rng.shuffle([0,1,2,3])): emit(f'alias[{index}]',value)
emit('stream.rand_u64()',rng.word())
source += ['    var empty: [i32] = []', '    var one = [17]', '    stream.shuffle(empty)', '    stream.shuffle(one)']
emit('stream.choose(one)',17)
source += ['    var one_copy = stream.shuffled(one)', '    one_copy[0] = 9']
emit('one[0]',17)
emit('stream.rand_range(-3, -3)',-3)
emit('stream.rand_u64()',rng.word())
source += ['    if stream.try_choose(empty) is Err(var reason):','        print(reason.code)','    else:','        print(-99)']; expected.append('3'); vectors += 1
source += ['    random.seed(42)', '    var data = [10, 20, 30]', '    var fixed: [i32, 3] = [10, 20, 30]']
rng = Reference(42)
emit('random.choose(data)',[10,20,30][rng.integer(64,True,0,3)])
emit('convert.unwrap(random.try_choose(fixed[0..<fixed.count]))',[10,20,30][rng.integer(64,True,0,3)])
source += ['    var copy = random.shuffled(data)']
for index,value in enumerate(rng.shuffle([10,20,30])):emit(f'copy[{index}]',value)
source += ['    var checked_copy = random.try_shuffled(data)', '    if checked_copy is Ok(var inner):']
for index,value in enumerate(rng.shuffle([10,20,30])):
    source.append(f'        print(inner[{index}])');expected.append(str(value));vectors+=1
source += ['    var done = random.try_shuffle(data)']
for index,value in enumerate(rng.shuffle([10,20,30])):emit(f'data[{index}]',value)
emit('random.rand_u64()',rng.word())

source += ['    var histogram = [0, 0, 0, 0, 0, 0, 0]', '    var histogram_engine = random.generator(seed = 42)', '    for index in 0..<4096:', '        var position = histogram_engine.rand_i32(7)', '        histogram[position] += 1']
rng=Reference(42); counts=[0]*7
for _ in range(4096): counts[rng.integer(32,True,0,7)]+=1
for index,value in enumerate(counts): emit(f'histogram[{index}]',value)

with tempfile.TemporaryDirectory(prefix='VOID random execution ') as directory:
    work=Path(directory)
    fixture=work/'random_test.void'
    raw=('\n'.join(source)+'\n').encode(); fixture.write_bytes(raw)
    oracle=('\n'.join(expected)+'\n').encode()
    h.run([compiler,'check',fixture],work,small=small)
    for flags in (['--no-opt'], []):
        h.run([compiler,'run',fixture,*flags],work,oracle,small=small)
        executable=work/('random'+str(bool(flags))+('.exe' if sys.platform=='win32' else ''))
        h.run([compiler,'build',fixture,'-o',executable,*flags],work,small=small)
        h.run([executable],work,oracle,sources=(fixture,),small=small)
    record={'compiler':str(compiler),'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256(raw).hexdigest(),'oracle_sha256':hashlib.sha256(oracle).hexdigest(),'vectors':vectors,'processes':h.checks,'small_stack':small}
    # Stable fixtures and exact expected output make independent review reproducible.
    evidence=h.ROOT/'docs/random/evidence'
    evidence.mkdir(exist_ok=True,parents=True)
    (evidence/'random_test.void').write_bytes(raw)
    (evidence/'expected.txt').write_bytes(oracle)
    (evidence/('execution-'+compiler.parent.name+('-stack' if small else '')+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print(f'Random execution: {vectors} vectors x 4 modes; {h.checks} processes passed; 1 MiB={small}')
