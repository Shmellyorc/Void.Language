"""Independent UTF-8 grammar, thresholds, budgets, fixed UTC and native file oracles."""
from pathlib import Path
from datetime import datetime, timedelta, timezone
import os, sys, tempfile, shutil, json
import support as h
compiler=Path(sys.argv[1]).resolve()
checks=0

def literal(text):
    return '"'+''.join('\\u{'+format(ord(c),'x')+'}' for c in text)+'"'
def escape(text):
    out=''
    for c in text:
        value=ord(c)
        if c in {'"','\\','\n','\r','\t'}:out+= {'"':'\\"','\\':'\\\\','\n':'\\n','\r':'\\r','\t':'\\t'}[c]
        elif value<32 or 127<=value<=159 or value in (0x2028,0x2029,0x61c,0x200e,0x200f) or 0x202a<=value<=0x202e or 0x2066<=value<=0x2069:out+='\\u'+format(value,'04x')
        else:out+=c
    return out.encode()
def record(severity,name,message,stamp=None):
    return ((stamp+' ').encode() if stamp else b'')+severity.upper().encode()+b' "'+escape(name)+b'" "'+escape(message)+b'"\n'
HEADER='''use log as l
use convert
use console
use time
private func require(ok: bool):
    if not ok:
        var failure: result<bool, string> = Err("Logging byte oracle assertion failed")
        convert.unwrap(failure)
func main():
'''
with tempfile.TemporaryDirectory(prefix='VOID-log-oracles-') as d:
    work=Path(d);source=work/'main.void';lines=[];expected=b''
    levels=['trace','debug','info','warn','error','off']
    for threshold,minimum in enumerate(levels):
        lines += [f'    var logger{threshold} = convert.unwrap(l.open(l.config(minimum = l.level.{minimum}, timestamps = false, console = l.console_sink.stdout)))']
        for rank,severity in enumerate(levels[:5]):
            lines += [f'    require(convert.unwrap(logger{threshold}.write(l.level.{severity}, "")) == {str(rank>=threshold).lower()})',f'    require(logger{threshold}.enabled(l.level.{severity}) == {str(rank>=threshold).lower()})']
            if rank>=threshold:expected+=record(severity,'','')
        lines += [f'    convert.unwrap(logger{threshold}.close())']
    controls=''.join(map(chr,list(range(32))+list(range(127,160))+[0x61c,0x200e,0x200f,*range(0x2028,0x202f),*range(0x2066,0x206a)]))
    text=controls+'雪🔥é e\u0301 {literal} " \\n \\u2028'
    lines += ['    var output_logger = convert.unwrap(l.open(l.config(timestamps = false, minimum = l.level.trace, console = l.console_sink.stdout, name = '+literal(text)+')))']
    for severity in levels[:5]:
        lines += [f'    convert.unwrap(output_logger.write(l.level.{severity}, {literal(text)}))']
        expected+=record(severity,text,text)
    lines+=['    convert.unwrap(output_logger.close())']
    for index,content in enumerate(['','A','雪','🔥','\n','"','\\','\u2029']):
        size=len(record('info','',content));budget=max(12,size)
        lines += [f'    var boundary{index} = convert.unwrap(l.open(l.config(timestamps = false, console = l.console_sink.stdout, max_record_bytes = {budget})))',f'    convert.unwrap(boundary{index}.info({literal(content)}))']
        expected+=record('info','',content)
        overflow=content+('x'*(budget-size+1))
        lines += [f'    if boundary{index}.info({literal(overflow)}) is Err(var problem):','        require(problem.kind is l.failure_kind.record_too_large)','    else:','        require(false)',f'    convert.unwrap(boundary{index}.close())']
    for index,settings in enumerate(['backups = -1','backups = 33','rotation_bytes = 0','max_record_bytes = 0','max_record_bytes = 11, timestamps = false','rotation_bytes = i64.max_value, backups = 32','file_path = "never-created", rotation_bytes = 64, max_record_bytes = 65','name = "\\n", timestamps = false, max_record_bytes = 13','console = l.console_sink.none']):
        lines += [f'    if l.open(l.config({settings})) is Err(var problem):','        require(problem.kind is l.failure_kind.invalid_config)','    else:','        require(false)']
    source.write_text(HEADER+'\n'.join(lines)+'\n', encoding='utf-8')
    h.run([compiler,'check',source],small=True)
    for optimized in (False,True):
        flags=[] if optimized else ['--no-opt']
        h.run([compiler,'run',source,'--no-cache',*flags],expected=expected,small=True);checks+=1
        obj=work/'oracles.o';exe=work/'oracles.app'
        h.run([compiler,'emit',source,'-o',obj,*flags],small=True);h.link(compiler,obj,exe,sources=[source]);h.run([exe],expected=expected,small=True,sources=[source]);checks+=3
    # Test-only source overlay exposes private pure framing with a fixed instant.
    library=work/'stdlib';shutil.copytree(h.ROOT/'stdlib',library)
    with (library/'log.void').open('a', encoding='utf-8') as f:
        f.write('''\nfunc test_frame(options: config, severity: level, message: string, stamp: string) -> result<[u8], failure>:
    return _frame(options, severity, message, stamp)
''')
    env=dict(os.environ,VOID_STDLIB_ROOT=str(library));lines=[];expected=b''
    instants=[(-62135596800,0),(253402300799,999999999),(-1,999999999),(0,0),(951782399,999999999),(951782400,123456789),(1609459199,999000001),(1609459200,1)]
    for index,(seconds,nanos) in enumerate(instants):
        value=datetime(1970, 1, 1, tzinfo=timezone.utc) + timedelta(seconds=seconds)
        stamp=f'{value.year:04d}-{value.month:02d}-{value.day:02d}T{value.hour:02d}:{value.minute:02d}:{value.second:02d}.{nanos//1000000:03d}Z'
        lines += [f'    var instant{index} = convert.unwrap(time.make_instant({seconds}, {nanos}))',f'    var stamp{index} = convert.unwrap(instant{index}.format("yyyy-MM-dd\'T\'HH:mm:ss.fff\'Z\'"))',f'    require(stamp{index} == {literal(stamp)})',f'    var bytes{index} = convert.unwrap(l.test_frame(l.config(), l.level.info, "clock", stamp{index}))',f'    convert.unwrap(console.stdout.write_bytes(bytes{index}))']
        expected+=record('info','','clock',stamp)
    source.write_text(HEADER+'\n'.join(lines)+'\n', encoding='utf-8')
    h.run([compiler,'run',source,'--no-cache'],expected=expected,small=True,env=env,sources=[library]);checks+=1
    # A cap-sized record uses managed buffers on a real hard 1 MiB stack.
    source.write_text(HEADER+'    var instance = convert.unwrap(l.open(l.config(timestamps = false, console = l.console_sink.stdout)))\n    convert.unwrap(instance.info('+literal('x'*65525)+'))\n    convert.unwrap(instance.close())\n', encoding='utf-8')
    cap_expected=record('info','','x'*65525)
    assert len(cap_expected)==65536
    h.run([compiler,'run',source,'--no-cache'],expected=cap_expected,small=True);checks+=1
    cap_obj=work/'cap.o';cap_exe=work/'cap.app'
    h.run([compiler,'emit',source,'-o',cap_obj],small=True);h.link(compiler,cap_obj,cap_exe,sources=[source]);h.run([cap_exe],expected=cap_expected,small=True,sources=[source]);checks+=3
    # A poisoned clock overlay proves timestamps-off and filtering never sample it.
    p=library/'time.void';text=p.read_text(encoding='utf-8');start=text.index('func now_utc()');end=text.index('\n\n',start)
    text=text[:start]+'''func now_utc() -> result<instant, error>:
    return Err(error(message = "clock sentinel", operation = "test_clock", code = 77))'''+text[end:];p.write_text(text, encoding='utf-8')
    source.write_text(HEADER+'''    var instance = convert.unwrap(l.open(l.config(timestamps = false, console = l.console_sink.stdout)))
    convert.unwrap(instance.info("clock unused"))
    convert.unwrap(instance.configure(l.config(console = l.console_sink.stdout)))
    require(not convert.unwrap(instance.debug("filtered clock unused")))
    if instance.info("clock fails") is Err(var problem):
        require(problem.kind is l.failure_kind.clock)
        if problem.clock_cause is Ok(var cause):
            require(cause.message == "clock sentinel" and cause.code == 77)
        else:
            require(false)
    else:
        require(false)
    convert.unwrap(instance.configure(l.config(timestamps = false, console = l.console_sink.stdout)))
    convert.unwrap(instance.info("clock failure did not fault"))
    convert.unwrap(instance.close())
''', encoding='utf-8')
    expected=record('info','','clock unused')+record('info','','clock failure did not fault')
    h.run([compiler,'run',source,'--no-cache'],expected=expected,small=True,env=env,sources=[library]);checks+=1
    p=library/'time.void';text=p.read_text(encoding='utf-8');start=text.index('func now_utc()');end=text.index('\n\n',start)
    text=text[:start]+'''func now_utc() -> result<instant, error>:
    return make_instant(i64.max_value)'''+text[end:];p.write_text(text, encoding='utf-8')
    source.write_text(HEADER+'''    var instance = convert.unwrap(l.open(l.config(console = l.console_sink.stdout)))
    if instance.info("format fails before output") is Err(var problem):
        require(problem.kind is l.failure_kind.clock)
        require(problem.operation == "format" and problem.clock_cause != null)
    else:
        require(false)
    convert.unwrap(instance.close())
''', encoding='utf-8')
    h.run([compiler,'run',source,'--no-cache'],expected=b'',small=True,env=env,sources=[library]);checks+=1
print(f'Logging independent byte/UTC oracles: {checks} executions/emit/link checks; thresholds=30, all specified Unicode controls, 8 budgets, 9 invalid configurations, 8 fixed instants')
