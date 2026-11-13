"""Build an isolated instrumented voidls from exact configured Linux recipes.

No production file/library is edited. The instrumented compiler translation unit
precedes the original static archives, so its definitions replace that archive
member. Source anchors are exact and fail closed after incompatible source edits.
Set VOID_G19_TRACE to a new output path to enable completion-only JSONL timings.
Without it the same binary supplies an observation-off control.

The --candidate variant is REJECTED for production: a disappeared project root
changes the original JSON-RPC error/server-exit behavior into an empty result.
It remains reproducible solely as preserved G19 experimental evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]

HEADER = r'''#pragma once
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
namespace g19 {
using Clock=std::chrono::steady_clock;
enum Id {dispatch,selection,first_capture,second_capture,dependency,probe,repair,
         graph_load,binding,candidates,result,response,recapture,count};
inline constexpr const char* names[]={"dispatch","selection","first_capture",
 "second_capture","dependency","probe","repair","graph_load","binding",
 "candidates","result","response","recapture"};
struct Phase;
inline thread_local bool active=false;
inline thread_local Phase* top=nullptr;
inline thread_local std::array<double,count> exclusive{},inclusive{};
inline thread_local std::array<unsigned,count> calls{};
inline double ms(Clock::duration d){return std::chrono::duration<double,std::milli>(d).count();}
struct Phase {
 Id id; bool enabled; Phase* parent=nullptr; Clock::time_point start; double children=0;
 explicit Phase(Id i):id(i),enabled(active){if(enabled){parent=top;top=this;start=Clock::now();}}
 void stop(){if(!enabled)return;auto elapsed=ms(Clock::now()-start);
  inclusive[id]+=elapsed;exclusive[id]+=elapsed-children;++calls[id];
  top=parent;if(parent)parent->children+=elapsed;enabled=false;}
 void next(Id i){stop();id=i;enabled=active;children=0;if(enabled){parent=top;top=this;start=Clock::now();}}
 ~Phase(){stop();}
};
struct Request {
 bool enabled=false; Clock::time_point start; double receipt_ms=0;
 Request(bool completion,Clock::time_point receipt){
  static const char* path=std::getenv("VOID_G19_TRACE");
  if(!completion || !path || !*path)return;
  enabled=active=true;top=nullptr;exclusive={};inclusive={};calls={};
  start=Clock::now();receipt_ms=ms(start-receipt);
 }
 ~Request(){if(!enabled)return;double elapsed=ms(Clock::now()-start);active=false;
  static FILE* file=std::fopen(std::getenv("VOID_G19_TRACE"),"a");
  if(!file)std::abort();
  std::fprintf(file,"{\"dispatch_ms\":%.9f,\"receipt_to_dispatch_ms\":%.9f,\"phases\":{",elapsed,receipt_ms);
  for(unsigned i=0;i<count;++i)std::fprintf(file,"%s\"%s\":{\"exclusive_ms\":%.9f,\"inclusive_ms\":%.9f,\"calls\":%u}",i?",":"",names[i],exclusive[i],inclusive[i],calls[i]);
  std::fprintf(file,"}}\n");std::fflush(file);
 }
};
}
'''


def replace(text, before, after):
    assert text.count(before) == 1, (before, text.count(before))
    return text.replace(before, after)


def instrument(directory, candidate=False):
    compiler = (ROOT / 'src/frontend/compiler.cpp').read_text()
    selection = (ROOT / 'src/frontend/project_selection.inc').read_text()
    server = (ROOT / 'src/lsp/main.cpp').read_text()
    if candidate:
        # Experimental only: a member probe already has exact overlay bytes.
        # Defer admission until the existing repaired-graph capture; never reuse
        # dependencies across requests, and never expose candidates before it.
        compiler = replace(compiler,
            'bool document=false,const EditorDiscovery* discovery=nullptr) {\n    const bool locked_source',
            'bool document=false,const EditorDiscovery* discovery=nullptr,const ManifestOverrides* pending_manifests=nullptr) {\n    const bool locked_source')
        compiler = replace(compiler,
            '    if(auto imports=import_assistance(entry,overrides,module,source,cursor,project))return *imports;',
            '    if(!pending_manifests)if(auto imports=import_assistance(entry,overrides,module,source,cursor,project))return *imports;')
        compiler = replace(compiler,
            'else if(project)(void)analyze_manifest_project_impl(project->root,project_entry,frozen,project->manifest_overlays,&probe,false,discovery,&recovery);',
            'else if(project || pending_manifests)(void)analyze_manifest_project_impl(project?project->root:entry,project_entry,frozen,project?project->manifest_overlays:*pending_manifests,&probe,false,discovery,&recovery);')
        anchor='    try {ProjectCapture capture(root,sources,manifests);return editor_assistance_impl'
        guard='''    if(!package_source(module))if(auto found=sources.find(module);found!=sources.end() && found->second) {
        const auto& text=found->second->text();
        if(cursor && cursor<=text.size() && text[cursor-1]=='.') {
            auto line=text.find_last_of("\\r\\n",cursor-1);line=line==std::string::npos?0:line+1;
            auto start=text.find_first_not_of(" \\t",line);
            if(start!=std::string::npos && text.compare(start,3,"use")!=0) {
                try {return editor_assistance_impl(root,sources,module,cursor,nullptr,entry,false,&discovery,&manifests);}
                catch(const CompilationError&){return {};}
            }
        }
    }
'''
        compiler = replace(compiler,anchor,guard+anchor)
    compiler = replace(compiler,
        '    try {ProjectCapture capture(root,sources,manifests);return editor_assistance_impl',
        '    try {g19::Phase timer(g19::first_capture);ProjectCapture capture(root,sources,manifests);timer.stop();return editor_assistance_impl')
    compiler = replace(compiler, '    const bool locked_source=project && package_source(module);',
        '    g19::Phase timer(g19::probe);\n    const bool locked_source=project && package_source(module);')
    compiler = replace(compiler, '    auto repaired=text;if(!operator_context)',
        '    timer.next(g19::repair);\n    auto repaired=text;if(!operator_context)')
    compiler = replace(compiler, '    auto frozen=overrides;frozen.insert_or_assign',
        '    timer.next(g19::probe);\n    auto frozen=overrides;frozen.insert_or_assign')
    compiler = replace(compiler,
        '    for(auto& [id,u]:units)if(u->application_body) {',
        '    g19::Phase timer(g19::binding);\n    for(auto& [id,u]:units)if(u->application_body) {')
    compiler = replace(compiler,
        '    VOID_SEMANTIC_HANDLER void editor_candidates(Expr& e, std::optional<Type> receiver = {}) {',
        '    VOID_SEMANTIC_HANDLER void editor_candidates(Expr& e, std::optional<Type> receiver = {}) {\n        g19::Phase timer(g19::candidates);')
    selection = replace(selection,
        'dependencies = project::verify_locked_dependencies(root, *manifest, !original);',
        'g19::Phase dependency_timer(g19::dependency);\n                dependencies = project::verify_locked_dependencies(root, *manifest, !original);')
    selection = replace(selection,
        '        owned=std::make_unique<ProjectCapture>(root,sources,manifests);auto& capture=*owned;',
        '        g19::Phase timer(g19::second_capture);\n        owned=std::make_unique<ProjectCapture>(root,sources,manifests);timer.next(g19::graph_load);auto& capture=*owned;')
    selection = replace(selection, '        ProjectCapture fresh(root,sources,manifests,&capture);',
        '        timer.next(g19::recapture);\n        ProjectCapture fresh(root,sources,manifests,&capture);')
    server = replace(server, '    j::Value editor_query(const std::string& method,const j::Object& params) {',
        '    j::Value editor_query(const std::string& method,const j::Object& params) {\n        g19::Phase timer(g19::selection);')
    server = replace(server, '            if(method=="textDocument/signatureHelp") {',
        '            timer.next(g19::result);\n            if(method=="textDocument/signatureHelp") {')
    server = replace(server, '        std::vector<std::optional<j::Value>> messages;',
        '        std::vector<std::optional<j::Value>> messages;\n        std::vector<g19::Clock::time_point> receipts;')
    server = replace(server, 'if(!body){eof=true;break;}bytes+=body->size();',
        'if(!body){eof=true;break;}receipts.push_back(g19::Clock::now());bytes+=body->size();')
    server = replace(server, '        for(auto& value:messages) {',
        '        std::size_t receipt_index=0;\n        for(auto& value:messages) {\n            const auto receipt=receipts[receipt_index++];\n            const auto* object=value?value->getAsObject():nullptr;\n            g19::Request measured(object && object->getString("method")=="textDocument/completion",receipt);\n            g19::Phase timer(g19::dispatch);')
    server = replace(server, 'void send(j::Value value) {',
        'void send(j::Value value) {\n    g19::Phase timer(g19::response);')
    for name, text in [('compiler.cpp',compiler),('project_selection.inc',selection),('main.cpp',server),('g19.hpp',HEADER)]:
        (directory/name).write_text(text)


def build(build_dir, output, candidate=False):
    if output == ROOT or ROOT in output.parents:
        raise ValueError('G19 overlays must be outside the checkout')
    output.mkdir(parents=True, exist_ok=False)
    instrument(output,candidate)
    recipes = json.loads((build_dir/'compile_commands.json').read_text())
    objects=[]
    commands=[]
    for source,include in [('src/frontend/compiler.cpp','src/frontend'),('src/lsp/main.cpp','src/lsp')]:
        recipe=next(r for r in recipes if Path(r['file'])==ROOT/source)
        args=shlex.split(recipe['command'])
        obj=output/(Path(source).stem+'.o')
        args[args.index('-o')+1]=str(obj)
        args[args.index('-c')+1]=str(output/Path(source).name)
        # Redirect dependency side effects as well as the object.
        for option in ['-MF','-MT']:
            if option in args: args[args.index(option)+1]=str(obj)+('.d' if option=='-MF' else '')
        args += ['-I'+str(ROOT/include),'-include',str(output/'g19.hpp')]
        commands.append(args)
        subprocess.run(args,cwd=recipe['directory'],check=True)
        objects.append(obj)
    args=shlex.split((build_dir/'CMakeFiles/voidls.dir/link.txt').read_text())
    i=next(i for i,a in enumerate(args) if a.endswith('/src/lsp/main.cpp.o'))
    args[i:i+1]=list(map(str,objects))
    args[args.index('-o')+1]=str(output/'voidls')
    for i,a in enumerate(args):
        if a.startswith('--dependency-file='): args[i]='--dependency-file='+str(output/'link.d')
        if a.startswith('-Wl,--thinlto-cache-dir='): args[i]='-Wl,--thinlto-cache-dir='+str(output/'thinlto')
    commands.append(args)
    subprocess.run(args,cwd=build_dir,check=True)
    (output/'recipes.json').write_text(json.dumps(commands,indent=2)+'\n')
    paths=['src/frontend/compiler.cpp','src/frontend/project_selection.inc','src/frontend/project_resolution.cpp','src/frontend/tooling.cpp','src/lsp/main.cpp']
    (output/'source-identity.json').write_text(json.dumps({p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build',type=Path)
    parser.add_argument('output',type=Path,help='New disposable directory outside checkout')
    parser.add_argument('--candidate',action='store_true',help='REJECTED G19 experiment: known root-error equivalence failure; never production')
    args=parser.parse_args()
    build(args.build.resolve(),args.output.resolve(),args.candidate)
