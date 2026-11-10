from datetime import datetime, timezone
from pathlib import Path
from shutil import disk_usage
import json
import platform
import sys

phase, = sys.argv[1:]
assert phase in ('before','after')
share = Path(r'\\vboxsvr\VM\project-slice7-g4-linux')
system = Path(Path.home().anchor)
space = disk_usage(system)
result = {'phase':phase,'captured_utc':datetime.now(timezone.utc).isoformat(),
          'platform':platform.platform(),'build':sys.getwindowsversion().build,
          'system':str(system),'total_bytes':space.total,'free_bytes':space.free}
(share/f'storage-{phase}.json').write_text(json.dumps(result,indent=2)+'\n')
