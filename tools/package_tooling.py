"""Stage an owner-local native server archive and optional target VSIX.
No network, publication, dependency copying or redistribution authorization.
"""
import argparse, hashlib, json, os, pathlib, platform, shutil, subprocess, tarfile, tempfile, zipfile
p=argparse.ArgumentParser();p.add_argument('--server',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--target',choices=['linux-x64','win32-x64','darwin-arm64'],required=True);p.add_argument('--vsix',action='store_true');p.add_argument('--llvm-notice',type=pathlib.Path);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[1];server=a.server.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
expected={'linux-x64':('Linux','x86_64'),'win32-x64':('Windows','AMD64'),'darwin-arm64':('Darwin','arm64')}
if (platform.system(),platform.machine())!=expected[a.target]:raise SystemExit('Build and audit the artifact on its native target; cross-platform claims are unsupported')
tool='ldd' if platform.system()=='Linux' else 'otool' if platform.system()=='Darwin' else 'llvm-readobj'
args=[tool,str(server)] if tool=='ldd' else [tool,'-L',str(server)] if tool=='otool' else [tool,'--coff-imports',str(server)]
audit=subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True).stdout
with tempfile.TemporaryDirectory(prefix='void-tooling-package-') as temp:
    stage=pathlib.Path(temp)/('void-tooling-'+a.target);(stage/'bin').mkdir(parents=True)
    shutil.copy2(server,stage/'bin'/server.name)
    notice=a.llvm_notice or next((p for p in [pathlib.Path('/usr/share/doc/llvm-23/copyright'),pathlib.Path('/usr/lib/llvm-23/share/llvm/LICENSE.TXT')] if p.is_file()),None)
    if not notice or not notice.is_file():raise SystemExit('Supply --llvm-notice with the pinned SDK LLVM license/notice file')
    (stage/'NOTICES').mkdir();shutil.copy2(notice,stage/'NOTICES/LLVM-23.1.4.txt')
    shutil.copytree(repo/'editors/vim',stage/'share/void/vim');shutil.copytree(repo/'editors/neovim',stage/'share/void/neovim')
    (stage/'DEPENDENCIES.txt').write_text(audit)
    (stage/'LOCAL-ACCEPTANCE.txt').write_text('Private local acceptance artifact. Repository UNLICENSED; no public redistribution authorized.\nDependencies listed separately are required from the host OS. This is not a self-contained portable bundle.\nLLVM/Clang/LLD 23.1.4 remains the build toolchain.\n')
    (stage/'manifest.json').write_text(json.dumps({'target':a.target,'sha256':hashlib.sha256(server.read_bytes()).hexdigest(),'dependencies':audit,'publicRedistribution':False},indent=2)+'\n')
    archive=out/(stage.name+('.zip' if os.name=='nt' else '.tar.gz'))
    if os.name=='nt':
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
            for f in stage.rglob('*'):
                if f.is_file():z.write(f,f.relative_to(stage.parent))
    else:
        with tarfile.open(archive,'w:gz') as tar:tar.add(stage,arcname=stage.name)
    # Verify archive extraction and an actual protocol handshake without SDK PATH.
    sys_path=pathlib.Path(temp)/'unpacked';shutil.unpack_archive(str(archive),str(sys_path))
    executable=sys_path/stage.name/'bin'/server.name
    def frame(v):
        b=json.dumps(v).encode();return b'Content-Length: '+str(len(b)).encode()+b'\r\n\r\n'+b
    fixture=pathlib.Path(temp)/'main.void';fixture.write_text('func entry(x: i32) -> i32:\n    return x\n')
    messages=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'capabilities':{}}},{'jsonrpc':'2.0','method':'initialized','params':{}},{'jsonrpc':'2.0','id':2,'method':'shutdown'},{'jsonrpc':'2.0','method':'exit'}]
    env=dict(os.environ);env['PATH']=os.defpath
    child=subprocess.run([str(executable),'--entry',str(fixture)],input=b''.join(map(frame,messages)),stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,timeout=30,check=True)
    assert b'voidToolingProtocol' in child.stdout,child.stdout
    print(archive)
    if a.vsix:
        extension=pathlib.Path(temp)/'extension';shutil.copytree(repo/'editors/vscode',extension,symlinks=True,ignore=shutil.ignore_patterns('server','out','dist','.vscode-test','node_modules'))
        shutil.copytree(repo/'editors/vscode/node_modules',extension/'node_modules',symlinks=True)
        (extension/'server').mkdir();shutil.copy2(server,extension/'server'/server.name)
        shutil.copytree(stage/'NOTICES',extension/'server/NOTICES')
        (extension/'server/DEPENDENCIES.txt').write_text(audit)
        (extension/'server/LOCAL-ACCEPTANCE.txt').write_text((stage/'LOCAL-ACCEPTANCE.txt').read_text())
        subprocess.run(['npm','run','build'],cwd=extension,check=True)
        subprocess.run([str(extension/('node_modules/.bin/vsce.cmd' if os.name=='nt' else 'node_modules/.bin/vsce')),'package','--no-dependencies','--allow-missing-repository','--skip-license','--target',a.target,'--out',str(out/('void-language-'+a.target+'.vsix'))],cwd=extension,check=True)
