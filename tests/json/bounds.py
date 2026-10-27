from pathlib import Path
import tempfile
import harness as h
source='''use json as json
struct link:
    next: link?
struct list_link:
    children: [list_link]
struct map_link:
    children: {string: map_link}
struct wrapper:
    item: i32
func parsed(text: string, budget: json.limits) -> i32:
    if json.parse(text, budget) is Err(var failure):
        return failure.code
    return 0
func written(text: string, budget: json.limits) -> i32:
    if json.stringify(text, budget) is Err(var failure):
        return failure.code
    return 0
func typed(text: string, budget: json.limits) -> i32:
    if json.parse_as<[i32]>(text, budget) is Err(var failure):
        return failure.code
    return 0
func main():
'''
expected=[]
def add(expression,want):
    global source
    source+='    print('+expression+')\n';expected.append(str(want))
for field in ['max_input_bytes','max_output_bytes','max_depth','max_nodes','max_string_bytes','max_number_bytes','max_object_members','max_work']:
    for limit in [0,-1]:add(f'parsed("0", json.limits({field} = {limit}))',13)
add('parsed("0", json.limits(max_depth = 129))',13)
for limit in [13,14,15]:add(f'parsed("null", json.limits(max_work = {limit}))',12 if limit<14 else 0)
for limit in [0,1,2]:add(f'parsed("0", json.limits(max_nodes = {limit}))',13 if limit==0 else 0)
add('parsed("0", json.limits(max_work = 9223372036854775807))',0)
for size in [2,3,4]:add('parsed("123", json.limits(max_input_bytes = '+str(size)+'))',12 if size<3 else 0)
for size in [2,3,4]:add('parsed("123", json.limits(max_number_bytes = '+str(size)+'))',12 if size<3 else 0)
for size in [1,2,3]:add('parsed('+h.literal('"é"')+', json.limits(max_string_bytes = '+str(size)+'))',12 if size<2 else 0)
for size in [1,2,3]:add('parsed('+h.literal('{"a":0,"b":0}')+', json.limits(max_object_members = '+str(size)+'))',12 if size<2 else 0)
for size in [2,3,4]:add('written("a", json.limits(max_output_bytes = '+str(size)+'))',12 if size<3 else 0)
for size in [7,8,9]:add('written('+h.literal('\0')+', json.limits(max_output_bytes = '+str(size)+'))',12 if size<8 else 0)
for size in [5,6,7]:add('typed("[1,2]", json.limits(max_nodes = '+str(size)+'))',12 if size<6 else 0)
for depth in [63,64,65,127,128,129]:add('parsed('+h.literal('['*depth+'0'+']'*depth)+', json.limits(max_depth = '+str(64 if depth<100 else 128)+'))',12 if depth in [65,129] else 0)
add('parsed('+h.literal('{"a":0,"b":0,"c":0}')+', json.limits(max_work = 8))',12)
source+='''    var cycle = link()
    cycle.next = cycle
    if json.stringify(cycle, json.limits(max_depth = 4)) is Err(var failure):
        print(failure.code)
    var list_cycle = list_link(children = [])
    list_cycle.children.push(list_cycle)
    if json.stringify(list_cycle, json.limits(max_depth = 4)) is Err(var failure):
        print(failure.code)
    var map_cycle = map_link(children = {})
    map_cycle.children["self"] = map_cycle
    if json.stringify(map_cycle, json.limits(max_depth = 4)) is Err(var failure):
        print(failure.code)
    if json.parse_as<[wrapper, 100000]>("[]", json.limits(max_nodes = 4)) is Err(var failure):
        print(failure.code)
'''
expected+=['12','12','12','11']
with tempfile.TemporaryDirectory(prefix='void-json-bounds-') as tmp:
    tmp=Path(tmp);src=tmp/'bounds.void';src.write_text(source);expected=('\n'.join(expected)+'\n').encode()
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',src,*flags],src,expected)
        native=tmp/('bounds-O2' if opt else 'bounds-O0')
        h.run([h.compiler,'build',src,'-o',native,*flags],src)
        h.run([native],src,expected)
print(f'bounds {h.profile}: {len(expected.splitlines())} vectors in 4 execution modes passed')
