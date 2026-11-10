from collections import defaultdict
from pathlib import Path
import json, shutil
root=Path.home()/'VS7I2'
build=root/'Debug'
share=Path(r'\\vboxsvr\VM\slice7-final-integrated')
data={}
for path in (root/'Release',build,root/'Debug-public-utf8',root/'Release-public-utf8',root/'Release-public'):
    totals=defaultdict(lambda:[0,0])
    for p in path.rglob('*'):
        if p.is_file():
            key=('family_' if p.name.startswith('void_') and '_cli_' in p.name else '')+p.suffix.lower()
            row=totals[key]
            row[0]+=1
            row[1]+=p.stat().st_size
    data[str(path)]=dict(totals)
data['free_bytes']=shutil.disk_usage(root).free
(share/'debug-disk.json').write_text(json.dumps(data,indent=2)+'\n')
