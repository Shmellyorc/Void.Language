"""Replay only unsuccessful cases from the completed Windows CTest runs."""
from pathlib import Path
import json, os, re, shutil, subprocess, sys, tempfile, xml.etree.ElementTree as ET

share=Path(r'\\vboxsvr\VM\slice7-final-integrated')
local=Path.home()/'VS7I2'
sdk=Path.home()/'VoidCI42377496702-Native/.toolchains/llvm-23.1.4/bin'
ctest=Path(r'C:\gcc\bin\ctest.exe')
results={}
modes=tuple(sys.argv[1:]) or ('Release','Debug')
assert set(modes)<={'Release','Debug'}
receipt='retry-results-'+modes[0]+'.json' if len(modes)==1 else 'retry-results.json'
for mode in modes:
    first=json.loads((share/(mode+'-result.json')).read_text())
    assert 'installed_sha256' in first,first
    test=ET.parse(share/(mode+'-regression.xml')).getroot()
    failed=sorted(case.get('name') for case in test.findall('testcase')
                  if case.find('failure') is not None)
    if not failed:
        results[mode]={'failed_initially':[]}
        continue
    scratch=Path(tempfile.mkdtemp(prefix='retry-',dir=local))
    record={'failed_initially':failed,'scratch':str(scratch)}
    results[mode]=record
    try:
        who=subprocess.check_output(['whoami.exe'],text=True).strip()
        subprocess.run(['icacls.exe',scratch,'/grant:r',who+':(OI)(CI)F','/inheritance:r'],check=True,capture_output=True)
        subprocess.run(['icacls.exe',scratch,'/remove:g','*S-1-3-4'],check=True,capture_output=True)
        record['acl']=subprocess.check_output(['icacls.exe',scratch],text=True)
        env=dict(os.environ,TEMP=str(scratch),TMP=str(scratch),VOID_CLANG=str(sdk/'clang.exe'),
                 PYTHONDONTWRITEBYTECODE='1',PATH=str(sdk)+os.pathsep+os.environ['PATH'])
        build=local/mode
        # The family CMake scripts locate generated executables through -DBIN.
        # These indirect targets are absent from the CTest command argv.
        families={'fixed_arrays','growable_arrays','arrays','generics','collections','dictionaries',
                  'properties','patterns','advanced','managed','managed_initialization',
                  'reference_values','composition','inheritance','interfaces','strings'}
        requested={name.removesuffix('_cli') for name in failed if name.endswith('_cli')} & families
        if requested:
            ninja=ctest.with_name('ninja.exe')
            targets=subprocess.check_output([ninja,'-C',build,'-t','targets','all'],text=True)
            selected=[]
            for family in sorted(requested):
                pattern=r'^void_'+re.escape(family)+r'_cli_\w+_(?:normal|optimized)\.exe(?=: )'
                found=re.findall(pattern,targets,re.M)
                assert len(found)>=2,(family,found)
                selected.extend(found)
            record['built_indirect_targets']=selected
            with (share/(mode+'-retry-build.txt')).open('wb') as output:
                argv=[ctest.with_name('cmake.exe'),'--build',build,'--parallel','3','--target',*selected]
                output.write((json.dumps(list(map(str,argv)))+'\n').encode())
                output.flush()
                subprocess.run(argv,stdout=output,stderr=subprocess.STDOUT,env=env,check=True,timeout=5400)
        with (share/(mode+'-retry.txt')).open('wb') as output:
            status=subprocess.run([ctest,'--test-dir',build,'--rerun-failed','--parallel','1',
                '--output-on-failure','--output-junit',share/(mode+'-retry.xml')],
                stdout=output,stderr=subprocess.STDOUT,env=env,timeout=3600).returncode
        shutil.copy2(build/'Testing/Temporary/LastTest.log',share/(mode+'-retry-LastTest.txt'))
        record['exit']=status
    finally:
        record['leftovers']=sorted(p.relative_to(scratch).as_posix() for p in scratch.rglob('*'))
        shutil.rmtree(scratch)
        record['removed']=not scratch.exists()
        (share/receipt).write_text(json.dumps(results,indent=2)+'\n')
(share/receipt).write_text(json.dumps(results,indent=2)+'\n')
sys.exit(0 if all(x.get('exit',0)==0 for x in results.values()) else 1)
