set pagination off
set confirm off
set breakpoint pending off
set disable-randomization off
set print thread-events off
handle SIGSEGV stop print nopass
handle SIGBUS stop print nopass
handle SIGABRT stop print nopass
python
import gdb,time,json,os
from pathlib import Path
ROOT=Path('/tmp/void-io-phase-capture-20261026-6vfao69v')
manifest=json.loads((ROOT/'manifest.json').read_text())
started=time.monotonic()
state={'attempt':None,'mode':None,'phase':'startup','exit_code':None,'signal':None,'compile_count':0}
records=[]
markers=[]
events=open(ROOT/'events.jsonl','a',buffering=1)
def emit(event,**detail):
    entry={'event':event,'elapsed_seconds':round(time.monotonic()-started,6),'attempt':state['attempt'],'mode':state['mode'],'phase':state['phase'],**detail}
    events.write(json.dumps(entry)+'\n');events.flush()
    gdb.write('CAPTURE '+json.dumps(entry)+'\n')
def on_exit(event):
    state['exit_code']=getattr(event,'exit_code',None)
    emit('inferior_exited',exit_code=state['exit_code'])
def on_stop(event):
    if isinstance(event,gdb.SignalEvent):
        state['signal']=event.stop_signal
        emit('signal_stop',signal=event.stop_signal)
        os.fsync(events.fileno())
gdb.events.exited.connect(on_exit)
gdb.events.stop.connect(on_stop)
class Boundary(gdb.Breakpoint):
    def __init__(self,address,label):
        super().__init__('*0x%x'%address,internal=True)
        self.label=label
    def stop(self):
        if self.label=='compile_begin':
            state['compile_count']+=1
            state['mode']='normal' if state['compile_count']==1 else 'O2'
        state['phase']=self.label
        detail={'pc':hex(int(gdb.parse_and_eval('$pc'))),'thread':gdb.selected_thread().global_num}
        if self.label=='compile_begin':detail['optimized_argument_ecx']=int(gdb.parse_and_eval('$ecx'))
        if self.label=='invoke_end':detail['answer_eax']=int(gdb.parse_and_eval('$eax'))
        emit('boundary',**detail)
        return False
class MainEntry(gdb.Breakpoint):
    def __init__(self):super().__init__('*main',internal=True,temporary=True)
    def stop(self):
        base=int(gdb.parse_and_eval('$pc'))
        emit('main_enter',main_address=hex(base),pid=gdb.selected_inferior().pid)
        for label,offset in manifest['normal_path_offsets_from_main'].items():markers.append(Boundary(base+int(offset,16),label))
        return False
for attempt in range(2,6):
    state.update(attempt=attempt,mode=None,phase='startup',exit_code=None,signal=None,compile_count=0)
    work=ROOT/('attempt-%02d'%attempt);work.mkdir()
    gdb.execute('set cwd '+str(work))
    emit('attempt_begin',cwd=str(work))
    entry=MainEntry()
    try:gdb.execute('run')
    except (gdb.error,KeyboardInterrupt) as error:emit('debugger_interruption',error=str(error))
    pid=gdb.selected_inferior().pid
    record={'attempt':attempt,'exit_code':state['exit_code'],'signal':state['signal'],'last_phase':state['phase'],'mode':state['mode'],'compile_count':state['compile_count'],'pid_at_stop':pid,'cwd':str(work)}
    if pid:
        emit('capture_stopped_inferior',pid=pid)
        for command in ['p $_siginfo','info threads','thread apply all bt full 64','info registers','x/i $pc','x/24i $pc-24','info proc mappings','info sharedlibrary']:
            gdb.write('CAPTURE_COMMAND '+command+'\n')
            try:gdb.execute(command)
            except gdb.error as error:emit('capture_command_error',command=command,error=str(error))
        core=work/'crash.core'
        try:
            gdb.execute('generate-core-file '+str(core))
            record['core_file']=str(core)
            record['core_bytes']=core.stat().st_size
        except (gdb.error,OSError) as error:record['core_error']=str(error)
        records.append(record)
        (ROOT/'results.json').write_text(json.dumps({'attempts':records,'stopped_on_first_abnormal':True},indent=2)+'\n')
        emit('batch_stopped_preserving_evidence',signal=state['signal'],core=record.get('core_file'))
        break
    records.append(record)
    (ROOT/'results.json').write_text(json.dumps({'attempts':records,'stopped_on_first_abnormal':False},indent=2)+'\n')
    for marker in markers:
        if marker.is_valid():marker.delete()
    markers.clear()
    if entry.is_valid():entry.delete()
    if state['exit_code']!=0 or state['compile_count']!=2:
        emit('batch_stopped_non_success',exit_code=state['exit_code'])
        break
else:emit('bounded_batch_finished_without_sigsegv',attempts=4)
events.close()
end
