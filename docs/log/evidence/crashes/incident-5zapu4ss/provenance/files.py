"""Native filesystem preflight, adoption, exact retention and reconfiguration oracles."""
from pathlib import Path
import sys, os, tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();checks=0
def check(value, detail='native filesystem oracle'):
    global checks
    assert value, detail
    checks+=1
HEADER='''use log as l
use convert
use io
private func require(ok: bool):
    if not ok:
        var failure: result<bool, string> = Err("Logging filesystem assertion failed")
        convert.unwrap(failure)
private func rejected(value: string, adopting: bool = false):
    var policy = l.existing_files.reject
    if adopting:
        policy = l.existing_files.adopt
    if l.open(l.config(console = l.console_sink.none, timestamps = false, file_path = value,
        rotation_bytes = 64, max_record_bytes = 64, existing = policy)) is Err(var problem):
        require(problem.file_cause != null)
    else:
        require(false)
func main():
'''
body='''    rejected("reject-active")
    rejected("reject-backup")
    rejected("hardlink")
    rejected("link", true)
    rejected("directory", true)
    rejected("special", true)
    rejected("torn", true)
    rejected("too-big", true)
    rejected("torn-backup", true)
    var adopted = convert.unwrap(l.open(l.config(console = l.console_sink.none,
        timestamps = false, file_path = "adopted", existing = l.existing_files.adopt,
        rotation_bytes = 64, max_record_bytes = 64)))
    convert.unwrap(adopted.info("append"))
    convert.unwrap(adopted.close())
    var holes = convert.unwrap(l.open(l.config(console = l.console_sink.none,
        timestamps = false, file_path = "holes", existing = l.existing_files.adopt,
        rotation_bytes = 64, max_record_bytes = 64)))
    convert.unwrap(holes.close())
    for retained in [0, 1, 3, 32]:
        var name = "rotate{retained}"
        var instance = convert.unwrap(l.open(l.config(console = l.console_sink.none,
            timestamps = false, file_path = name, rotation_bytes = 32,
            max_record_bytes = 32, backups = retained)))
        for index in 0..<45:
            convert.unwrap(instance.info("record{index:00}"))
        convert.unwrap(instance.close())
    var settings = l.config(console = l.console_sink.none, timestamps = false,
        file_path = "policy", rotation_bytes = 64, max_record_bytes = 64, backups = 3)
    var instance = convert.unwrap(l.open(settings))
    for index in 0..<15:
        convert.unwrap(instance.info("item{index:00}"))
    settings.backups = 1
    convert.unwrap(instance.configure(settings))
    # Unknown historical names are outside the current set and survive.
    convert.unwrap(io.write_text("policy.7", "unrelated historical sentinel\\n"))
    settings.name = "metadata"
    convert.unwrap(instance.configure(settings))
    settings.backups = 3
    convert.unwrap(instance.configure(settings))
    settings.rotation_bytes = 32
    settings.max_record_bytes = 32
    if instance.configure(settings) is Err(var problem):
        require(problem.file_cause != null)
    else:
        require(false)
    # Failed preflight leaves the previous ready logger usable.
    convert.unwrap(instance.info("continues"))
    settings.rotation_bytes = 64
    settings.max_record_bytes = 64
    settings.file_path = "replacement"
    convert.unwrap(instance.configure(settings))
    convert.unwrap(instance.warn("new path"))
    convert.unwrap(instance.close())
    var growing = l.config(console = l.console_sink.none, timestamps = false,
        file_path = "grow", rotation_bytes = 64, max_record_bytes = 64, backups = 1)
    var owner = convert.unwrap(l.open(growing))
    growing.backups = 3
    if owner.configure(growing) is Err(var problem):
        require(problem.file_cause != null)
    else:
        require(false)
    growing.existing = l.existing_files.adopt
    convert.unwrap(owner.configure(growing))
    convert.unwrap(owner.close())
    var module_settings = l.config(console = l.console_sink.none, timestamps = false,
        file_path = "module", rotation_bytes = 64, max_record_bytes = 64, backups = 3)
    convert.unwrap(l.configure(module_settings))
    for index in 0..<15:
        convert.unwrap(l.info("item{index:00}"))
    convert.unwrap(l.shutdown())
    module_settings.backups = 1
    convert.unwrap(l.configure(module_settings))
    convert.unwrap(l.shutdown())
'''
with tempfile.TemporaryDirectory(prefix='VOID-log-files-') as d:
    work=Path(d);source=work/'main.void';source.write_text(HEADER+body)
    h.run([compiler,'check',source],small=True)
    for optimized in (False,True):
        flags=[] if optimized else ['--no-opt'];obj=work/'files.o';exe=work/'files.app'
        h.run([compiler,'emit',source,'-o',obj,*flags],small=True);h.link(compiler,obj,exe,sources=[source]);checks+=2
        for mode in ('jit','aot'):
            cwd=work/(str(optimized)+mode);cwd.mkdir()
            for name,data in {'reject-active':b'external\n','reject-backup.2':b'external backup\n','torn':b'torn','too-big':b'x'*64+b'\n','torn-backup.2':b'torn','adopted':b'before\n','adopted.2':b'older\n','holes.2':b'hole\n','grow.3':b'new authority\n','outside':b'outside sentinel\n'}.items():(cwd/name).write_bytes(data)
            os.link(cwd/'outside',cwd/'hardlink');(cwd/'link').symlink_to(cwd/'outside');(cwd/'directory').mkdir()
            if os.name=='posix':os.mkfifo(cwd/'special')
            else:(cwd/'special').mkdir()
            before={p.name:(p.stat().st_ino,p.read_bytes()) for p in cwd.iterdir() if p.is_file() and not p.is_symlink()}
            command=[compiler,'run',source,'--no-cache',*flags] if mode=='jit' else [exe]
            result=h.run(command,cwd=cwd,expected=b'',small=True,sources=[source]);checks+=1
            try:
                for name in ('reject-active','reject-backup.2','torn','too-big','torn-backup.2','outside'):
                    check((cwd/name).stat().st_ino==before[name][0] and (cwd/name).read_bytes()==before[name][1],name)
                for name in ('reject-backup','torn-backup'):check(not (cwd/name).exists(),name)
                check((cwd/'adopted').read_bytes()==b'before\nINFO "" "append"\n')
                check((cwd/'adopted').stat().st_ino==before['adopted'][0])
                check((cwd/'holes').read_bytes()==b'' and (cwd/'holes.2').read_bytes()==b'hole\n')
                for retained in (0,1,3,32):
                    name='rotate'+str(retained)
                    for slot in range(retained+1):
                        expected=f'INFO "" "record{44-slot:02}"\n'.encode()
                        check((cwd/(name+('.'+str(slot) if slot else ''))).read_bytes()==expected,(name,slot))
                    check(not (cwd/(name+'.'+str(retained+1))).exists())
                check((cwd/'policy.7').read_bytes()==b'unrelated historical sentinel\n')
                check((cwd/'replacement').read_bytes()==b'WARN "metadata" "new path"\n')
                check((cwd/'grow.3').read_bytes()==b'new authority\n')
                check(not (cwd/'module.2').exists() and not (cwd/'module.3').exists())
            except AssertionError as error:
                h.crash.preserve(command,cwd,result,source_inputs=[source],provenance_inputs=[Path(__file__)],failure_kind='test_assertion',evidence_context={'native_file_oracle':str(error)})
                raise
print(f'Logging filesystem: {checks} native source/oracle checks; exact 0/1/3/32 retention, holes, preflight, adoption, count/path/bound reconfiguration')
