"""Informational warmed native timing; no timing-sensitive correctness gate."""
from pathlib import Path
import tempfile, json
import harness as h

corpus={
    'save': '{"name":"Ada","hp":100,"levels":[10,20,30],"flags":{"ready":true,"sound":false},"note":"escaped\\ntext","id":9007199254740993}',
    'config': '{"window":{"width":1920,"height":1080},"theme":"dark","paths":["a/b","c"],"optional":null}',
    'deep64': '['*64+'0'+']'*64,
    'wide128': '{'+','.join('"key'+str(n)+'":'+str(n) for n in range(128))+'}',
}
def nodes(item):
    if isinstance(item,dict):return 1+sum(nodes(x) for x in item.values())
    if isinstance(item,list):return 1+sum(nodes(x) for x in item)
    return 1
source='''use json as json
use time as clock
use convert as convert
func measure(label: string, text: string):
    var node = convert.unwrap(json.parse(text))
    var saved = convert.unwrap(json.stringify(node))
    var start = convert.unwrap(clock.monotonic_now())
    for index in 0..<20:
        node = convert.unwrap(json.parse(text))
    var parsed = convert.unwrap(clock.monotonic_now())
    for index in 0..<20:
        saved = convert.unwrap(json.stringify(node))
    var finished = convert.unwrap(clock.monotonic_now())
    var parse_duration = convert.unwrap(clock.elapsed(start, parsed))
    var write_duration = convert.unwrap(clock.elapsed(parsed, finished))
    print("{label} {text.byte_length} {saved.byte_length} {parse_duration.seconds} {parse_duration.nanosecond} {write_duration.seconds} {write_duration.nanosecond}")
func main():
'''
for label,text in corpus.items():source+='    measure('+h.literal(label)+', '+h.literal(text)+')\n'
with tempfile.TemporaryDirectory(prefix='void-json-measure-') as tmp:
    tmp=Path(tmp);src=tmp/'measure.void';src.write_text(source)
    native=tmp/'measure'
    h.run([h.compiler,'build',src,'-o',native],src,timeout=300)
    result=h.run([native],src,timeout=300)
    samples=[]
    for line in result.stdout.decode().splitlines():
        label,size,output,ps,pn,ws,wn=line.split()
        assert int(size)==len(corpus[label].encode()) and int(output)==int(size),line
        samples.append({'case':label,'bytes':int(size),'nodes':nodes(json.loads(corpus[label])),
                        'iterations':20,'parse_ns':int(ps)*1000000000+int(pn),
                        'stringify_ns':int(ws)*1000000000+int(wn)})
    h.receipt.with_suffix('.samples.json').write_text(json.dumps({'profile':h.profile,'note':'Warmed native O2, compilation excluded. Stringify includes fresh DOM conversion. Informational only; GC/fault/root observations are in json_host.', 'samples':samples},indent=2)+'\n')
    print(json.dumps(samples,indent=2))
