"""Pinned, disposable Windows AWS-LC/curl research build; no production setup."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
import subprocess
import tarfile
import time
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT/'.toolchains/native-awslc'
LLVM = ROOT/'.toolchains/native-schannel/sdk/clang+llvm-23.1.4-x86_64-pc-windows-msvc/bin'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dependencies-only', action='store_true')
    parser.add_argument('--multi', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    pins = json.loads((ROOT/'third_party/http-versions.json').read_text())['dependencies']
    report = {'schema':1, 'scope':'isolated Windows architecture research', 'commands':[], 'archives':{}}
    receipt = OUT/'build-receipt.json'
    if receipt.exists(): shutil.copyfile(receipt,OUT/('earlier-build-receipt-'+hashlib.sha256(receipt.read_bytes()).hexdigest()[:12]+'.json'))
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    def acquire(name, url, digest):
        path = OUT/'downloads'/name; path.parent.mkdir(exist_ok=True)
        if not path.exists():
            with opener.open(url, timeout=120) as src, path.open('wb') as dst:
                while chunk := src.read(1024*1024): dst.write(chunk)
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != digest: raise RuntimeError('archive digest mismatch: '+name)
        report['archives'][name] = {'sha256':actual, 'url':url}
        return path
    source = {}
    for pin in pins:
        if pin['name'] not in ('aws-lc','nghttp2'): continue
        archive = acquire(pin['archive'], pin['url'], pin['sha256'])
        directory = pin.get('source_dir', pin['name']+'-'+pin['version'])
        source[pin['name']] = OUT/'source'/directory
        if not source[pin['name']].exists():
            (OUT/'source').mkdir(exist_ok=True)
            with tarfile.open(archive) as bundle: bundle.extractall(OUT/'source', filter='data')
    nasm_archive = acquire('nasm-2.16.03-win64.zip',
        'https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip',
        '3ee4782247bcb874378d02f7eab4e294a84d3d15f3f6ee2de2f47a46aa7226e6')
    nasm = OUT/'tools/nasm-2.16.03/nasm.exe'
    if not nasm.exists():
        with zipfile.ZipFile(nasm_archive) as bundle:
            for item in bundle.infolist():
                if not (OUT/'tools'/item.filename).resolve().is_relative_to((OUT/'tools').resolve()): raise RuntimeError('archive path')
            bundle.extractall(OUT/'tools')
    def run(label, command):
        started = time.monotonic()
        log_path = OUT/(label+'.log')
        if log_path.exists(): shutil.copyfile(log_path, OUT/('earlier-'+label+'-'+hashlib.sha256(log_path.read_bytes()).hexdigest()[:12]+'.log'))
        with log_path.open('w') as log:
            p = subprocess.run(list(map(str,command)), stdout=log, stderr=subprocess.STDOUT)
        report['commands'].append({'label':label, 'exit':p.returncode, 'seconds':round(time.monotonic()-started,3),
            'argv':[str(x).replace(str(ROOT),'<repository>') for x in command]})
        (OUT/'build-receipt.json').write_text(json.dumps(report,indent=2)+'\n')
        if p.returncode: raise RuntimeError(label+' failed; original log retained')
        print(label+' passed',flush=True)
    run('nasm-version',[nasm,'-v'])
    for profile in ('Debug','Release'):
        base = OUT/profile
        common = ['-G','Ninja',f'-DCMAKE_C_COMPILER={LLVM}/clang-cl.exe',f'-DCMAKE_CXX_COMPILER={LLVM}/clang-cl.exe',
                  f'-DCMAKE_BUILD_TYPE={profile}','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DCMAKE_POLICY_DEFAULT_CMP0091=NEW']
        aws = base/'aws-lc'; ng = base/'nghttp2'; prefix = base/'prefix'
        run(profile+'-aws-configure',['cmake','-S',source['aws-lc'],'-B',aws,*common,
            '-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF','-DBUILD_LIBSSL=ON','-DDISABLE_GO=ON','-DENABLE_SOURCE_MODIFICATION=OFF',
            '-DPERL_EXECUTABLE=C:/Program Files/Git/usr/bin/perl.exe',
            f'-DNASM_EXECUTABLE={nasm}',f'-DCMAKE_ASM_NASM_COMPILER={nasm}'])
        run(profile+'-aws-build',['cmake','--build',aws,'--target','ssl','crypto','--parallel','4'])
        run(profile+'-nghttp2-configure',['cmake','-S',source['nghttp2'],'-B',ng,*common,f'-DCMAKE_INSTALL_PREFIX={prefix}',
            '-DENABLE_LIB_ONLY=ON','-DBUILD_SHARED_LIBS=OFF','-DBUILD_STATIC_LIBS=ON','-DBUILD_TESTING=OFF','-DENABLE_DOC=OFF'])
        run(profile+'-nghttp2-build',['cmake','--build',ng,'--parallel','4'])
        run(profile+'-nghttp2-install',['cmake','--install',ng])
        if args.dependencies_only: continue
        build = base/('multi' if args.multi else 'curl')
        run(profile+'-curl-configure',['cmake','-S',HERE,'-B',build,*common,
            f'-DAWS_SOURCE={source["aws-lc"]}',f'-DAWS_BUILD={aws}',f'-DH2_PREFIX={prefix}',f'-DSTUDY_MULTI={"ON" if args.multi else "OFF"}'])
        run(profile+'-curl-build',['cmake','--build',build,'--target','probe','--parallel','4'])

if __name__ == '__main__': main()
