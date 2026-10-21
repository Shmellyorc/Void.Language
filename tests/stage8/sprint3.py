"""Stage 8 Sprint 3 text and recoverable collection process acceptance."""
import os, pathlib, subprocess, sys, tempfile
compiler=pathlib.Path(sys.argv[1]).resolve();clang=sys.argv[2];linker=sys.argv[3] if len(sys.argv)>3 else ''
runtime=pathlib.Path(sys.argv[4]).resolve();fixture=pathlib.Path(sys.argv[5]).resolve();sanitizers=sys.argv[6]=='ON'
link_flags=['--driver-mode=g++',*(['--ld-path='+linker] if linker else []),*(['-fsanitize=address,undefined'] if sanitizers else [])]
if os.name=='nt':link_flags += ['-fms-runtime-lib=dll','-llegacy_stdio_definitions']
checks=0

def run(args,ok=True):
    global checks
    result=subprocess.run([str(x) for x in args],capture_output=True,text=True,encoding="utf-8",timeout=60);checks+=1
    assert result.returncode==(0 if ok else 1),(args,result.returncode,result.stdout,result.stderr)
    return result

text=r'''func decode(const bytes: [u8]) -> result<string, string>:
    return string.from_utf8(bytes = bytes)
func entry(input: i32) -> i32:
    var empty = ""
    if empty.length != 0 or empty.byte_length != 0 or empty.to_utf8().count != 0:
        return 10
    var text = "A🎮\0e\u{301}"
    if text.length != 5 or text.byte_length != 9:
        return 11
    if text[0] != "A" or text[1 as u8] != "🎮" or text[2 as i16] != "\0" or text[-2 as i64] != "e" or text[-1] != "\u{301}" or text[-5 as i8] != "A":
        return 12
    var bytes = text.to_utf8()
    if bytes.count != 9 or bytes[1] != 240 or bytes[5] != 0:
        return 13
    var alias = bytes
    var copy = text.to_utf8()
    if decode(bytes) is Ok(var original):
        bytes[0] = 66
        if original != text or copy[0] != 65 or alias[0] != 66:
            return 14
    else:
        return 15
    const owned: [u8] = [65, 0, 240, 159, 142, 174]
    if decode(owned) is Ok(var decoded):
        if decoded != "A\0🎮":
            return 16
    else:
        return 17
    var zero: [u8] = []
    if decode(zero) is Ok(var nothing):
        if nothing != "":
            return 18
    else:
        return 19
    var allbytes: [u8] = []
    for byte in 0..127:
        allbytes.push(byte as u8)
    if decode(allbytes) is Ok(var ascii):
        if ascii.length != 128 or ascii.byte_length != 128 or ascii[0] != "\0" or ascii[-1] != "\u{7f}":
            return 20
    else:
        return 21
    return 1
'''
for t in ['i8','i16','i32','i64','u8','u16','u32','u64']:
    text=text.replace('    return 1\n',f'    var index_{t}: {t} = 1\n    if text[index_{t}] != "🎮":\n        return 22\n    return 1\n',1)

collections='''func safe<t>(const items: [t], index: i64) -> result<t, string>:
    return items.get(index)
struct node:
    value: i32
value struct payload:
    numbers: [i32, 2]
    child: node
func entry(input: i32) -> i32:
    var list = [10, 20]
    var alias = list
    if not (safe(list, -1) is Ok(_)):
        return 9
    if list.get(index = -1) is Ok(var last):
        if last != 20:
            return 10
    else:
        return 11
    if list.get(-3) is Err(var bad):
        if bad != "index out of range":
            return 12
    else:
        return 13
    if list.pop() is Ok(var popped):
        if popped != 20 or alias.count != 1:
            return 14
    else:
        return 15
    list.pop()
    if list.pop() is Err(var empty):
        if empty != "empty list" or alias.count != 0:
            return 16
    else:
        return 17
    var fixed: [i32, 2] = [30, 40]
    if fixed.get(-2) is Ok(var first):
        if first != 30:
            return 18
    else:
        return 19
    var none: [i32, 0] = []
    if not (none.get(0) is Err(_)):
        return 20
    var nullable: [node?] = [null]
    if not (nullable.get(0) is Ok(null)) or not (nullable.get(1) is Err(_)):
        return 29
    var child = node(value = 7)
    var values = [payload(numbers = [1, 2], child = child)]
    if values.get(0) is Ok(var item):
        item.numbers[0] = 99
        item.child.value = 8
        if values[0].numbers[0] != 1 or child.value != 8:
            return 21
    else:
        return 22
    if values.pop() is Ok(var removed):
        if removed.numbers[0] != 1 or removed.child.value != 8 or values.count != 0:
            return 23
    else:
        return 24
    var dictionary = {"a\\0b": child}
    if dictionary.get(key = "a\\0b") is Ok(var found):
        found.value = 9
        if child.value != 9:
            return 25
    else:
        return 26
    if dictionary.get("absent") is Err(var missing):
        if missing != "key not found" or dictionary.count != 1:
            return 27
    else:
        return 28
    for value in dictionary:
        dictionary.get("absent")
        dictionary.get("a\\0b")
    return 1
'''
indexchecks=['func entry(x: i32) -> i32:','    var list = [1, 2]','    var fixed: [i32, 2] = [1, 2]']
for t in ['i8','i16','i32','i64','u8','u16','u32','u64']:
    for receiver in ['list','fixed']:
        for index in ['0','1',t+'.max_value']+(['-1','-2','-3',t+'.min_value'] if t.startswith('i') else []):
            # Width explicitly contextualized, including both 64-bit extremes.
            success=index in ['0','1','-1','-2']
            indexchecks.extend([f'    var {receiver}_{t}_{len(indexchecks)}: {t} = {index}'])
            name=indexchecks[-1].split(':')[0].strip().split()[1]
            indexchecks.extend([f'    if not ({receiver}.get({name}) is '+('Ok(_)' if success else 'Err(_)')+'):', '        return 30'])
indexchecks.append('    return 1')

order='''struct state:
    calls: i32
    items: [i32]
func owner(s: state) -> [i32]:
    s.calls += 1
    return s.items
func index(s: state) -> i32:
    s.calls += 10
    s.items.push(3)
    return -1
func entry(x: i32) -> i32:
    var s = state(items = [1, 2])
    if owner(s).get(index(s)) is Ok(var value):
        if value != 3 or s.calls != 11:
            return 10
    else:
        return 11
    for item in s.items:
        s.items.get(0)
        s.items.get(99)
    return 1
'''

with tempfile.TemporaryDirectory(prefix='VOID sprint3 ') as temp:
    root=pathlib.Path(temp)
    for name,source in [('text',text),('collections',collections),('widths','\n'.join(indexchecks)+'\n'),('order',order)]:
        path=root/(name+'.void');path.write_text(source,encoding='utf-8');run([compiler,'check',path])
        for flags in [[],['--no-opt']]:
            assert run([compiler,'run',path,'0',*flags]).stdout.strip()=='1'
            obj=root/(name+str(len(flags))+'.o');exe=root/(name+str(len(flags))+('.exe' if os.name=='nt' else '.bin'))
            run([compiler,'emit',path,'-o',obj,*flags]);run([clang,*link_flags,obj,runtime,fixture,'-o',exe]);assert run([exe,'0']).stdout.strip()=='1'
    malformed=[[128],[192,175],[224,128,128],[237,160,128],[244,144,128,128],[245,128,128,128],[240,159,142],[194],[65,128],[255],[226,40,161]]
    path=root/'malformed.void'
    lines=['func entry(x: i32) -> i32:']
    for n,bytes in enumerate(malformed):
        lines.extend([f'    var bytes{n}: [u8] = '+str(bytes),f'    if string.from_utf8(bytes{n}) is Err(var message{n}):',f'        if message{n} != "invalid UTF-8":','            return 40','    else:','        return 41'])
    lines.append('    return 1');path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    for flags in [[],['--no-opt']]:
        assert run([compiler,'run',path,'0',*flags]).stdout.strip()=='1'
        obj=root/('malformed'+str(len(flags))+'.o');exe=root/('malformed'+str(len(flags))+('.exe' if os.name=='nt' else '.bin'));run([compiler,'emit',path,'-o',obj,*flags]);run([clang,*link_flags,obj,runtime,fixture,'-o',exe]);assert run([exe,'0']).stdout.strip()=='1'
    for expression in ['""[0]','"abc"[3]','"abc"[-4]','"abc"[i64.min_value]','"abc"[u64.max_value]']:
        path=root/'bounds.void';path.write_text('func entry(x: i32) -> i32:\n    var value = '+expression+'\n    return 1\n',encoding='utf-8');run([compiler,'check',path])
        for flags in [[],['--no-opt']]:
            assert 'String index' in run([compiler,'run',path,'0',*flags],False).stderr
            obj=root/('bounds'+str(len(flags))+'.o');exe=root/('bounds'+str(len(flags))+('.exe' if os.name=='nt' else '.bin'));run([compiler,'emit',path,'-o',obj,*flags]);run([clang,*link_flags,obj,runtime,fixture,'-o',exe]);failure=run([exe,'0'],False);assert 'String index' in failure.stdout+failure.stderr

    invalid=['var s = "x"\n    s[0] = "a"','var s = "x"\n    s.length = 2','var v = "a"[true]','var v = "a"[0.5]','var bytes: [i32] = [1]\n    var v = string.from_utf8(bytes)','var v = [1].get(true)','var v = [1].get(0.5)','var a: [i32, 1] = [1]\n    a.pop()','const a = [1]\n    a.pop()','const a = [1]\n    var b = a\n    b.pop()','var d = {1: 2}\n    d.get(true)']
    for body in invalid:
        path=root/'invalid.void';path.write_text('func entry(x: i32) -> i32:\n    '+body+'\n    return x\n',encoding='utf-8');run([compiler,'check',path],False)
    prefix='struct node:\n    value: i32\nvalue struct carrier:\n    children: [node]\n'
    for body in ['const a = [node()]\n    if a.get(0) is Ok(var child):\n        child.value = 2','const d = {1: node()}\n    if d.get(1) is Ok(var child):\n        child.value = 2','const a: [node, 1] = [node()]\n    if a.get(0) is Ok(var child):\n        child.value = 2','const a = [carrier(children = [node()])]\n    if a.get(0) is Ok(var item):\n        item.children[0].value = 2']:
        path=root/'readonly.void';path.write_text(prefix+'func entry(x: i32) -> i32:\n    '+body+'\n    return x\n',encoding='utf-8');assert 'readonly' in run([compiler,'check',path],False).stderr.lower()
    path=root/'iterator.void';path.write_text('func entry(x: i32) -> i32:\n    var a = [1, 2]\n    for value in a:\n        a.pop()\n    return 1\n',encoding='utf-8')
    for flags in [[],['--no-opt']]:assert 'structurally modified' in run([compiler,'run',path,'0',*flags],False).stderr.lower()
print(f'Sprint 3: {checks} text/collection normal/O2 JIT, linked AOT and diagnostic process checks passed')
