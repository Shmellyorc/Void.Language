"""Verify the existing 15 gallery contracts with the existing CLI/async adapters."""
import argparse,html.parser,json,pathlib,re,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('build');p.add_argument('--compiler',default='clang++-19');p.add_argument('--output',required=True);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[1];build=pathlib.Path(a.build).resolve()
class Gallery(html.parser.HTMLParser):
    cases=[]
    def handle_starttag(self,tag,attrs):
        attrs=dict(attrs)
        if attrs.get('class')=='guide-result':self.cases.append((attrs['data-source'],attrs['data-input'],attrs['data-expected']))
gallery=Gallery();gallery.feed((repo/'website/public/examples/index.html').read_text());assert len(gallery.cases)==15
results=[]
def run(command):return subprocess.check_output([str(x) for x in command],text=True,stderr=subprocess.STDOUT).strip()
with tempfile.TemporaryDirectory(prefix='VOID examples acceptance ') as d:
    for n,(url,value,expected) in enumerate(gallery.cases):
        source=repo/'website/public'/url.lstrip('/');async_entry=bool(re.search(r'^async func entry\(',source.read_text(),re.M))
        run([build/'voidc','check',source])
        for optimized in (False,True):
            if not async_entry:assert run([build/'voidc','run',source,value,*([] if optimized else ['--no-opt'])])==expected,(url,'JIT')
            obj=pathlib.Path(d)/f'{n}-{optimized}.o';exe=obj.with_suffix('')
            if async_entry:run([build/'void_async_tests','--emit',source,obj,'optimized' if optimized else 'normal'])
            else:run([build/'voidc','emit',source,'-o',obj,*([] if optimized else ['--no-opt'])])
            run([a.compiler,obj,build/'libvoid_runtime.a',build/'libvoid_native_fixture.a','-o',exe])
            assert run([exe,value])==expected,(url,'AOT')
        results.append({'source':url,'input':value,'expected':expected,'check':True,'normal_o2_jit':not async_entry,'normal_o2_aot':True,'async_adapter':async_entry})
pathlib.Path(a.output).write_text(json.dumps(results,indent=2)+'\n');print('15 gallery contracts: checks and normal/O2 native AOT passed; synchronous normal/O2 JIT passed')
