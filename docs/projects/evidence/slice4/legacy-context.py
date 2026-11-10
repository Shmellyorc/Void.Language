from pathlib import Path
import json, subprocess, tempfile, sys
compiler=Path(sys.argv[1]).resolve()
rows=[]
source='''use os as system
func entry(n: i32) -> i32:
    if system.data_dir() is Ok(var inherited):
        if system.data_dir("org.test.legacy") is Ok(var explicit):
            if inherited == explicit:
                return n + 2
    return 99
'''
policy="""schema=1
[project]
id='org.test.project'
name='Project'
version='1.2.3'
kind='application'
entry='legacy.void'
[application]
id='org.test.legacy'
[build.release]
debug_info='none'
[build.debug]
debug_info='none'
"""
try:
    with tempfile.TemporaryDirectory(prefix='VOID S4 legacy context ') as tmp:
        root=Path(tmp)/'checkout';root.mkdir()
        stage=Path(tmp)/'installed';stage.mkdir()
        (root/'void.project.toml').write_text(policy)
        (root/'legacy.void').write_text(source)
        def run(args,expected=None):
            p=subprocess.run(list(map(str,args)),cwd=stage,capture_output=True,text=True,timeout=120)
            rows.append({'argv':list(map(str,args)),'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
            assert p.returncode==0 and (expected is None or p.stdout==expected),rows[-1]
        binaries=[]
        for profile in ('debug','release'):
            run([compiler,'run','--project',root,'--profile',profile,'--no-cache','--','17'],'19\n')
            binary=stage/profile
            run([compiler,'build','--project',root,'--profile',profile,'-o',binary]);binaries.append(binary)
        (root/'legacy.void').unlink();(root/'void.project.toml').unlink();root.rmdir()
        for binary in binaries:run([binary,'17'],'19\n')
    print('Legacy manifest entry context and numeric argument: 6 processes passed')
finally:Path(sys.argv[2]).write_text(json.dumps(rows,indent=2)+'\n')
