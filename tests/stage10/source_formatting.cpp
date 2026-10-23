#include "void/backend.hpp"
#include "void/tooling.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
unsigned checks=0;
void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);++checks;}
auto analyze(const std::string& text){auto a=vd::analyze_source({"/format.void",text});if(!a.program)throw vd::CompilationError(a.diagnostics);++checks;return a.program;}
auto bindings(const vd::ValidatedAnalysis& a){std::multiset<std::tuple<std::string,std::string,std::string,bool,std::string>> out;for(const auto* refs:{&a.references(),&a.editor_references()})for(const auto& r:*refs)out.emplace(r.from,r.to,r.spelling,r.implicit,r.binding_contract);return out;}
std::string format(const std::string& text){auto out=vd::format_source({"/format.void",text});check(bool(out),"safe formatting refused:\n"+text);check(vd::format_source({"/format.void",*out})==out,"formatting not idempotent");auto before=analyze(text),after=analyze(*out);check(before->compatibility_contract()==after->compatibility_contract(),"formatting changed contracts");check(bindings(*before)==bindings(*after),"formatting changed bindings");vd::ir::validate(after->lower());++checks;check(before->modules().front().source.text()==text,"formatter mutated original source");return *out;}
std::string crlf(std::string text){std::string result;for(char c:text){if(c=='\n')result+='\r';result+=c;}return result;}
const std::string program=R"(# preserve é😀 and comment spacing  
value struct box<t>:
  private secret: t
  const id: i32 = 7
  value: t
  operator +(const left: box<t>,const right: box<t>) -> box<t>:
    return box<t>(value=left.value+right.value)
  operator %(const left: box<t>,const right: box<t>) -> box<t>:
    return box<t>(value=left.value%right.value)
  operator ==(const left: box<t>,const right: box<t>) -> bool:
    return left.value==right.value
func (self box<t>) string() -> string:
  return "{{box}}:{self.value:000}"
private func (self box<t>) read() -> t:
  return self.secret
private property (self box<t>) amount: t:
  get:
    return self.value
  set(value):
    self.value=value

func entry(input: i32)->i32:
  var item=box<i32>(value=input)
  item+=box<i32>(value=2)
  item%=box<i32>(value=5)
  item.amount+=1 # preserve this comment  
  var text="é😀 {{escaped}} {item.value:>12|000} {"nested {input}"}"
  var triple="""first  
    {(
        input # } is comment text
        + 2
    ):000}
\u{1f600} {{ untouched }}
last"""
  if item!=box<i32>(value=0):
    return item.amount+item.id
  return input
)";
}
int main(){try{
    for(const auto& text:{program,crlf(program),std::string("\xef\xbb\xbf")+program,program.substr(0,program.size()-1)}){
        auto out=format(text);check(out!=text,"fixture did not exercise a rewrite");
        for(const auto& literal:{std::string("\"é😀 {{escaped}} {item.value:>12|000} {\"nested {input}\"}\""),std::string("# preserve é😀 and comment spacing  "),std::string("# preserve this comment  ")})check(out.find(literal)!=std::string::npos,"literal/comment bytes changed");
        auto begin=text.find("\"\"\"");auto end=text.find("\"\"\"",begin+3)+3;check(out.find(text.substr(begin,end-begin))!=std::string::npos,"triple literal changed");
        auto endings=[](const std::string& s){std::string result;for(char c:s)if(c=='\r'||c=='\n')result+=c;return result;};check(endings(out)==endings(text),"physical line endings changed");
    }
    // All glyphs and compounds remain physical tokens, with exact owner contracts.
    for(const std::string glyph:{"+","-","*","/","%","==","<",">","<=",">="}){
        bool comparison=glyph=="=="||glyph=="<"||glyph==">"||glyph=="<="||glyph==">=";
        auto text="value struct number:\n  value: i32\n  operator "+glyph+"(const left: number,const right: number) -> "+(comparison?"bool":"number")+":\n    return "+(comparison?"true":"left")+"\nfunc entry(input: i32)->i32:\n  var a=number(value=input)\n  var b=a"+glyph+"a\n";
        if(!comparison)text+="  a"+glyph+"=a\n";text+="  return input\n";format(text);
    }
    for(const std::string glyph:{"+","-"})format("value struct number:\n  value: i32\n  operator "+glyph+"(const value: number) -> number:\n    return value\nfunc entry(input: i32)->i32:\n  var a="+glyph+"number(value=input)\n  return a.value\n");
    const std::string unsafe="func entry(input: i32) -> i32:\n  var text = \"{(func() -> i32:\n    return input\n  )()}\"\n  return text.length as i32\n";
    analyze(unsafe);auto guarded=vd::format_source({"/format.void",unsafe});check(!guarded || *guarded==unsafe,"indent-sensitive hole received unsafe rewrite");
    const std::string nested="func entry(input: i32) -> i32:\n  var text = \"{(func() -> i32:\n        return input\n  )()}\"\n  return text.length as i32\n";format(nested);
    for(const auto& bad:{"func entry(:\n", "value struct n:\n  operator !=(const a: n,const b: n) -> bool:\n    return false\n", "func entry(input: i32) -> i32:\n  var text = \"{input + }\"\n  return input\n", "func entry(input: i32) -> i32:\n  var text = \"{(input]}\"\n  return input\n", "\tfunc entry(:\n"})check(!vd::format_source({"/format.void",bad}),"malformed syntax formatted");
    // Semantic failures remain errors, with spans in their own original snapshots.
    for(const auto& text:{std::string("func entry(input: i32) -> i32:\n  var text=\"é😀 {input:0.#0}\"\n  return missing\n"),crlf("func entry(input: i32) -> i32:\n  var text=\"é😀 {input:0.#0}\"\n  return missing\n")}){
        auto out=vd::format_source({"/format.void",text});check(bool(out),"syntactically valid diagnostic fixture refused");
        for(const auto& snapshot:{text,*out}){auto a=vd::analyze_source({"/format.void",snapshot});check(!a.program,"formatter concealed errors");bool pattern=false,unknown=false;for(const auto& d:a.diagnostics){check(d.primary.source->text()==snapshot,"diagnostic lost immutable source");auto at=d.primary.begin;check(at<=snapshot.size(),"diagnostic out of bounds");if(d.description.find("Required fraction")!=std::string::npos){pattern=true;check(at==snapshot.find("0.#0")+3,"pattern diagnostic not at original digit");}if(d.description.find("missing")!=std::string::npos){unknown=true;check(at==snapshot.find("missing"),"unknown value diagnostic shifted");}}check(pattern&&unknown,"expected diagnostic not retained");}
    }
    // Whitespace variants exercise determinism and stable indentation without a new lexer.
    for(unsigned indent:{1u,2u,3u,4u,6u,8u})for(bool windows:{false,true}){
        auto text="func entry(input:i32)->i32:\n"+std::string(indent,' ')+"var a=input # keep\n\n"+std::string(indent,' ')+"return a\n";
        if(windows)text=crlf(text);auto out=format(text);check(out.find("    var a = input # keep")!=std::string::npos,"indentation not canonical");
    }
    auto pretty=format(program);auto original=analyze(program),changed=analyze(pretty);for(bool optimized:{false,true}){vd::Jit jit;check(jit.compile(*original,optimized)->invoke(4)==9 && jit.compile(*changed,optimized)->invoke(4)==9,"formatter changed native effects/evaluation");}
    std::cout<<checks<<" source formatting/diagnostic checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
