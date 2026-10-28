"""Portable application-ID validation precedes native discovery, with all four placements."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();small='--small-stack' in sys.argv
valid=['a','0','com.studio.game','a_b-c.d','com0','com10','lpt0','auxiliary','a'*255]
invalid=['','a'*256,'A','aB','a b','雪','../a','.','..','a/../b','a/b','a\\b','a:b','a..b','.a','a.','-a','a-','_a','a_','con','con.game','prn','aux','nul']
invalid += [f'{x}{n}{suffix}' for x in ('com','lpt') for n in range(1,10) for suffix in ('','.game')]
def literal(value):
    return '"'+value.replace('\\','\\\\').replace('"','\\"')+'"'
with tempfile.TemporaryDirectory(prefix='VOID OS errors ') as directory:
    work=Path(directory)
    fixture=work/'main.void'
    s=['use os','func main():','    var accepted = 0','    var rejected = 0']
    for i,value in enumerate(valid+invalid):
        for role in ('data','config','cache','state'):
            s += [f'    if os.{role}_dir({literal(value)}) is Err(var reason{i}{role}):']
            code=3 if value in valid and sys.platform=='linux' else 2
            s += [f'        if reason{i}{role}.code == {code} and reason{i}{role}.operation == "os.{role}_dir" and reason{i}{role}.os_code == 0 and reason{i}{role}.native_domain == "":','            rejected += 1']
            s += ['    else:','        accepted += 1']
    s += ['    print(accepted, rejected)']
    fixture.write_text('\n'.join(s)+'\n')
    env=dict(os.environ,HOME='relative',XDG_DATA_HOME='',XDG_CONFIG_HOME='',XDG_CACHE_HOME='',XDG_STATE_HOME='')
    expected=f'0 {4*(len(valid)+len(invalid))}\n'.encode() if sys.platform=='linux' else f'{4*len(valid)} {4*len(invalid)}\n'.encode()
    for flags in (['--no-opt','--no-cache'],['--no-cache']):
        h.run([compiler,'run',fixture,*flags],work,expected,env=env,small=small)
        binary=work/('errors'+str(bool('--no-opt' in flags))+('.exe' if os.name=='nt' else ''))
        h.run([compiler,'build',fixture,'-o',binary,*[flag for flag in flags if flag!='--no-cache']],work,small=small)
        h.run([binary],work,expected,env=env,small=small,sources=(fixture,))
    # Embedded NULs are constructed through existing strict string decoding.
    fixture.write_text('''use os
use convert
func main():
    var zero = convert.unwrap(string.from_utf8([0]))
    for name in ["", "a=b", zero, "a" + zero + "b"]:
        if os.env(name) is Err(var reason):
            print(reason.code, reason.operation, reason.os_code, reason.native_domain)
    if os.data_dir("a" + zero + "b") is Err(var reason):
        print(reason.code, reason.operation, reason.os_code, reason.native_domain)
''')
    expected=b'2 os.env 0 \n'*4+b'2 os.data_dir 0 \n'
    for flags in (['--no-opt','--no-cache'],['--no-cache']):
        h.run([compiler,'run',fixture,*flags],work,expected,env=env,small=small)
print(f'OS errors: {h.checks} process checks, {len(valid)} valid/{len(invalid)} invalid IDs x four getters; hard_stack_1MiB={small}')
