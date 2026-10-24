"""Run the original/repaired LSP fixture with a cp1252 default, on Linux."""
from pathlib import Path
import sys
original_write=Path.write_text
def windows_default(self,data,encoding=None,errors=None,newline=None):
    return original_write(self,data,encoding='cp1252' if encoding is None else encoding,errors=errors,newline=newline)
Path.write_text=windows_default
file=Path('tests/stage10/lsp_source_formatting.py').resolve()
source=Path(sys.argv[1]).read_text(encoding='utf-8')
sys.argv=[str(file),str(Path('build/ci-official-debug/voidls').resolve())]
exec(compile(source,str(file),'exec'),{'__name__':'__main__','__file__':str(file)})
