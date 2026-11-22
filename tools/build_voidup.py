"""Build the VOID source application and its native platform bootstrap prefix.

This development tool does no installation, trust or activation work. All such
decisions remain in apps/voidup/*.void. Requires a matching built voidc and its
native manifests; output must be new. Use an Ubuntu-built compiler for the floor.
"""
from pathlib import Path
import argparse, hashlib, json, os, platform, re, shutil, subprocess, sys, tempfile
ROOT = Path(__file__).resolve().parents[1]
from engineering_storage import evidence_directory
os.environ.setdefault('VOID_CRASH_EVIDENCE',str(evidence_directory('voidup-qualification')/'crashes'))
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
from crash import run

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path)
    parser.add_argument('--prefix',required=True,type=Path)
    parser.add_argument('--no-opt',action='store_true')
    parser.add_argument('--trust-config',type=Path,help='Reviewed public root configuration; no private key inputs')
    parser.add_argument('--licenses',required=True,type=Path,
                        help='corresponding compiler component share/void/licenses')
    options=parser.parse_args()
    if not options.licenses.is_dir():
        parser.error('--licenses must name the matching existing compiler license directory')
    if (platform.system(),platform.machine().lower()) not in [('Linux','x86_64'),('Windows','amd64'),('Windows','x86_64'),('Darwin','arm64')]:
        raise SystemExit('Use a native supported x64 Linux/Windows or ARM64 macOS build environment')
    suffix='.exe' if os.name=='nt' else ''
    compiler=options.compiler.resolve();prefix=options.prefix.resolve()
    version=subprocess.check_output([str(compiler),'--version'],text=True)
    if 'LLVM 23.1.4\n' not in version:raise RuntimeError('bootstrap compiler must use LLVM 23.1.4')
    prefix.mkdir(mode=0o700)  # Exclusive: never replace an existing application.
    (prefix/'bin').mkdir();(prefix/'lib/void').mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix='voidup-build-') as folder:
        staging=Path(folder);driver=staging/('voidc'+suffix);shutil.copy2(compiler,driver)
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
        source=ROOT/'apps/voidup'
        production=False
        if options.trust_config:
            from voidup_release import read_public_config
            config=read_public_config(options.trust_config)
            source=staging/'source';shutil.copytree(ROOT/'apps/voidup',source)
            production=config['trust']=='production'
            (source/'trust_keys.void').write_text('const up_key_ids: [string] = '+json.dumps([k['id'] for k in config['keys']])+'\nconst up_public_keys: [string] = '+json.dumps([k['public'] for k in config['keys']])+'\n')
            (source/'distribution.void').write_text('const up_official_catalog: string = '+json.dumps(config['catalog'])+'\nconst up_production: bool = '+str(production).lower()+'\nconst up_source_enabled: bool = true\nconst up_trust_label: string = '+json.dumps('production 2-of-3 trust' if production else 'private engineering test trust; not for public distribution')+'\n')
        # Native compilation uses the local producer scratch directory. Publish
        # completed files into the requested prefix afterwards, which may be a
        # normal writable network share. This also keeps linker scratch local.
        compiled=staging/('voidup'+suffix)
        command=[driver,'build',source/'main.void','-o',compiled]
        if options.no_opt:command.append('--no-opt')
        result=run(list(map(str,command)),cwd=ROOT,capture_output=True,
                   source_inputs=(ROOT/'apps/voidup',),timeout=900)
        (prefix/'build.stdout').write_bytes(result.stdout)
        (prefix/'build.stderr').write_bytes(result.stderr)
        if result.returncode:raise SystemExit(result.returncode)
        shutil.copy2(compiled,prefix/'bin'/('voidup'+suffix))
        if sys.platform=='darwin':
            from voidup_macos import relocate
            relocate(prefix,{prefix/'bin/voidup':prefix/'bin/voidup'},[ROOT/'.toolchains',compiler.parent,prefix],dependency_directory='lib/void')
        # The small launcher delegates every toolchain operation to native
        # VOIDUP. It only retains this bootstrap and integrates the user's PATH.
        compiled_source_sha256={path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(source.glob('*.void'))}
        identity_inputs=[prefix/'bin'/('voidup'+suffix),ROOT/'apps/voidup/launcher.cpp',Path(__file__)]+sorted((prefix/'lib').rglob('*'))+sorted(options.licenses.rglob('*'))
        identity_state=hashlib.sha256()
        for item in identity_inputs:
            if item.is_file():
                identity_state.update(item.name.encode());identity_state.update(hashlib.sha256(item.read_bytes()).digest())
        identity=identity_state.hexdigest()
        clang=Path(lines[1]);clangxx=clang.with_name('clang-cl.exe' if os.name=='nt' else 'clang++')
        target=staging/('voidup-init'+suffix)
        if os.name=='nt':
            launch=[clangxx,'/nologo','/std:c++17','/EHsc','/MT','/O2','/clang:-fuse-ld=lld',f'/DVOIDUP_BOOTSTRAP_ID="{identity}"',ROOT/'apps/voidup/launcher.cpp',f'/Fe:{target}','/link','/STACK:1048576','advapi32.lib','shell32.lib','ole32.lib','user32.lib']
        else:
            platform_flags=[f'--ld-path={clang.with_name("ld.lld")}']
            if sys.platform=='darwin':
                sdk=subprocess.check_output(['/usr/bin/xcrun','--show-sdk-path'],text=True).strip()
                apple=Path(subprocess.check_output(['/usr/bin/xcrun','--find','clang++'],text=True).strip())
                headers=Path(sdk)/'usr/include/c++/v1'
                if not (headers/'stop_token').is_file():headers=apple.parent.parent/'include/c++/v1'
                if not (headers/'stop_token').is_file():raise ValueError('Select Xcode 26+ vendor libc++ headers')
                platform_flags=['-stdlib=libc++','-nostdinc++','-isystem',str(headers),'-isysroot',sdk,'-mmacosx-version-min=14.0','-arch','arm64']
            launch=[clangxx,'--no-default-config','-std=c++17','-O2',*platform_flags,f'-DVOIDUP_BOOTSTRAP_ID="{identity}"',ROOT/'apps/voidup/launcher.cpp','-o',target]
        subprocess.run(list(map(str,launch)),check=True,cwd=staging)
        shutil.copy2(target,prefix/'bin'/target.name)
        dependency_audit={}
        for name in ['voidup','voidup-init']:
            binary=prefix/'bin'/(name+suffix)
            if sys.platform=='darwin':
                from voidup_macos import inspect
                dependency_audit[name]=inspect(binary)
                if any(not d.startswith(('/usr/lib/','/System/Library/','@loader_path/')) for d in dependency_audit[name]['dependencies']):raise ValueError('Unrelocated macOS bootstrap dependency')
                continue
            inspector=clang.with_name('llvm-readobj.exe' if os.name=='nt' else 'llvm-readelf')
            flags=['--coff-imports'] if os.name=='nt' else ['--dynamic','--version-info']
            inspection=subprocess.check_output([str(inspector),*flags,str(binary)],text=True)
            dependency_audit[name]={'libraries':sorted(set(re.findall(r'Name: ([^\r\n]+)' if os.name=='nt' else r'\(NEEDED\).*\[([^\]]+)\]',inspection))),
                'abi_versions':sorted(set(re.findall(r'GLIBC(?:XX)?_[0-9.]+',inspection)))}
    shutil.copytree(options.licenses,prefix/'share/void/licenses',symlinks=False)
    def digest(path):
        state=hashlib.sha256()
        with path.open('rb') as source:
            for block in iter(lambda:source.read(1048576),b''):state.update(block)
        return state.hexdigest()
    source_inputs=sorted((ROOT/'apps/voidup').glob('*.void'))+[ROOT/'apps/voidup/launcher.cpp']+sorted((ROOT/'stdlib').rglob('*.void'))
    receipt=dict(schema=1,compiler=str(compiler),compiler_sha256=digest(compiler),
        compiler_version=version,host=dict(system=platform.system(),machine=platform.machine(),libc=platform.libc_ver()),
        source=str(ROOT/'apps/voidup/main.void'),optimized=not options.no_opt,
        source_sha256={str(path.relative_to(ROOT)):digest(path) for path in source_inputs},
        compiled_source_sha256=compiled_source_sha256,
        native_dependency_audit=dependency_audit,
        files={path.relative_to(prefix).as_posix():dict(bytes=path.stat().st_size,sha256=digest(path))
            for path in sorted(prefix.rglob('*')) if path.is_file()},
        production_trust=production,public_distribution_qualified=False,
        bootstrap_id=identity,trust_config_sha256=digest(options.trust_config) if options.trust_config else None)
    (prefix/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(prefix/'bin/voidup')

if __name__=='__main__':main()
