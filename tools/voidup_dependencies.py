"""Record and resolve the actual native runtime closure before packaging."""
import os
from pathlib import Path
import re
import subprocess
import sys

def audit(prefix,sdk,images):
    prefix=Path(prefix).resolve();sdk=Path(sdk).resolve();records={}
    environment=dict(os.environ)
    for key in ('LD_LIBRARY_PATH','LD_PRELOAD','DYLD_LIBRARY_PATH','DYLD_INSERT_LIBRARIES','DYLD_FALLBACK_LIBRARY_PATH'):
        environment.pop(key,None)
    for image in images:
        image=Path(image).resolve()
        if sys.platform=='darwin':
            from voidup_macos import inspect
            records[str(image.relative_to(prefix))]=inspect(image);continue
        windows=os.name=='nt'
        inspector=sdk/'bin'/('llvm-readobj.exe' if windows else 'llvm-readelf')
        args=['--file-headers','--coff-imports'] if windows else ['--file-header','--dynamic','--version-info']
        report=subprocess.check_output([str(inspector),*args,str(image)],text=True,env=environment)
        resolved={}
        if windows:
            if 'IMAGE_FILE_MACHINE_AMD64' not in report:raise ValueError('Non-x64 Windows payload image')
            libraries=sorted(set(re.findall(r'^\s*Name: (\S+\.dll)\s*$',report,re.M|re.I)))
            for library in libraries:
                if library.lower().startswith(('api-ms-win-','ext-ms-')):resolved[library]='Windows API-set';continue
                local=image.parent/library;system=Path(os.environ['SystemRoot'])/'System32'/library
                if local.is_file():resolved[library]=str(local.relative_to(prefix))
                elif system.is_file():resolved[library]='System32/'+library
                else:raise ValueError('Unbundled Windows DLL: '+library)
            if image.parent==prefix/'bin' and any(re.fullmatch(r'(?:msvcr\d.*|msvcp\d.*|vcruntime\d.*|ucrtbased)\.dll',name,re.I) for name in libraries):
                raise ValueError('VOID executable violates the static /MT CRT contract')
        else:
            if 'Advanced Micro Devices X86-64' not in report and 'EM_X86_64' not in report:raise ValueError('Non-x64 Linux payload image')
            libraries=sorted(set(re.findall(r'\(NEEDED\).*\[([^\]]+)\]',report)))
            loaded=subprocess.check_output(['/usr/bin/ldd',str(image)],text=True,env=environment,stderr=subprocess.STDOUT)
            if 'not found' in loaded:raise ValueError('Unresolved Linux payload dependency: '+loaded)
            for library,path in re.findall(r'^\s*(\S+) => (/\S+) ',loaded,re.M):
                location=Path(path).resolve()
                if location.is_relative_to(prefix):resolved[library]=str(location.relative_to(prefix))
                elif any(location.is_relative_to(Path(root)) for root in ['/usr/lib','/lib','/lib64']):resolved[library]=str(location)
                else:raise ValueError('Linux payload depends on producer-local library: '+path)
        records[str(image.relative_to(prefix))]={'libraries':libraries,'abi_versions':sorted(set(re.findall(r'GLIBC(?:XX)?_[0-9.]+',report))),'resolved_dependencies':resolved}
    return records
