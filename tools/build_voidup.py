"""Build the VOID source application and a relocatable Linux bootstrap prefix.

This development tool does no installation, trust or activation work. All such
decisions remain in apps/voidup/*.void. Requires a matching built voidc and its
native manifests; output must be new. Use an Ubuntu-built compiler for the floor.
"""
from pathlib import Path
import argparse, json, os, shutil, subprocess, sys, tempfile
ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault('VOID_CRASH_EVIDENCE',str(ROOT/'docs/voidup/evidence/phase_b/crashes'))
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
from crash import run

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--prefix',required=True,type=Path)
    parser.add_argument('--no-opt',action='store_true')
    parser.add_argument('--licenses',required=True,type=Path,
                        help='corresponding compiler component share/void/licenses')
    options=parser.parse_args()
    if sys.platform!='linux':
        raise SystemExit('bootstrap packaging is Linux-only; native platform packaging is unqualified')
    compiler=options.compiler.resolve();prefix=options.prefix.resolve()
    prefix.mkdir(mode=0o700)  # Exclusive: never replace an existing application.
    (prefix/'bin').mkdir();(prefix/'lib/void').mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix='voidup-build-') as folder:
        staging=Path(folder);driver=staging/'voidc';shutil.copy2(compiler,driver)
        config=compiler.parent/'void-native.txt'
        lines=config.read_text().splitlines()
        assert lines[0]=='VOID native 23.1.4 v2'
        for index in range(1,5):
            if lines[index] and not Path(lines[index]).is_absolute():
                lines[index]=str((compiler.parent/lines[index]).resolve())
        marker='@VOID_NATIVE_DIR@'
        lines=[line.replace(marker,str(compiler.parent)) for line in lines]
        for index,line in enumerate(lines):
            if line=='-rpath':lines[index+2]='$ORIGIN/../lib/void'
        (staging/'void-native.txt').write_text('\n'.join(lines)+'\n')
        (staging/'void-native-resources.txt').write_text((compiler.parent/'void-native-resources.txt').read_text().replace(marker,str(compiler.parent)))
        for line in lines[5:]:
            library=Path(line)
            if library.is_absolute() and '.so' in library.name:
                # Include every SONAME alias as a regular file. No ambient
                # loader path is needed at runtime.
                for alias in library.parent.glob(library.name+'*'):
                    if alias.is_file():shutil.copy2(alias,prefix/'lib/void'/alias.name)
        command=[driver,'build',ROOT/'apps/voidup/main.void','-o',prefix/'bin/voidup']
        if options.no_opt:command.append('--no-opt')
        result=run(list(map(str,command)),cwd=ROOT,capture_output=True,
                   source_inputs=(ROOT/'apps/voidup',),timeout=900)
        (prefix/'build.stdout').write_bytes(result.stdout)
        (prefix/'build.stderr').write_bytes(result.stderr)
        if result.returncode:raise SystemExit(result.returncode)
    shutil.copytree(options.licenses,prefix/'share/void/licenses',symlinks=False)
    (prefix/'build.json').write_text(json.dumps(dict(schema=1,compiler=str(compiler),
        source=str(ROOT/'apps/voidup/main.void'),optimized=not options.no_opt,
        production_trust=False),indent=2)+'\n')
    print(prefix/'bin/voidup')

if __name__=='__main__':main()
