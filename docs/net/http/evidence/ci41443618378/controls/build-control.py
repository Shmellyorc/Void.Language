from pathlib import Path
import json,shlex,subprocess,shutil
root=Path.cwd(); build=root/'build/llvm23-release'; out=root/'.toolchains/ci41443618378/control';out.mkdir(exist_ok=True)
entries=json.load(open(build/'compile_commands.json'))
def compile(file, source, obj):
 e=next(x for x in entries if x['file'].endswith(file));cmd=shlex.split(e['command']);cmd[cmd.index('-o')+1]=str(obj);cmd[cmd.index('-c')+1]=str(source);subprocess.run(cmd,cwd=e['directory'],check=True)
s=(root/'src/runtime/http.cpp').read_text().replace('transfer.options.protocol == Protocol::http2_required ? 1L : 0L','1L')
(out/'http.cpp').write_text(s);compile('/src/runtime/http.cpp',out/'http.cpp',out/'http.cpp.o')
compile('/tests/net/http/dns.cpp',root/'tests/net/http/dns.cpp',out/'dns.cpp.o')
shutil.copyfile(build/'libvoid_runtime.a',out/'libvoid_runtime.a');subprocess.run([str(root/'.toolchains/llvm-23.1.4/bin/llvm-ar'),'r',str(out/'libvoid_runtime.a'),str(out/'http.cpp.o')],check=True)
cmd=shlex.split((build/'CMakeFiles/void_http_dns.dir/link.txt').read_text());cmd[cmd.index('CMakeFiles/void_http_dns.dir/tests/net/http/dns.cpp.o')]=str(out/'dns.cpp.o');cmd[cmd.index('libvoid_runtime.a')]=str(out/'libvoid_runtime.a');cmd[cmd.index('-o')+1]=str(out/'dns-before');subprocess.run(cmd,cwd=build,check=True)
cmd[cmd.index(str(out/'libvoid_runtime.a'))]='libvoid_runtime.a';cmd[cmd.index('-o')+1]=str(out/'dns-after');subprocess.run(cmd,cwd=build,check=True)
