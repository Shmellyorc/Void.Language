# Developer-only fixture generator; checked-in .void fixtures run without Python.
# Independent oracle: Python unlimited-precision integers and IEEE bit neighbors.
# This imports no compiler code and never invokes VOID to calculate expectations.
from pathlib import Path
root=Path(__file__).resolve().parent / 'programs'
for signed in (True,False):
 for n in (8,16,32,64):
  t=('i' if signed else 'u')+str(n); modulus=1<<n; low=-(1<<(n-1)) if signed else 0; high=(1<<(n-1))-1 if signed else modulus-1
  def wrap(x):
   x%=modulus
   return x-modulus if signed and x>(modulus//2-1) else x
  cases=[(high,'+',1),(low,'-',1),(high,'*',3),(low,'*',-1 if signed else high),(7,'/',3),(7,'%',3),(high,'/',3),(high,'%',3)]
  if signed: cases += [(low,'/',-1),(low,'%',-1),(-7,'/',3),(-7,'%',3),(7,'/',-3),(7,'%',-3)]
  else: cases +=[(high,'+',high),(0,'-',high)]
  pre=[]; funcs=[];checks=[]
  for idx,(a,op,b) in enumerate(cases):
   q=abs(a)//abs(b)*(1 if (a<0)==(b<0) else -1) if b else 0
   ans=wrap(a+b if op=='+' else a-b if op=='-' else a*b if op=='*' else q if op=='/' else a-q*b)
   pre.append(f'const folded_{idx}: {t} = ({a}) {op} ({b})')
   funcs.append(f'func compute_{idx}(a: {t}, b: {t}) -> {t}:\n    return a {op} b\n')
   checks.append(f'    if folded_{idx} != {ans} or compute_{idx}(({a}) + seed - seed, {b}) != {ans}:\n        return {idx+2}\n')
  text='\n'.join(pre)+'\n\n'+'\n'.join(funcs)+f'\nfunc entry(value: i32) -> i32:\n    var seed: {t} = value as {t}\n'+''.join(checks)+f'    var default_value: {t}\n    if default_value != 0:\n        return 99\n    var counter: {t} = 0\n    for counter < 3:\n        counter++\n    counter *= 2\n    counter /= 3\n    counter %= 2\n    if counter != 0:\n        return 98\n    return 1\n'
  text = text.replace('    return 1\n', f'    var negated: {t} = -seed\n    if negated + seed != 0:\n        return 97\n    return 1\n')
  (root/(t+'.void')).write_text(text)


from pathlib import Path
p=Path(__file__).resolve().parent / 'programs'
valid=[('i8','-128','i64','-128'),('u64','18446744073709551615','f64','18446744073709551616.0'),('i64','-9223372036854775808','f32','-9223372036854775808.0'),('u32','4294967295','i64','4294967295'),('f64','255.9','u8','255'),('f64','-128.9','i8','-128'),('f64','-0.9','u64','0'),('f64','9223372036854774784.0','i64','9223372036854774784'),('f64','18446744073709549568.0','u64','18446744073709549568'),('f64','1.0 / 0.0','f32','1.0 / 0.0'),('f64','0.0 / 0.0','f32',None),('f32','-0.0','f64','-0.0')]
pre=[];funcs=[];checks=[]
for i,(a,v,b,ans) in enumerate(valid):
 pre.append(f'const folded_{i}: {b} = ({v} as {a}) as {b}')
 funcs.append(f'func convert_{i}(x: {a}) -> {b}:\n    return x as {b}\n')
 expr=f'convert_{i}({v})'
 checks.append(f'    if {expr} '+(f'== {expr} or folded_{i} == folded_{i}' if ans is None else f'!= ({ans}) or folded_{i} != ({ans})')+f':\n        return {i+2}\n')
(p/'conversions.void').write_text('\n'.join(pre)+'\n\n'+'\n'.join(funcs)+'\nfunc entry(value: i32) -> i32:\n'+''.join(checks)+'    return 1\n')
invalid=[('i32','300','i8'),('i64','-1','u64'),('u64','18446744073709551615','i64'),('f64','0.0 / 0.0','i32'),('f64','1.0 / 0.0','u64'),('f64','9223372036854775808.0','i64'),('f64','18446744073709551616.0','u64'),('f64','128.0','i8'),('f64','-129.0','i8'),('f64','-1.0','u8'),('f64','999999999999999999999999999999999999999999999999999999999999999999.0','f32')]
for i,(a,v,b) in enumerate(invalid):
 (p/f'fail_cast_{i}.void').write_text(f'func convert(x: {a}) -> {b}:\n    return x as {b}\n\nfunc entry(value: i32) -> i32:\n    var result: {b} = convert({v})\n    return 99\n')
for t in ('i8','i64','u8','u64'):
 (p/f'fail_div_{t}.void').write_text(f'func divide(x: {t}, y: {t}) -> {t}:\n    return x / y\n\nfunc entry(value: i32) -> i32:\n    var result = divide(7, value as {t})\n    return 99\n')


from pathlib import Path
import math,struct
root=Path(__file__).resolve().parent / 'programs';pre=[];decl=[];checks=[];idx=0
for f in ('f32','f64'):
 def rounded(v): return struct.unpack('f',struct.pack('f',v))[0] if f=='f32' else v
 def previous(v):
  if f=='f64': return math.nextafter(v,-math.inf)
  bits=struct.unpack('I',struct.pack('f',v))[0]
  return struct.unpack('f',struct.pack('I',bits+(1 if v<0 else -1)))[0]
 def spelling(v): return format(v,'.60f').rstrip('0')+'0' if v==int(v) else format(v,'.60f').rstrip('0')
 for sign in (True,False):
  for n in (8,16,32,64):
   t=('i' if sign else 'u')+str(n);lo=-(2.0**(n-1)) if sign else 0.;upper=2.0**(n-1 if sign else n)
   for value in (lo,previous(upper),rounded(-0.75),rounded(lo-0.5) if sign and n<=32 and f=='f64' else lo):
    key=str(idx);idx+=1;v=spelling(value);expected=str(int(value))
    pre.append(f'const folded_{key}: {t} = ({v} as {f}) as {t}')
    decl.append(f'func convert_{key}(value: {f}) -> {t}:\n    return value as {t}\n')
    checks.append(f'    if convert_{key}({v}) != {expected} or folded_{key} != {expected}:\n        return {idx+1}\n')
   for side,bad in (('upper',upper),('lower',rounded(lo-1) if rounded(lo-1)<lo else previous(lo))):
    # Unsigned lower invalid is exactly -1; signed uses first representable
    # value whose truncation is below MIN.
    if not sign and side=='lower': bad=-1.
    if sign and side=='lower' and bad>lo-1: bad=previous(bad)
    (root/f'fail_boundary_{f}_{t}_{side}.void').write_text(f'func convert(value: {f}) -> {t}:\n    return value as {t}\nfunc entry(value: i32) -> i32:\n    var result = convert({spelling(bad)})\n    return 99\n')
(root/'float_boundaries.void').write_text('\n'.join(pre)+'\n\n'+'\n'.join(decl)+'\nfunc entry(value: i32) -> i32:\n'+''.join(checks)+'    return 1\n')

names=['i8','i16','i32','i64','u8','u16','u32','u64','f32','f64']; decl=[];check=[]
for i,a in enumerate(names):
 for j,b in enumerate(names):
  key=f'{i}_{j}';decl.append(f'const folded_{key}: {b} = (31 as {a}) as {b}\nfunc convert_{key}(a: {a}) -> {b}:\n    return a as {b}\n');check.append(f'    if convert_{key}(value as {a}) != value as {b} or folded_{key} != 31:\n        return {i*10+j+2}\n')
(Path(__file__).resolve().parent/'programs/all_conversions.void').write_text('\n'.join(decl)+'\nfunc entry(value: i32) -> i32:\n'+''.join(check)+'    return 1\n')


from pathlib import Path
names=['i8','i16','i32','i64','u8','u16','u32','u64'];decl=[]; calls=[]
for t in names:
 n=int(t[1:]); high=(1<<(n-1 if t[0]=='i' else n))-1
 decl.append(f'func calculate_{t}(value: {t}) -> {t}:\n    return (value * 7 + {high}) / 3 % 11\n')
 calls.append(f'(calculate_{t}(value as {t}) as i32)')
text='\n'.join(decl)+'''\nfunc floating(value: f32, wide: f64) -> i32:
    return ((value + 0.5) * 3) as i32 + ((wide + 0.25) * 4) as i32
func entry(value: i32) -> i32:
    return '''+' + '.join(calls)+' + floating(value as f32, value as f64)\n'
(Path(__file__).resolve().parent/'programs/dynamic_widths.void').write_text(text)
for value in (0,1,10):
 answer=0
 for t in names:
  n=int(t[1:]); high=(1<<(n-1 if t[0]=='i' else n))-1; z=(value*7+high)%(1<<n)
  if t[0]=='i' and z >= 1<<(n-1): z-=1<<n
  q=abs(z)//3*(1 if z>=0 else -1); rem=q-(abs(q)//11*(1 if q>=0 else -1))*11
  answer+=rem
 answer+=int((value+0.5)*3)+int((value+0.25)*4)
 print(value,answer)
