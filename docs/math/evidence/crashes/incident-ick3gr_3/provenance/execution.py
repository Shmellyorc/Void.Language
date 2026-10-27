"""Real normal/O2 JIT and separately linked O0/O2 AOT, exact bits and MPFR ULPs."""
from pathlib import Path
import hashlib
import json
import math
import os
import re
import sys
import tempfile
import bootstrap as h
from reference import bits, value, rounded, classify, ulps, lerp, inverse_lerp

compiler=Path(sys.argv[1]).resolve();small='--small-stack' in sys.argv
bank_raw=Path(__file__).with_name('vectors.json').read_bytes()
bank=json.loads(bank_raw)
bank_sha256=hashlib.sha256(bank_raw).hexdigest()
source=['use math','use encoding/binary','use convert','''
func raw_bits<t>(value: t) -> u64:
    var bytes = binary.encode(value)
    var result: u64 = 0
    for index in bytes.count - 1..0:
        result = result * 256 + (bytes[index] as u64)
    return result
''']
for p in (32,64):
    source += [f'func stored_f{p}(raw: u64) -> f{p}:','    var bytes: [u8] = []',f'    for index in 0..<{p//8}:','        bytes.push((raw % 256) as u8)','        raw = raw / 256',f'    return convert.unwrap(binary.decode<f{p}>(bytes))','']
cases=[];groups=[]

def group(name):
    groups.append(name);source.extend(['',f'func {name}():'])

def emit(expression, expected, precision=None, mode='exact', label=None, tolerance=0):
    source.append('    print('+('raw_bits('+expression+')' if precision else expression)+')')
    cases.append({'label':label or expression,'precision':precision,'mode':mode,'expected':expected,'max_ulp':tolerance})

def num(x,p):
    return f'stored_f{p}({bits(rounded(x,p),p)})'

def call(name,arguments,p):
    return f'math.{name}<f{p}>('+', '.join(num(x,p) for x in arguments)+')'

def exact(name,args,answer,p):
    emit(call(name,args,p), 'nan' if math.isnan(answer) else bits(rounded(answer,p),p),p,'nan' if math.isnan(answer) else 'exact')

for t in ('i8','u8','i16','u16','i32','u32','i64','u64'):
    group('core_'+t)
    width=int(t[1:]);signed=t[0]=='i';low=-(1<<(width-1)) if signed else 0;high=(1<<(width-int(signed)))-1
    for fn,args,expected in [('min',[low,high],low),('min',[high,low],low),('max',[low,high],high),('max',[high,low],high),('clamp',[low,0,high],0),('clamp',[high,low,high],high),('abs',[high],high),('sign',[low],-1 if signed else 0),('sign',[high],1),('sign',[0],0)]:
        emit(f'math.{fn}<{t}>('+', '.join(map(str,args))+')',str(expected),mode='text')
for p in (32,64):
    group('constants_f'+str(p))
    for name,raw in [('pi',0x40490fdb if p==32 else 0x400921fb54442d18),('tau',0x40c90fdb if p==32 else 0x401921fb54442d18),('e',0x402df854 if p==32 else 0x4005bf0a8b145769)]:
        emit(f'math.{name}_f{p}',raw,p)
    maximum=value(0x7f7fffff if p==32 else 0x7fefffffffffffff,p);sub=value(1,p)
    pinf,ninf,nan=math.inf,-math.inf,math.nan;negzero=-0.0
    group('selectors_f'+str(p))
    for a in (0.,negzero):
        for b in (0.,negzero):
            exact('min',[a,b],negzero if bits(a,p)>>(p-1) or bits(b,p)>>(p-1) else 0.,p)
            exact('max',[a,b],negzero if bits(a,p)>>(p-1) and bits(b,p)>>(p-1) else 0.,p)
    for a,b in [(nan,1.),(1.,nan),(nan,nan),(-maximum,maximum),(maximum,-maximum),(pinf,ninf)]:
        exact('min',[a,b],nan if math.isnan(a) or math.isnan(b) else min(a,b),p)
        exact('max',[a,b],nan if math.isnan(a) or math.isnan(b) else max(a,b),p)
    for a in (nan,pinf,ninf,0.,negzero,sub,-sub,maximum,-maximum):
        exact('sign',[a],a if math.isnan(a) or a==0 else -1. if a<0 else 1.,p)
        exact('abs',[a],abs(a),p)
        for fn,result in [('is_nan',math.isnan(a)),('is_infinite',math.isinf(a)),('is_finite',math.isfinite(a))]:
            emit(call(fn,[a],p),str(result).lower(),mode='text')
    for a,lo,hi in [(negzero,-1.,1.),(0.,negzero,0.),(nan,ninf,pinf),(pinf,ninf,pinf),(ninf,ninf,pinf),(1.,2.,2.)]:
        exact('clamp',[a,lo,hi],a if math.isnan(a) or lo<=a<=hi else lo if a<lo else hi,p)
    group('rounding_f'+str(p))
    inputs=[0.,negzero,sub,-sub,maximum,-maximum,pinf,ninf,nan,float(1<<(24 if p==32 else 53)),float(1<<64)]
    for x in (-2.5,-1.5,-0.5,0.5,1.5,2.5):
        raw=bits(x,p);inputs.extend([value(raw-1,p),x,value(raw+1,p)])
    for fn,operation in [('floor',math.floor),('ceil',math.ceil),('trunc',math.trunc),('round',round)]:
        for x in inputs:
            answer=x if not math.isfinite(x) or x==0 else float(operation(x))
            if answer==0:answer=math.copysign(0.,x)
            exact(fn,[x],answer,p)
    group('interpolation_f'+str(p))
    triples=[(negzero,0.,0.),(negzero,0.,1.),(negzero,0.,.5),(1.,1.,pinf),(0.,10.,.25),(10.,0.,.25),(-maximum,maximum,.5),(maximum,-maximum,.25),(maximum,maximum,.5),(negzero,1.,0.),(1.,negzero,1.),(0.,10.,-1.),(0.,10.,2.),(sub,sub*3,.5),(1.,value(bits(1.,p)+1,p),.5),(ninf,0.,0.),(0.,pinf,1.),(0.,1.,nan),(0.,1.,pinf)]
    inverse=[(0.,10.,2.5),(10.,0.,2.5),(-maximum,maximum,0.),(maximum,-maximum,0.),(maximum,value(bits(maximum,p)-1,p),-maximum),(sub,sub*3,sub*2),(0.,sub,sub*2),(0.,0.,1.),(negzero,0.,0.),(0.,1.,negzero),(0.,1.,1.),(0.,10.,-10.),(0.,10.,20.),(ninf,0.,0.),(0.,pinf,1.),(0.,1.,nan)]
    for fn,triples,oracle in [('lerp',triples,lerp),('inverse_lerp',inverse,inverse_lerp)]:
        for triple in triples:exact(fn,triple,oracle(*triple,p),p)
    group('special_unary_f'+str(p))
    for fn in ('sin','tan','asin','atan','sqrt','radians','degrees'):
        for zero in (0.,negzero):exact(fn,[zero],zero,p)
    for fn in ('sin','cos','tan','asin','acos','atan','sqrt','exp','exp2','log','log2','radians','degrees'):
        exact(fn,[nan],nan,p)
    pi=value(0x40490fdb if p==32 else 0x400921fb54442d18,p);half=rounded(pi/2,p);quarter=rounded(pi/4,p)
    for fn in ('sin','cos','tan','asin','acos'):
        for infinity in (pinf,ninf):exact(fn,[infinity],nan,p)
    for fn in ('exp','exp2'):
        for x,answer in [(0.,1.),(negzero,1.),(pinf,pinf),(ninf,0.)]:exact(fn,[x],answer,p)
    for fn in ('exp','exp2'):
        exact(fn,[1000. if fn=='exp' else 2000.],pinf,p)
        exact(fn,[-1000. if fn=='exp' else -2000.],0.,p)
    for fn in ('log','log2'):
        for x,answer in [(0.,ninf),(negzero,ninf),(-1.,nan),(ninf,nan),(1.,0.),(pinf,pinf)]:exact(fn,[x],answer,p)
    for fn in ('radians','degrees'):
        for infinity in (pinf,ninf):exact(fn,[infinity],infinity,p)
        factor=rounded(pi/rounded(180.,p),p) if fn=='radians' else rounded(rounded(180.,p)/pi,p)
        for x in (1.,180.,-360.,maximum,sub):exact(fn,[x],rounded(x*factor,p),p)
    for x,answer in [(-1.,nan),(ninf,nan),(pinf,pinf)]:exact('sqrt',[x],answer,p)
    for x in (-1.,1.):
        exact('asin',[x],math.copysign(half,x),p);exact('acos',[x],pi if x<0 else 0.,p)
    for x in (0.,negzero):exact('cos',[x],1.,p);exact('acos',[x],half,p)
    for x in (-2.,2.):exact('asin',[x],nan,p);exact('acos',[x],nan,p)
    for x in (pinf,ninf):exact('atan',[x],math.copysign(half,x),p)
    group('atan2_special_f'+str(p))
    for y in (ninf,-1.,negzero,0.,1.,pinf,nan):
        for x in (ninf,-1.,negzero,0.,1.,pinf,nan):
            sy=bool(bits(y,p)>>(p-1));sx=bool(bits(x,p)>>(p-1))
            if math.isnan(y) or math.isnan(x):answer=nan
            elif y==0:answer=math.copysign(pi if sx else 0.,y)
            elif x==0:answer=math.copysign(half,y)
            elif math.isinf(y) and math.isinf(x):answer=math.copysign(rounded(pi-quarter,p) if sx else quarter,y)
            elif math.isinf(y):answer=math.copysign(half,y)
            elif math.isinf(x):answer=math.copysign(pi if sx else 0.,y)
            else:answer=math.copysign(rounded(pi-quarter,p) if sx else quarter,y)
            if math.isfinite(y) and y != 0 and math.isfinite(x) and x != 0:
                emit(call('atan2',[y,x],p),bits(rounded(answer,p),p),p,'ulp',tolerance=4 if p==32 else 8)
            else:exact('atan2',[y,x],answer,p)
    group('pow_special_f'+str(p))
    odd=float((1<<(24 if p==32 else 53))-1);huge=float(1<<80)
    for base in (nan,1.,-1.,negzero,0.,ninf,pinf):
        for exponent in (nan,ninf,pinf,negzero,0.,-3.,-2.,-1.,-.5,.5,1.,2.,3.,odd,-odd,huge,-huge):
            negative=bool(bits(base,p)>>(p-1));isodd=math.isfinite(exponent) and exponent==math.floor(exponent) and abs(exponent)%2==1
            if exponent==0 or base==1 or base==-1 and math.isinf(exponent):answer=1.
            elif math.isnan(base) or math.isnan(exponent):answer=nan
            elif math.isinf(exponent):answer=0. if (abs(base)<1)==(exponent>0) else pinf
            elif base==0:answer=math.copysign(0. if exponent>0 else pinf,-1. if negative and isodd else 1.)
            elif math.isinf(base):answer=math.copysign(pinf if exponent>0 else 0.,-1. if negative and isodd else 1.)
            elif base==-1:answer=nan if exponent!=math.floor(exponent) else -1. if isodd else 1.
            else:raise AssertionError((base,exponent))
            exact('pow',[base,exponent],answer,p)
    for base in (-2.,-.5,2.,.5):
        for exponent in (pinf,ninf):
            exact('pow',[base,exponent],0. if (abs(base)<1)==(exponent>0) else pinf,p)
    for base in (-2.,-.5):
        for exponent in (-3.,-2.,2.,3.,.5,odd,-odd,huge,-huge):
            if exponent==.5:answer=nan
            elif abs(exponent)>2000:answer=pinf if (abs(base)>1)==(exponent>0) else 0.;answer=math.copysign(answer,-1. if abs(exponent)==odd else 1.)
            else:answer=base**int(exponent)
            exact('pow',[base,exponent],answer,p)
# The mathematical oracle is separately recorded from the mandated per-operation
# source algorithm. Conditioning error is reported, without a universal ULP gate.
for p in (32,64):
    for fn in ('lerp','inverse_lerp'):
        group('mathematical_'+fn+'_f'+str(p))
        for row in bank['interpolation_vectors']:
            if row['precision']==p and row['function']==fn:
                expression=f'math.{fn}<f{p}>('+', '.join(f'stored_f{p}({x})' for x in row['inputs'])+')'
                emit(expression,row['ordered_algorithm_bits'],p)
                cases[-1]['mathematical_expected']=row['mathematical_expected']
# Group independent bank by function and precision to bound individual source bodies.
for p in (32,64):
    for fn in ('sin','cos','tan','asin','acos','atan','atan2','sqrt','pow','exp','exp2','log','log2'):
        group('reference_'+fn+'_f'+str(p))
        for row in bank['vectors']:
            if row['precision']==p and row['function']==fn:
                expression=f'math.{fn}<f{p}>('+', '.join(f'stored_f{p}({x})' for x in row['inputs'])+')'
                emit(expression,row['expected'],p,'ulp',tolerance=row['max_ulp'])
                for key in ('category','absolute_magnitude_upper','absolute_magnitude_lower','absolute_error_upper'):
                    if key in row:cases[-1][key]=row[key]
source += ['', 'func entry(input: i32) -> i32:',*[f'    {name}()' for name in groups],'    print(raw_bits(math.sin(input as f64)))','    return 0']
# Keep homogeneous vector banks in runtime data tables rather than expanding a
# facade body at hundreds of source callsites. The exact case order is unchanged.
def parsed(line):
    match=re.fullmatch(r'    print\(raw_bits\(math\.(\w+)<f(32|64)>\((.*)\)\)\)',line)
    if not match:return None
    fn,p,args=match.groups();inputs=re.findall(r'stored_f'+p+r'\((\d+)\)',args)
    if ', '.join('stored_f'+p+'('+x+')' for x in inputs)!=args:return None
    return (fn,p,len(inputs)),inputs
compact=[];index=0;batch=0
while index<len(source):
    first=parsed(source[index]);end=index+1
    if first:
        while end<len(source) and parsed(source[end]) and parsed(source[end])[0]==first[0]:end+=1
    if first and end-index>=4:
        (fn,p,arity),_=first;rows=[parsed(line)[1] for line in source[index:end]]
        for argument in range(arity):
            compact.append(f'    var batch{batch}_arg{argument}: [u64] = ['+', '.join(row[argument] for row in rows)+']')
        compact.append(f'    for batch{batch}_index in 0..<batch{batch}_arg0.count:')
        arguments=', '.join(f'stored_f{p}(batch{batch}_arg{argument}[batch{batch}_index])' for argument in range(arity))
        compact.append(f'        print(raw_bits(math.{fn}<f{p}>({arguments})))');batch+=1
    else:compact.extend(source[index:end])
    index=end
source=compact
row=next(x for x in bank['vectors'] if x['precision']==64 and x['function']=='sin' and x['inputs']==[bits(1.,64)])
cases += [{'label':'dynamic sin(input as f64)','precision':64,'mode':'ulp','expected':row['expected'],'max_ulp':8},{'label':'entry result','precision':None,'mode':'text','expected':'0','max_ulp':0}]

def verify(output):
    lines=output.decode().splitlines();assert len(lines)==len(cases),(len(lines),len(cases),lines[-10:])
    worst={};mathematical=[]
    for row,line in zip(cases,lines):
        if row['mode']=='text':assert line==row['expected'],(row,line);continue
        actual=int(line);p=row['precision'];kind=classify(actual,p)
        if row['mode']=='nan':assert kind=='nan',(row,hex(actual));continue
        expected=row['expected'];expected_kind=classify(expected,p)
        if 'mathematical_expected' in row:
            truth=row['mathematical_expected'];mathematical.append({'label':row['label'],'precision':p,'actual_bits':actual,'correctly_rounded_mathematical_bits':truth,'ulp_distance':ulps(actual,truth,p)})
        if row['mode']=='exact' or expected_kind!='finite':assert actual==expected,(row,hex(actual),hex(expected));continue
        assert kind=='finite',(row,hex(actual))
        magnitude=abs(value(actual,p))
        if 'absolute_magnitude_upper' in row:assert magnitude<=row['absolute_magnitude_upper'],(row,hex(actual))
        if 'absolute_magnitude_lower' in row:assert magnitude>=row['absolute_magnitude_lower'],(row,hex(actual))
        if 'absolute_error_upper' in row:assert abs(value(actual,p)-value(expected,p))<=row['absolute_error_upper'],(row,hex(actual))
        distance=ulps(actual,expected,p)
        assert distance<=row['max_ulp'],(row,hex(actual),hex(expected),distance)
        # A sign mismatch or flushed subnormal cannot hide inside an ULP tolerance.
        assert actual>>(p-1)==expected>>(p-1),(row,hex(actual))
        key=row['label'].split('<')[0]+'_f'+str(p);worst[key]=max(worst.get(key,0),distance)
    return {'native_max_ulp':worst,'interpolation_mathematical_error':mathematical}

with tempfile.TemporaryDirectory(prefix='VOID math execution ') as directory:
    work=Path(directory);fixture=work/'math_test.void';raw=('\n'.join(source)+'\n').encode();fixture.write_bytes(raw)
    evidence=h.ROOT/'docs/math/evidence';evidence.mkdir(exist_ok=True,parents=True)
    label=compiler.parent.name+('-stack' if small else '')
    source_copy=evidence/('math_test-'+label+'.void')
    expectations_copy=evidence/('expectations-'+label+'.json')
    bank_copy=evidence/('vectors-'+bank_sha256+'.json')
    def retain_artifact(path,content):
        if path.exists() and path.read_bytes()!=content:
            previous=path.read_bytes();backup=path.with_name(path.stem+'-prior-'+hashlib.sha256(previous).hexdigest()[:12]+path.suffix)
            if not backup.exists():backup.write_bytes(previous)
        path.write_bytes(content)
    retain_artifact(source_copy,raw)
    retain_artifact(expectations_copy,(json.dumps(cases,indent=2)+'\n').encode())
    if not bank_copy.exists():
        with bank_copy.open('xb') as stream:stream.write(bank_raw)
        bank_copy.chmod(0o444)
    assert bank_copy.read_bytes()==bank_raw,bank_copy
    print(f'Math check: {len(cases)} vectors, {len(raw)} source bytes',flush=True)
    h.run([compiler,'check',fixture],work,small=small)
    observations=[]
    for mode,flags in [('O0',['--no-opt']),('O2',[])]:
        print('Math JIT '+mode,flush=True)
        result=h.run([compiler,'run',fixture,'1',*flags],work,small=small,timeout=300)
        observations.append({'mode':'JIT '+mode,'worst_ulp':verify(result.stdout),'stdout_sha256':hashlib.sha256(result.stdout).hexdigest()})
        executable=work/('math-'+mode+('.exe' if os.name=='nt' else ''))
        print('Math linked AOT '+mode,flush=True)
        h.run([compiler,'build',fixture,'-o',executable,*flags],work,small=small,timeout=300)
        result=h.run([executable,'1'],work,sources=(fixture,),small=small)
        observations.append({'mode':'AOT '+mode,'worst_ulp':verify(result.stdout),'stdout_sha256':hashlib.sha256(result.stdout).hexdigest()})
    record={'compiler':str(compiler),'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256(raw).hexdigest(),'vectors_sha256':bank_sha256,'source_artifact':str(source_copy.relative_to(h.ROOT)),'expectations_artifact':str(expectations_copy.relative_to(h.ROOT)),'reference_artifact':str(bank_copy.relative_to(h.ROOT)),'vectors':len(cases),'independent_mpfr_vectors':len(bank['vectors']),'independent_mpfr_interpolation_vectors':len(bank['interpolation_vectors']),'processes':h.checks,'small_stack':small,'observations':observations}
    retain_artifact(evidence/('execution-'+label+'.json'),(json.dumps(record,indent=2)+'\n').encode())
print(f'Math execution: {len(cases)} vectors x four real modes; {h.checks} processes passed; 1 MiB={small}')
