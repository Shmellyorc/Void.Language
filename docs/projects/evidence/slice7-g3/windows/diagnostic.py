from concurrent.futures import ThreadPoolExecutor
from hashlib import file_digest
from pathlib import Path
import json
import re
import subprocess

share=Path(r'\\vboxsvr\VM\project-slice7-g3')
home=Path.home()
build=home/'VoidS7-G2-Release'
exact_source=home/'VoidS7-G2-source'
scratch=home/'VoidS7-G3-concurrency'
scratch.mkdir(exist_ok=False)
evidence={}
def save():
    (share/'diagnostic-results.json').write_text(json.dumps(evidence,indent=2)+'\n')
def sha(path):
    with path.open('rb') as stream:return file_digest(stream,'sha256').hexdigest()
def execute(label,command,cwd=build):
    with (share/('diagnostic-'+label+'.log')).open('wb') as log:
        result=subprocess.run(command,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,timeout=600)
    evidence[label]={'command':command,'cwd':str(cwd),'exit':result.returncode}
    save()
    return result.returncode

prior=json.loads((share.parent/'project-slice7-g2'/'native-results.json').read_text())
profile=next(x for x in prior['profiles'] if x['profile']=='Release')
assert sha(build/'void_tests.exe')==profile['binaries']['void_tests']
source=exact_source/'tests/analysis/tests.cpp'
assert sha(source)==prior['source_inputs_sha256']['tests/analysis/tests.cpp']
original=source.read_text()
needle='stop=true; for(auto& t:callers)t.join(); check(!wrong && calls>0 && old->invoke(10)==53,"concurrent call/lease failure");'
replacement='''stop=true; for(auto& t:callers)t.join();
        const auto completed_calls = calls.load();
        const bool wrong_result = wrong.load();
        const auto old_value = !wrong_result && completed_calls > 0 ? old->invoke(10) : 0;
        if (wrong_result || !completed_calls || old_value != 53)
            std::cerr << "analysis control optimize=" << optimize << " calls=" << completed_calls
                      << " wrong=" << wrong_result << " old_checked=" << (!wrong_result && completed_calls > 0)
                      << " old_value=" << old_value << '\\n';
        check(!wrong_result && completed_calls > 0 && old_value == 53,"concurrent call/lease failure");'''
assert original.count(needle)==1
patched=scratch/'analysis-diagnostic.cpp'
patched.write_text(original.replace(needle,replacement),encoding='utf-8')
(share/'analysis-diagnostic.cpp.txt').write_bytes(patched.read_bytes())
evidence['source']={'original':sha(source),'diagnostic':sha(patched),'g2_release_original_executable':sha(build/'void_tests.exe')}
save()

info=json.loads((share/'inspect-g2.json').read_text())
cl=info['compile']['command']
base=str(home/'VoidCI42377496702-Native')
for tail in (r'include',r'third_party\curl\include',r'tests\integration'):
    cl=cl.replace('-I'+base+'\\'+tail,'-I'+str(exact_source)+'\\'+tail)
obj=scratch/'analysis-diagnostic.obj'
cl=re.sub(r'/Fo\S+',lambda _: '/Fo'+str(obj),cl)
cl=re.sub(r'/Fd\S+',lambda _: '/Fd'+str(scratch/'analysis.pdb'),cl)
cl=cl[:cl.index('-c -- ')+6]+str(patched)
assert execute('compile',cl)==0
entries=json.loads(subprocess.check_output([r'C:\gcc\bin\ninja.exe','-t','compdb','-x'],cwd=build,text=True))
expanded=next(x['command'] for x in entries if x.get('output','').replace('\\','/')=='void_tests.exe')
rsp=expanded.split('/nologo ',1)[1].split('  /out:void_tests.exe',1)[0]
needle_obj=r'CMakeFiles\void_tests.dir\tests\analysis\tests.cpp.obj'
assert rsp.count(needle_obj)==1 and 'void_llvm.lib' in rsp and '@CMakeFiles' not in rsp, rsp[:200]
response=scratch/'link.rsp'
response.write_text(rsp.replace(needle_obj,str(obj)),encoding='utf-8')
exe=scratch/'void-analysis-diagnostic.exe'
link=info['link'].replace(r'@CMakeFiles\void_tests.rsp','@'+str(response))
link=link.replace(r'--intdir=CMakeFiles\void_tests.dir','--intdir='+str(scratch))
for opt,old,name in (('/out:','void_tests.exe',exe),('/implib:','void_tests.lib',scratch/'analysis.lib'),
                     ('/pdb:','void_tests.pdb',scratch/'analysis.pdb')):
    link=link.replace(opt+old,opt+str(name))
assert execute('link',link)==0
evidence['artifacts']={'executable':sha(exe),'diagnostic_object':sha(obj),'response':sha(response)}
save()
names={
 'analysis':[exe,'analysis',scratch/'analysis fixtures'],
 'reload':[build/'void_tests.exe','reload'],
 'numeric':[build/'void_tests.exe','numeric',exact_source]
}
test_file=scratch/'CTestTestfile.cmake'
test_file.write_text(''.join('add_test('+name+' '+' '.join('"'+str(arg).replace('\\','/')+'"' for arg in argv)+')\n'
                             for name,argv in names.items()))
ctest=r'C:\gcc\bin\ctest.exe'
selection=['--output-on-failure','-j','2','-R','^(analysis|reload|numeric)$','--repeat','until-fail:32']
debug=home/'VoidS7-G2-Debug'
with ThreadPoolExecutor(max_workers=2) as executor:
    a=executor.submit(execute,'release-overlap',[ctest,'--test-dir',str(scratch),*selection],scratch)
    b=executor.submit(execute,'debug-overlap',[ctest,'--test-dir',str(debug),*selection],debug)
    a.result()
    b.result()
evidence['status']='complete'
save()
