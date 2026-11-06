import qualify as h
import time
for profile in ['debug','strict']:
 while not (h.E/(profile+'-LastTest.log')).exists():time.sleep(5)
 h.run('editor-'+profile,['python3',h.AREA/'editor_capture.py','editor-'+profile,str(h.ROOT/('build/llvm23-'+profile))])
