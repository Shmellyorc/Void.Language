"""Native macOS packaging helpers; no SDK redistribution or trust decisions.

Use Apple's install_name_tool and codesign to relocate the admitted Mach-O
dependency closure. Ad-hoc signing permits local qualification only; Developer
ID signing/notarization remains a separate, authorized distribution operation.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

SYSTEM=('/usr/lib/','/System/Library/')

def output(*args):
    return subprocess.check_output(list(map(str,args)),text=True)

def inspect(image):
    if output('/usr/bin/lipo','-archs',image).strip()!='arm64':
        raise ValueError('macOS payload must contain native ARM64 images: '+str(image))
    loads=output('/usr/bin/otool','-l',image)
    # LC_BUILD_VERSION minos or legacy LC_VERSION_MIN_MACOSX, never SDK version.
    minima=re.findall(r'cmd LC_BUILD_VERSION\n(?:(?!\n\s*cmd ).)*?\n\s*minos ([0-9.]+)',loads,re.S)
    minima+=re.findall(r'cmd LC_VERSION_MIN_MACOSX\n\s*cmdsize \d+\n\s*version ([0-9.]+)',loads)
    platforms=re.findall(r'cmd LC_BUILD_VERSION\n\s*cmdsize \d+\n\s*platform (\S+)',loads)
    if any(p not in ('1','MACOS','macos') for p in platforms):raise ValueError('Mach-O payload targets a different Apple platform')
    if not minima or any(tuple(map(int,v.split('.')))>(14,0,0) for v in [m+'.0' if m.count('.')==1 else m for m in minima]):
        raise ValueError('Mach-O minimum exceeds the declared macOS 14.0 floor: '+str(image))
    dependencies=[line.strip().split(' (',1)[0] for line in output('/usr/bin/otool','-L',image).splitlines()[1:]]
    ids=output('/usr/bin/otool','-D',image).splitlines()[1:]
    identifier=ids[0].strip() if ids else None
    dependencies=[d for d in dependencies if d!=identifier]
    rpaths=re.findall(r'cmd LC_RPATH\n\s*cmdsize \d+\n\s*path (.+?) \(offset \d+\)',loads)
    return dict(dependencies=dependencies,rpaths=rpaths,identifier=identifier,minimum=minima)

def relocate(prefix, origins, allowed_roots, dependency_directory='sdk/lib/void-dependencies'):
    if sys.platform!='darwin':raise ValueError('Mach-O relocation requires native macOS')
    prefix=Path(prefix).resolve();allowed_roots=[Path(p).resolve() for p in allowed_roots]
    def package_path(path):
        # macOS /var may alias /private/var. Use the same canonical spelling
        # for package images, copy targets and the final relative inventory.
        path=Path(path).resolve()
        if not path.is_relative_to(prefix):raise ValueError('Mach-O path is outside package: '+str(path))
        return path
    destination=package_path(prefix/dependency_directory);records={};copied={};visiting=set()
    executables=[package_path(image) for image in origins]
    def internal_rpath(path,image):
        # Keep already relocatable search paths without editing signed LLVM
        # binaries. Resolve '..' and symlinks; absolute/ambient/unknown paths
        # are not portable. A shared dylib may be loaded by any packaged entry.
        if path=='@loader_path' or path.startswith('@loader_path/'):
            token='@loader_path';bases=[image.parent]
        elif path=='@executable_path' or path.startswith('@executable_path/'):
            token='@executable_path';bases=[p.parent for p in executables]
        else:return False
        return bool(bases) and all(Path(str(base)+path[len(token):]).resolve().is_relative_to(prefix) for base in bases)
    def resolve(reference,origin,details,executable):
        def expand(path):
            return path.replace('@loader_path',str(origin.parent)).replace('@executable_path',str(executable.parent))
        if reference.startswith('@rpath/'):
            candidates=[Path(expand(p))/reference[len('@rpath/'):] for p in details['rpaths']]
            # LLVM's adjacent lib directory is an admitted SDK location; no
            # Homebrew, DYLD_* or ambient loader search is used by the producer.
            candidates.append(origin.parent.parent/'lib'/reference[len('@rpath/'):])
        else:candidates=[Path(expand(reference))]
        for candidate in candidates:
            if candidate.is_absolute() and candidate.is_file():
                candidate=candidate.resolve()
                if any(candidate.is_relative_to(root) for root in allowed_roots):return candidate
                raise ValueError('Non-system dependency is outside admitted build/SDK inputs: '+str(candidate))
        raise ValueError('Unresolved Mach-O dependency: '+reference)
    def visit(image,origin,executable):
        image=package_path(image)
        if image in records:return
        if image in visiting:return
        visiting.add(image);details=inspect(image);changes=[]
        for reference in details['dependencies']:
            if reference.startswith(SYSTEM):continue
            library=resolve(reference,origin,details,executable)
            target=package_path(destination/library.name)
            if target.name in copied and copied[target.name]!=library:
                if library.read_bytes()!=copied[target.name].read_bytes():raise ValueError('Conflicting Mach-O dependency basenames')
            if target.name not in copied:
                destination.mkdir(parents=True,exist_ok=True);shutil.copy2(library,target);copied[target.name]=library
            visit(target,library,executable)
            changes+=['-change',reference,'@loader_path/'+os.path.relpath(target,image.parent)]
        if details['identifier']:changes+=['-id','@rpath/'+image.name]
        for path in details['rpaths']:
            if not internal_rpath(path,image):changes+=['-delete_rpath',path]
        if changes:
            # Path edits invalidate existing signatures. Re-sign only after all
            # edits; this does not create a production signing credential.
            signed=subprocess.run(['/usr/bin/codesign','--display',str(image)],capture_output=True).returncode==0
            if signed:subprocess.run(['/usr/bin/codesign','--remove-signature',str(image)],check=True)
            subprocess.run(['/usr/bin/install_name_tool',*changes,str(image)],check=True)
            subprocess.run(['/usr/bin/codesign','--force','--sign','-',str(image)],check=True)
        records[image]=inspect(image);visiting.remove(image)
    for image,origin in origins.items():visit(Path(image),Path(origin).resolve(),Path(origin).resolve())
    for image,details in records.items():
        for path in details['rpaths']:
            if not internal_rpath(path,image):raise ValueError('Mach-O RPATH escaped the relocated payload: '+path)
        for reference in details['dependencies']:
            if reference.startswith(SYSTEM):continue
            if not reference.startswith('@loader_path/') or not (image.parent/reference[len('@loader_path/'):]).resolve().is_relative_to(prefix):
                raise ValueError('Mach-O dependency escaped the relocated payload')
            if not (image.parent/reference[len('@loader_path/'):]).is_file():raise ValueError('Relocated dependency is missing')
        subprocess.run(['/usr/bin/codesign','--verify','--strict',str(image)],check=True)
    return {p.relative_to(prefix).as_posix():d for p,d in records.items()}

def driver_wrapper():
    # Native links/imports resolve the consumer's selected Apple SDK. Never
    # retain a producer's Xcode path or redistribute Apple's SDK/vendor headers.
    return '''#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(/usr/bin/dirname "$0")" && pwd)
sdk=$(/usr/bin/xcrun --sdk macosx --show-sdk-path)
case "$sdk" in /*) ;; *) echo 'VOID requires a selected macOS development SDK' >&2; exit 1 ;; esac
cpp=false
for argument do
  case "$argument" in --driver-mode=g++|c++|c++-header|objective-c++) cpp=true ;; esac
done
if "$cpp"; then
  apple=$(/usr/bin/xcrun --find clang++)
  headers="$sdk/usr/include/c++/v1"
  if [ ! -f "$headers/stop_token" ]; then headers="${apple%/*}/../include/c++/v1"; fi
  if [ ! -f "$headers/stop_token" ]; then echo 'Native C++ linking requires selected Xcode 26+ vendor headers' >&2; exit 1; fi
  exec "$here/clang" -isysroot "$sdk" -stdlib=libc++ -nostdinc++ -isystem "$headers" "$@"
fi
exec "$here/clang" -isysroot "$sdk" "$@"
'''

def relocate_manifest(lines):
    if lines[0]!='VOID native 23.1.4 v2':raise ValueError('Unsupported native manifest')
    result=lines[:];result[1:3]=['../sdk/bin/void-clang','../sdk/bin/ld64.lld']
    flags=[];index=5
    while index<len(lines):
        if lines[index] in ('-isystem','-isysroot'):
            if index+1==len(lines):raise ValueError('Missing native SDK/header operand')
            index+=2;continue
        if lines[index]=='-nostdinc++':index+=1;continue
        flags.append(lines[index]);index+=1
    return result[:5]+flags
