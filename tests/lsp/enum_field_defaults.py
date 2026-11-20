"""TestGame regression: pure field defaults and enum-hole recovery at 1 MiB."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,tempfile
from editor_recovery import InteractiveClient,query,roles,update

server,compiler,report=(Path(p).resolve() for p in sys.argv[1:])
fixtures=Path(__file__).parent/'fixtures/enum_field_defaults'
source=(fixtures/'main.void').read_text();records=[];inputs={}
identity={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (server,compiler)}
def save():
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(schema=1,identities=identity,stack_bytes=1048576,
                                     inputs=inputs,records=records),indent=2)+'\n')
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def record_requests(client):
    original=client.request
    def recorded(method,params=None,*args,**kwargs):
        response,notes=original(method,params,*args,**kwargs)
        records.append(dict(method=method,response=response,notes=notes));save()
        return response,notes
    client.request=recorded
def invoke(args,status=0,output=None,error=None):
    args=list(map(str,args));r=subprocess.run(args,capture_output=True,text=True,timeout=60,
                                             preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=args,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr)
    records.append(row);save();assert r.returncode==status,row
    if output is not None:assert r.stdout==output,row
    if error is not None:assert error in r.stderr and '.void:' in r.stderr,row
    assert not any(t in r.stderr for t in ('AddressSanitizer','runtime error:','$equality$','/@pkg/')),row

FAMILY='''private enum mode:
    idle
    text(value: string)
enum code(u8):
    ready = 3
enum packet<t>:
    data(value: t)
func seven() -> i32:
    return 7
struct defaults:
    state: mode = mode.text("ready")
    number: code = code.ready
    selected: option<i32> = Some(9)
    result: result<i32, string> = Ok(7)
    callback: func() -> i32 = seven
    generic: packet<i32> = packet<i32>.data(5)
func (s defaults) init():
    pass
struct derived extends defaults:
    state2: mode = mode.idle
func (s derived) init():
    super.init()
    if not (s.state2 is mode.idle):
        print("wrong derived default")
struct box<t>:
    state: mode = mode.idle
    value: t
func (s box<t>) init():
    pass
func main():
    var s = derived()
    var b = box<i32>()
    print(s.state == mode.text("ready"), s.number == code.ready,
          s.selected is Some(9), s.result is Ok(7), s.callback() == 7,
          s.generic is packet<i32>.data(5), b.state is mode.idle)
'''
with tempfile.TemporaryDirectory(prefix='VOID enum defaults ') as directory:
    root=Path(directory);main=root/'main.void'
    for p in fixtures.glob('*.void'):shutil.copyfile(p,root/p.name)
    family=root/'family.void';family.write_text(FAMILY)
    for file,output in ((main,'true\n'),(family,'true true true true true true true\n')):
        inputs[file.name]=file.read_text();invoke([compiler,'check',file])
        for n,flags in enumerate((['--no-opt'],[])):
            invoke([compiler,'run',file,'--no-cache',*flags],output=output)
            image=root/(file.stem+str(n));invoke([compiler,'build',file,'-o',image,*flags]);invoke([image],output=output)

    # A missing default remains a required field; a bad default remains invalid.
    for name,text,error in (
        ('required',source.replace(' = scene_state.none',''),'initialized field'),
        ('wrong_type',source.replace('state: scene_state = scene_state.none','state: scene_state = 7'),'Type mismatch'),
        ('unknown',source.replace('state: scene_state = scene_state.none','state: scene_state = scene_state.missing'),'Unknown enum variant'),
    ):
        file=root/(name+'.void');file.write_text(text);inputs[file.name]=text;invoke([compiler,'check',file],1,error=error)

    # Real editor traffic includes a source error, provider queries, and repair
    # in one process. Never fabricate a constant for a recovered partial member.
    with InteractiveClient(server,main,preexec_fn=limits if os.name=='posix' else None) as client:
        record_requests(client)
        response,_=client.request('initialize',{'capabilities':{'textDocument':{
            'publishDiagnostics':{'versionSupport':True},
            'semanticTokens':{'requests':{'full':True},'formats':['relative'],
                              'tokenTypes':['enum','enumMember','struct','property','parameter'],
                              'tokenModifiers':[]}},
            'workspace':{'semanticTokens':{'refreshSupport':True}}},
            'initializationOptions':{'voidInteractivePresentation':True}})
        legend=response['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        client.notify('initialized',{});client.sync()
        assert not update(client,main,source,1)
        baseline=roles(client,main,source,legend)
        assert (9,'state','property') in baseline and (9,'scene_state','enum') in baseline,baseline
        assert 'i32' in str(query(client,'hover',main,source,'value: i32'))
        version=1
        for tail in ('','scene','scene_state','scene_state.','scene_state.n','scene_state.no',
                     'scene_state.missing','scene_state.none','scene_state.','scene_state.none'):
            current=source.replace('state: scene_state = scene_state.none','state: scene_state = '+tail)
            version+=1;inputs['edit-'+str(version)]=current
            errors=update(client,main,current,version)
            if tail=='scene_state.none':assert not errors,errors
            else:assert errors,tail
            tokens=roles(client,main,current,legend)
            # Missing expressions can make the parser reject the containing
            # declaration. Typed enum prefixes retain a safe presentation tree.
            if tail in ('scene_state.','scene_state.n','scene_state.no','scene_state.none'):
                assert any(word=='value' and kind=='parameter' for _,word,kind in tokens),tokens
                hover=query(client,'hover',main,current,'value: i32')
                # Hover uses the fully validated recovery index, not the
                # presentation-only tree with an unevaluable field default.
                if tail=='scene_state.none':assert hover and 'i32' in str(hover),hover
            query(client,'documentSymbol',main)
            if tail.startswith('scene_state.'):
                completion=query(client,'completion',main,current,'state: scene_state = '+tail,after=True)
                labels={item['label'] for item in completion}
                if tail in ('scene_state.','scene_state.n','scene_state.no'):assert 'none' in labels,completion
            if tail=='scene_state.none':assert tokens==baseline,(baseline,tokens)
        # Replay the observed unmatched-list transition before repairing the enum.
        broken=source.replace('active: [scene] = []','active: [scene] = ]')
        version+=1;assert update(client,main,broken,version)
        query(client,'semanticTokens/full',main)
        version+=1;assert not update(client,main,source,version)
        assert roles(client,main,source,legend)==baseline
        client.shutdown()
    # Cold-open a typed prefix: there are no earlier semantic tokens to reuse.
    cold=source.replace('state: scene_state = scene_state.none','state: scene_state = scene_state.n')
    main.write_text(cold)
    with InteractiveClient(server,main,preexec_fn=limits if os.name=='posix' else None) as client:
        record_requests(client)
        response,_=client.request('initialize',{'capabilities':{
            'textDocument':{'semanticTokens':{'requests':{'full':True},'formats':['relative'],
                                            'tokenTypes':['enum','parameter'],'tokenModifiers':[]}},
            'workspace':{'semanticTokens':{'refreshSupport':True}}},
            'initializationOptions':{'voidInteractivePresentation':True}})
        legend=response['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        client.notify('initialized',{});client.sync();assert update(client,main,cold,1)
        color=roles(client,main,cold,legend)
        assert (2,'scene_state','enum') in color and any(word=='value' and kind=='parameter' for _,word,kind in color),color
        assert not update(client,main,source,2)
        assert 'i32' in str(query(client,'hover',main,source,'value: i32'))
        client.shutdown()
save();print('Enum field defaults: JIT/AOT O0/O2, required fields, live recovery and repair passed at 1 MiB')
