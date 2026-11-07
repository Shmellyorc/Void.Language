"""COFF source checks only. These do not establish Windows application support."""
from pathlib import Path
import hashlib,json,subprocess,time
root=Path.cwd();evidence=root/'docs/voidup/evidence/phase_b'
output=root/'build/voidup-phaseb-cross';output.mkdir(exist_ok=True)
sources=['host_abi','host_directory','host_directory_windows','host_process','host_process_windows','host_archive','host_crypto','host_error','application','filesystem']
record=dict(schema=1,native_execution=False,scope='MinGW x64 source compile only; no Windows VOIDUP JIT/AOT acceptance',commands=[])
for source in sources:
 path=root/'src/runtime'/(source+'.cpp');target=output/(source+'.o')
 args=['x86_64-w64-mingw32-g++','-std=c++20','-O2','-D_WIN32_WINNT=0x0A00','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-Iinclude','-Isrc/runtime','-Ibuild/voidup-cross/providers/include','-c',str(path),'-o',str(target)]
 stamp=time.monotonic();result=subprocess.run(args,capture_output=True)
 (evidence/('windows-phaseb-'+source+'.stdout')).write_bytes(result.stdout)
 (evidence/('windows-phaseb-'+source+'.stderr')).write_bytes(result.stderr)
 record['commands'].append(dict(command=args,status=result.returncode,seconds=time.monotonic()-stamp,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),object_sha256=hashlib.sha256(target.read_bytes()).hexdigest() if result.returncode==0 else None))
 (evidence/'windows-phaseb-cross.json').write_text(json.dumps(record,indent=2)+'\n')
 if result.returncode:raise SystemExit(result.returncode)
print('Ten native source units cross-compiled; Windows VOIDUP remains UNQUALIFIED')
