"""Development-only MPFR oracle. Regenerate with Python + system libmpfr (no VOID).

Every stored input is first rounded to its IEEE format, then passed to MPFR
exactly via binary64. Evaluate at 256 and 512 bits, independently round directly
to binary32/binary64 using MPFR_RNDN, and require identical reference bits.
The checked-in JSON is sufficient for ordinary test runs; MPFR is not required.
"""
from pathlib import Path
import ctypes as c
import ctypes.util
import json
import struct
from reference import bits, value, rounded, lerp, inverse_lerp

class Number(c.Structure):
    _fields_ = [('precision', c.c_long), ('sign', c.c_int), ('exponent', c.c_long), ('limbs', c.c_void_p)]

lib = c.CDLL(ctypes.util.find_library('mpfr'))
lib.mpfr_get_version.restype = c.c_char_p
lib.mpfr_init2.argtypes = [c.POINTER(Number), c.c_long]
lib.mpfr_clear.argtypes = [c.POINTER(Number)]
lib.mpfr_set_d.argtypes = [c.POINTER(Number), c.c_double, c.c_int]
lib.mpfr_get_d.argtypes = [c.POINTER(Number), c.c_int]; lib.mpfr_get_d.restype = c.c_double
lib.mpfr_get_flt.argtypes = [c.POINTER(Number), c.c_int]; lib.mpfr_get_flt.restype = c.c_float
lib.mpfr_get_str.argtypes = [c.c_void_p,c.POINTER(c.c_long),c.c_int,c.c_size_t,c.POINTER(Number),c.c_int]
lib.mpfr_get_str.restype = c.c_void_p
lib.mpfr_free_str.argtypes = [c.c_void_p]


def evaluate(name, arguments, precision, working):
    operands = [Number() for _ in arguments]
    output = Number(); lib.mpfr_init2(c.byref(output), working)
    temporary = Number(); lib.mpfr_init2(c.byref(temporary), working)
    try:
        for operand, raw in zip(operands, arguments):
            lib.mpfr_init2(c.byref(operand), working)
            lib.mpfr_set_d(c.byref(operand), value(raw, precision), 0)
        def binary(kind, destination, left, right):
            fn=getattr(lib,'mpfr_'+kind)
            fn.argtypes=[c.POINTER(Number)]*3+[c.c_int]
            fn(c.byref(destination),c.byref(left),c.byref(right),0)
        if name=='lerp':
            binary('sub',temporary,operands[1],operands[0])
            binary('mul',temporary,temporary,operands[2])
            binary('add',output,operands[0],temporary)
        elif name=='inverse_lerp':
            binary('sub',temporary,operands[2],operands[0])
            binary('sub',output,operands[1],operands[0])
            binary('div',output,temporary,output)
        else:
            fn = getattr(lib, 'mpfr_' + name)
            fn.argtypes = [c.POINTER(Number)] * (len(operands) + 1) + [c.c_int]
            fn(c.byref(output), *(c.byref(x) for x in operands), 0)
        expected = bits((lib.mpfr_get_flt if precision == 32 else lib.mpfr_get_d)(c.byref(output), 0), precision)
        exponent = c.c_long()
        pointer = lib.mpfr_get_str(None,c.byref(exponent),10,78,c.byref(output),0)
        digits = c.string_at(pointer).decode();lib.mpfr_free_str(pointer)
        return expected, {'digits':digits,'exponent':exponent.value}
    finally:
        for operand in operands:lib.mpfr_clear(c.byref(operand))
        lib.mpfr_clear(c.byref(output));lib.mpfr_clear(c.byref(temporary))

records=[]
for p in (32,64):
    maximum=value(0x7f7fffff if p==32 else 0x7fefffffffffffff,p)
    sub=value(1,p)
    tests={
        'sin':[-1000,-3.25,-0.125,0.125,1,3.25,1000,maximum,-maximum],
        'cos':[-1000,-3.25,-0.125,0.125,1,3.25,1000,maximum],
        'tan':[-100,-1.5,-0.125,0.125,1.5,100,maximum],
        'asin':[-1,-0.999,-0.5,sub,0.5,0.999,1],
        'acos':[-1,-0.999,-0.5,0,0.5,0.999,1],
        'atan':[-maximum,-100,-1,-sub,sub,1,100,maximum],
        'sqrt':[sub,value(2,p),value(0x007fffff if p==32 else 0x000fffffffffffff,p),0.125,2,3,maximum],
        'exp':[-100 if p==32 else -744,-87 if p==32 else -709,-1,-sub,sub,0.5,1,88 if p==32 else 709],
        'exp2':[-149 if p==32 else -1074,-126 if p==32 else -1022,-3.25,-sub,sub,0.5,3.25,127 if p==32 else 1023],
        'log':[sub,value(2,p),0.125,0.999,1.001,2,10,maximum],
        'log2':[sub,value(2,p),0.125,0.999,1.001,2,10,maximum],
    }
    for name, inputs in tests.items():
        for number in inputs:
            arguments=[bits(rounded(number,p),p)]
            expected,decimal=evaluate(name,arguments,p,256)
            assert expected==evaluate(name,arguments,p,512)[0],(p,name,arguments)
            records.append({'function':name,'precision':p,'inputs':arguments,'expected':expected,'decimal':decimal,'max_ulp':0 if name=='sqrt' else 4 if p==32 else 8})
    for name, pairs in {
        'atan2':[(1,1),(1,-1),(-1,-1),(-1,1),(maximum,sub),(sub,maximum),(maximum,-maximum),(1,3),(-7,0.125)],
        'pow':[(2,0.5),(0.125,1.25),(1.001,31.25),(2,-149 if p==32 else -1074),(2,-126 if p==32 else -1022),(2,127 if p==32 else 1023),(10,-3.25),(0.5,-1.5),(123,0.125)],
    }.items():
        for pair in pairs:
            arguments=[bits(rounded(x,p),p) for x in pair]
            expected,decimal=evaluate(name,arguments,p,256)
            assert expected==evaluate(name,arguments,p,512)[0],(p,name,arguments)
            records.append({'function':name,'precision':p,'inputs':arguments,'expected':expected,'decimal':decimal,'max_ulp':4 if p==32 else 8})
# References immediately around the stored pi and pi/2, with dedicated
# near-zero absolute bounds and pole range checks in addition to ULP distance.
for p in (32,64):
    pi_raw=0x40490fdb if p==32 else 0x400921fb54442d18
    half_raw=bits(rounded(value(pi_raw,p)/2,p),p)
    for name,center,category in [('sin',pi_raw,'near_zero'),('cos',half_raw,'near_zero'),('tan',half_raw,'near_pole')]:
        for raw in (center-1,center,center+1):
            expected,decimal=evaluate(name,[raw],p,256)
            assert expected==evaluate(name,[raw],p,512)[0],(p,name,raw)
            row={'function':name,'precision':p,'inputs':[raw],'expected':expected,'decimal':decimal,'max_ulp':4 if p==32 else 8,'category':category}
            if category=='near_zero':
                row['absolute_magnitude_upper']=0.000001 if p==32 else 0.00000000000001
                row['absolute_error_upper']=2.0**(-43 if p==32 else -95)
            else:row['absolute_magnitude_lower']=1000000 if p==32 else 100000000000000
            records.append(row)
interpolation=[]
for p in (32,64):
    maximum=value(0x7f7fffff if p==32 else 0x7fefffffffffffff,p);sub=value(1,p)
    adjacent=value(bits(1.,p)+1,p);adjacent2=value(bits(1.,p)+2,p)
    previous=value(bits(maximum,p)-1,p)
    for name,triples,algorithm in [
        ('lerp',[(0.,10.,.25),(10.,0.,.25),(-maximum,maximum,.5),(-maximum,maximum,.25),(maximum,-maximum,.25),(maximum,maximum,.5),(0.,10.,-1.),(0.,10.,2.),(sub,sub*3,.5),(1.,adjacent,.5),(adjacent,1.,.5),(-.1,.2,.3),(previous,maximum,.75)],lerp),
        ('inverse_lerp',[(0.,10.,2.5),(-maximum,maximum,0.),(maximum,-maximum,0.),(maximum,previous,-maximum),(sub,sub*3,sub*2),(0.,sub,sub*2),(0.,10.,-10.),(-.1,.2,.3),(1.,adjacent,adjacent2)],inverse_lerp)]:
        for triple in triples:
            arguments=[bits(rounded(x,p),p) for x in triple]
            expected,decimal=evaluate(name,arguments,p,256)
            assert expected==evaluate(name,arguments,p,512)[0],(p,name,arguments)
            stored=[value(x,p) for x in arguments]
            interpolation.append({'function':name,'precision':p,'inputs':arguments,'ordered_algorithm_bits':bits(algorithm(*stored,p),p),'mathematical_expected':expected,'decimal':decimal})
output={'oracle':'MPFR','version':lib.mpfr_get_version().decode(),'working_precision_bits':[256,512],'rounding':'MPFR_RNDN (nearest, ties to even), direct target conversion','input_contract':'Unsigned decimal integers are exact IEEE input bits; evaluate the stored input, not the originating decimal. Decimal reference = digits * 10^(exponent - len(abs(digits))).','vectors':records,'interpolation_vectors':interpolation}
path=Path(__file__).with_name('vectors.json');path.write_text(json.dumps(output,indent=2)+'\n')
print(f'{len(records)} independent MPFR {output["version"]} vectors written to {path}')
