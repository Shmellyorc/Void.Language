"""Execution-time Linux root/encoding policies and independent JIT/AOT process identity."""
from pathlib import Path
import os
import platform
import shutil
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve()
small='--small-stack' in sys.argv
if sys.platform != 'linux':
    raise SystemExit('Linux policy fixture requires Linux; native adapters have separate host tests')
import pwd
source=h.ROOT/'tests/os/locations.void'
exec_host=h.ROOT/'tests/os/exec_host.py'

def line(key,text):
    return f'{key} ok {list(text.encode())}\n'.encode()
def err(key,code,op):
    return f'{key} {code} os.{op}  0\n'.encode()
def expected(env):
    out=b''
    home=env.get('HOME',b'')
    home=home.encode() if isinstance(home,str) else home
    if not home: home=pwd.getpwuid(os.getuid()).pw_dir.encode()
    def location(key,raw,op):
        if not raw.startswith(b'/'):return err(key,3,op)
        try:return line(key,raw.decode())
        except UnicodeDecodeError:return err(key,4,op)
    for key,variable,suffix in [('data','XDG_DATA_HOME',b'.local/share'),('config','XDG_CONFIG_HOME',b'.config'),('cache','XDG_CACHE_HOME',b'.cache'),('state','XDG_STATE_HOME',b'.local/state')]:
        raw=env.get(variable,b'');raw=raw.encode() if isinstance(raw,str) else raw
        raw=raw if raw.startswith(b'/') else home.rstrip(b'/')+b'/'+suffix
        out+=location(key,raw+b'/com.studio.game',key+'_dir')
    out+=location('home',home,'home_dir')
    raw=env.get('TMPDIR',b'');raw=raw.encode() if isinstance(raw,str) else raw
    out+=location('temp',raw or b'/tmp','temp_dir')
    value=env.get('VOID_OS_VALUE')
    if value is None:out+=err('env',1,'env')
    else:
        try:out+=line('env',value.decode() if isinstance(value,bytes) else value)
        except UnicodeDecodeError:out+=err('env',4,'env')
    out+=err('missing',1,'env')
    arch={'x86_64':'x86_64','aarch64':'aarch64','i686':'x86','armv7l':'arm'}.get(platform.machine(),'unknown')
    return out+f'image true\nresources true\npid true\nplatform linux\narch {arch}\nbundle false\n'.encode()
with tempfile.TemporaryDirectory(prefix='VOID OS execution ') as directory:
    work=Path(directory);fixture=work/'main.void';fixture.write_bytes(source.read_bytes())
    base=dict(os.environ)
    for name in ('HOME','TMPDIR','XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME','XDG_STATE_HOME','VOID_OS_VALUE','VOID_OS_MISSING'):base.pop(name,None)
    base.update(HOME=str(work/'uncreated 雪 home'),VOID_OS_VALUE='  雪😀\n=a=b  ')
    roots={name:str(work/(name+' 雪 # % /../literal')) for name in ('XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME','XDG_STATE_HOME')}
    cases=[base,dict(base,**roots),dict(base,HOME='relative',**roots),dict(base,HOME='relative'),dict(base,HOME=''),
        {k:v for k,v in base.items() if k!='HOME'},dict(base,TMPDIR=''),dict(base,TMPDIR='relative'),
        dict(base,TMPDIR=str(work/'temporary nonexistent')),dict(base,VOID_OS_VALUE='')]
    for name in roots:
        for value in ('','relative',b'relative\xff',b'/absolute\xff'):
            cases.append(dict(base,**{name:value}))
    cases.extend([dict(base,HOME=b'/home\xff'),dict(base,HOME=b'relative\xff'),dict(base,TMPDIR=b'/tmp\xff'),dict(base,VOID_OS_VALUE=b'\xff')])
    for flags in (['--no-opt','--no-cache'],['--no-cache']):
        # Actual normal/O2 JIT, with an unrelated cwd and no source-identity fallback.
        for env in cases[:4]+cases[-4:]:
            h.run([sys.executable,exec_host,compiler,'run',fixture,*flags],work,expected(env),env=env,small=small)
        binary=work/('native-'+str(bool(flags[0]=='--no-opt')))
        h.run([compiler,'build',fixture,'-o',binary,*[flag for flag in flags if flag!='--no-cache']],work,env=base,small=small)
        for env in cases:
            h.run([sys.executable,exec_host,binary],work,expected(env),env=env,sources=(fixture,),small=small)
        # Exec through a launch symlink still describes the actual image.
        alias=work/('launch-'+binary.name);alias.symlink_to(binary)
        h.run([sys.executable,exec_host,alias],work,expected(base),env=base,sources=(fixture,),small=small)
    assert not (work/'uncreated 雪 home').exists()
    assert not (work/'temporary nonexistent').exists()
    # No discovery-created directories anywhere below test-owned tree.
    assert not any(x.is_dir() for x in work.iterdir() if x.name!='.void'),list(work.iterdir())
print(f'OS location/identity: {h.checks} process checks passed; hard_stack_1MiB={small}')
