from pathlib import Path
import json,shlex,subprocess,sys
root=Path.cwd();profile=sys.argv[1];build=root/('build/llvm23-'+profile);work=root/'.toolchains/ci41336679181'/('simulate-'+profile);work.mkdir(exist_ok=True)
# Select only the Apple conversion branch, leaving host platform headers and
# every other runtime translation unit unchanged. This is not a macOS build.
source=work/'conversion.cpp';source.write_text((root/'src/runtime/conversion.cpp').read_text().replace('#ifdef __APPLE__','#if 1'))
entry=next(e for e in json.loads((build/'compile_commands.json').read_text()) if e['file'].endswith('/src/runtime/conversion.cpp'))
args=shlex.split(entry['command']);obj=work/'conversion.o';args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(source);args+=['-I'+str(root/'src/runtime')];subprocess.run(args,cwd=build,check=True)
args=shlex.split((build/'CMakeFiles/void_decimal_float.dir/link.txt').read_text());args.insert(1,str(obj));exe=work/'probe';args[args.index('-o')+1]=str(exe);subprocess.run(args,cwd=build,check=True);subprocess.run([exe],check=True)
