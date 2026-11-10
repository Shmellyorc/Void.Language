from pathlib import Path
import json
import subprocess
home=Path.home()
w=Path(r'\\vboxsvr\VM\project-slice7-g3')
for gate in (1,2):
    b=home/f'VoidS7-G{gate}-Release'
    commands=json.loads((b/'compile_commands.json').read_text())
    entry=next(c for c in commands if c['file'].replace('\\','/').endswith('tests/analysis/tests.cpp'))
    last=subprocess.check_output([r'C:\gcc\bin\ninja.exe','-t','commands','void_tests.exe'],cwd=b,text=True).splitlines()[-1]
    (w/f'inspect-g{gate}.json').write_text(json.dumps({'compile':entry,'link':last},indent=2)+'\n')
